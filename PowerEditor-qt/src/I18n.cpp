#include "I18n.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace {

// Candidate directories where compiled .qm files may live, in priority order.
QStringList appTranslationSearchPaths()
{
    QStringList paths;
    paths << QStringLiteral(":/translations/");
    paths << QCoreApplication::applicationDirPath() + QStringLiteral("/translations/");
    paths << QStringLiteral("/usr/share/notepadpp-qt/translations/");
    return paths;
}

// Try to load a translator from any of the search paths.
// On success: installs the translator into `app`, sets `parent`, and returns true.
// On failure: deletes the QTranslator and returns false.
bool tryLoadAndInstall(QApplication* app,
                       QTranslator* translator,
                       const QString& filenameStem,
                       const QStringList& searchPaths)
{
    for (const QString& dir : searchPaths) {
        if (translator->load(filenameStem, dir)) {
            translator->setParent(app);
            app->installTranslator(translator);
            return true;
        }
    }
    return false;
}

// Try to load a Qt-base translator from Qt's own translations directory.
// Tries qtbase_<locale>.qm first, then qt_<locale>.qm as fallback.
bool tryLoadQtBaseTranslator(QApplication* app, const QString& localeCode)
{
    const QString qtTranslationsDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    auto* qtTranslator = new QTranslator();
    if (qtTranslator->load(QStringLiteral("qtbase_") + localeCode, qtTranslationsDir)) {
        qtTranslator->setParent(app);
        app->installTranslator(qtTranslator);
        return true;
    }
    if (qtTranslator->load(QStringLiteral("qt_") + localeCode, qtTranslationsDir)) {
        qtTranslator->setParent(app);
        app->installTranslator(qtTranslator);
        return true;
    }

    // Also try the translation search paths in case Qt translations are bundled
    // alongside the app translations (common for portable / AppImage builds).
    const QStringList searchPaths = appTranslationSearchPaths();
    for (const QString& dir : searchPaths) {
        if (qtTranslator->load(QStringLiteral("qtbase_") + localeCode, dir)) {
            qtTranslator->setParent(app);
            app->installTranslator(qtTranslator);
            return true;
        }
        if (qtTranslator->load(QStringLiteral("qt_") + localeCode, dir)) {
            qtTranslator->setParent(app);
            app->installTranslator(qtTranslator);
            return true;
        }
    }

    delete qtTranslator;
    return false;
}

} // namespace

namespace I18n {

bool loadLocale(QApplication* app, const QString& localeCode)
{
    if (!app || localeCode.isEmpty()) {
        return false;
    }

    // Install Qt's standard widget strings (file dialogs, message boxes) first,
    // so the app strings layer on top.
    tryLoadQtBaseTranslator(app, localeCode);

    // Install the application-specific translator.
    auto* appTranslator = new QTranslator();
    const QString stem = QStringLiteral("notepadpp-qt_") + localeCode;
    const QStringList searchPaths = appTranslationSearchPaths();

    if (!tryLoadAndInstall(app, appTranslator, stem, searchPaths)) {
        delete appTranslator;
        return false;
    }
    return true;
}

bool loadDefaultTranslations(QApplication* app)
{
    if (!app) {
        return false;
    }

    // Per project policy: pt_BR is the default UI language. Honour the system
    // locale only when it explicitly starts with "pt" (e.g. pt_PT, pt_BR);
    // otherwise still default to pt_BR as the user preference.
    const QString systemName = QLocale::system().name(); // e.g. "pt_BR", "en_US"
    QString chosen = QStringLiteral("pt_BR");
    if (systemName.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive)) {
        chosen = QStringLiteral("pt_BR");
    }

    return loadLocale(app, chosen);
}

} // namespace I18n
