#include "EolMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>

#include "ScintillaEdit.h"

EolMenu::EolMenu(QObject* parent)
    : QObject(parent)
{
}

QMenu* EolMenu::createMenu(QWidget* menuParent)
{
    if (m_menu)
        return m_menu;

    m_menu = new QMenu(tr("&End-of-Line"), menuParent);

    // --- Set Mode submenu (radio actions) ---
    m_setModeMenu = m_menu->addMenu(tr("Set Mode"));

    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);

    m_setLfAction = m_setModeMenu->addAction(tr("LF (Unix / macOS)"));
    m_setLfAction->setCheckable(true);
    m_setLfAction->setData(static_cast<int>(SC_EOL_LF));
    m_modeGroup->addAction(m_setLfAction);
    connect(m_setLfAction, &QAction::triggered, this,
            [this]() { applySetMode(SC_EOL_LF); });

    m_setCrlfAction = m_setModeMenu->addAction(tr("CRLF (Windows)"));
    m_setCrlfAction->setCheckable(true);
    m_setCrlfAction->setData(static_cast<int>(SC_EOL_CRLF));
    m_modeGroup->addAction(m_setCrlfAction);
    connect(m_setCrlfAction, &QAction::triggered, this,
            [this]() { applySetMode(SC_EOL_CRLF); });

    m_setCrAction = m_setModeMenu->addAction(tr("CR (classic Mac)"));
    m_setCrAction->setCheckable(true);
    m_setCrAction->setData(static_cast<int>(SC_EOL_CR));
    m_modeGroup->addAction(m_setCrAction);
    connect(m_setCrAction, &QAction::triggered, this,
            [this]() { applySetMode(SC_EOL_CR); });

    m_menu->addSeparator();

    // --- Convert Document actions ---
    m_convertLfAction = m_menu->addAction(tr("Convert Document to LF"));
    connect(m_convertLfAction, &QAction::triggered, this,
            [this]() { applyConvert(SC_EOL_LF); });

    m_convertCrlfAction = m_menu->addAction(tr("Convert Document to CRLF"));
    connect(m_convertCrlfAction, &QAction::triggered, this,
            [this]() { applyConvert(SC_EOL_CRLF); });

    m_convertCrAction = m_menu->addAction(tr("Convert Document to CR"));
    connect(m_convertCrAction, &QAction::triggered, this,
            [this]() { applyConvert(SC_EOL_CR); });

    updateActionsEnabled();
    return m_menu;
}

void EolMenu::setActiveEditor(ScintillaEdit* editor)
{
    m_editor = editor;
    updateActionsEnabled();
    syncCurrentMode();
}

void EolMenu::syncCurrentMode()
{
    if (!m_editor || !m_modeGroup)
        return;

    const int mode = static_cast<int>(m_editor->eOLMode());

    QAction* target = nullptr;
    switch (mode) {
        case SC_EOL_LF:   target = m_setLfAction;   break;
        case SC_EOL_CRLF: target = m_setCrlfAction; break;
        case SC_EOL_CR:   target = m_setCrAction;   break;
        default: break;
    }

    if (target) {
        QSignalBlocker blockGroup(m_modeGroup);
        QSignalBlocker blockLf(m_setLfAction);
        QSignalBlocker blockCrlf(m_setCrlfAction);
        QSignalBlocker blockCr(m_setCrAction);
        target->setChecked(true);
    }
}

void EolMenu::applySetMode(int mode)
{
    if (!m_editor)
        return;

    m_editor->setEOLMode(mode);
    emit eolModeChanged(mode);
}

void EolMenu::applyConvert(int mode)
{
    if (!m_editor)
        return;

    m_editor->beginUndoAction();
    m_editor->convertEOLs(mode);
    m_editor->endUndoAction();

    // Ensure new lines also use the converted EOL going forward.
    m_editor->setEOLMode(mode);

    syncCurrentMode();
    emit eolModeChanged(mode);
}

void EolMenu::updateActionsEnabled()
{
    const bool enabled = !m_editor.isNull();

    if (m_setModeMenu)
        m_setModeMenu->setEnabled(enabled);
    if (m_setLfAction)       m_setLfAction->setEnabled(enabled);
    if (m_setCrlfAction)     m_setCrlfAction->setEnabled(enabled);
    if (m_setCrAction)       m_setCrAction->setEnabled(enabled);
    if (m_convertLfAction)   m_convertLfAction->setEnabled(enabled);
    if (m_convertCrlfAction) m_convertCrlfAction->setEnabled(enabled);
    if (m_convertCrAction)   m_convertCrAction->setEnabled(enabled);
}
