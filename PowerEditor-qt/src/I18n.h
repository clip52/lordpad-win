#pragma once

class QApplication;
class QString;

namespace I18n {
    // Loads pt-BR translation + Qt's own pt-BR translation if QLocale default is pt or no override is set.
    // Looks for compiled .qm in: 1) :/translations/   2) <appdir>/translations/   3) /usr/share/notepadpp-qt/translations/
    // Returns true if a translator was successfully installed.
    bool loadDefaultTranslations(QApplication* app);

    // Force load a specific locale code like "pt_BR" or "en_US". Useful if user overrides via Preferences.
    bool loadLocale(QApplication* app, const QString& localeCode);
}
