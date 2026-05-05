#pragma once

#include <QObject>
#include <QString>

class QMenu;
class QAction;
class QActionGroup;
class QWidget;
class ScintillaEdit;
class MainWindow;

class LanguagesMenu : public QObject {
    Q_OBJECT
public:
    explicit LanguagesMenu(QObject* parent = nullptr);

    // Builds and returns a QMenu titled "&Languages" (translatable).
    // Top section: pinned favorites (C#, Python, JavaScript, PHP, MySQL, JSON), separator,
    // then alphabetical full list. The menu is owned by menuParent (typically the QMenuBar).
    QMenu* createMenu(QWidget* menuParent);

    // Set the editor that the menu will operate on. Pass nullptr when no tab is active
    // (the menu will be disabled in that case).
    void setActiveEditor(ScintillaEdit* editor);

    // Reflect the currently active language as a checked item. Call after the language
    // changes (e.g. after applyLexerForPath). Empty lexerName checks "Plain Text".
    void syncCheckedLanguage(const QString& lexerName);

signals:
    // Emitted when the user picks a language. Payload = Lexilla lexer name
    // (e.g. "python", "cpp", "javascript", "phpscript", "json", "sql").
    // Empty string means "Plain Text".
    void languageSelected(const QString& lexerName);

private slots:
    void onActionTriggered();

private:
    QAction* addLanguageAction(QMenu* parentMenu, const QString& label, const QString& lexerName);

    QMenu* m_menu = nullptr;
    QActionGroup* m_group = nullptr;
    ScintillaEdit* m_editor = nullptr;
};
