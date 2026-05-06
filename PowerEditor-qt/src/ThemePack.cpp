#include "ThemePack.h"

#include <QApplication>
#include <QColor>
#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QString>

#include "ScintillaEdit.h"

// ---------------------------------------------------------------------------
// Cores e paletas
// ---------------------------------------------------------------------------
//
// Scintilla espera COLORREF no formato 0x00BBGGRR — little-endian. Por isso
// vermelho 0xFF0000 (HTML) vira 0x0000FF aqui.
//
namespace {

inline int sciColor(const QColor& c)
{
    return (c.blue() << 16) | (c.green() << 8) | c.red();
}

// Estrutura única que descreve um theme pack — usada tanto para o QPalette
// quanto para o Scintilla. Manter cores por nome facilita ajustes finos.
struct Pack {
    // Janelas / chrome
    QColor bg;            // editor background, base window
    QColor fg;            // foreground default (texto)
    QColor altBg;         // gutter / línea-corrente / fold margin
    QColor selBg;         // seleção
    QColor lineNumberFg;  // número de linha
    QColor caret;         // caret
    QColor highlight;     // accent / highlight (botões, foco)
    QColor highlightFg;

    // Sintaxe (cpp lexer)
    QColor comment;
    QColor number;
    QColor keyword;
    QColor string;
    QColor charLit;
    QColor preproc;
    QColor op;
    QColor identifier;
    QColor type;          // type keyword / class — opcional, fallback p/ keyword

    bool   isLight = false;
};

// --- Solarized (Ethan Schoonover) -----------------------------------------
// base03=#002b36  base02=#073642  base01=#586e75  base00=#657b83
// base0 =#839496  base1 =#93a1a1  base2 =#eee8d5  base3 =#fdf6e3
// yellow=#b58900 orange=#cb4b16 red=#dc322f magenta=#d33682
// violet=#6c71c4 blue  =#268bd2 cyan  =#2aa198 green=#859900
Pack solarizedLight()
{
    Pack p;
    p.bg          = "#fdf6e3"; // base3
    p.fg          = "#657b83"; // base00
    p.altBg       = "#eee8d5"; // base2
    p.selBg       = "#eee8d5"; // base2
    p.lineNumberFg= "#93a1a1"; // base1
    p.caret       = "#586e75"; // base01
    p.highlight   = "#268bd2"; // blue
    p.highlightFg = "#fdf6e3"; // base3
    p.comment     = "#93a1a1"; // base1
    p.number      = "#d33682"; // magenta
    p.keyword     = "#859900"; // green
    p.string      = "#2aa198"; // cyan
    p.charLit     = "#2aa198"; // cyan
    p.preproc     = "#cb4b16"; // orange
    p.op          = "#586e75"; // base01
    p.identifier  = "#268bd2"; // blue
    p.type        = "#b58900"; // yellow
    p.isLight     = true;
    return p;
}

Pack solarizedDark()
{
    Pack p;
    p.bg          = "#002b36"; // base03
    p.fg          = "#839496"; // base0
    p.altBg       = "#073642"; // base02
    p.selBg       = "#073642"; // base02
    p.lineNumberFg= "#586e75"; // base01
    p.caret       = "#93a1a1"; // base1
    p.highlight   = "#268bd2"; // blue
    p.highlightFg = "#fdf6e3"; // base3
    p.comment     = "#586e75"; // base01
    p.number      = "#d33682"; // magenta
    p.keyword     = "#859900"; // green
    p.string      = "#2aa198"; // cyan
    p.charLit     = "#2aa198"; // cyan
    p.preproc     = "#cb4b16"; // orange
    p.op          = "#93a1a1"; // base1
    p.identifier  = "#268bd2"; // blue
    p.type        = "#b58900"; // yellow
    p.isLight     = false;
    return p;
}

// --- Monokai (clássico, sublime-ish) --------------------------------------
Pack monokai()
{
    Pack p;
    p.bg          = "#272822";
    p.fg          = "#f8f8f2";
    p.altBg       = "#3e3d32";
    p.selBg       = "#49483e";
    p.lineNumberFg= "#75715e";
    p.caret       = "#f8f8f0";
    p.highlight   = "#f92672";
    p.highlightFg = "#f8f8f2";
    p.comment     = "#75715e";
    p.number      = "#ae81ff";
    p.keyword     = "#f92672";
    p.string      = "#e6db74";
    p.charLit     = "#e6db74";
    p.preproc     = "#f92672";
    p.op          = "#f8f8f2";
    p.identifier  = "#a6e22e"; // function/identifier highlight
    p.type        = "#66d9ef";
    p.isLight     = false;
    return p;
}

// --- Nord (arcticicestudio) -----------------------------------------------
// nord0=#2e3440 nord1=#3b4252 nord3=#4c566a nord4=#d8dee9 nord6=#eceff4
// nord7=#8fbcbb nord8=#88c0d0 nord9=#81a1c1 nord10=#5e81ac
// nord11=#bf616a nord13=#ebcb8b nord14=#a3be8c nord15=#b48ead
Pack nord()
{
    Pack p;
    p.bg          = "#2e3440"; // nord0
    p.fg          = "#d8dee9"; // nord4
    p.altBg       = "#3b4252"; // nord1
    p.selBg       = "#434c5e"; // nord2-ish
    p.lineNumberFg= "#4c566a"; // nord3
    p.caret       = "#eceff4"; // nord6
    p.highlight   = "#88c0d0"; // nord8
    p.highlightFg = "#2e3440"; // nord0
    p.comment     = "#4c566a"; // nord3
    p.number      = "#b48ead"; // nord15
    p.keyword     = "#81a1c1"; // nord9
    p.string      = "#a3be8c"; // nord14
    p.charLit     = "#a3be8c"; // nord14
    p.preproc     = "#5e81ac"; // nord10
    p.op          = "#eceff4"; // nord6
    p.identifier  = "#88c0d0"; // nord8 (functions)
    p.type        = "#8fbcbb"; // nord7
    p.isLight     = false;
    return p;
}

const Pack* lookup(ThemePackId id)
{
    static const Pack pSL = solarizedLight();
    static const Pack pSD = solarizedDark();
    static const Pack pMK = monokai();
    static const Pack pND = nord();
    switch (id) {
        case ThemePackId::SolarizedLight: return &pSL;
        case ThemePackId::SolarizedDark:  return &pSD;
        case ThemePackId::Monokai:        return &pMK;
        case ThemePackId::Nord:           return &pND;
        case ThemePackId::None:
        default:                          return nullptr;
    }
}

// QPalette derivada do Pack — baseada em Window/Base/Text/Highlight + ToolTip.
QPalette buildPalette(const Pack& p)
{
    QPalette pal;
    pal.setColor(QPalette::Window,          p.altBg);
    pal.setColor(QPalette::WindowText,      p.fg);
    pal.setColor(QPalette::Base,            p.bg);
    pal.setColor(QPalette::AlternateBase,   p.altBg);
    pal.setColor(QPalette::Text,            p.fg);
    pal.setColor(QPalette::PlaceholderText, p.lineNumberFg);
    pal.setColor(QPalette::Button,          p.altBg);
    pal.setColor(QPalette::ButtonText,      p.fg);
    pal.setColor(QPalette::BrightText,      p.highlight);
    pal.setColor(QPalette::Highlight,       p.highlight);
    pal.setColor(QPalette::HighlightedText, p.highlightFg);
    pal.setColor(QPalette::ToolTipBase,     p.altBg);
    pal.setColor(QPalette::ToolTipText,     p.fg);
    pal.setColor(QPalette::Link,            p.highlight);
    pal.setColor(QPalette::LinkVisited,     p.preproc);

    // Disabled — esmaecer texto.
    QColor dimmed = p.lineNumberFg;
    pal.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
    pal.setColor(QPalette::Disabled, QPalette::Text,       dimmed);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);
    return pal;
}

// Reescreve TODOS os 33 estilos do lexer cpp + universais 32..39 + chrome.
//
// Mapeamento Scintilla (cpp / SCE_C_*):
//   0  DEFAULT     1  COMMENT       2  COMMENTLINE   3  COMMENTDOC
//   4  NUMBER      5  WORD(kw)      6  STRING        7  CHARACTER
//   8  UUID        9  PREPROCESSOR  10 OPERATOR      11 IDENTIFIER
//   12 STRINGEOL   13 VERBATIM      14 REGEX         15 COMMENTLINEDOC
//   16 WORD2       17 COMMENTDOCKW  18 COMMENTDOCKWERR  19 GLOBALCLASS
//   20 STRINGRAW   21 TRIPLEVERBATIM 22 HASHQUOTEDSTRING 23 PREPROCESSORCOMMENT
//   24 PREPROCESSORCOMMENTDOC 25 USERLITERAL 26 TASKMARKER 27 ESCAPESEQUENCE
// Universais:
//   32 STYLE_DEFAULT  33 STYLE_LINENUMBER  34 STYLE_BRACELIGHT
//   35 STYLE_BRACEBAD 36 STYLE_CONTROLCHAR 37 STYLE_INDENTGUIDE
//   38 STYLE_CALLTIP  39 STYLE_FOLDDISPLAYTEXT
void applyEditorPack(ScintillaEdit* ed, const Pack& p,
                     const QString& fontFamily, int fontSize)
{
    if (!ed) return;

    const int fg  = sciColor(p.fg);
    const int bg  = sciColor(p.bg);
    const int alt = sciColor(p.altBg);
    const int ln  = sciColor(p.lineNumberFg);

    // STYLE_DEFAULT (32) primeiro — depois STYLECLEARALL propaga para os 0..31.
    const QByteArray fontUtf8 = fontFamily.isEmpty()
        ? QByteArrayLiteral("Monospace")
        : fontFamily.toUtf8();
    const int sz = (fontSize > 0 && fontSize < 64) ? fontSize : 11;

    ed->styleSetFore(STYLE_DEFAULT, fg);
    ed->styleSetBack(STYLE_DEFAULT, bg);
    ed->styleSetFont(STYLE_DEFAULT, fontUtf8.constData());
    ed->styleSetSize(STYLE_DEFAULT, sz);
    ed->styleClearAll();

    // Sintaxe — fore para os índices do lexer cpp (e compatíveis).
    auto setFg = [&](int s, const QColor& c) {
        ed->styleSetFore(s, sciColor(c));
    };
    auto setBg = [&](int s, const QColor& c) {
        ed->styleSetBack(s, sciColor(c));
    };

    // Garantir que o background dos 0..31 já é o do pack (styleClearAll fez,
    // mas reforçamos para o caso de algum lexer ter sobrescrito antes).
    for (int s = 0; s < 32; ++s) setBg(s, p.bg);

    setFg(0,  p.fg);          // DEFAULT
    setFg(1,  p.comment);     // COMMENT
    setFg(2,  p.comment);     // COMMENTLINE
    setFg(3,  p.comment);     // COMMENTDOC
    setFg(4,  p.number);      // NUMBER
    setFg(5,  p.keyword);     // WORD (keywords)
    setFg(6,  p.string);      // STRING
    setFg(7,  p.charLit);     // CHARACTER
    setFg(8,  p.preproc);     // UUID
    setFg(9,  p.preproc);     // PREPROCESSOR
    setFg(10, p.op);          // OPERATOR
    setFg(11, p.identifier);  // IDENTIFIER
    setFg(12, p.string);      // STRINGEOL
    setFg(13, p.string);      // VERBATIM
    setFg(14, p.string);      // REGEX
    setFg(15, p.comment);     // COMMENTLINEDOC
    setFg(16, p.type);        // WORD2 (secondary keywords / types)
    setFg(17, p.comment);     // COMMENTDOCKW
    setFg(18, p.preproc);     // COMMENTDOCKWERR
    setFg(19, p.type);        // GLOBALCLASS
    setFg(20, p.string);      // STRINGRAW
    setFg(21, p.charLit);     // TRIPLEVERBATIM
    setFg(22, p.number);      // HASHQUOTEDSTRING
    setFg(23, p.comment);     // PREPROCESSORCOMMENT
    setFg(24, p.comment);     // PREPROCESSORCOMMENTDOC
    setFg(25, p.number);      // USERLITERAL
    setFg(26, p.keyword);     // TASKMARKER
    setFg(27, p.preproc);     // ESCAPESEQUENCE

    // Universais 33..39.
    ed->styleSetFore(STYLE_LINENUMBER, ln);
    ed->styleSetBack(STYLE_LINENUMBER, alt);
    ed->styleSetFore(STYLE_BRACELIGHT, sciColor(p.highlight));
    ed->styleSetBack(STYLE_BRACELIGHT, bg);
    ed->styleSetFore(STYLE_BRACEBAD,   sciColor(QColor("#ff5555")));
    ed->styleSetBack(STYLE_BRACEBAD,   bg);
    ed->styleSetFore(STYLE_CONTROLCHAR, ln);
    ed->styleSetBack(STYLE_CONTROLCHAR, bg);
    ed->styleSetFore(STYLE_INDENTGUIDE, ln);
    ed->styleSetBack(STYLE_INDENTGUIDE, bg);
    ed->styleSetFore(STYLE_CALLTIP,    fg);
    ed->styleSetBack(STYLE_CALLTIP,    alt);
    ed->styleSetFore(STYLE_FOLDDISPLAYTEXT, ln);
    ed->styleSetBack(STYLE_FOLDDISPLAYTEXT, alt);

    // Caret + linha corrente.
    ed->setCaretFore(sciColor(p.caret));
    ed->setCaretLineBack(sciColor(p.altBg));
    ed->setCaretLineVisible(true);

    // Seleção.
    ed->setSelBack(true, sciColor(p.selBg));

    // Fold margin.
    ed->setFoldMarginColour(true, sciColor(p.altBg));
    ed->setFoldMarginHiColour(true, sciColor(p.altBg));
}

constexpr const char* kSettingsOrg     = "clip52";
constexpr const char* kSettingsApp     = "notepadpp-qt";
constexpr const char* kSettingsKeyPack = "Theme/PackId";

} // namespace

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------
namespace ThemePack {

QString displayName(ThemePackId id)
{
    switch (id) {
        case ThemePackId::None:           return QObject::tr("Padrão (sem pack)");
        case ThemePackId::SolarizedLight: return QStringLiteral("Solarized Claro");
        case ThemePackId::SolarizedDark:  return QStringLiteral("Solarized Escuro");
        case ThemePackId::Monokai:        return QStringLiteral("Monokai");
        case ThemePackId::Nord:           return QStringLiteral("Nord");
    }
    return QStringLiteral("Padrão (sem pack)");
}

void applyToApp(QApplication* app, ThemePackId id)
{
    if (!app) return;
    const Pack* p = lookup(id);
    if (!p) return; // None — deixa o tema base intacto
    app->setPalette(buildPalette(*p));
}

void applyToEditor(ScintillaEdit* editor, ThemePackId id,
                   const QString& fontFamily, int fontSize)
{
    if (!editor) return;
    const Pack* p = lookup(id);
    if (!p) return; // None — deixa o tema base intacto
    applyEditorPack(editor, *p, fontFamily, fontSize);
}

ThemePackId loaded()
{
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    const int v = s.value(QString::fromLatin1(kSettingsKeyPack),
                          static_cast<int>(ThemePackId::None)).toInt();
    switch (v) {
        case static_cast<int>(ThemePackId::SolarizedLight):
        case static_cast<int>(ThemePackId::SolarizedDark):
        case static_cast<int>(ThemePackId::Monokai):
        case static_cast<int>(ThemePackId::Nord):
            return static_cast<ThemePackId>(v);
        default:
            return ThemePackId::None;
    }
}

void save(ThemePackId id)
{
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    s.setValue(QString::fromLatin1(kSettingsKeyPack),
               static_cast<int>(id));
}

} // namespace ThemePack
