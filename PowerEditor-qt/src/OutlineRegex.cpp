// OutlineRegex.cpp
// Heuristic, regex-driven symbol extraction per language.
//
// Design notes:
//   * We iterate per-line so capturing a 1-based line number is trivial and
//     cheap. Multiline patterns are intentionally avoided; QRegularExpression
//     anchored MultilineOption is still requested where useful for clarity.
//   * Each language has a small ordered set of patterns. The first pattern
//     that matches on a given line wins, preventing duplicate emissions
//     (e.g. JS class line should not also match the generic method shape).
//   * Indentation tracking: for languages that use blocks (Python, JS, TS,
//     Java, C#, Ruby, PHP), we keep a stack of (indent, symbolIndex) so that
//     methods inside classes get parentIndex pointing to the enclosing class.
//     This is best-effort — mixed tabs/spaces or unusual styles may break it,
//     but it produces a usable tree for typical code.
//   * C/C++ method/function detection is conservative because the regex
//     suggested in the spec is famously prone to false positives (e.g. it
//     matches `if (x) {`). We add a small keyword denylist.

#include "OutlineRegex.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QStringView>
#include <QVector>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Count leading whitespace columns. Tabs count as 1 column — this is heuristic
// and matches what most editors show for outline purposes.
int leadingIndent(const QString& line)
{
    int i = 0;
    while (i < line.size() && (line[i] == QLatin1Char(' ') || line[i] == QLatin1Char('\t')))
        ++i;
    return i;
}

// Normalize lexer name: lowercase, strip leading dot, collapse aliases.
QString canonicalLexer(const QString& raw)
{
    QString s = raw.trimmed().toLower();
    if (s.startsWith(QLatin1Char('.')))
        s = s.mid(1);

    static const QHash<QString, QString> kAlias = {
        {QStringLiteral("py"),         QStringLiteral("python")},
        {QStringLiteral("python3"),    QStringLiteral("python")},
        {QStringLiteral("js"),         QStringLiteral("javascript")},
        {QStringLiteral("jsx"),        QStringLiteral("javascript")},
        {QStringLiteral("mjs"),        QStringLiteral("javascript")},
        {QStringLiteral("cjs"),        QStringLiteral("javascript")},
        {QStringLiteral("ts"),         QStringLiteral("typescript")},
        {QStringLiteral("tsx"),        QStringLiteral("typescript")},
        {QStringLiteral("c"),          QStringLiteral("cpp")},
        {QStringLiteral("c++"),        QStringLiteral("cpp")},
        {QStringLiteral("cxx"),        QStringLiteral("cpp")},
        {QStringLiteral("cc"),         QStringLiteral("cpp")},
        {QStringLiteral("h"),          QStringLiteral("cpp")},
        {QStringLiteral("hpp"),        QStringLiteral("cpp")},
        {QStringLiteral("cs"),         QStringLiteral("csharp")},
        {QStringLiteral("c#"),         QStringLiteral("csharp")},
        {QStringLiteral("rb"),         QStringLiteral("ruby")},
        {QStringLiteral("rs"),         QStringLiteral("rust")},
        {QStringLiteral("golang"),     QStringLiteral("go")},
        {QStringLiteral("htm"),        QStringLiteral("html")},
        {QStringLiteral("xhtml"),      QStringLiteral("html")},
    };
    return kAlias.value(s, s);
}

// Pop indentation stack down to entries strictly less indented than `indent`.
// Returns the new parent index (-1 if empty).
int unwindStack(QVector<QPair<int,int>>& stack, int indent)
{
    while (!stack.isEmpty() && stack.last().first >= indent)
        stack.removeLast();
    return stack.isEmpty() ? -1 : stack.last().second;
}

// Decorate a method's name with its enclosing class name, e.g. "Foo.bar".
QString qualified(const QList<OutlineSymbol>& syms, int parent, const QString& name)
{
    if (parent < 0 || parent >= syms.size())
        return name;
    return syms[parent].name + QLatin1Char('.') + name;
}

// ---------------------------------------------------------------------------
// Per-language parsers
// ---------------------------------------------------------------------------

void parsePython(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reFn(
        QStringLiteral(R"(^\s*(?:async\s+)?def\s+(\w+)\s*\()"));
    static const QRegularExpression reCls(
        QStringLiteral(R"(^\s*class\s+(\w+))"));

    QVector<QPair<int,int>> stack; // (indent, symbolIndex)
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#')))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reCls.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = m.captured(1);
            s.kind = QStringLiteral("class");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
            stack.append({indent, out.size() - 1});
        } else if (auto m2 = reFn.match(line); m2.hasMatch()) {
            OutlineSymbol s;
            s.name = qualified(out, parent, m2.captured(1));
            s.kind = (parent >= 0 && out[parent].kind == QStringLiteral("class"))
                       ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
            stack.append({indent, out.size() - 1});
        }
    }
}

void parseJsTs(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reFn(
        QStringLiteral(R"(^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+))"));
    static const QRegularExpression reCls(
        QStringLiteral(R"(^\s*(?:export\s+)?(?:abstract\s+)?class\s+(\w+))"));
    static const QRegularExpression reArrow(
        QStringLiteral(R"(^\s*(?:export\s+)?(?:const|let|var)?\s*(\w+)\s*[:=]\s*(?:async\s+)?(?:function\b|\([^)]*\)\s*=>|[\w<>,\s]*=>))"));
    static const QRegularExpression reMethod(
        QStringLiteral(R"(^\s*(?:public|private|protected|static|async)?\s*(\w+)\s*\([^)]*\)\s*\{)"));
    static const QRegularExpression reIface(
        QStringLiteral(R"(^\s*(?:export\s+)?interface\s+(\w+))"));

    // Reserved keywords that look like method definitions but aren't.
    static const QSet<QString> kSkip = {
        QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("while"),
        QStringLiteral("switch"), QStringLiteral("catch"), QStringLiteral("return"),
        QStringLiteral("function"), QStringLiteral("do"), QStringLiteral("else"),
    };

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//"))
            || trimmed.startsWith(QStringLiteral("*")))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reCls.match(line); m.hasMatch()) {
            OutlineSymbol s{ m.captured(1), QStringLiteral("class"), i + 1, parent };
            out.append(s);
            stack.append({indent, out.size() - 1});
            continue;
        }
        if (auto m = reIface.match(line); m.hasMatch()) {
            OutlineSymbol s{ m.captured(1), QStringLiteral("interface"), i + 1, parent };
            out.append(s);
            stack.append({indent, out.size() - 1});
            continue;
        }
        if (auto m = reFn.match(line); m.hasMatch()) {
            OutlineSymbol s{ m.captured(1), QStringLiteral("function"), i + 1, parent };
            out.append(s);
            continue;
        }
        if (auto m = reArrow.match(line); m.hasMatch()) {
            OutlineSymbol s{ m.captured(1), QStringLiteral("function"), i + 1, parent };
            out.append(s);
            continue;
        }
        if (auto m = reMethod.match(line); m.hasMatch()) {
            const QString name = m.captured(1);
            if (kSkip.contains(name)) continue;
            // Only emit as a method when we're nested inside a class/interface;
            // top-level matches are usually false positives in JS.
            if (parent >= 0
                && (out[parent].kind == QStringLiteral("class")
                    || out[parent].kind == QStringLiteral("interface"))) {
                OutlineSymbol s{ qualified(out, parent, name),
                                 QStringLiteral("method"), i + 1, parent };
                out.append(s);
            }
        }
    }
}

void parseCpp(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reType(
        QStringLiteral(R"(^\s*(?:template\s*<[^>]+>\s*)?(struct|class)\s+(\w+))"));
    // Function/method shape: optional return type + name(params) + optional const + '{'.
    // We accept patterns where the body opens on the same line OR on a line
    // ending with ')' followed by no semicolon — handled separately below.
    static const QRegularExpression reFnOpen(
        QStringLiteral(R"(^\s*(?:[\w:<>,\s\*&]+\s+)?(\w+)\s*\([^;]*\)\s*(?:const)?\s*\{)"));

    static const QSet<QString> kSkip = {
        QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("while"),
        QStringLiteral("switch"), QStringLiteral("catch"), QStringLiteral("return"),
        QStringLiteral("sizeof"), QStringLiteral("do"), QStringLiteral("else"),
        QStringLiteral("static_assert"), QStringLiteral("decltype"),
    };

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//"))
            || trimmed.startsWith(QStringLiteral("*"))
            || trimmed.startsWith(QStringLiteral("#")))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reType.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = m.captured(2);
            s.kind = (m.captured(1) == QStringLiteral("class"))
                       ? QStringLiteral("class") : QStringLiteral("struct");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
            stack.append({indent, out.size() - 1});
            continue;
        }
        if (auto m = reFnOpen.match(line); m.hasMatch()) {
            const QString name = m.captured(1);
            if (kSkip.contains(name)) continue;
            // Skip obvious constructor-call expressions: lines starting with
            // "return " were already filtered, but assignments like
            // "auto x = foo(bar) { ... }" are exceedingly rare in C++.
            OutlineSymbol s;
            s.name = qualified(out, parent, name);
            s.kind = (parent >= 0 && (out[parent].kind == QStringLiteral("class")
                                       || out[parent].kind == QStringLiteral("struct")))
                       ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
        }
    }
}

void parseJavaCs(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reCls(
        QStringLiteral(R"(^\s*(?:public|private|protected|internal)?\s*(?:static\s+|abstract\s+|sealed\s+|final\s+)*(class|interface|struct|enum)\s+(\w+))"));
    static const QRegularExpression reMethod(
        QStringLiteral(R"(^\s*(?:public|private|protected|internal|static|final|abstract|virtual|override|sealed|async)\s+(?:[\w<>\[\],\s]+\s+)?(\w+)\s*\([^)]*\)\s*(?:throws\s+[\w,\s]+)?\s*[\{;])"));

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//"))
            || trimmed.startsWith(QStringLiteral("*")))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reCls.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = m.captured(2);
            const QString k = m.captured(1);
            s.kind = (k == QStringLiteral("interface")) ? QStringLiteral("interface")
                   : (k == QStringLiteral("enum"))      ? QStringLiteral("enum")
                   : (k == QStringLiteral("struct"))    ? QStringLiteral("struct")
                                                        : QStringLiteral("class");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
            stack.append({indent, out.size() - 1});
            continue;
        }
        if (auto m = reMethod.match(line); m.hasMatch()) {
            const QString name = m.captured(1);
            // Filter common keywords that can slip through.
            if (name == QStringLiteral("if") || name == QStringLiteral("for")
                || name == QStringLiteral("while") || name == QStringLiteral("switch"))
                continue;
            OutlineSymbol s;
            s.name = qualified(out, parent, name);
            s.kind = (parent >= 0 && (out[parent].kind == QStringLiteral("class")
                                       || out[parent].kind == QStringLiteral("interface")
                                       || out[parent].kind == QStringLiteral("struct")))
                       ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
        }
    }
}

void parseGo(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reFn(
        QStringLiteral(R"(^\s*func\s+(?:\(([^)]+)\)\s+)?(\w+)\s*\()"));
    static const QRegularExpression reType(
        QStringLiteral(R"(^\s*type\s+(\w+)\s+(struct|interface))"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (auto m = reType.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = m.captured(1);
            s.kind = (m.captured(2) == QStringLiteral("interface"))
                       ? QStringLiteral("interface") : QStringLiteral("struct");
            s.line = i + 1;
            out.append(s);
            continue;
        }
        if (auto m = reFn.match(line); m.hasMatch()) {
            OutlineSymbol s;
            QString name = m.captured(2);
            const QString recv = m.captured(1).trimmed();
            if (!recv.isEmpty()) {
                // Receiver looks like "r *Receiver" or "r Receiver".
                const QStringList parts = recv.split(QRegularExpression(QStringLiteral(R"(\s+)")),
                                                     Qt::SkipEmptyParts);
                if (!parts.isEmpty()) {
                    QString recvType = parts.last();
                    if (recvType.startsWith(QLatin1Char('*')))
                        recvType = recvType.mid(1);
                    name = recvType + QLatin1Char('.') + name;
                    s.kind = QStringLiteral("method");
                }
            }
            if (s.kind.isEmpty())
                s.kind = QStringLiteral("function");
            s.name = name;
            s.line = i + 1;
            out.append(s);
        }
    }
}

void parseRust(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reFn(
        QStringLiteral(R"(^\s*(?:pub(?:\([^)]*\))?\s+)?(?:async\s+)?(?:unsafe\s+)?fn\s+(\w+))"));
    static const QRegularExpression reStruct(
        QStringLiteral(R"(^\s*(?:pub(?:\([^)]*\))?\s+)?struct\s+(\w+))"));
    static const QRegularExpression reEnum(
        QStringLiteral(R"(^\s*(?:pub(?:\([^)]*\))?\s+)?enum\s+(\w+))"));
    static const QRegularExpression reTrait(
        QStringLiteral(R"(^\s*(?:pub(?:\([^)]*\))?\s+)?trait\s+(\w+))"));
    static const QRegularExpression reImpl(
        QStringLiteral(R"(^\s*impl(?:\s*<[^>]+>)?\s+(?:([\w<>:]+)\s+for\s+)?(\w+))"));

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//")))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reStruct.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("struct"), i + 1, parent });
            continue;
        }
        if (auto m = reEnum.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("enum"), i + 1, parent });
            continue;
        }
        if (auto m = reTrait.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("interface"), i + 1, parent });
            continue;
        }
        if (auto m = reImpl.match(line); m.hasMatch()) {
            QString name = m.captured(2);
            const QString trait = m.captured(1);
            if (!trait.isEmpty())
                name = trait + QStringLiteral(" for ") + name;
            OutlineSymbol s{ name, QStringLiteral("class"), i + 1, parent };
            out.append(s);
            stack.append({indent, out.size() - 1});
            continue;
        }
        if (auto m = reFn.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = qualified(out, parent, m.captured(1));
            s.kind = (parent >= 0) ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
        }
    }
}

void parseRuby(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reDef(
        QStringLiteral(R"(^\s*def\s+(?:self\.)?(\w+))"));
    static const QRegularExpression reCls(
        QStringLiteral(R"(^\s*class\s+(\w+))"));
    static const QRegularExpression reMod(
        QStringLiteral(R"(^\s*module\s+(\w+))"));

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reCls.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("class"), i + 1, parent });
            stack.append({indent, out.size() - 1});
        } else if (auto m = reMod.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("module"), i + 1, parent });
            stack.append({indent, out.size() - 1});
        } else if (auto m = reDef.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = qualified(out, parent, m.captured(1));
            s.kind = (parent >= 0) ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
        }
    }
}

void parsePhp(const QStringList& lines, QList<OutlineSymbol>& out)
{
    static const QRegularExpression reFn(
        QStringLiteral(R"(^\s*(?:(?:public|private|protected|static|final|abstract)\s+)*function\s+(\w+))"));
    static const QRegularExpression reCls(
        QStringLiteral(R"(^\s*(?:abstract\s+|final\s+)?class\s+(\w+))"));
    static const QRegularExpression reIface(
        QStringLiteral(R"(^\s*interface\s+(\w+))"));
    static const QRegularExpression reTrait(
        QStringLiteral(R"(^\s*trait\s+(\w+))"));

    QVector<QPair<int,int>> stack;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//"))
            || trimmed.startsWith(QStringLiteral("#"))
            || trimmed.startsWith(QStringLiteral("*")))
            continue;

        const int indent = leadingIndent(line);
        const int parent = unwindStack(stack, indent);

        if (auto m = reCls.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("class"), i + 1, parent });
            stack.append({indent, out.size() - 1});
        } else if (auto m = reIface.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("interface"), i + 1, parent });
            stack.append({indent, out.size() - 1});
        } else if (auto m = reTrait.match(line); m.hasMatch()) {
            out.append({ m.captured(1), QStringLiteral("module"), i + 1, parent });
            stack.append({indent, out.size() - 1});
        } else if (auto m = reFn.match(line); m.hasMatch()) {
            OutlineSymbol s;
            s.name = qualified(out, parent, m.captured(1));
            s.kind = (parent >= 0) ? QStringLiteral("method") : QStringLiteral("function");
            s.line = i + 1;
            s.parentIndex = parent;
            out.append(s);
        }
    }
}

void parseCss(const QStringList& lines, QList<OutlineSymbol>& out)
{
    // Capture selector lists ending with '{'. We strip the brace and any
    // trailing whitespace; the selector text becomes the symbol name.
    static const QRegularExpression reSel(
        QStringLiteral(R"(^\s*([^\{\}\/]+?)\s*\{)"));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("/*"))
            || trimmed.startsWith(QStringLiteral("*"))
            || trimmed.startsWith(QStringLiteral("//")))
            continue;
        // Skip property-only lines (contain ':' before '{' is fine for selectors
        // like a:hover, but a stray "color: red;" must not match — it has ';').
        if (trimmed.contains(QLatin1Char(';')))
            continue;
        if (auto m = reSel.match(line); m.hasMatch()) {
            QString sel = m.captured(1).trimmed();
            if (sel.isEmpty()) continue;
            // Filter at-rules' keyword body openings such as "@media (...) {".
            // We still include them as selectors — useful for navigation.
            out.append({ sel, QStringLiteral("selector"), i + 1, -1 });
        }
    }
}

void parseJson(const QStringList& lines, QList<OutlineSymbol>& out)
{
    // Top-level keys only: keys whose indentation level is exactly 1 step
    // (after the opening '{'). We approximate "top level" as keys appearing
    // before any nested closing '}' at column 0.
    static const QRegularExpression reKey(
        QStringLiteral(R"(^\s*\"([^\"]+)\"\s*:)"));
    int depth = 0;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        // Crude depth tracking: count braces outside strings is hard with
        // regex, so we count them naively. Adequate for well-formed JSON.
        for (QChar ch : line) {
            if (ch == QLatin1Char('{') || ch == QLatin1Char('[')) ++depth;
            else if (ch == QLatin1Char('}') || ch == QLatin1Char(']')) --depth;
        }
        // We want keys that live at depth 1 right after their line's content
        // is parsed; this is fuzzy but good enough for a navigator.
        if (auto m = reKey.match(line); m.hasMatch()) {
            // Heuristic: require the key to be indented by 1-4 spaces or 1 tab.
            const int indent = leadingIndent(line);
            if (indent >= 1 && indent <= 4)
                out.append({ m.captured(1), QStringLiteral("key"), i + 1, -1 });
        }
    }
}

void parseHtml(const QStringList& lines, QList<OutlineSymbol>& out)
{
    // Headings <h1>..<h6>. Capture inner text (without further tags).
    static const QRegularExpression reH(
        QStringLiteral(R"(<\s*h([1-6])[^>]*>(.*?)<\s*/\s*h\1\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        auto it = reH.globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
            QString text = m.captured(2);
            // Strip inner HTML tags from the heading content.
            static const QRegularExpression reTag(QStringLiteral("<[^>]+>"));
            text.replace(reTag, QString());
            text = text.trimmed();
            if (text.isEmpty())
                text = QStringLiteral("h%1").arg(m.captured(1));
            out.append({ text, QStringLiteral("section"), i + 1, -1 });
        }
    }
}

void parseXml(const QStringList& lines, QList<OutlineSymbol>& out)
{
    // Top-level tags: opening tags whose indentation is 0 (or only whitespace
    // before column 0). Skip XML declarations and comments.
    static const QRegularExpression reTag(
        QStringLiteral(R"(^\s*<\s*([A-Za-z_][\w\-:]*))"));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("<?")) || trimmed.startsWith(QStringLiteral("<!")))
            continue;
        if (leadingIndent(line) > 0)
            continue;
        if (auto m = reTag.match(line); m.hasMatch())
            out.append({ m.captured(1), QStringLiteral("tag"), i + 1, -1 });
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QList<OutlineSymbol> OutlineRegex::parse(const QString& content, const QString& lexerName)
{
    QList<OutlineSymbol> out;
    if (content.isEmpty())
        return out;

    const QString lexer = canonicalLexer(lexerName);
    // Splitting once up front; QString::split with KeepEmptyParts preserves
    // line numbering for blank lines.
    const QStringList lines = content.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")),
                                            Qt::KeepEmptyParts);

    if (lexer == QStringLiteral("python"))            parsePython(lines, out);
    else if (lexer == QStringLiteral("javascript"))   parseJsTs(lines, out);
    else if (lexer == QStringLiteral("typescript"))   parseJsTs(lines, out);
    else if (lexer == QStringLiteral("cpp"))          parseCpp(lines, out);
    else if (lexer == QStringLiteral("java"))         parseJavaCs(lines, out);
    else if (lexer == QStringLiteral("csharp"))       parseJavaCs(lines, out);
    else if (lexer == QStringLiteral("go"))           parseGo(lines, out);
    else if (lexer == QStringLiteral("rust"))         parseRust(lines, out);
    else if (lexer == QStringLiteral("ruby"))         parseRuby(lines, out);
    else if (lexer == QStringLiteral("php"))          parsePhp(lines, out);
    else if (lexer == QStringLiteral("css"))          parseCss(lines, out);
    else if (lexer == QStringLiteral("scss"))         parseCss(lines, out);
    else if (lexer == QStringLiteral("less"))         parseCss(lines, out);
    else if (lexer == QStringLiteral("json"))         parseJson(lines, out);
    else if (lexer == QStringLiteral("html"))         parseHtml(lines, out);
    else if (lexer == QStringLiteral("xml"))          parseXml(lines, out);
    // Unknown lexer: return empty list; caller decides on fallback UI.

    return out;
}

QStringList OutlineRegex::supportedLexers()
{
    return {
        QStringLiteral("python"),
        QStringLiteral("javascript"),
        QStringLiteral("typescript"),
        QStringLiteral("cpp"),
        QStringLiteral("java"),
        QStringLiteral("csharp"),
        QStringLiteral("go"),
        QStringLiteral("rust"),
        QStringLiteral("ruby"),
        QStringLiteral("php"),
        QStringLiteral("css"),
        QStringLiteral("scss"),
        QStringLiteral("less"),
        QStringLiteral("json"),
        QStringLiteral("html"),
        QStringLiteral("xml"),
    };
}
