// AutoCompleter.cpp
// Implementation notes:
//   * Uses ScintillaEdit's `charAdded(int)` notification to drive the
//     auto-trigger path; explicit triggers come through triggerNow().
//   * Word collection is performed in UTF-8 byte space so caret offsets
//     returned by Scintilla map naturally to QByteArray indices.
//   * Decoding to QString is done only for the final filter/sort/dedupe
//     step (and the eventual SCI_AUTOCSHOW payload, which is UTF-8).

#include "AutoCompleter.h"

#include <algorithm>

namespace {

// Settings keys (kept inline to keep this component dependency-free).
constexpr const char* kKeyAuto       = "autoComplete/auto";
constexpr const char* kKeyTriggerLen = "autoComplete/triggerLen";

constexpr int kMinTriggerLen = 1;
constexpr int kMaxTriggerLen = 6;
constexpr int kMaxEntries    = 200;

// Threshold above which we keep a word-list cache instead of rebuilding.
constexpr qint64 kLargeBufferBytes = 500 * 1024;

// Tolerated edit count before invalidating the cached word list.
constexpr quint64 kCacheModSlack = 50;

inline int clampTrigger(int n) {
    if (n < kMinTriggerLen) return kMinTriggerLen;
    if (n > kMaxTriggerLen) return kMaxTriggerLen;
    return n;
}

} // namespace

AutoCompleter::AutoCompleter(QObject* parent)
    : QObject(parent) {
    QSettings s;
    m_autoTrigger = s.value(kKeyAuto, true).toBool();
    m_triggerLen  = clampTrigger(s.value(kKeyTriggerLen, 3).toInt());
}

bool AutoCompleter::isWordChar(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

void AutoCompleter::setActiveEditor(ScintillaEdit* editor) {
    if (m_editor == editor)
        return;

    // Detach from previous editor.
    if (m_connCharAdded)
        QObject::disconnect(m_connCharAdded);
    if (m_connModified)
        QObject::disconnect(m_connModified);
    m_connCharAdded = QMetaObject::Connection();
    m_connModified  = QMetaObject::Connection();

    // Reset cache state on editor switch.
    m_cachedWords.clear();
    m_cachedTextLen = -1;
    m_cachedModCounter = 0;
    m_modCounter = 0;
    m_lastBuildModCounter = 0;

    m_editor = editor;
    if (!m_editor)
        return;

    configureEditor();

    m_connCharAdded = QObject::connect(
        m_editor, &ScintillaEditBase::charAdded,
        this, &AutoCompleter::onCharAdded);
    m_connModified = QObject::connect(
        m_editor, &ScintillaEditBase::modified,
        this, &AutoCompleter::onModified);
}

void AutoCompleter::configureEditor() {
    if (!m_editor) return;
    // Respect case for both filtering and matching.
    m_editor->autoCSetIgnoreCase(false);
    m_editor->autoCSetCaseInsensitiveBehaviour(0 /* SC_CASEINSENSITIVEBEHAVIOUR_RESPECTCASE */);
    m_editor->autoCSetSeparator(static_cast<sptr_t>(' '));
    m_editor->autoCSetMaxHeight(8);
}

bool AutoCompleter::isAutoTrigger() const {
    return m_autoTrigger;
}

void AutoCompleter::setAutoTrigger(bool b) {
    if (m_autoTrigger == b) return;
    m_autoTrigger = b;
    QSettings s;
    s.setValue(kKeyAuto, b);
    s.sync();
}

int AutoCompleter::triggerLength() const {
    return m_triggerLen;
}

void AutoCompleter::setTriggerLength(int n) {
    const int clamped = clampTrigger(n);
    if (m_triggerLen == clamped) return;
    m_triggerLen = clamped;
    QSettings s;
    s.setValue(kKeyTriggerLen, clamped);
    s.sync();
}

void AutoCompleter::onCharAdded(int /*ch*/) {
    if (!m_autoTrigger) return;
    showCompletions(m_triggerLen);
}

void AutoCompleter::onModified(Scintilla::ModificationFlags /*type*/,
                               Scintilla::Position /*position*/,
                               Scintilla::Position /*length*/,
                               Scintilla::Position /*linesAdded*/,
                               const QByteArray& /*text*/,
                               Scintilla::Position /*line*/,
                               Scintilla::FoldLevel /*foldNow*/,
                               Scintilla::FoldLevel /*foldPrev*/) {
    // Coarse modification counter: any change bumps it. The cache validity
    // check uses the difference between this counter and the value saved at
    // the last cache rebuild.
    ++m_modCounter;
}

void AutoCompleter::triggerNow() {
    // Bypass the configured minimum: at least 1 char prefix is enough.
    showCompletions(1);
}

QString AutoCompleter::currentPrefix(int& prefixByteLen) const {
    prefixByteLen = 0;
    if (!m_editor) return QString();

    const sptr_t pos = m_editor->currentPos();
    if (pos <= 0) return QString();

    // Walk left over word chars by re-reading the buffer in small chunks.
    // Fastest path: ask Scintilla for the word start at this position.
    const sptr_t wordStart = m_editor->wordStartPosition(pos, /*onlyWordCharacters=*/true);
    if (wordStart >= pos) return QString();

    QByteArray range = m_editor->textRange(static_cast<int>(wordStart),
                                           static_cast<int>(pos));
    if (range.isEmpty()) return QString();

    QString prefix = QString::fromUtf8(range);

    // Filter again on our own definition of word chars (Scintilla's may differ
    // from ours for non-ASCII letters depending on configured word chars).
    int keep = 0;
    for (int i = 0; i < prefix.size(); ++i) {
        if (isWordChar(prefix.at(i))) ++keep;
        else { keep = 0; /* break sequence; only contiguous run before caret matters */ }
    }
    // Take the trailing contiguous run.
    int start = prefix.size();
    while (start > 0 && isWordChar(prefix.at(start - 1))) --start;
    prefix = prefix.mid(start);

    prefixByteLen = prefix.toUtf8().size();
    (void)keep;
    return prefix;
}

QStringList AutoCompleter::collectWords() {
    if (!m_editor) return {};

    const sptr_t lenS = m_editor->textLength();
    const qint64 textLen = static_cast<qint64>(lenS);

    // Cache decision: only memoise for large buffers.
    const bool largeBuffer = textLen > kLargeBufferBytes;
    if (largeBuffer && !m_cachedWords.isEmpty()
        && m_cachedTextLen == textLen
        && (m_modCounter - m_lastBuildModCounter) <= kCacheModSlack) {
        return m_cachedWords;
    }

    QByteArray bytes = m_editor->getText(static_cast<sptr_t>(textLen + 1));
    // getText() may include the trailing NUL; trim it.
    if (bytes.endsWith('\0')) bytes.chop(1);

    QString text = QString::fromUtf8(bytes);

    QSet<QString> uniq;
    uniq.reserve(1024);
    const int n = text.size();
    int i = 0;
    while (i < n) {
        // Skip non-word characters.
        while (i < n && !isWordChar(text.at(i))) ++i;
        const int start = i;
        while (i < n && isWordChar(text.at(i))) ++i;
        if (i > start) {
            uniq.insert(text.mid(start, i - start));
        }
    }

    QStringList words(uniq.begin(), uniq.end());
    std::sort(words.begin(), words.end());

    if (largeBuffer) {
        m_cachedWords = words;
        m_cachedTextLen = textLen;
        m_lastBuildModCounter = m_modCounter;
        m_cachedModCounter = m_modCounter;
    } else {
        // Drop any stale cache to free memory once we leave the large regime.
        m_cachedWords.clear();
        m_cachedTextLen = -1;
    }
    return words;
}

void AutoCompleter::showCompletions(int minLen) {
    if (!m_editor) return;

    int prefixByteLen = 0;
    const QString prefix = currentPrefix(prefixByteLen);
    if (prefix.isEmpty() || prefix.size() < minLen) {
        if (m_editor->autoCActive())
            m_editor->autoCCancel();
        return;
    }

    QStringList all = collectWords();
    if (all.isEmpty()) {
        if (m_editor->autoCActive())
            m_editor->autoCCancel();
        return;
    }

    // Filter case-sensitively by prefix and exclude an exact match for the
    // word currently being typed.
    QStringList matches;
    matches.reserve(qMin(all.size(), kMaxEntries));
    for (const QString& w : all) {
        if (!w.startsWith(prefix, Qt::CaseSensitive))
            continue;
        if (w == prefix)
            continue;
        matches.append(w);
        if (matches.size() >= kMaxEntries)
            break;
    }

    if (matches.isEmpty()) {
        if (m_editor->autoCActive())
            m_editor->autoCCancel();
        return;
    }

    // Build space-separated UTF-8 list. Words don't contain spaces by
    // construction (only word chars), so ' ' is safe as separator.
    const QByteArray listUtf8 = matches.join(QLatin1Char(' ')).toUtf8();

    m_editor->autoCShow(static_cast<sptr_t>(prefixByteLen),
                        listUtf8.constData());
}
