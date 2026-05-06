#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>

#include "ScintillaEdit.h"

class QTimer;
class QMouseEvent;
class QEvent;

// UrlHyperlink — detects URLs in a ScintillaEdit and lets the user
// Ctrl+Click them to open in the system browser. Standalone module:
// no edits to existing files are required to use it. Just call
// UrlHyperlink::installFor(editor) once for any new editor.
//
// Implementation uses a Scintilla indicator (number 8 — above the
// 0..7 range typically reserved by Lexilla lexers) drawn in blue/
// underlined. Rescans are debounced (300 ms) on every text change.
class UrlHyperlink : public QObject {
    Q_OBJECT
public:
    // Indicator number used to mark URL ranges. Picked above the
    // typical lexer-used range (0..7).
    static constexpr int kIndicator = 8;

    explicit UrlHyperlink(QObject* parent = nullptr);
    ~UrlHyperlink() override;

    // Attach the hyperlink behavior to a ScintillaEdit. Idempotent:
    // attaching the same editor twice is a no-op. Automatic detach
    // happens when the editor is destroyed.
    void attach(ScintillaEdit* editor);

    // Detach from a previously attached editor. Indicators are
    // cleared and click interception is removed.
    void detach(ScintillaEdit* editor);

    // Globally enable/disable URL detection across all attached
    // editors. When disabled, indicators are cleared and clicks are
    // not intercepted.
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

    // Convenience entry point for callers that don't want to manage
    // a UrlHyperlink instance themselves: a single shared singleton
    // is used internally.
    static void installFor(ScintillaEdit* editor);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onEditorDestroyed(QObject* obj);
    void onRescanTimeout();

private:
    struct EditorEntry {
        QPointer<ScintillaEdit> editor;
        QTimer* debounce = nullptr;
    };

    void configureIndicator(ScintillaEdit* editor) const;
    void scheduleRescan(ScintillaEdit* editor);
    void rescan(ScintillaEdit* editor);
    void clearIndicators(ScintillaEdit* editor) const;
    bool tryOpenAt(ScintillaEdit* editor, const QPoint& viewportPos) const;

    bool m_enabled = true;
    QHash<ScintillaEdit*, EditorEntry> m_entries;
    // Track viewport widgets we've installed an event filter on, so
    // we can find the owning editor from a filtered QObject quickly.
    QHash<QObject*, ScintillaEdit*> m_viewportToEditor;
};
