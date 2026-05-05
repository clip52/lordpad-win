#pragma once
#include <QString>

class QApplication;
class ScintillaEdit;

enum class AppTheme { Light, Dark, Dracula };

namespace ThemeManager {
    void apply(QApplication* app, AppTheme theme);
    // Apply theme styles AND a font family/size to a Scintilla editor.
    // Caller passes the font from Settings (so theme module stays decoupled).
    void applyToScintilla(ScintillaEdit* editor, AppTheme theme,
                          const QString& fontFamily, int fontSize);
    // Convenience overload with default font (used early in startup before Settings is read).
    inline void applyToScintilla(ScintillaEdit* editor, AppTheme theme) {
        applyToScintilla(editor, theme, QStringLiteral("Monospace"), 11);
    }
    AppTheme current();
}
