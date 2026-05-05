#include "MultiView.h"

#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QFocusEvent>
#include <QEvent>
#include <QIcon>

#include "EditorTab.h"
#include "CrossGroupTabBar.h"

namespace {
// QTabWidget::setTabBar is protected; expose it via a using-declaration so we can
// install a CrossGroupTabBar at construction time.
class TabGroupWidget : public QTabWidget {
public:
    using QTabWidget::QTabWidget;
    using QTabWidget::setTabBar;
};
}

MultiView::MultiView(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    layout->addWidget(m_splitter);

    m_primary = createGroup();
    m_splitter->addWidget(m_primary);

    m_currentGroup = m_primary;
}

QTabWidget* MultiView::createGroup()
{
    auto* tw = new TabGroupWidget(m_splitter);
    auto* bar = new CrossGroupTabBar(tw);
    tw->setTabBar(bar);
    tw->setTabsClosable(m_tabsClosable);
    tw->setMovable(m_tabsMovable);
    tw->setDocumentMode(true);
    wireGroup(tw);
    // Cross-group drop: move the dragged tab from sourceBar's parent QTabWidget
    // into THIS group at the drop position.
    connect(bar, &CrossGroupTabBar::tabDroppedFromOther, this,
            [this, dstBar = bar](CrossGroupTabBar* sourceBar, int sourceIndex, int insertIndex) {
        if (!sourceBar || sourceBar == dstBar) return;
        auto* srcTw = qobject_cast<QTabWidget*>(sourceBar->parentWidget());
        auto* dstTw = qobject_cast<QTabWidget*>(dstBar->parentWidget());
        if (!srcTw || !dstTw || srcTw == dstTw) return;
        if (sourceIndex < 0 || sourceIndex >= srcTw->count()) return;

        QWidget* w = srcTw->widget(sourceIndex);
        const QString title = srcTw->tabText(sourceIndex);
        const QIcon icon = srcTw->tabIcon(sourceIndex);
        const QString tip = srcTw->tabToolTip(sourceIndex);
        srcTw->removeTab(sourceIndex);
        if (!w) return;
        const int clamped = qBound(0, insertIndex, dstTw->count());
        const int newIdx = dstTw->insertTab(clamped, w, icon, title);
        dstTw->setTabToolTip(newIdx, tip);
        dstTw->setCurrentIndex(newIdx);
        setCurrentGroup(dstTw);
        if (auto* w2 = dstTw->currentWidget()) w2->setFocus();
    });
    return tw;
}

void MultiView::wireGroup(QTabWidget* group)
{
    if (!group) return;
    connect(group, &QTabWidget::currentChanged,
            this, &MultiView::onTabWidgetCurrentChanged);
    connect(group, &QTabWidget::tabCloseRequested,
            this, &MultiView::onTabWidgetCloseRequested);

    // Track focus on the tab widget itself and on its tab bar so that whichever
    // group last received user attention becomes the "current group".
    group->installEventFilter(this);
    if (group->tabBar())
        group->tabBar()->installEventFilter(this);
}

void MultiView::setCurrentGroup(QTabWidget* group)
{
    if (!group || group == m_currentGroup) return;
    m_currentGroup = group;
}

int MultiView::addTab(EditorTab* tab, const QString& title)
{
    if (!tab) return -1;
    QTabWidget* target = m_currentGroup ? m_currentGroup : m_primary;
    const int index = target->addTab(tab, title);
    target->setCurrentIndex(index);
    setCurrentGroup(target);
    return index;
}

EditorTab* MultiView::currentTab() const
{
    if (!m_currentGroup) return nullptr;
    return qobject_cast<EditorTab*>(m_currentGroup->currentWidget());
}

int MultiView::currentTabIndex() const
{
    if (!m_currentGroup) return -1;
    return m_currentGroup->currentIndex();
}

QTabWidget* MultiView::currentGroup() const
{
    return m_currentGroup;
}

QTabWidget* MultiView::primaryGroup() const
{
    return m_primary;
}

QTabWidget* MultiView::secondaryGroup() const
{
    return m_secondary;
}

int MultiView::tabCount() const
{
    int n = m_primary ? m_primary->count() : 0;
    if (m_secondary) n += m_secondary->count();
    return n;
}

EditorTab* MultiView::tabAt(int globalIndex) const
{
    if (globalIndex < 0) return nullptr;
    const int primaryCount = m_primary ? m_primary->count() : 0;
    if (globalIndex < primaryCount)
        return qobject_cast<EditorTab*>(m_primary->widget(globalIndex));
    if (m_secondary) {
        const int secIdx = globalIndex - primaryCount;
        if (secIdx < m_secondary->count())
            return qobject_cast<EditorTab*>(m_secondary->widget(secIdx));
    }
    return nullptr;
}

QPair<QTabWidget*, int> MultiView::locateTab(EditorTab* tab) const
{
    if (!tab) return { nullptr, -1 };
    if (m_primary) {
        const int idx = m_primary->indexOf(tab);
        if (idx >= 0) return { m_primary, idx };
    }
    if (m_secondary) {
        const int idx = m_secondary->indexOf(tab);
        if (idx >= 0) return { m_secondary, idx };
    }
    return { nullptr, -1 };
}

bool MultiView::isSplit() const
{
    return m_secondary != nullptr;
}

void MultiView::splitView(Orientation orient)
{
    const Qt::Orientation qo = (orient == Orientation::Horizontal)
        ? Qt::Horizontal
        : Qt::Vertical;

    const bool firstSplit = (m_secondary == nullptr);

    if (firstSplit) {
        m_secondary = createGroup();
        // A QTabWidget with zero tabs has near-zero size hint and would be
        // invisible after the splitter distributes sizes; enforce a minimum
        // so the user always sees the new pane even before any tabs land.
        m_secondary->setMinimumWidth(160);
        m_secondary->setMinimumHeight(120);
        m_splitter->addWidget(m_secondary);
    }

    m_splitter->setOrientation(qo);

    // On first split, move the active tab from primary into the new group so the
    // user immediately sees split content. (If primary has only 1 tab total we leave
    // it in primary and let the secondary stay empty — user can drag/move tabs over.)
    if (firstSplit && m_primary && m_primary->count() >= 2) {
        const int idx = m_primary->currentIndex();
        if (idx >= 0) {
            QWidget* w = m_primary->widget(idx);
            const QString title = m_primary->tabText(idx);
            const QIcon icon = m_primary->tabIcon(idx);
            const QString tip = m_primary->tabToolTip(idx);
            m_primary->removeTab(idx);
            const int newIdx = m_secondary->addTab(w, icon, title);
            m_secondary->setTabToolTip(newIdx, tip);
            m_secondary->setCurrentIndex(newIdx);
            setCurrentGroup(m_secondary);
            if (auto* w2 = m_secondary->currentWidget()) w2->setFocus();
        }
    }

    // Re-apply current tab settings to both groups.
    setTabsClosable(m_tabsClosable);
    setTabsMovable(m_tabsMovable);

    // Force 50/50 split. Using the splitter's CURRENT extent — if zero (just shown),
    // fall back to a sensible default so we don't end up with a 0/N split.
    const int total = (qo == Qt::Horizontal) ? m_splitter->width()
                                             : m_splitter->height();
    const int half = (total > 100) ? total / 2 : 400;
    m_splitter->setSizes({ half, half });
}

void MultiView::unsplit()
{
    if (!m_secondary) return;

    // Move all secondary tabs into primary in order.
    while (m_secondary->count() > 0) {
        QWidget* w = m_secondary->widget(0);
        const QString title = m_secondary->tabText(0);
        const QIcon icon = m_secondary->tabIcon(0);
        const QString tooltip = m_secondary->tabToolTip(0);
        m_secondary->removeTab(0);
        if (w) {
            const int newIdx = m_primary->addTab(w, icon, title);
            m_primary->setTabToolTip(newIdx, tooltip);
        }
    }

    QTabWidget* dying = m_secondary;
    m_secondary = nullptr;

    if (m_currentGroup == dying)
        m_currentGroup = m_primary;

    dying->removeEventFilter(this);
    if (dying->tabBar())
        dying->tabBar()->removeEventFilter(this);
    dying->setParent(nullptr);
    dying->deleteLater();
}

void MultiView::moveCurrentTabToOtherGroup()
{
    if (!m_currentGroup) return;

    if (!m_secondary)
        splitView(Orientation::Horizontal);

    QTabWidget* src = m_currentGroup;
    QTabWidget* dst = (src == m_primary) ? m_secondary : m_primary;
    if (!src || !dst || src == dst) return;

    const int idx = src->currentIndex();
    if (idx < 0) return;

    QWidget* w = src->widget(idx);
    const QString title = src->tabText(idx);
    const QIcon icon = src->tabIcon(idx);
    const QString tooltip = src->tabToolTip(idx);

    src->removeTab(idx);
    if (!w) return;

    const int newIdx = dst->addTab(w, icon, title);
    dst->setTabToolTip(newIdx, tooltip);
    dst->setCurrentIndex(newIdx);

    setCurrentGroup(dst);
    if (auto* w2 = dst->currentWidget())
        w2->setFocus();
}

void MultiView::setTabsClosable(bool b)
{
    m_tabsClosable = b;
    if (m_primary)   m_primary->setTabsClosable(b);
    if (m_secondary) m_secondary->setTabsClosable(b);
}

void MultiView::setTabsMovable(bool b)
{
    m_tabsMovable = b;
    if (m_primary)   m_primary->setMovable(b);
    if (m_secondary) m_secondary->setMovable(b);
}

bool MultiView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusIn) {
        // Determine which group this object belongs to.
        QTabWidget* group = nullptr;
        if (watched == m_primary || (m_primary && watched == m_primary->tabBar()))
            group = m_primary;
        else if (m_secondary &&
                 (watched == m_secondary || watched == m_secondary->tabBar()))
            group = m_secondary;

        if (group)
            setCurrentGroup(group);
    }
    return QWidget::eventFilter(watched, event);
}

void MultiView::onTabWidgetCurrentChanged(int index)
{
    auto* group = qobject_cast<QTabWidget*>(sender());
    if (!group) return;

    setCurrentGroup(group);

    EditorTab* tab = (index >= 0)
        ? qobject_cast<EditorTab*>(group->widget(index))
        : nullptr;
    emit currentTabChanged(tab);
}

void MultiView::onTabWidgetCloseRequested(int index)
{
    auto* group = qobject_cast<QTabWidget*>(sender());
    if (!group) return;
    if (index < 0 || index >= group->count()) return;

    if (auto* tab = qobject_cast<EditorTab*>(group->widget(index)))
        emit tabCloseRequested(tab);
}
