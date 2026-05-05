#pragma once
#include <QWidget>
#include <QPair>

class QSplitter;
class QTabWidget;
class EditorTab;

class MultiView : public QWidget {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit MultiView(QWidget* parent = nullptr);

    // Add a tab to the currently focused group (or first group if none focused).
    int addTab(EditorTab* tab, const QString& title);

    // Returns the currently focused tab (nullptr if none).
    EditorTab* currentTab() const;
    int currentTabIndex() const;     // index within its own group
    QTabWidget* currentGroup() const;
    QTabWidget* primaryGroup() const;
    QTabWidget* secondaryGroup() const;   // nullptr if not split

    int tabCount() const;            // total across both groups
    EditorTab* tabAt(int globalIndex) const;   // walks groups in order

    // Find a tab by EditorTab*; returns its group + local index, or {nullptr,-1} if not present.
    QPair<QTabWidget*, int> locateTab(EditorTab* tab) const;

    // Splits if currently single-group; toggles orientation if already split; un-splits if user calls unsplit().
    void splitView(Orientation orient);
    void unsplit();
    bool isSplit() const;

    // Move the active tab to the OTHER group (creating split if needed).
    void moveCurrentTabToOtherGroup();

    void setTabsClosable(bool b);
    void setTabsMovable(bool b);

signals:
    void currentTabChanged(EditorTab* tab);
    void tabCloseRequested(EditorTab* tab);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onTabWidgetCurrentChanged(int index);
    void onTabWidgetCloseRequested(int index);

private:
    QTabWidget* createGroup();
    void wireGroup(QTabWidget* group);
    void setCurrentGroup(QTabWidget* group);

    QSplitter* m_splitter = nullptr;
    QTabWidget* m_primary = nullptr;
    QTabWidget* m_secondary = nullptr;     // nullptr until first split
    QTabWidget* m_currentGroup = nullptr;  // tracks last-focused / last-activated group

    bool m_tabsClosable = true;
    bool m_tabsMovable  = true;

    // TODO: Cross-group drag-drop is a future enhancement. For now we expose the
    // explicit moveCurrentTabToOtherGroup() to relocate a tab between groups.
};
