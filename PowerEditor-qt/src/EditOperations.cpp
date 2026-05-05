#include "EditOperations.h"

#include "ScintillaEdit.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Range helpers
// ---------------------------------------------------------------------------

// Determine the byte range to operate on. If selection is empty, we cover the
// whole document. Returns [start, end). Also reports whether the editor had a
// real selection (useful when the caller wants to restore it).
struct WorkRange {
    long long start = 0;
    long long end = 0;
    bool hadSelection = false;
};

WorkRange computeWorkRange(ScintillaEdit* editor)
{
    WorkRange r;
    if (editor->selectionEmpty()) {
        r.hadSelection = false;
        r.start = 0;
        r.end = editor->length();
    } else {
        r.hadSelection = true;
        r.start = editor->selectionStart();
        r.end = editor->selectionEnd();
    }
    return r;
}

// Read [start, end) from the buffer as a QByteArray (UTF-8 bytes from Scintilla).
QByteArray readRange(ScintillaEdit* editor, long long start, long long end)
{
    if (end <= start) {
        return QByteArray();
    }
    editor->setTargetRange(start, end);
    return editor->targetText();
}

// Replace [start, end) with `bytes` via SCI_TARGET. Returns the new end position.
long long replaceRange(ScintillaEdit* editor, long long start, long long end,
                       const QByteArray& bytes)
{
    editor->setTargetRange(start, end);
    editor->replaceTarget(bytes.size(), bytes.constData());
    return start + bytes.size();
}

// ---------------------------------------------------------------------------
// EOL helpers
// ---------------------------------------------------------------------------

// Split a single line's bytes into [body, eol]. The "eol" is the trailing
// "\r\n", "\n", or "\r" (if any). The body excludes that EOL.
void splitEol(const QByteArray& line, QByteArray& body, QByteArray& eol)
{
    int n = line.size();
    if (n >= 2 && line[n - 2] == '\r' && line[n - 1] == '\n') {
        body = line.left(n - 2);
        eol = QByteArray("\r\n");
    } else if (n >= 1 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
        body = line.left(n - 1);
        eol = QByteArray(1, line[n - 1]);
    } else {
        body = line;
        eol = QByteArray();
    }
}

// Returns whether `bytes` ends with any kind of EOL.
bool endsWithEol(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return false;
    char c = bytes[bytes.size() - 1];
    return c == '\n' || c == '\r';
}

// ---------------------------------------------------------------------------
// Line range helpers
// ---------------------------------------------------------------------------

// Compute first/last line indices touched by [start, end). If the range is the
// whole document and empty, [0, lineCount-1] is returned.
struct LineRange {
    long long firstLine = 0;
    long long lastLine = 0;     // inclusive
    long long blockStart = 0;   // byte offset of firstLine
    long long blockEnd = 0;     // byte offset just after lastLine (incl. its EOL)
};

LineRange computeLineRange(ScintillaEdit* editor, long long start, long long end)
{
    LineRange lr;
    long long lineCount = editor->lineCount();
    if (lineCount <= 0) {
        return lr;
    }

    lr.firstLine = editor->lineFromPosition(start);

    // If the range ends exactly at the start of a line and spans more than zero
    // characters, that trailing line shouldn't be included.
    long long endLine = editor->lineFromPosition(end);
    if (end > start && end == editor->positionFromLine(endLine) && endLine > lr.firstLine) {
        endLine -= 1;
    }
    if (endLine >= lineCount) endLine = lineCount - 1;
    lr.lastLine = endLine;

    lr.blockStart = editor->positionFromLine(lr.firstLine);

    // For the block end, take the start of (lastLine + 1) if it exists,
    // otherwise the document length.
    if (lr.lastLine + 1 < lineCount) {
        lr.blockEnd = editor->positionFromLine(lr.lastLine + 1);
    } else {
        lr.blockEnd = editor->length();
    }
    return lr;
}

// Pull the bytes of every line in [firstLine, lastLine] (each entry includes its
// trailing EOL if Scintilla provides it).
std::vector<QByteArray> readLines(ScintillaEdit* editor, long long firstLine,
                                  long long lastLine)
{
    std::vector<QByteArray> out;
    out.reserve(static_cast<size_t>(lastLine - firstLine + 1));
    for (long long line = firstLine; line <= lastLine; ++line) {
        out.push_back(editor->getLine(line));
    }
    return out;
}

// Decide which EOL to use when reassembling sorted lines: the first non-empty
// EOL we see, falling back to "\n".
QByteArray detectEol(const std::vector<QByteArray>& lines)
{
    for (const auto& line : lines) {
        QByteArray body;
        QByteArray eol;
        splitEol(line, body, eol);
        if (!eol.isEmpty()) {
            return eol;
        }
    }
    return QByteArray("\n");
}

// ---------------------------------------------------------------------------
// Title-case helper
// ---------------------------------------------------------------------------

QString toTitleCase(const QString& input)
{
    QString out;
    out.reserve(input.size());
    bool atWordStart = true;
    for (int i = 0; i < input.size(); ++i) {
        QChar ch = input.at(i);
        if (ch.isLetter()) {
            if (atWordStart) {
                out.append(ch.toUpper());
            } else {
                out.append(ch.toLower());
            }
            atWordStart = false;
        } else {
            out.append(ch);
            // Digits keep us inside a "word" (so "abc123def" -> "Abc123def").
            if (!ch.isLetterOrNumber()) {
                atWordStart = true;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Generic case-change driver
// ---------------------------------------------------------------------------

enum class CaseMode { Upper, Lower, Title };

void applyCase(ScintillaEdit* editor, CaseMode mode)
{
    WorkRange range = computeWorkRange(editor);
    if (range.end <= range.start) {
        return;
    }

    QByteArray bytes = readRange(editor, range.start, range.end);
    QString text = QString::fromUtf8(bytes);
    QString transformed;
    switch (mode) {
        case CaseMode::Upper: transformed = text.toUpper(); break;
        case CaseMode::Lower: transformed = text.toLower(); break;
        case CaseMode::Title: transformed = toTitleCase(text); break;
    }
    QByteArray newBytes = transformed.toUtf8();

    editor->beginUndoAction();
    long long newEnd = replaceRange(editor, range.start, range.end, newBytes);
    editor->endUndoAction();

    if (range.hadSelection) {
        editor->setSel(range.start, newEnd);
    }
}

// ---------------------------------------------------------------------------
// Sort variants
// ---------------------------------------------------------------------------

enum class SortMode { Ascending, Descending, Unique };

void sortLines(ScintillaEdit* editor, SortMode mode)
{
    WorkRange range = computeWorkRange(editor);

    LineRange lr;
    if (range.hadSelection) {
        lr = computeLineRange(editor, range.start, range.end);
    } else {
        long long lineCount = editor->lineCount();
        if (lineCount <= 0) return;
        lr.firstLine = 0;
        lr.lastLine = lineCount - 1;
        lr.blockStart = 0;
        lr.blockEnd = editor->length();
    }

    if (lr.lastLine < lr.firstLine) {
        return;
    }

    std::vector<QByteArray> lines = readLines(editor, lr.firstLine, lr.lastLine);
    if (lines.empty()) {
        return;
    }

    QByteArray fallbackEol = detectEol(lines);

    // Split each into body/eol.
    struct Item {
        QByteArray body;
        QByteArray eol;
    };
    std::vector<Item> items;
    items.reserve(lines.size());
    for (const auto& l : lines) {
        Item it;
        splitEol(l, it.body, it.eol);
        items.push_back(std::move(it));
    }

    if (mode == SortMode::Ascending || mode == SortMode::Descending) {
        std::stable_sort(items.begin(), items.end(),
                         [mode](const Item& a, const Item& b) {
                             QString sa = QString::fromUtf8(a.body);
                             QString sb = QString::fromUtf8(b.body);
                             int cmp = QString::localeAwareCompare(sa, sb);
                             return mode == SortMode::Ascending ? (cmp < 0) : (cmp > 0);
                         });
    } else { // Unique -- preserve first-occurrence order, drop later duplicates.
        std::vector<Item> unique;
        unique.reserve(items.size());
        for (auto& it : items) {
            bool seen = false;
            for (const auto& u : unique) {
                if (u.body == it.body) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                unique.push_back(std::move(it));
            }
        }
        items.swap(unique);
    }

    // Reassemble. Each line keeps its own EOL if it had one, otherwise we
    // append the detected fallback EOL -- except for the very last line, which
    // we leave terminator-less when its source line had no EOL (so we don't
    // mutate the trailing newline state of the document).
    QByteArray rebuilt;
    rebuilt.reserve(static_cast<int>(lr.blockEnd - lr.blockStart));

    // Was the last source line terminated?
    QByteArray lastBody;
    QByteArray lastEol;
    splitEol(lines.back(), lastBody, lastEol);
    bool sourceLastTerminated = !lastEol.isEmpty();

    for (size_t i = 0; i < items.size(); ++i) {
        rebuilt.append(items[i].body);
        bool isLast = (i + 1 == items.size());
        if (!isLast) {
            rebuilt.append(items[i].eol.isEmpty() ? fallbackEol : items[i].eol);
        } else {
            if (sourceLastTerminated) {
                rebuilt.append(items[i].eol.isEmpty() ? fallbackEol : items[i].eol);
            }
        }
    }

    editor->beginUndoAction();
    long long newEnd = replaceRange(editor, lr.blockStart, lr.blockEnd, rebuilt);
    editor->endUndoAction();

    if (range.hadSelection) {
        editor->setSel(lr.blockStart, newEnd);
    }
}

} // namespace

namespace EditOperations {

// ---------------------------------------------------------------------------
// trimTrailingWhitespace
// ---------------------------------------------------------------------------

void trimTrailingWhitespace(ScintillaEdit* editor)
{
    if (!editor) return;
    long long lineCount = editor->lineCount();
    if (lineCount <= 0) return;

    long long caret = editor->currentPos();
    long long docLen = editor->length();

    // Build the entire new document text in one pass and replace once.
    QByteArray rebuilt;
    rebuilt.reserve(static_cast<int>(docLen));

    bool changed = false;
    for (long long line = 0; line < lineCount; ++line) {
        QByteArray bytes = editor->getLine(line);
        QByteArray body;
        QByteArray eol;
        splitEol(bytes, body, eol);

        int trimEnd = body.size();
        while (trimEnd > 0) {
            char c = body[trimEnd - 1];
            if (c == ' ' || c == '\t' || c == '\r') {
                --trimEnd;
            } else {
                break;
            }
        }
        if (trimEnd != body.size()) {
            changed = true;
            body.truncate(trimEnd);
        }
        rebuilt.append(body);
        rebuilt.append(eol);
    }

    if (!changed) {
        return;
    }

    editor->beginUndoAction();
    editor->setTargetRange(0, docLen);
    editor->replaceTarget(rebuilt.size(), rebuilt.constData());
    editor->endUndoAction();

    // Best-effort caret restore: clamp to current document length.
    long long newLen = editor->length();
    if (caret > newLen) caret = newLen;
    if (caret < 0) caret = 0;
    editor->gotoPos(caret);
}

// ---------------------------------------------------------------------------
// Case conversions
// ---------------------------------------------------------------------------

void toUpperSelection(ScintillaEdit* editor)
{
    if (!editor) return;
    applyCase(editor, CaseMode::Upper);
}

void toLowerSelection(ScintillaEdit* editor)
{
    if (!editor) return;
    applyCase(editor, CaseMode::Lower);
}

void toTitleSelection(ScintillaEdit* editor)
{
    if (!editor) return;
    applyCase(editor, CaseMode::Title);
}

// ---------------------------------------------------------------------------
// Sort variants
// ---------------------------------------------------------------------------

void sortLinesAscending(ScintillaEdit* editor)
{
    if (!editor) return;
    sortLines(editor, SortMode::Ascending);
}

void sortLinesDescending(ScintillaEdit* editor)
{
    if (!editor) return;
    sortLines(editor, SortMode::Descending);
}

void sortLinesUnique(ScintillaEdit* editor)
{
    if (!editor) return;
    sortLines(editor, SortMode::Unique);
}

// ---------------------------------------------------------------------------
// duplicateLine
// ---------------------------------------------------------------------------

void duplicateLine(ScintillaEdit* editor)
{
    if (!editor) return;

    if (editor->selectionEmpty()) {
        long long caret = editor->currentPos();
        long long line = editor->lineFromPosition(caret);
        QByteArray lineBytes = editor->getLine(line);

        QByteArray body;
        QByteArray eol;
        splitEol(lineBytes, body, eol);

        // Build the duplicate -- always insert it as a new line *below* the
        // current one. If the source line lacked an EOL (last line of file),
        // we have to inject one before the duplicate so they don't merge.
        QByteArray insertion;
        if (eol.isEmpty()) {
            insertion = QByteArray("\n");
            insertion.append(body);
        } else {
            insertion = body;
            insertion.append(eol);
        }

        long long lineEnd;
        if (eol.isEmpty()) {
            lineEnd = editor->length();
        } else {
            // positionFromLine(line+1) is the start of the next line, i.e. just
            // past this line's EOL.
            long long lineCount = editor->lineCount();
            if (line + 1 < lineCount) {
                lineEnd = editor->positionFromLine(line + 1);
            } else {
                lineEnd = editor->length();
            }
        }

        editor->beginUndoAction();
        editor->insertText(lineEnd, insertion.constData());
        editor->endUndoAction();
    } else {
        long long start = editor->selectionStart();
        long long end = editor->selectionEnd();
        QByteArray bytes = readRange(editor, start, end);

        QByteArray insertion = bytes;
        if (!endsWithEol(bytes)) {
            insertion.prepend(QByteArray("\n"));
        }

        editor->beginUndoAction();
        editor->insertText(end, insertion.constData());
        editor->endUndoAction();
    }
}

// ---------------------------------------------------------------------------
// moveLineUp / moveLineDown
// ---------------------------------------------------------------------------

namespace {

void moveLineImpl(ScintillaEdit* editor, bool up)
{
    long long lineCount = editor->lineCount();
    if (lineCount <= 1) return;

    long long firstLine;
    long long lastLine;
    bool hadSelection = !editor->selectionEmpty();
    long long selStart = editor->selectionStart();
    long long selEnd = editor->selectionEnd();

    if (hadSelection) {
        firstLine = editor->lineFromPosition(selStart);
        long long endLine = editor->lineFromPosition(selEnd);
        if (selEnd > selStart && selEnd == editor->positionFromLine(endLine) && endLine > firstLine) {
            endLine -= 1;
        }
        lastLine = endLine;
    } else {
        long long caret = editor->currentPos();
        firstLine = editor->lineFromPosition(caret);
        lastLine = firstLine;
    }

    if (up) {
        if (firstLine == 0) return;
    } else {
        if (lastLine >= lineCount - 1) return;
    }

    // Block bytes (incl. their EOLs).
    long long blockStart = editor->positionFromLine(firstLine);
    long long blockEnd = (lastLine + 1 < lineCount)
                            ? editor->positionFromLine(lastLine + 1)
                            : editor->length();

    long long neighborLine = up ? (firstLine - 1) : (lastLine + 1);
    long long neighborStart = editor->positionFromLine(neighborLine);
    long long neighborEnd = (neighborLine + 1 < lineCount)
                                ? editor->positionFromLine(neighborLine + 1)
                                : editor->length();

    QByteArray blockBytes = readRange(editor, blockStart, blockEnd);
    QByteArray neighborBytes = readRange(editor, neighborStart, neighborEnd);

    // Edge case: the block reaches the EOF and lacks an EOL on its last line.
    // After the swap the (formerly) last line is in the middle, so it must end
    // with an EOL or it will be glued to the neighbor. Conversely, the neighbor
    // bytes (which become last) may already have one; if so, strip it so we
    // preserve the document's trailing-EOL state.
    bool blockHasTrailingEol = endsWithEol(blockBytes);
    bool neighborHasTrailingEol = endsWithEol(neighborBytes);

    QByteArray newRegion;
    QByteArray fallbackEol("\n");
    if (blockHasTrailingEol) {
        // Pick the block's EOL as fallback if available.
        QByteArray dummyBody;
        QByteArray be;
        splitEol(blockBytes, dummyBody, be);
        if (!be.isEmpty()) fallbackEol = be;
    } else if (neighborHasTrailingEol) {
        QByteArray dummyBody;
        QByteArray ne;
        splitEol(neighborBytes, dummyBody, ne);
        if (!ne.isEmpty()) fallbackEol = ne;
    }

    if (up) {
        // New layout: block, then neighbor.
        QByteArray b = blockBytes;
        QByteArray n = neighborBytes;
        if (!endsWithEol(b)) {
            b.append(fallbackEol);
        }
        if (endsWithEol(n) && !blockHasTrailingEol) {
            // The block originally was at EOF without EOL -- preserve that by
            // dropping the EOL from what is now the final line.
            QByteArray nb;
            QByteArray neol;
            splitEol(n, nb, neol);
            n = nb;
        }
        newRegion = b;
        newRegion.append(n);

        editor->beginUndoAction();
        long long regionStart = neighborStart;
        long long regionEnd = blockEnd;
        editor->setTargetRange(regionStart, regionEnd);
        editor->replaceTarget(newRegion.size(), newRegion.constData());
        editor->endUndoAction();

        if (hadSelection) {
            long long newSelStart = regionStart;
            long long newSelEnd = regionStart + b.size();
            editor->setSel(newSelStart, newSelEnd);
        } else {
            // Move caret with the line.
            long long caret = editor->currentPos();
            long long delta = caret - blockStart;
            editor->gotoPos(regionStart + delta);
        }
    } else {
        // Move down: new layout is neighbor, then block.
        QByteArray b = blockBytes;
        QByteArray n = neighborBytes;
        if (!endsWithEol(n)) {
            n.append(fallbackEol);
        }
        if (endsWithEol(b) && !neighborHasTrailingEol) {
            QByteArray bb;
            QByteArray beol;
            splitEol(b, bb, beol);
            b = bb;
        }
        newRegion = n;
        newRegion.append(b);

        editor->beginUndoAction();
        long long regionStart = blockStart;
        long long regionEnd = neighborEnd;
        editor->setTargetRange(regionStart, regionEnd);
        editor->replaceTarget(newRegion.size(), newRegion.constData());
        editor->endUndoAction();

        if (hadSelection) {
            long long newSelStart = regionStart + n.size();
            long long newSelEnd = newSelStart + b.size();
            editor->setSel(newSelStart, newSelEnd);
        } else {
            long long caret = editor->currentPos();
            long long delta = caret - blockStart;
            editor->gotoPos(regionStart + n.size() + delta);
        }
    }
}

} // namespace

void moveLineUp(ScintillaEdit* editor)
{
    if (!editor) return;
    moveLineImpl(editor, /*up*/ true);
}

void moveLineDown(ScintillaEdit* editor)
{
    if (!editor) return;
    moveLineImpl(editor, /*up*/ false);
}

// ---------------------------------------------------------------------------
// tabsToSpaces / spacesToTabs
// ---------------------------------------------------------------------------

void tabsToSpaces(ScintillaEdit* editor, int tabWidth)
{
    if (!editor) return;
    if (tabWidth <= 0) tabWidth = 4;

    WorkRange range = computeWorkRange(editor);
    if (range.end <= range.start) return;

    QByteArray bytes = readRange(editor, range.start, range.end);
    QByteArray spaces(tabWidth, ' ');

    QByteArray out;
    out.reserve(bytes.size());
    bool changed = false;
    for (int i = 0; i < bytes.size(); ++i) {
        char c = bytes[i];
        if (c == '\t') {
            out.append(spaces);
            changed = true;
        } else {
            out.append(c);
        }
    }

    if (!changed) return;

    editor->beginUndoAction();
    long long newEnd = replaceRange(editor, range.start, range.end, out);
    editor->endUndoAction();

    if (range.hadSelection) {
        editor->setSel(range.start, newEnd);
    }
}

void spacesToTabs(ScintillaEdit* editor, int tabWidth)
{
    if (!editor) return;
    if (tabWidth <= 0) tabWidth = 4;

    WorkRange range = computeWorkRange(editor);
    if (range.end <= range.start) return;

    QByteArray bytes = readRange(editor, range.start, range.end);

    // Walk byte by byte. Track whether we are currently at the start of a
    // logical line. Only convert runs of *exactly* `tabWidth` consecutive
    // spaces, and only at the start of a line (per the spec). Each successful
    // conversion keeps us "still at line start" so multiple leading indents
    // collapse to multiple tabs.
    QByteArray out;
    out.reserve(bytes.size());
    bool changed = false;
    bool atLineStart = true;
    int i = 0;
    const int n = bytes.size();
    while (i < n) {
        if (atLineStart) {
            // Try to consume runs of exactly tabWidth spaces.
            while (i + tabWidth <= n) {
                bool allSpaces = true;
                for (int k = 0; k < tabWidth; ++k) {
                    if (bytes[i + k] != ' ') { allSpaces = false; break; }
                }
                if (!allSpaces) break;
                out.append('\t');
                i += tabWidth;
                changed = true;
            }
            atLineStart = false;
            if (i >= n) break;
        }

        char c = bytes[i++];
        out.append(c);
        if (c == '\n' || c == '\r') {
            // Coalesce \r\n: still treat the character right after as line-start.
            atLineStart = true;
        }
    }

    if (!changed) return;

    editor->beginUndoAction();
    long long newEnd = replaceRange(editor, range.start, range.end, out);
    editor->endUndoAction();

    if (range.hadSelection) {
        editor->setSel(range.start, newEnd);
    }
}

} // namespace EditOperations
