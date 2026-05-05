// EditEnhancements.h
//
// Small editor behaviours that improve usability:
//   * Bracket auto-close (interactive, hooks ScintillaEdit::charAdded).
//   * Tab / Shift+Tab indent / unindent of the current selection (or current line
//     when no selection is active).
//   * "Reload from disk" helper that swaps the editor buffer for the on-disk
//     file's contents and resets the modified state.
//
// All editor-mutating helpers are static so callers can route shortcuts /
// menu actions straight to them. The auto-close behaviour requires an instance
// because it needs to manage the editor signal connection lifetime and persist
// its own enabled flag through QSettings.

#pragma once

#include <QObject>
#include <QString>

class ScintillaEdit;

class EditEnhancements : public QObject {
    Q_OBJECT
public:
    explicit EditEnhancements(QObject* parent = nullptr);

    bool autoCloseEnabled() const;
    void setAutoCloseEnabled(bool b);   // persisted

    // Installs / uninstalls the auto-close hook. Passing nullptr detaches.
    void setActiveEditor(ScintillaEdit* editor);

    // Indent / unindent the current selection (or the current line if no
    // selection). `tabWidth` is how many spaces equal one tab; the helper
    // chooses '\t' vs spaces based on the editor's SCI_GETUSETABS state.
    static void indentSelection(ScintillaEdit* editor, int tabWidth);
    static void unindentSelection(ScintillaEdit* editor, int tabWidth);

    // Reload the file at `path` and replace the editor's content. Returns true
    // on success. Sets the editor's modified state to clean. Caller should
    // re-apply lexer + theme afterwards.
    static bool reloadFromDisk(ScintillaEdit* editor, const QString& path,
                               QString* errorOut = nullptr);

signals:
    void autoCloseChanged(bool b);

private slots:
    void onCharAdded(int ch);

private:
    bool m_autoClose = true;
    ScintillaEdit* m_editor = nullptr;
    QMetaObject::Connection m_connCharAdded;
};
