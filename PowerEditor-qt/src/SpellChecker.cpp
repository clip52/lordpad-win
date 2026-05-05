#include "SpellChecker.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QByteArray>

#include <hunspell/hunspell.hxx>
#include "ScintillaEdit.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr const char* kDictDir       = "/usr/share/hunspell";
constexpr const char* kSettingsLang  = "spellCheck/lang";
constexpr const char* kSettingsEnab  = "spellCheck/enabled";
constexpr const char* kFallbackLang1 = "pt_BR";
constexpr const char* kFallbackLang2 = "en_US";

QString affPathFor(const QString& code) {
    return QStringLiteral("%1/%2.aff").arg(kDictDir, code);
}
QString dicPathFor(const QString& code) {
    return QStringLiteral("%1/%2.dic").arg(kDictDir, code);
}
bool dictExists(const QString& code) {
    return QFileInfo::exists(affPathFor(code)) && QFileInfo::exists(dicPathFor(code));
}
} // namespace

SpellChecker::SpellChecker(QObject* parent)
    : QObject(parent)
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);
    connect(m_debounce, &QTimer::timeout, this, &SpellChecker::runPendingRecheck);

    QSettings s;
    QString defaultLang = dictExists(kFallbackLang1)
                              ? QString::fromLatin1(kFallbackLang1)
                              : QString::fromLatin1(kFallbackLang2);
    m_lang    = s.value(kSettingsLang, defaultLang).toString();
    m_enabled = s.value(kSettingsEnab, false).toBool();

    // Load dictionary now (without triggering recheck — no editor yet).
    if (dictExists(m_lang)) {
        const QByteArray aff = affPathFor(m_lang).toLocal8Bit();
        const QByteArray dic = dicPathFor(m_lang).toLocal8Bit();
        m_hun = new Hunspell(aff.constData(), dic.constData());
    }
}

SpellChecker::~SpellChecker() {
    delete m_hun;
    m_hun = nullptr;
}

bool SpellChecker::isEnabled() const {
    return m_enabled;
}

void SpellChecker::setEnabled(bool b) {
    if (m_enabled == b) return;
    m_enabled = b;

    QSettings s;
    s.setValue(kSettingsEnab, b);

    if (b) {
        recheckAll();
    } else {
        clearAllIndicators();
    }
    emit enabledChanged(b);
}

QString SpellChecker::currentLanguage() const {
    return m_lang;
}

QStringList SpellChecker::availableLanguages() const {
    QDir d(kDictDir);
    QStringList affs = d.entryList(QStringList() << QStringLiteral("*.aff"),
                                   QDir::Files, QDir::Name);
    QStringList codes;
    codes.reserve(affs.size());
    for (const QString& f : affs) {
        QString base = QFileInfo(f).completeBaseName();
        // Only keep entries that have a matching .dic
        if (QFileInfo::exists(dicPathFor(base))) {
            codes << base;
        }
    }
    std::sort(codes.begin(), codes.end());
    codes.removeDuplicates();
    return codes;
}

bool SpellChecker::setLanguage(const QString& langCode) {
    if (!dictExists(langCode)) return false;

    delete m_hun;
    m_hun = nullptr;

    const QByteArray aff = affPathFor(langCode).toLocal8Bit();
    const QByteArray dic = dicPathFor(langCode).toLocal8Bit();
    m_hun = new Hunspell(aff.constData(), dic.constData());

    m_lang = langCode;
    QSettings s;
    s.setValue(kSettingsLang, m_lang);

    if (m_enabled && m_editor) {
        recheckAll();
    }
    emit languageChanged(m_lang);
    return true;
}

void SpellChecker::setActiveEditor(ScintillaEdit* editor) {
    if (m_editor == editor) return;

    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }
    m_editor = editor;
    m_indicatorConfigured = false;

    if (!m_editor) return;

    connect(m_editor, &ScintillaEditBase::modified,
            this, &SpellChecker::onModified);

    configureIndicator();

    if (m_enabled) {
        recheckAll();
    }
}

void SpellChecker::configureIndicator() {
    if (!m_editor || m_indicatorConfigured) return;
    // INDIC_SQUIGGLE = 1; Scintilla COLORREF is BBGGRR, so 0x0000FF == red.
    m_editor->indicSetStyle(kSpellIndicator, /*INDIC_SQUIGGLE*/ 1);
    m_editor->indicSetFore(kSpellIndicator, 0x0000FF);
    m_indicatorConfigured = true;
}

void SpellChecker::clearAllIndicators() {
    if (!m_editor) return;
    configureIndicator();
    const sptr_t len = m_editor->length();
    if (len <= 0) return;
    m_editor->setIndicatorCurrent(kSpellIndicator);
    m_editor->indicatorClearRange(0, len);
}

void SpellChecker::recheckAll() {
    if (!m_editor || !m_enabled || !m_hun) return;
    configureIndicator();

    const sptr_t total = m_editor->length();
    m_editor->setIndicatorCurrent(kSpellIndicator);
    if (total > 0) {
        m_editor->indicatorClearRange(0, total);
    }
    if (total <= 0) return;
    if (total > kMaxBufferBytes) {
        // Buffer too large — leave cleared, do nothing.
        return;
    }

    QByteArray buf = m_editor->get_text_range(0, static_cast<int>(total));
    checkWordsInText(buf, 0);
}

void SpellChecker::recheckRange(int start, int end) {
    if (!m_editor || !m_enabled || !m_hun) return;
    if (end <= start) return;
    configureIndicator();

    const sptr_t total = m_editor->length();
    if (total <= 0) return;
    if (total > kMaxBufferBytes) {
        // Too large — clear globally and bail.
        m_editor->setIndicatorCurrent(kSpellIndicator);
        m_editor->indicatorClearRange(0, total);
        return;
    }

    if (start < 0) start = 0;
    if (end > static_cast<int>(total)) end = static_cast<int>(total);
    if (end <= start) return;

    m_editor->setIndicatorCurrent(kSpellIndicator);
    m_editor->indicatorClearRange(start, end - start);

    QByteArray buf = m_editor->get_text_range(start, end);
    checkWordsInText(buf, start);
}

void SpellChecker::checkWordsInText(const QByteArray& utf8, int baseOffset) {
    if (!m_hun || utf8.isEmpty()) return;

    // Convert chunk to QString for unicode-aware splitting, but track byte
    // offsets via UTF-8 prefix lengths.
    const QString text = QString::fromUtf8(utf8);
    if (text.isEmpty()) return;

    static const QRegularExpression re(
        QStringLiteral("[^\\p{L}\\p{N}_]+"));

    // Splitting via regex would lose offsets; iterate via globalMatch on a
    // word-matching regex instead.
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}_]+"));

    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        const QString word = m.captured(0);
        if (word.length() < 2) continue;

        // Skip pure-digit tokens.
        bool allDigits = true;
        for (QChar c : word) {
            if (!c.isDigit()) { allDigits = false; break; }
        }
        if (allDigits) continue;

        const QByteArray wordUtf8 = word.toUtf8();
        const std::string ws = wordUtf8.toStdString();
        if (m_hun->spell(ws) != 0) continue; // OK

        // Compute byte start/length for indicator fill.
        const int charStart = m.capturedStart(0);
        const QString prefix = text.left(charStart);
        const int byteStart  = prefix.toUtf8().size();
        const int byteLen    = wordUtf8.size();

        m_editor->setIndicatorCurrent(kSpellIndicator);
        m_editor->indicatorFillRange(baseOffset + byteStart, byteLen);
    }
}

QStringList SpellChecker::suggestionsAt(int position) const {
    if (!m_editor || !m_hun) return {};
    QPair<int,int> r = wordRangeAt(position);
    if (r.first < 0 || r.second <= r.first) return {};

    QByteArray wordUtf8 = m_editor->get_text_range(r.first, r.second);
    if (wordUtf8.isEmpty()) return {};

    const std::string ws = wordUtf8.toStdString();
    if (m_hun->spell(ws) != 0) return {}; // word is OK

    std::vector<std::string> sugg = m_hun->suggest(ws);
    QStringList out;
    out.reserve(static_cast<int>(sugg.size()));
    for (const auto& s : sugg) {
        out << QString::fromUtf8(s.c_str(), static_cast<int>(s.size()));
        if (out.size() >= kMaxSuggestions) break;
    }
    return out;
}

QPair<int,int> SpellChecker::wordRangeAt(int position) const {
    if (!m_editor) return {-1, -1};
    const sptr_t total = m_editor->length();
    if (position < 0 || position > static_cast<int>(total)) return {-1, -1};

    // Use Scintilla's word boundary helpers.
    sptr_t start = m_editor->wordStartPosition(position, true);
    sptr_t end   = m_editor->wordEndPosition(position, true);
    if (end <= start) return {-1, -1};

    QByteArray w = m_editor->get_text_range(static_cast<int>(start),
                                            static_cast<int>(end));
    if (w.isEmpty()) return {-1, -1};

    // Reject ranges that contain no letter/digit (defensive).
    QString s = QString::fromUtf8(w);
    bool hasWordChar = false;
    for (QChar c : s) {
        if (c.isLetter() || c.isNumber() || c == QLatin1Char('_')) {
            hasWordChar = true;
            break;
        }
    }
    if (!hasWordChar) return {-1, -1};

    return {static_cast<int>(start), static_cast<int>(end)};
}

void SpellChecker::onModified(Scintilla::ModificationFlags type,
                              Scintilla::Position position,
                              Scintilla::Position length,
                              Scintilla::Position /*linesAdded*/,
                              const QByteArray& /*text*/,
                              Scintilla::Position /*line*/,
                              Scintilla::FoldLevel /*foldNow*/,
                              Scintilla::FoldLevel /*foldPrev*/)
{
    if (!m_enabled || !m_editor || !m_hun) return;

    // Only react to actual text insertions/deletions.
    using MF = Scintilla::ModificationFlags;
    const auto rawType = static_cast<unsigned int>(type);
    const auto rawInsert = static_cast<unsigned int>(MF::InsertText);
    const auto rawDelete = static_cast<unsigned int>(MF::DeleteText);
    if ((rawType & (rawInsert | rawDelete)) == 0) return;
    const bool isInsert = (rawType & rawInsert) != 0;

    const sptr_t total = m_editor->length();
    if (total > kMaxBufferBytes) {
        // Too big — clear once and bail.
        m_editor->setIndicatorCurrent(kSpellIndicator);
        if (total > 0) m_editor->indicatorClearRange(0, total);
        return;
    }

    // Compute touched line range.
    int from = static_cast<int>(position);
    int to   = isInsert ? static_cast<int>(position + length)
                        : static_cast<int>(position);
    if (from < 0) from = 0;
    if (to < from) to = from;
    if (to > static_cast<int>(total)) to = static_cast<int>(total);

    sptr_t firstLine = m_editor->lineFromPosition(from);
    sptr_t lastLine  = m_editor->lineFromPosition(to);
    int lineStart = static_cast<int>(m_editor->positionFromLine(firstLine));
    int lineEnd   = static_cast<int>(m_editor->lineEndPosition(lastLine));
    if (lineEnd < lineStart) lineEnd = lineStart;

    // Merge with any pending range and debounce.
    if (m_pendingStart < 0) {
        m_pendingStart = lineStart;
        m_pendingEnd   = lineEnd;
    } else {
        m_pendingStart = std::min(m_pendingStart, lineStart);
        m_pendingEnd   = std::max(m_pendingEnd,   lineEnd);
    }
    m_debounce->start();
}

void SpellChecker::runPendingRecheck() {
    if (!m_enabled || !m_editor || !m_hun) {
        m_pendingStart = m_pendingEnd = -1;
        m_pendingFull = false;
        return;
    }
    if (m_pendingFull || m_pendingStart < 0) {
        m_pendingStart = m_pendingEnd = -1;
        m_pendingFull = false;
        recheckAll();
        return;
    }
    int s = m_pendingStart;
    int e = m_pendingEnd;
    m_pendingStart = m_pendingEnd = -1;
    m_pendingFull = false;
    recheckRange(s, e);
}
