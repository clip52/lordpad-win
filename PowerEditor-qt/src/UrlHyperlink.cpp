#include "UrlHyperlink.h"

#include <QAbstractScrollArea>
#include <QByteArray>
#include <QDesktopServices>
#include <QEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <Scintilla.h>

namespace {

// SCI message numbers — using the macros from Scintilla.h directly
// keeps us in lockstep with the bundled Scintilla version.
constexpr int kSciSetIndicatorCurrent = SCI_SETINDICATORCURRENT;
constexpr int kSciIndicatorFillRange  = SCI_INDICATORFILLRANGE;
constexpr int kSciIndicatorClearRange = SCI_INDICATORCLEARRANGE;
constexpr int kSciIndicatorAllOnFor   = SCI_INDICATORALLONFOR;
constexpr int kSciIndicatorStart      = SCI_INDICATORSTART;
constexpr int kSciIndicatorEnd        = SCI_INDICATOREND;
constexpr int kSciIndicSetStyle       = SCI_INDICSETSTYLE;
constexpr int kSciIndicSetFore        = SCI_INDICSETFORE;
constexpr int kSciIndicSetUnder       = SCI_INDICSETUNDER;
constexpr int kSciIndicSetHoverStyle  = SCI_INDICSETHOVERSTYLE;
constexpr int kSciIndicSetHoverFore   = SCI_INDICSETHOVERFORE;
constexpr int kSciPositionFromPoint   = SCI_POSITIONFROMPOINT;
constexpr int kSciTextLength          = SCI_GETTEXTLENGTH;
constexpr int kIndicPlain             = INDIC_PLAIN;

// Scintilla colors are 0x00BBGGRR. Hyperlink blue (#3366CC):
// R=0x33, G=0x66, B=0xCC -> 0xCC6633.
constexpr sptr_t kHyperlinkColor = 0xCC6633;

// Regex matching the most common URL schemes. Trailing punctuation
// is trimmed below so URLs at end-of-sentence still open cleanly.
const char* kUrlRegexPattern =
    R"((?:https?|ftp|file)://[^\s"'<>()\[\]{}]+)";

// Trailing punctuation that's almost never part of a real URL.
const QString kTrailingTrim = QStringLiteral(".,;:!?)\"'>");

QString sanitizeUrl(QString url) {
    while (!url.isEmpty() && kTrailingTrim.contains(url.back())) {
        url.chop(1);
    }
    return url;
}

UrlHyperlink* sharedInstance() {
    static UrlHyperlink instance;
    return &instance;
}

} // namespace

UrlHyperlink::UrlHyperlink(QObject* parent) : QObject(parent) {}

UrlHyperlink::~UrlHyperlink() = default;

void UrlHyperlink::installFor(ScintillaEdit* editor) {
    sharedInstance()->attach(editor);
}

void UrlHyperlink::attach(ScintillaEdit* editor) {
    if (!editor || m_entries.contains(editor)) {
        return;
    }

    EditorEntry entry;
    entry.editor   = editor;
    entry.debounce = new QTimer(this);
    entry.debounce->setSingleShot(true);
    entry.debounce->setInterval(300);
    entry.debounce->setProperty("urlhl_editor",
                                QVariant::fromValue<QObject*>(editor));
    connect(entry.debounce, &QTimer::timeout,
            this, &UrlHyperlink::onRescanTimeout);

    m_entries.insert(editor, entry);

    connect(editor, &QObject::destroyed,
            this, &UrlHyperlink::onEditorDestroyed);

    // notifyChange() fires on any document modification — perfect
    // hook for "text changed" debounced rescans.
    connect(editor, &ScintillaEditBase::notifyChange,
            this, [this, editor]() { scheduleRescan(editor); });

    if (m_enabled) {
        configureIndicator(editor);
    }

    // Install event filter on the viewport (Scintilla is a
    // QAbstractScrollArea) so we see Ctrl+Click before Scintilla.
    if (auto* scroll = qobject_cast<QAbstractScrollArea*>(editor)) {
        QWidget* vp = scroll->viewport();
        if (vp) {
            vp->installEventFilter(this);
            m_viewportToEditor.insert(vp, editor);
        }
    }

    // Initial scan so URLs already in the document are highlighted.
    if (m_enabled) {
        rescan(editor);
    }
}

void UrlHyperlink::detach(ScintillaEdit* editor) {
    if (!editor) {
        return;
    }
    auto it = m_entries.find(editor);
    if (it == m_entries.end()) {
        return;
    }

    disconnect(editor, nullptr, this, nullptr);

    if (auto* scroll = qobject_cast<QAbstractScrollArea*>(editor)) {
        QWidget* vp = scroll->viewport();
        if (vp) {
            vp->removeEventFilter(this);
            m_viewportToEditor.remove(vp);
        }
    }

    clearIndicators(editor);

    if (it->debounce) {
        it->debounce->stop();
        it->debounce->deleteLater();
    }
    m_entries.erase(it);
}

void UrlHyperlink::setEnabled(bool on) {
    if (m_enabled == on) {
        return;
    }
    m_enabled = on;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        ScintillaEdit* editor = it.key();
        if (!editor) continue;
        if (m_enabled) {
            configureIndicator(editor);
            rescan(editor);
        } else {
            clearIndicators(editor);
        }
    }
}

void UrlHyperlink::onEditorDestroyed(QObject* obj) {
    // Editor is being destroyed; remove its entry and any viewport
    // mapping pointing at it.
    auto* editor = static_cast<ScintillaEdit*>(obj);
    auto it = m_entries.find(editor);
    if (it != m_entries.end()) {
        if (it->debounce) {
            it->debounce->stop();
            it->debounce->deleteLater();
        }
        m_entries.erase(it);
    }
    for (auto vit = m_viewportToEditor.begin();
         vit != m_viewportToEditor.end();) {
        if (vit.value() == editor) {
            vit = m_viewportToEditor.erase(vit);
        } else {
            ++vit;
        }
    }
}

void UrlHyperlink::onRescanTimeout() {
    auto* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    auto* obj = timer->property("urlhl_editor").value<QObject*>();
    auto* editor = qobject_cast<ScintillaEdit*>(obj);
    if (editor && m_enabled) {
        rescan(editor);
    }
}

void UrlHyperlink::configureIndicator(ScintillaEdit* editor) const {
    if (!editor) return;
    editor->send(kSciIndicSetStyle, kIndicator, kIndicPlain);
    editor->send(kSciIndicSetFore,  kIndicator, kHyperlinkColor);
    editor->send(kSciIndicSetUnder, kIndicator, 1);
    // Hover style: keep style (plain) but flip the cursor — and use
    // a slightly darker shade. We re-use the same style number
    // because INDIC_PLAIN is already underline-friendly.
    editor->send(kSciIndicSetHoverStyle, kIndicator, kIndicPlain);
    editor->send(kSciIndicSetHoverFore,  kIndicator, kHyperlinkColor);
}

void UrlHyperlink::scheduleRescan(ScintillaEdit* editor) {
    if (!m_enabled) return;
    auto it = m_entries.find(editor);
    if (it == m_entries.end() || !it->debounce) return;
    it->debounce->start();
}

void UrlHyperlink::clearIndicators(ScintillaEdit* editor) const {
    if (!editor) return;
    const sptr_t length = editor->send(kSciTextLength);
    if (length <= 0) return;
    editor->send(kSciSetIndicatorCurrent, kIndicator);
    editor->send(kSciIndicatorClearRange, 0, length);
}

void UrlHyperlink::rescan(ScintillaEdit* editor) {
    if (!editor) return;

    // Pull the document text. For huge files this is a copy, but
    // it keeps the matcher simple; debouncing keeps the cost bounded.
    QByteArray bytes = editor->getText(editor->textLength() + 1);
    // textLength + 1 returns NUL-terminated buffer; trim that NUL.
    if (bytes.endsWith('\0')) {
        bytes.chop(1);
    }
    const QString text = QString::fromUtf8(bytes);

    editor->send(kSciSetIndicatorCurrent, kIndicator);
    editor->send(kSciIndicatorClearRange, 0, bytes.size());

    static const QRegularExpression re(QString::fromLatin1(kUrlRegexPattern));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        QString matched = m.captured(0);
        const QString trimmed = sanitizeUrl(matched);
        if (trimmed.isEmpty()) continue;

        // Convert QString character offsets to UTF-8 byte offsets,
        // since Scintilla works in bytes.
        const int qStart = m.capturedStart(0);
        const int qLen   = trimmed.length();
        const QByteArray prefix = text.left(qStart).toUtf8();
        const QByteArray chunk  = text.mid(qStart, qLen).toUtf8();
        editor->send(kSciIndicatorFillRange, prefix.size(), chunk.size());
    }
}

bool UrlHyperlink::eventFilter(QObject* watched, QEvent* event) {
    if (!m_enabled) {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseButtonPress) {
        return QObject::eventFilter(watched, event);
    }
    auto editorIt = m_viewportToEditor.find(watched);
    if (editorIt == m_viewportToEditor.end() || !editorIt.value()) {
        return QObject::eventFilter(watched, event);
    }
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton ||
        !(mouse->modifiers() & Qt::ControlModifier)) {
        return QObject::eventFilter(watched, event);
    }

    const QPoint pos = mouse->position().toPoint();
    if (tryOpenAt(editorIt.value(), pos)) {
        // Swallow the event so Scintilla doesn't move the caret /
        // start a selection on the Ctrl+Click.
        event->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool UrlHyperlink::tryOpenAt(ScintillaEdit* editor,
                             const QPoint& viewportPos) const {
    if (!editor) return false;
    const sptr_t pos = editor->send(kSciPositionFromPoint,
                                    viewportPos.x(), viewportPos.y());
    if (pos < 0) return false;

    // Bitmask of all indicators on at this position; bit N is
    // indicator N. If our bit is unset, no URL here.
    const sptr_t mask = editor->send(kSciIndicatorAllOnFor, pos);
    if ((mask & (sptr_t(1) << kIndicator)) == 0) {
        return false;
    }

    const sptr_t start = editor->send(kSciIndicatorStart, kIndicator, pos);
    const sptr_t end   = editor->send(kSciIndicatorEnd,   kIndicator, pos);
    if (end <= start) return false;

    const QByteArray bytes = editor->textRange(static_cast<int>(start),
                                               static_cast<int>(end));
    QString url = QString::fromUtf8(bytes);
    url = sanitizeUrl(url);
    if (url.isEmpty()) return false;

    QDesktopServices::openUrl(QUrl(url));
    return true;
}
