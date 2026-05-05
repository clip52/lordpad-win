#pragma once

class ScintillaEdit;

namespace EditOperations {

// Trim trailing whitespace on every line of the document. Preserves caret position
// (best-effort -- line may have shrunk).
void trimTrailingWhitespace(ScintillaEdit* editor);

// Convert the current selection (or whole document if none) to upper / lower / title case.
void toUpperSelection(ScintillaEdit* editor);
void toLowerSelection(ScintillaEdit* editor);
void toTitleSelection(ScintillaEdit* editor);   // Capitalize First Letter Of Each Word

// Sort selected lines (or whole document if none).
void sortLinesAscending(ScintillaEdit* editor);
void sortLinesDescending(ScintillaEdit* editor);
void sortLinesUnique(ScintillaEdit* editor);    // remove duplicates, preserve original order of first occurrence

// Duplicate the current line (or selection if any) below.
void duplicateLine(ScintillaEdit* editor);

// Move the current line up / down. If selection spans multiple lines, move the entire block.
void moveLineUp(ScintillaEdit* editor);
void moveLineDown(ScintillaEdit* editor);

// Convert tabs to spaces / spaces to tabs in the current selection (or whole document).
// `tabWidth` controls how many spaces equal one tab.
void tabsToSpaces(ScintillaEdit* editor, int tabWidth);
void spacesToTabs(ScintillaEdit* editor, int tabWidth);

} // namespace EditOperations
