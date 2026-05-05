#pragma once

#include <QObject>
#include <QPointer>

class QMenu;
class QAction;
class QActionGroup;
class QWidget;
class ScintillaEdit;

class EolMenu : public QObject
{
    Q_OBJECT
public:
    explicit EolMenu(QObject* parent = nullptr);

    QMenu* createMenu(QWidget* menuParent);

    // Bind to editor; menu actions become enabled/disabled accordingly.
    void setActiveEditor(ScintillaEdit* editor);

    // Reflect the current EOL mode on the radio actions (call after editor changes file).
    void syncCurrentMode();

signals:
    void eolModeChanged(int sciEolMode);   // SC_EOL_CRLF=0, SC_EOL_CR=1, SC_EOL_LF=2

private:
    void applySetMode(int mode);
    void applyConvert(int mode);
    void updateActionsEnabled();

    QPointer<ScintillaEdit> m_editor;

    QPointer<QMenu>        m_menu;
    QPointer<QMenu>        m_setModeMenu;
    QPointer<QActionGroup> m_modeGroup;

    QPointer<QAction> m_setLfAction;
    QPointer<QAction> m_setCrlfAction;
    QPointer<QAction> m_setCrAction;

    QPointer<QAction> m_convertLfAction;
    QPointer<QAction> m_convertCrlfAction;
    QPointer<QAction> m_convertCrAction;
};
