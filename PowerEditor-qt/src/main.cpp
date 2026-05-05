#include "MainWindow.h"
#include "Settings.h"
#include "Theme.h"
#include "I18n.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("clip52");
    QCoreApplication::setOrganizationDomain("github.com/clip52");
    QCoreApplication::setApplicationName("notepadpp-qt");
    QCoreApplication::setApplicationVersion("0.1.0");

    // Default UI language: pt-BR (loadDefaultTranslations falls back to pt-BR per project policy).
    I18n::loadDefaultTranslations(&app);

    QCommandLineParser parser;
    parser.setApplicationDescription("Notepad++ Qt — native Linux Qt6 port");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Files to open", "[files...]");
    parser.process(app);

    ThemeManager::apply(&app, Settings::instance().darkTheme() ? AppTheme::Dark : AppTheme::Light);

    MainWindow w;
    w.show();

    const QStringList files = parser.positionalArguments();
    for (const QString& f : files) w.openFile(f);

    return app.exec();
}
