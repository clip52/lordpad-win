#pragma once
//
// ThemePack — camada opcional de tema aplicada DEPOIS do AppTheme base.
//
// Não invade o enum AppTheme {Light, Dark, Dracula} de Theme.h. Em vez disso,
// reaplica QPalette no QApplication e reescreve as cores Scintilla (estilos do
// lexer cpp + universais 32..39) em cima do que o ThemeManager já fez.
//
// Uso típico:
//     ThemeManager::apply(app, AppTheme::Dark);
//     ThemeManager::applyToScintilla(editor, AppTheme::Dark, font, size);
//     ThemePack::applyToApp(app, ThemePackId::Monokai);
//     ThemePack::applyToEditor(editor, ThemePackId::Monokai, font, size);
//
// Persistência: QSettings("clip52","notepadpp-qt"), chave "Theme/PackId" (int).
//
#include <QString>

class QApplication;
class ScintillaEdit;

enum class ThemePackId {
    None = 0,         // não aplicar pack — apenas o AppTheme base
    SolarizedLight = 1,
    SolarizedDark  = 2,
    Monokai        = 3,
    Nord           = 4
};

namespace ThemePack {

// Nome amigável (pt-BR) para uso em menus/UI.
QString displayName(ThemePackId id);

// Aplica QPalette do pack ao QApplication. Para None, é no-op.
void applyToApp(QApplication* app, ThemePackId id);

// Reescreve cores Scintilla (estilos do lexer cpp + universais 32..39 + caret,
// linha-corrente, seleção, número de linha, fold). Para None, é no-op.
void applyToEditor(ScintillaEdit* editor, ThemePackId id,
                   const QString& fontFamily, int fontSize);

// Persistência via QSettings("clip52","notepadpp-qt"), chave "Theme/PackId".
ThemePackId loaded();
void save(ThemePackId id);

} // namespace ThemePack
