#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QList>
#include <QIcon>
#include <QPointer>
#include <QString>
#include <QColor>

class QAction;
class QTabWidget;
class QPoint;
class EditorTab;

// TabExtras: standalone module that augments QTabWidget instances (our
// "tab groups") with Notepad++-style per-tab features:
//   - Pin tab           (move to leftmost slot, mark with pin icon, survives "close all")
//   - Lock tab          (read-only via SCI_SETREADONLY)
//   - Color tab         (per-tab text color via QTabBar::setTabTextColor)
//   - Close Others / Close To Right / Close To Left
//
// All visible state is persisted by absolute file path in
// QSettings("clip52", "notepadpp-qt") under the "TabExtras" group,
// so reopening a previously pinned/locked/colored file restores its state.
//
// This class does NOT own the QTabWidgets it observes; it merely connects to
// their signals and tracks them with QPointer.
class TabExtras : public QObject {
    Q_OBJECT
public:
    explicit TabExtras(QObject* parent = nullptr);
    ~TabExtras() override;

    // Wire up a QTabWidget so it shows the tab context menu and reapplies
    // persisted state to its tabs. Safe to call once per group; idempotent.
    void attachTabWidget(QTabWidget* tw);

    // Action factories. Each action operates on the tab that is *current*
    // in whichever group most recently emitted activity. Use these to
    // populate the View menu, the toolbar, or shortcuts.
    QAction* makePinAction(QObject* parent);
    QAction* makeLockAction(QObject* parent);
    QAction* makeColorAction(QObject* parent);
    QAction* makeCloseOthersAction(QObject* parent);
    QAction* makeCloseToRightAction(QObject* parent);
    QAction* makeCloseToLeftAction(QObject* parent);

    // True if the tab whose EditorTab maps to this absolute path is pinned.
    // Useful so callers can refuse "close all" requests.
    bool isPinned(const QString& absPath) const;

public slots:
    // Per-tab operations — operate on (tw, index). If tw is null they fall
    // back to the most recently active group.
    void pinTab(QTabWidget* tw, int index);
    void lockTab(QTabWidget* tw, int index);
    void colorTab(QTabWidget* tw, int index);
    void closeOthers(QTabWidget* tw, int index);
    void closeToRight(QTabWidget* tw, int index);
    void closeToLeft(QTabWidget* tw, int index);

private slots:
    void onCustomContextMenu(const QPoint& pos);
    void onTabWidgetDestroyed(QObject* obj);
    void onCurrentChanged(int index);

private:
    // Lookups
    EditorTab* editorTabAt(QTabWidget* tw, int index) const;
    QString pathAt(QTabWidget* tw, int index) const;
    QTabWidget* resolveGroup(QTabWidget* tw) const;

    // Persistence
    void loadState();
    void savePinned();
    void saveLocked();
    void saveColors();
    void applyPersistedTo(QTabWidget* tw, int index);

    // Visuals
    QIcon pinIcon() const;
    void refreshTabDecoration(QTabWidget* tw, int index);

    // Tracked groups
    QList<QPointer<QTabWidget>> m_groups;
    QPointer<QTabWidget>        m_lastActive;

    // In-memory mirror of persisted state, keyed by absolute file path.
    QSet<QString>             m_pinned;
    QSet<QString>             m_locked;
    QHash<QString, QString>   m_colors;   // path -> "#RRGGBB"
};
