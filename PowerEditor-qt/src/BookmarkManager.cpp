#include "BookmarkManager.h"

#include "ScintillaEdit.h"

namespace {
constexpr int kBookmarkMarker = 1;
constexpr int kBookmarkMask = (1 << kBookmarkMarker);
} // namespace

BookmarkManager::BookmarkManager(QObject* parent)
    : QObject(parent)
{
}

void BookmarkManager::setActiveEditor(ScintillaEdit* editor)
{
    m_editor = editor;
    if (!m_editor)
        return;

    // Configure the bookmark marker definition. SC_MARK_BOOKMARK with a
    // friendly red palette readable in both light and dark themes.
    m_editor->markerDefine(kBookmarkMarker, SC_MARK_BOOKMARK);
    m_editor->markerSetFore(kBookmarkMarker, 0xFF6464); // foreground
    m_editor->markerSetBack(kBookmarkMarker, 0xFFA0A0); // background
}

void BookmarkManager::toggleAtCaret()
{
    if (!m_editor)
        return;

    const sptr_t pos = m_editor->currentPos();
    const sptr_t line = m_editor->lineFromPosition(pos);
    const sptr_t markers = m_editor->markerGet(line);
    if (markers & kBookmarkMask)
        m_editor->markerDelete(line, kBookmarkMarker);
    else
        m_editor->markerAdd(line, kBookmarkMarker);
}

void BookmarkManager::gotoNext()
{
    if (!m_editor)
        return;

    const sptr_t pos = m_editor->currentPos();
    const sptr_t curLine = m_editor->lineFromPosition(pos);
    const sptr_t total = m_editor->lineCount();

    sptr_t target = m_editor->markerNext(curLine + 1, kBookmarkMask);
    if (target < 0) {
        // wrap from the start
        target = m_editor->markerNext(0, kBookmarkMask);
    }
    if (target < 0)
        return; // no bookmarks at all

    Q_UNUSED(total);
    m_editor->gotoLine(target);
}

void BookmarkManager::gotoPrevious()
{
    if (!m_editor)
        return;

    const sptr_t pos = m_editor->currentPos();
    const sptr_t curLine = m_editor->lineFromPosition(pos);
    const sptr_t total = m_editor->lineCount();

    sptr_t target = -1;
    if (curLine > 0)
        target = m_editor->markerPrevious(curLine - 1, kBookmarkMask);
    if (target < 0) {
        // wrap from the bottom
        const sptr_t last = (total > 0) ? (total - 1) : 0;
        target = m_editor->markerPrevious(last, kBookmarkMask);
    }
    if (target < 0)
        return;

    m_editor->gotoLine(target);
}

void BookmarkManager::clearAll()
{
    if (!m_editor)
        return;
    m_editor->markerDeleteAll(kBookmarkMarker);
}

QList<int> BookmarkManager::bookmarkedLines() const
{
    QList<int> result;
    if (!m_editor)
        return result;

    sptr_t line = 0;
    while (true) {
        const sptr_t found = m_editor->markerNext(line, kBookmarkMask);
        if (found < 0)
            break;
        result.append(static_cast<int>(found));
        line = found + 1;
    }
    return result;
}
