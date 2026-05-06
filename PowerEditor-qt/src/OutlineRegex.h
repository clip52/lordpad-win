// OutlineRegex.h
// Regex-based, language-aware symbol extractor for the Function List panel.
//
// This module is intentionally heuristic: it does NOT build a real AST.
// It scans line-by-line with QRegularExpression patterns chosen per lexer
// and returns a flat list of OutlineSymbol entries (with optional parent
// linking via parentIndex) suitable for populating a QTreeWidget/QListView.
//
// Quality target: ~70% recall on idiomatic source files. False positives
// are tolerated; consumers may filter by `kind` or by a follow-up pass.
//
// Usage (typical, from FunctionListPanel):
//     auto symbols = OutlineRegex::parse(text, "python");
//     for (const auto& sym : symbols) {
//         // sym.name, sym.kind, sym.line, sym.parentIndex
//     }
//
// Supported lexer names — see supportedLexers(). Unknown names yield an
// empty list; callers can fall back to a generic indicator.
//
// pt-BR: comentarios podem aparecer em portugues; identificadores e API
// permanecem em ingles para consistencia com o restante do PowerEditor-qt.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct OutlineSymbol {
    QString name;          // e.g. "myFunction" or "MyClass.method"
    QString kind;          // function | method | class | struct | enum |
                           // interface | module | selector | section | key | tag
    int     line = 0;      // 1-based line number where the symbol starts
    int     parentIndex = -1; // index of parent symbol in the returned list,
                              // or -1 if top-level. Useful for tree views.
};

namespace OutlineRegex {

// Parse `content` according to `lexerName` (case-insensitive).
// Returns an empty list when the lexer is unknown or no symbols are found.
QList<OutlineSymbol> parse(const QString& content, const QString& lexerName);

// Lexer identifiers recognized by parse(). The panel can use this to decide
// whether to show the symbol tree or a "no outline available" fallback.
QStringList supportedLexers();

} // namespace OutlineRegex
