#pragma once

#include <QString>
#include <QStringList>

class ScintillaEdit;

namespace LexerMap {

// Returns Lexilla lexer name for an extension (lowercase, no dot), or "" if no match.
QString lexerNameForExtension(const QString& ext);

// Convenience: pick a lexer by examining the file path's extension.
// Returns lexer name or "" if no match.
QString lexerNameForPath(const QString& path);

// Apply lexer to a ScintillaEdit instance:
//   - resolve lexer by path (or pass empty path for plain text)
//   - call SCI_SETILEXER with a Lexilla::CreateLexer(name) result
//   - configure default keywords + reset styles
//   - if no match, leave editor in plain-text mode (no lexer)
void applyLexerForPath(ScintillaEdit* editor, const QString& path);

// Apply a Lexilla lexer directly by its canonical name (e.g. "python", "javascript", "json").
// Empty name → plain text (no lexer). Performs runtime fallbacks (javascript→cpp, phpscript→html).
void applyLexerByName(ScintillaEdit* editor, const QString& lexerName);

// List of file extensions we know about (for filter dialogs).
QStringList supportedExtensions();

} // namespace LexerMap
