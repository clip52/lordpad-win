#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QFile>
#include <QColor>
#include <QString>
#include <QByteArray>

#include "ScintillaEdit.h"

namespace {

AppTheme g_currentTheme = AppTheme::Light;

// Scintilla expects COLORREF as 0x00BBGGRR
inline int toSciColor(const QColor& c)
{
    return (c.blue() << 16) | (c.green() << 8) | c.red();
}

QPalette buildLightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#F5F5F5"));
    p.setColor(QPalette::WindowText,      QColor("#000000"));
    p.setColor(QPalette::Base,            QColor("#FFFFFF"));
    p.setColor(QPalette::AlternateBase,   QColor("#F0F0F0"));
    p.setColor(QPalette::ToolTipBase,     QColor("#FFFFDC"));
    p.setColor(QPalette::ToolTipText,     QColor("#000000"));
    p.setColor(QPalette::Text,            QColor("#000000"));
    p.setColor(QPalette::Button,          QColor("#F5F5F5"));
    p.setColor(QPalette::ButtonText,      QColor("#000000"));
    p.setColor(QPalette::BrightText,      QColor("#FF0000"));
    p.setColor(QPalette::Link,            QColor("#0066CC"));
    p.setColor(QPalette::Highlight,       QColor("#ADD6FF"));
    p.setColor(QPalette::HighlightedText, QColor("#000000"));
    p.setColor(QPalette::PlaceholderText, QColor("#808080"));

    p.setColor(QPalette::Disabled, QPalette::Text,            QColor("#A0A0A0"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor("#A0A0A0"));
    p.setColor(QPalette::Disabled, QPalette::WindowText,      QColor("#A0A0A0"));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#A0A0A0"));
    return p;
}

QPalette buildDarkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#252526"));
    p.setColor(QPalette::WindowText,      QColor("#D4D4D4"));
    p.setColor(QPalette::Base,            QColor("#1E1E1E"));
    p.setColor(QPalette::AlternateBase,   QColor("#2D2D30"));
    p.setColor(QPalette::ToolTipBase,     QColor("#2D2D30"));
    p.setColor(QPalette::ToolTipText,     QColor("#D4D4D4"));
    p.setColor(QPalette::Text,            QColor("#D4D4D4"));
    p.setColor(QPalette::Button,          QColor("#3C3C3C"));
    p.setColor(QPalette::ButtonText,      QColor("#D4D4D4"));
    p.setColor(QPalette::BrightText,      QColor("#FF5555"));
    p.setColor(QPalette::Link,            QColor("#3794FF"));
    p.setColor(QPalette::Highlight,       QColor("#264F78"));
    p.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    p.setColor(QPalette::PlaceholderText, QColor("#858585"));

    p.setColor(QPalette::Disabled, QPalette::Text,            QColor("#6D6D6D"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor("#6D6D6D"));
    p.setColor(QPalette::Disabled, QPalette::WindowText,      QColor("#6D6D6D"));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#A0A0A0"));
    return p;
}

// Dracula — official palette: https://draculatheme.com/contribute
//   bg #282A36, current line #44475A, fg #F8F8F2, comment #6272A4
//   cyan #8BE9FD, green #50FA7B, orange #FFB86C, pink #FF79C6, purple #BD93F9, red #FF5555, yellow #F1FA8C
QPalette buildDraculaPalette()
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#282A36"));
    p.setColor(QPalette::WindowText,      QColor("#F8F8F2"));
    p.setColor(QPalette::Base,            QColor("#21222C"));
    p.setColor(QPalette::AlternateBase,   QColor("#343746"));
    p.setColor(QPalette::ToolTipBase,     QColor("#44475A"));
    p.setColor(QPalette::ToolTipText,     QColor("#F8F8F2"));
    p.setColor(QPalette::Text,            QColor("#F8F8F2"));
    p.setColor(QPalette::Button,          QColor("#44475A"));
    p.setColor(QPalette::ButtonText,      QColor("#F8F8F2"));
    p.setColor(QPalette::BrightText,      QColor("#FF5555"));
    p.setColor(QPalette::Link,            QColor("#8BE9FD"));
    p.setColor(QPalette::Highlight,       QColor("#BD93F9"));
    p.setColor(QPalette::HighlightedText, QColor("#282A36"));
    p.setColor(QPalette::PlaceholderText, QColor("#6272A4"));

    p.setColor(QPalette::Disabled, QPalette::Text,            QColor("#6272A4"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor("#6272A4"));
    p.setColor(QPalette::Disabled, QPalette::WindowText,      QColor("#6272A4"));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#A0A0A0"));
    return p;
}

QString loadQss(AppTheme theme)
{
    QString path;
    switch (theme) {
        case AppTheme::Dark:    path = QStringLiteral(":/themes/dark.qss"); break;
        case AppTheme::Dracula: path = QStringLiteral(":/themes/dracula.qss"); break;
        case AppTheme::Light:
        default:                path = QStringLiteral(":/themes/light.qss"); break;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    const QByteArray data = f.readAll();
    f.close();
    return QString::fromUtf8(data);
}

} // namespace

namespace ThemeManager {

void apply(QApplication* app, AppTheme theme)
{
    if (!app) {
        return;
    }

    QApplication::setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    switch (theme) {
        case AppTheme::Dark:    palette = buildDarkPalette();    break;
        case AppTheme::Dracula: palette = buildDraculaPalette(); break;
        case AppTheme::Light:
        default:                palette = buildLightPalette();   break;
    }

    app->setPalette(palette);
    app->setStyleSheet(loadQss(theme));

    g_currentTheme = theme;
}

void applyToScintilla(ScintillaEdit* editor, AppTheme theme,
                      const QString& fontFamily, int fontSize)
{
    if (!editor) {
        return;
    }

    QColor bg, fg, caret, caretLineBg, selBg, lnBg, lnFg, foldBg;

    switch (theme) {
        case AppTheme::Dark:
            bg = "#1E1E1E"; fg = "#D4D4D4"; caret = "#FFFFFF";
            caretLineBg = "#2A2A2A"; selBg = "#264F78";
            lnBg = "#1E1E1E"; lnFg = "#858585"; foldBg = "#252526";
            break;
        case AppTheme::Dracula:
            bg = "#282A36"; fg = "#F8F8F2"; caret = "#F8F8F2";
            caretLineBg = "#44475A"; selBg = "#44475A";
            lnBg = "#282A36"; lnFg = "#6272A4"; foldBg = "#21222C";
            break;
        case AppTheme::Light:
        default:
            bg = "#FFFFFF"; fg = "#000000"; caret = "#000000";
            caretLineBg = "#F0F0F0"; selBg = "#ADD6FF";
            lnBg = "#F0F0F0"; lnFg = "#808080"; foldBg = "#F0F0F0";
            break;
    }

    // Default style: applied to STYLE_DEFAULT then propagated via STYLECLEARALL.
    const QByteArray fontUtf8 = fontFamily.isEmpty()
        ? QByteArrayLiteral("Monospace")
        : fontFamily.toUtf8();
    const int sz = (fontSize > 0 && fontSize < 64) ? fontSize : 11;
    editor->styleSetFore(STYLE_DEFAULT, toSciColor(fg));
    editor->styleSetBack(STYLE_DEFAULT, toSciColor(bg));
    editor->styleSetFont(STYLE_DEFAULT, fontUtf8.constData());
    editor->styleSetSize(STYLE_DEFAULT, sz);
    editor->styleClearAll();

    // ----- Syntax-highlighting palette (applied AFTER styleClearAll) -----
    // Style indices below are the conventions used by most Lexilla lexers
    // (cpp, python, css, html, sql, etc.). JSON / a few others use different
    // indices but the colors will still be readable in those cases.
    QColor sComment, sNumber, sKeyword, sString, sChar, sPreproc, sOperator, sIdentifier;
    switch (theme) {
        case AppTheme::Dark:
            sComment    = "#6A9955"; sNumber  = "#B5CEA8"; sKeyword = "#569CD6";
            sString     = "#CE9178"; sChar    = "#D7BA7D"; sPreproc = "#C586C0";
            sOperator   = "#D4D4D4"; sIdentifier = "#9CDCFE";
            break;
        case AppTheme::Dracula:
            sComment    = "#6272A4"; sNumber  = "#BD93F9"; sKeyword = "#FF79C6";
            sString     = "#F1FA8C"; sChar    = "#F1FA8C"; sPreproc = "#BD93F9";
            sOperator   = "#F8F8F2"; sIdentifier = "#50FA7B";
            break;
        case AppTheme::Light:
        default:
            sComment    = "#008000"; sNumber  = "#FF8000"; sKeyword = "#0000FF";
            sString     = "#A31515"; sChar    = "#A31515"; sPreproc = "#804080";
            sOperator   = "#000080"; sIdentifier = "#000000";
            break;
    }

    auto setFg = [&](int style, const QColor& c) {
        editor->styleSetFore(style, toSciColor(c));
    };
    setFg(1,  sComment);     // SCE_C_COMMENT, SCE_PY_COMMENTLINE, SCE_CSS_COMMENT, SCE_SQL_COMMENT, ...
    setFg(2,  sComment);     // SCE_C_COMMENTLINE, SCE_PY_NUMBER (close enough), ...
    setFg(3,  sComment);     // SCE_C_COMMENTDOC
    setFg(4,  sNumber);      // SCE_C_NUMBER, SCE_PY_STRING (off, but readable), SCE_HJ_NUMBER
    setFg(5,  sKeyword);     // SCE_C_WORD (keywords), SCE_PY_WORD, SCE_HTML_TAG
    setFg(6,  sString);      // SCE_C_STRING, SCE_PY_CHARACTER, SCE_HTML_DOUBLESTRING
    setFg(7,  sChar);        // SCE_C_CHARACTER, SCE_PY_WORD, SCE_HTML_SINGLESTRING
    setFg(8,  sPreproc);     // SCE_C_UUID
    setFg(9,  sPreproc);     // SCE_C_PREPROCESSOR
    setFg(10, sOperator);    // SCE_C_OPERATOR, SCE_PY_OPERATOR, SCE_CSS_OPERATOR
    setFg(11, sIdentifier);  // SCE_C_IDENTIFIER, SCE_PY_IDENTIFIER
    setFg(12, sString);      // SCE_C_STRINGEOL, SCE_PY_TRIPLE
    setFg(13, sString);      // SCE_C_VERBATIM, SCE_PY_TRIPLEDOUBLE
    setFg(14, sString);      // SCE_C_REGEX
    setFg(15, sComment);     // SCE_C_COMMENTLINEDOC
    setFg(16, sKeyword);     // SCE_C_WORD2 (secondary keywords)
    setFg(17, sComment);     // SCE_C_COMMENTDOCKEYWORD
    setFg(19, sIdentifier);  // SCE_C_GLOBALCLASS, SCE_PY_DECORATOR
    setFg(20, sString);      // SCE_C_STRINGRAW
    setFg(21, sChar);        // SCE_C_TRIPLEVERBATIM
    setFg(22, sNumber);      // SCE_C_HASHQUOTEDSTRING
    setFg(24, sNumber);      // SCE_C_USERLITERAL
    setFg(25, sKeyword);     // SCE_C_TASKMARKER

    // Caret + caret line.
    editor->setCaretFore(toSciColor(caret));
    editor->setCaretLineBack(toSciColor(caretLineBg));
    editor->setCaretLineVisible(true);

    // Selection background.
    editor->setSelBack(true, toSciColor(selBg));

    // Line number margin (style 33 = STYLE_LINENUMBER).
    editor->styleSetFore(STYLE_LINENUMBER, toSciColor(lnFg));
    editor->styleSetBack(STYLE_LINENUMBER, toSciColor(lnBg));

    // Fold margin colours.
    editor->setFoldMarginColour(true, toSciColor(foldBg));
    editor->setFoldMarginHiColour(true, toSciColor(foldBg));
}

AppTheme current()
{
    return g_currentTheme;
}

} // namespace ThemeManager
