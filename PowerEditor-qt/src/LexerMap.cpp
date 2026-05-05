#include "LexerMap.h"

#include <QFileInfo>
#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

#include "ScintillaEdit.h"
#include "Theme.h"

// Lexilla.h requires Scintilla::ILexer5 to be visible when included as C++.
// ScintillaEdit.h already pulls in the Scintilla namespace headers (ILexer.h),
// but to be safe in case the include order changes we forward-declare it.
namespace Scintilla {
class ILexer5;
}

#include <Lexilla.h>

namespace LexerMap {

namespace {

// Build the extension -> lexer-name table once and reuse it.
const QHash<QString, QString>& extensionTable()
{
    static const QHash<QString, QString> table = {
        // C / C++ family
        { QStringLiteral("cpp"),      QStringLiteral("cpp") },
        { QStringLiteral("cxx"),      QStringLiteral("cpp") },
        { QStringLiteral("cc"),       QStringLiteral("cpp") },
        { QStringLiteral("hpp"),      QStringLiteral("cpp") },
        { QStringLiteral("hxx"),      QStringLiteral("cpp") },
        { QStringLiteral("h"),        QStringLiteral("cpp") },
        { QStringLiteral("c"),        QStringLiteral("cpp") },

        // Python
        { QStringLiteral("py"),       QStringLiteral("python") },
        { QStringLiteral("pyw"),      QStringLiteral("python") },

        // JavaScript / TypeScript (compile-time map = "javascript";
        // applyLexerForPath does runtime fallback to "cpp" if Lexilla
        // can't create a "javascript" lexer).
        { QStringLiteral("js"),       QStringLiteral("javascript") },
        { QStringLiteral("mjs"),      QStringLiteral("javascript") },
        { QStringLiteral("cjs"),      QStringLiteral("javascript") },
        { QStringLiteral("ts"),       QStringLiteral("javascript") },
        { QStringLiteral("tsx"),      QStringLiteral("javascript") },
        { QStringLiteral("jsx"),      QStringLiteral("javascript") },

        // Data / config
        { QStringLiteral("json"),     QStringLiteral("json") },
        { QStringLiteral("yaml"),     QStringLiteral("yaml") },
        { QStringLiteral("yml"),      QStringLiteral("yaml") },
        { QStringLiteral("toml"),     QStringLiteral("toml") },
        { QStringLiteral("ini"),      QStringLiteral("props") },
        { QStringLiteral("conf"),     QStringLiteral("props") },
        { QStringLiteral("cfg"),      QStringLiteral("props") },

        // Markup / web
        { QStringLiteral("xml"),      QStringLiteral("html") },
        { QStringLiteral("html"),     QStringLiteral("html") },
        { QStringLiteral("htm"),      QStringLiteral("html") },
        { QStringLiteral("xhtml"),    QStringLiteral("html") },
        { QStringLiteral("svg"),      QStringLiteral("html") },
        { QStringLiteral("css"),      QStringLiteral("css") },
        { QStringLiteral("md"),       QStringLiteral("markdown") },
        { QStringLiteral("markdown"), QStringLiteral("markdown") },

        // Shells
        { QStringLiteral("sh"),       QStringLiteral("bash") },
        { QStringLiteral("bash"),     QStringLiteral("bash") },
        { QStringLiteral("zsh"),      QStringLiteral("bash") },

        // Other languages
        { QStringLiteral("rb"),       QStringLiteral("ruby") },
        { QStringLiteral("go"),       QStringLiteral("cpp") },
        { QStringLiteral("rs"),       QStringLiteral("rust") },
        { QStringLiteral("java"),     QStringLiteral("cpp") },
        { QStringLiteral("kt"),       QStringLiteral("cpp") },
        { QStringLiteral("kts"),      QStringLiteral("cpp") },
        { QStringLiteral("sql"),      QStringLiteral("sql") },
        { QStringLiteral("lua"),      QStringLiteral("lua") },
        { QStringLiteral("php"),      QStringLiteral("phpscript") },

        // Build / TeX / scripting
        { QStringLiteral("cmake"),    QStringLiteral("cmake") },
        { QStringLiteral("tex"),      QStringLiteral("latex") },
        { QStringLiteral("latex"),    QStringLiteral("latex") },
        { QStringLiteral("r"),        QStringLiteral("r") },
        { QStringLiteral("pl"),       QStringLiteral("perl") },
        { QStringLiteral("pm"),       QStringLiteral("perl") },
    };
    return table;
}

// Resolve a basename (case-insensitive) to a lexer for files that have no
// extension or use special filenames. Returns empty string if no match.
QString lexerForBasename(const QString& basename)
{
    const QString lower = basename.toLower();
    if (lower == QStringLiteral("dockerfile"))
        return QStringLiteral("bash");
    if (lower == QStringLiteral("makefile") || lower == QStringLiteral("gnumakefile"))
        return QStringLiteral("makefile");
    if (lower == QStringLiteral("cmakelists.txt"))
        return QStringLiteral("cmake");
    return QString();
}

// Try to create a Lexilla lexer; on null return an empty string so the
// caller can apply a fallback. Otherwise return the original name.
Scintilla::ILexer5* tryCreateLexer(const QString& name)
{
    if (name.isEmpty())
        return nullptr;
    const QByteArray utf8 = name.toUtf8();
    return CreateLexer(utf8.constData());
}

} // namespace

QString lexerNameForExtension(const QString& ext)
{
    if (ext.isEmpty())
        return QString();
    const QString lower = ext.toLower();
    const auto& table = extensionTable();
    const auto it = table.constFind(lower);
    if (it == table.constEnd())
        return QString();
    return it.value();
}

QString lexerNameForPath(const QString& path)
{
    if (path.isEmpty())
        return QString();

    const QFileInfo info(path);
    const QString basename = info.fileName();

    // 1. Special-case basenames first (Dockerfile, Makefile, CMakeLists.txt, ...).
    const QString fromBasename = lexerForBasename(basename);
    if (!fromBasename.isEmpty())
        return fromBasename;

    // 2. Otherwise extract the extension after the last '.', lowercase it,
    //    and look it up.
    const int dot = basename.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot == basename.size() - 1)
        return QString();
    const QString ext = basename.mid(dot + 1).toLower();
    return lexerNameForExtension(ext);
}

// Keywords per lexer. Index 0 = primary keywords (Scintilla style 5 in cpp lexer
// and most others). Index 1 = secondary keywords (style 16 in cpp). Empty string
// means no keywords for that slot. Multiple Lexilla lexers reuse these slots.
struct KeywordSet {
    const char* primary;
    const char* secondary;
};

KeywordSet keywordsFor(const QString& requestedName, const QString& effectiveLexer)
{
    // Try the user-requested name first (e.g. "javascript" even though the lexer
    // we ended up with is "cpp"), then fall back to the effective lexer name.
    auto pick = [](const QString& n) -> KeywordSet {
        // C / C++ keywords (also fallback for Java/Go/Kotlin/Scala/Swift).
        if (n == QStringLiteral("cpp"))
            return {
                "alignas alignof and and_eq asm auto bitand bitor bool break case catch "
                "char char16_t char32_t class compl const constexpr const_cast continue "
                "decltype default delete do double dynamic_cast else enum explicit export "
                "extern false float for friend goto if inline int long mutable namespace "
                "new noexcept not not_eq nullptr operator or or_eq private protected public "
                "register reinterpret_cast return short signed sizeof static static_assert "
                "static_cast struct switch template this thread_local throw true try typedef "
                "typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq",
                ""
            };
        if (n == QStringLiteral("javascript"))
            return {
                "abstract arguments async await break case catch class const continue debugger "
                "default delete do else enum export extends false final finally for from function "
                "get goto if implements import in instanceof interface let new null of package "
                "private protected public return set static super switch this throw true try "
                "typeof undefined var void while with yield",
                "Array Boolean Date Error Function JSON Map Math NaN Number Object Promise "
                "Proxy Reflect RegExp Set String Symbol WeakMap WeakSet console document window"
            };
        if (n == QStringLiteral("python"))
            return {
                "False None True and as assert async await break class continue def del elif "
                "else except finally for from global if import in is lambda nonlocal not or "
                "pass raise return try while with yield match case",
                "abs all any bin bool bytes callable chr classmethod compile complex dict dir "
                "divmod enumerate eval exec filter float format frozenset getattr globals "
                "hasattr hash help hex id input int isinstance issubclass iter len list locals "
                "map max min next object oct open ord pow print property range repr reversed "
                "round set setattr slice sorted staticmethod str sum super tuple type vars zip"
            };
        if (n == QStringLiteral("phpscript"))
            return {
                "abstract and array as break callable case catch class clone const continue "
                "declare default die do echo else elseif empty enddeclare endfor endforeach "
                "endif endswitch endwhile eval exit extends final finally for foreach function "
                "global goto if implements include include_once instanceof insteadof interface "
                "isset list match namespace new null or print private protected public readonly "
                "require require_once return static switch throw trait true try unset use var "
                "while xor yield false",
                ""
            };
        if (n == QStringLiteral("sql"))
            return {
                "absolute action add all alter and any are as asc assertion at authorization "
                "begin between bigint binary bit blob both by call cascade case cast catalog "
                "char character check close coalesce collate column commit connect constraint "
                "continue convert corresponding count create cross current current_date "
                "current_time current_timestamp current_user cursor date day deallocate dec "
                "decimal declare default deferrable deferred delete desc describe descriptor "
                "diagnostics disconnect distinct domain double drop else end end-exec escape "
                "except exception exec execute exists explain external false fetch first float "
                "for foreign found from full get global go goto grant group having hour identity "
                "if immediate in include index indicator initially inner input insensitive "
                "insert int integer intersect interval into is isolation join key language "
                "last left level like limit local lower match max min minute module names "
                "national natural nchar next no not null nullif numeric of offset on only open "
                "option or order outer output overlaps partial position precision prepare "
                "preserve primary prior privileges procedure public read real references "
                "relative replace restrict revoke right rollback rows schema scroll second "
                "section select session session_user set show size smallint some space sql "
                "sqlcode sqlerror sqlstate substring sum system_user table temporary then time "
                "timestamp to top trailing transaction translate translation trim true union "
                "unique unknown update upper usage user using value values varchar varying view "
                "when whenever where with work write year zone",
                ""
            };
        if (n == QStringLiteral("json"))
            return { "true false null", "" };
        if (n == QStringLiteral("css"))
            return {
                "color background background-color font font-size font-family font-weight "
                "padding margin border display position top left right bottom width height "
                "min-width max-width min-height max-height float clear overflow visibility "
                "opacity z-index text-align vertical-align line-height letter-spacing "
                "text-decoration text-transform white-space cursor list-style box-shadow "
                "border-radius transition transform animation flex grid gap",
                "absolute relative fixed static block inline inline-block none auto bold "
                "italic underline center left right top bottom middle hidden visible solid "
                "dashed dotted double pointer default red blue green white black gray "
                "transparent flex grid"
            };
        if (n == QStringLiteral("html") || n == QStringLiteral("xml"))
            return {
                "a abbr address article aside audio b base body button canvas caption code "
                "col colgroup data datalist dd details dialog div dl dt em embed fieldset "
                "figcaption figure footer form h1 h2 h3 h4 h5 h6 head header hr html i iframe "
                "img input ins kbd label legend li link main map mark menu meta meter nav "
                "noscript object ol optgroup option output p param picture pre progress q "
                "rb rp rt ruby s samp script section select small source span strong style "
                "sub summary sup table tbody td template textarea tfoot th thead time title "
                "tr track u ul var video wbr",
                ""
            };
        return {nullptr, nullptr};
    };

    KeywordSet ks = pick(requestedName);
    if (!ks.primary) ks = pick(effectiveLexer);
    if (!ks.primary) ks = {"", ""};
    return ks;
}

// Per-lexer palette overlay. Theme.cpp applies a generic cpp-oriented palette
// (style 1=comment, 5=keyword, 6=string, ...). Lexers that use different style
// indices (HTML, XML, JSON, properties, ...) get their own mapping here.
namespace {

inline int sciColor(const QColor& c) {
    return (c.blue() << 16) | (c.green() << 8) | c.red();
}

struct SyntaxColors {
    QColor comment, number, keyword, string, charLit, preproc, op, identifier, error;
};

SyntaxColors paletteForCurrentTheme()
{
    const AppTheme t = ThemeManager::current();
    SyntaxColors s;
    if (t == AppTheme::Dark) {
        s.comment    = "#6A9955"; s.number   = "#B5CEA8"; s.keyword = "#569CD6";
        s.string     = "#CE9178"; s.charLit  = "#D7BA7D"; s.preproc = "#C586C0";
        s.op         = "#D4D4D4"; s.identifier = "#9CDCFE"; s.error  = "#F44747";
    } else if (t == AppTheme::Dracula) {
        s.comment    = "#6272A4"; s.number   = "#BD93F9"; s.keyword = "#FF79C6";
        s.string     = "#F1FA8C"; s.charLit  = "#F1FA8C"; s.preproc = "#BD93F9";
        s.op         = "#F8F8F2"; s.identifier = "#50FA7B"; s.error  = "#FF5555";
    } else { // Light
        s.comment    = "#008000"; s.number   = "#FF8000"; s.keyword = "#0000FF";
        s.string     = "#A31515"; s.charLit  = "#A31515"; s.preproc = "#804080";
        s.op         = "#000080"; s.identifier = "#000000"; s.error  = "#FF0000";
    }
    return s;
}

void applyHtmlPalette(ScintillaEdit* editor, const SyntaxColors& s)
{
    auto setFg = [&](int style, const QColor& c) {
        editor->styleSetFore(style, sciColor(c));
    };
    // SCE_H_* indices (Scintilla HTML lexer):
    setFg(0,  s.identifier);   // DEFAULT (text outside tags)
    setFg(1,  s.keyword);      // TAG
    setFg(2,  s.error);        // TAGUNKNOWN
    setFg(3,  s.identifier);   // ATTRIBUTE
    setFg(4,  s.number);       // ATTRIBUTEUNKNOWN
    setFg(5,  s.number);       // NUMBER
    setFg(6,  s.string);       // DOUBLESTRING
    setFg(7,  s.string);       // SINGLESTRING
    setFg(8,  s.identifier);   // OTHER
    setFg(9,  s.comment);      // COMMENT
    setFg(10, s.preproc);      // ENTITY
    setFg(11, s.keyword);      // TAGEND
    setFg(12, s.preproc);      // XMLSTART
    setFg(13, s.preproc);      // XMLEND
    setFg(14, s.error);        // SCRIPT (raw script content marker)
    setFg(15, s.preproc);      // ASP
    setFg(16, s.preproc);      // ASPAT
    setFg(17, s.string);       // CDATA
    setFg(18, s.preproc);      // QUESTION (PHP, ASP)
    setFg(19, s.string);       // VALUE
    setFg(20, s.preproc);      // XCCOMMENT
}

void applyJsonPalette(ScintillaEdit* editor, const SyntaxColors& s)
{
    auto setFg = [&](int style, const QColor& c) {
        editor->styleSetFore(style, sciColor(c));
    };
    // SCE_JSON_* indices:
    setFg(0,  s.identifier);   // DEFAULT
    setFg(1,  s.number);       // NUMBER
    setFg(2,  s.string);       // STRING
    setFg(3,  s.error);        // STRINGEOL
    setFg(4,  s.identifier);   // PROPERTYNAME
    setFg(5,  s.preproc);      // ESCAPESEQUENCE
    setFg(6,  s.comment);      // LINECOMMENT
    setFg(7,  s.comment);      // BLOCKCOMMENT
    setFg(8,  s.op);           // OPERATOR
    setFg(9,  s.preproc);      // URI
    setFg(10, s.preproc);      // COMPACTIRI
    setFg(11, s.keyword);      // KEYWORD (true/false/null)
    setFg(12, s.keyword);      // LDKEYWORD
    setFg(13, s.error);        // ERROR
}

void applyPropsPalette(ScintillaEdit* editor, const SyntaxColors& s)
{
    auto setFg = [&](int style, const QColor& c) {
        editor->styleSetFore(style, sciColor(c));
    };
    // SCE_PROPS_* indices (INI/properties files):
    setFg(0, s.identifier);  // DEFAULT
    setFg(1, s.comment);     // COMMENT
    setFg(2, s.keyword);     // SECTION  (e.g. [Section])
    setFg(3, s.op);          // ASSIGNMENT (=)
    setFg(4, s.string);      // DEFVAL (value after =)
    setFg(5, s.identifier);  // KEY
}

} // namespace

void applyLexerByName(ScintillaEdit* editor, const QString& lexerName)
{
    if (!editor)
        return;

    if (lexerName.isEmpty()) {
        editor->setILexer(0);
        return;
    }

    QString effective = lexerName;
    Scintilla::ILexer5* lexer = tryCreateLexer(lexerName);
    if (!lexer) {
        if (lexerName == QStringLiteral("javascript")) {
            effective = QStringLiteral("cpp");
            lexer = tryCreateLexer(effective);
        } else if (lexerName == QStringLiteral("phpscript")) {
            effective = QStringLiteral("html");
            lexer = tryCreateLexer(effective);
        }
    }

    if (!lexer) {
        editor->setILexer(0);
        return;
    }

    editor->setILexer(reinterpret_cast<sptr_t>(lexer));

    // Set primary + secondary keywords so the lexer can classify tokens. Without
    // this, words like "function" / "if" / "var" stay style 11 (identifier).
    const KeywordSet ks = keywordsFor(lexerName, effective);
    if (ks.primary && *ks.primary)     editor->setKeyWords(0, ks.primary);
    if (ks.secondary && *ks.secondary) editor->setKeyWords(1, ks.secondary);

    // Per-lexer style palette overlay (Theme.cpp's generic cpp-oriented colors
    // are wrong for lexers that use other style indices — HTML/XML, JSON, INI).
    const SyntaxColors palette = paletteForCurrentTheme();
    if (effective == QStringLiteral("html") || effective == QStringLiteral("xml")) {
        applyHtmlPalette(editor, palette);
    } else if (effective == QStringLiteral("json")) {
        applyJsonPalette(editor, palette);
    } else if (effective == QStringLiteral("props")) {
        applyPropsPalette(editor, palette);
    }
    // For cpp/python/css/sql/etc the generic palette in Theme.cpp already
    // covers the right indices.

    editor->colourise(0, -1);
}

void applyLexerForPath(ScintillaEdit* editor, const QString& path)
{
    if (!editor)
        return;
    applyLexerByName(editor, lexerNameForPath(path));
}

QStringList supportedExtensions()
{
    QStringList keys = extensionTable().keys();
    keys.sort();
    return keys;
}

} // namespace LexerMap
