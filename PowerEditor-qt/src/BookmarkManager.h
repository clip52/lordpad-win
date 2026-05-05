#pragma once
#include <QObject>
#include <QList>

class ScintillaEdit;

class BookmarkManager : public QObject {
    Q_OBJECT
public:
    explicit BookmarkManager(QObject* parent = nullptr);

    // Bind to an editor. Calling with nullptr detaches.
    // The manager will (re-)apply the bookmark marker definition to the editor.
    void setActiveEditor(ScintillaEdit* editor);

    // Toggle bookmark on the line containing the caret. (1-based line OK; uses caret's line.)
    void toggleAtCaret();

    // Move caret to the next/previous bookmarked line (wrapping). No-op if no bookmarks.
    void gotoNext();
    void gotoPrevious();

    // Remove all bookmarks in the active editor.
    void clearAll();

    // Returns the 0-based line numbers of all current bookmarks, sorted ascending.
    QList<int> bookmarkedLines() const;

private:
    ScintillaEdit* m_editor = nullptr;
};
