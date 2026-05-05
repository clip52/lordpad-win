#include "CommandPalette.h"

#include <QDialog>
#include <QLineEdit>
#include <QListView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QKeyEvent>
#include <QShortcut>
#include <QFont>
#include <QHash>
#include <QVariant>
#include <QWidget>
#include <QModelIndex>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QPointer>
#include <QStringList>
#include <QSet>

#include <limits>

namespace {

// User roles used in the model.
constexpr int kRoleActionPtr = Qt::UserRole + 1;
constexpr int kRoleScore     = Qt::UserRole + 2;
constexpr int kRoleEnabled   = Qt::UserRole + 3;

// Compute a fuzzy match score between query and target (case-insensitive).
// Returns INT_MIN if not all query characters can be matched in order.
// Scoring: each consecutive char contributes +2; each non-consecutive (gap) contributes -1.
// Empty query returns 0 (treated as "matches everything").
int fuzzyScore(const QString& query, const QString& target)
{
    if (query.isEmpty())
        return 0;

    const QString q = query.toLower();
    const QString t = target.toLower();

    int score        = 0;
    int qi           = 0;
    int lastMatchPos = -2; // sentinel so the first match is never "consecutive"

    for (int ti = 0; ti < t.size() && qi < q.size(); ++ti) {
        if (t[ti] == q[qi]) {
            if (ti == lastMatchPos + 1) {
                score += 2;        // contiguous
            } else {
                score -= 1;        // gap
            }
            lastMatchPos = ti;
            ++qi;
        }
    }

    if (qi < q.size())
        return std::numeric_limits<int>::min();

    return score;
}

} // namespace

// ---------------------------------------------------------------------------
// Custom proxy model implementing fuzzy filter + score-based ordering.
// ---------------------------------------------------------------------------
class CommandPaletteFilterProxy : public QSortFilterProxyModel {
public:
    explicit CommandPaletteFilterProxy(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(true);
    }

    void setQuery(const QString& q)
    {
        if (m_query == q)
            return;
        m_query = q;
        recomputeScores();
        invalidate();
        sort(0);
    }

    QString query() const { return m_query; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (m_query.isEmpty())
            return true;

        const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!idx.isValid())
            return false;

        const auto it = m_scores.constFind(sourceRow);
        if (it == m_scores.constEnd())
            return false;
        return it.value() != std::numeric_limits<int>::min();
    }

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        if (!m_query.isEmpty()) {
            const int ls = m_scores.value(left.row(),  std::numeric_limits<int>::min());
            const int rs = m_scores.value(right.row(), std::numeric_limits<int>::min());
            if (ls != rs)
                return ls > rs; // higher score first
        }
        // Alphabetical fallback (case-insensitive).
        const QString lt = sourceModel()->data(left,  Qt::DisplayRole).toString();
        const QString rt = sourceModel()->data(right, Qt::DisplayRole).toString();
        return lt.compare(rt, Qt::CaseInsensitive) < 0;
    }

private:
    void recomputeScores()
    {
        m_scores.clear();
        QAbstractItemModel* src = sourceModel();
        if (!src)
            return;
        const int rows = src->rowCount();
        for (int r = 0; r < rows; ++r) {
            const QString text = src->data(src->index(r, 0), Qt::DisplayRole).toString();
            m_scores.insert(r, fuzzyScore(m_query, text));
        }
    }

    QString          m_query;
    QHash<int, int>  m_scores;
};

// ---------------------------------------------------------------------------
// CommandPalette implementation
// ---------------------------------------------------------------------------
CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
    resize(600, 400);

    // Thin border so the frameless dialog still has a visible edge.
    setStyleSheet(QStringLiteral(
        "CommandPalette { border: 1px solid palette(mid); }"
    ));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Type a command..."));
    QFont searchFont = m_search->font();
    searchFont.setPointSize(12);
    m_search->setFont(searchFont);
    m_search->installEventFilter(this);
    layout->addWidget(m_search);

    m_list = new QListView(this);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setUniformItemSizes(true);
    m_list->setFocusPolicy(Qt::NoFocus); // keep focus in the search line edit
    layout->addWidget(m_list, /*stretch*/ 1);

    m_model = new QStandardItemModel(this);
    m_proxy = new CommandPaletteFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->sort(0);

    m_list->setModel(m_proxy);

    connect(m_search, &QLineEdit::textChanged,
            this,     &CommandPalette::onSearchTextChanged);
    connect(m_search, &QLineEdit::returnPressed,
            this,     &CommandPalette::onItemActivated);
    connect(m_list,   &QAbstractItemView::activated,
            this,     [this](const QModelIndex&) { onItemActivated(); });
}

void CommandPalette::setActions(const QList<QAction*>& actions)
{
    m_model->clear();

    for (QAction* action : actions) {
        if (!action)
            continue;
        if (action->isSeparator())
            continue;
        // Skip submenu title actions (those carry a non-null QMenu).
        if (action->menu() != nullptr)
            continue;

        const QString display = buildDisplayText(action);
        if (display.isEmpty())
            continue;

        auto* item = new QStandardItem(display);
        item->setEditable(false);
        item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(action)),
                      kRoleActionPtr);
        const bool enabled = action->isEnabled();
        item->setData(enabled, kRoleEnabled);
        if (!enabled) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
        m_model->appendRow(item);
    }

    // Re-apply current filter & sorting.
    m_proxy->setQuery(m_search ? m_search->text() : QString());

    if (m_proxy->rowCount() > 0) {
        m_list->setCurrentIndex(m_proxy->index(0, 0));
    }
}

void CommandPalette::presentCentered()
{
    if (QWidget* p = parentWidget()) {
        const QRect pg = p->geometry();
        const QPoint center = pg.center();
        QRect g = geometry();
        g.moveCenter(center);
        // Map to global if parent has a parent (i.e. parent geometry is in its own parent's coords).
        if (p->parentWidget()) {
            g.moveCenter(p->mapToGlobal(p->rect().center()));
        }
        setGeometry(g);
    }

    m_search->clear();
    m_proxy->setQuery(QString());
    if (m_proxy->rowCount() > 0)
        m_list->setCurrentIndex(m_proxy->index(0, 0));

    show();
    raise();
    activateWindow();
    m_search->setFocus(Qt::ActiveWindowFocusReason);
    m_search->selectAll();
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
            case Qt::Key_Down:
                moveSelection(+1);
                return true;
            case Qt::Key_Up:
                moveSelection(-1);
                return true;
            case Qt::Key_PageDown:
                moveSelection(+10);
                return true;
            case Qt::Key_PageUp:
                moveSelection(-10);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                onItemActivated();
                return true;
            case Qt::Key_Escape:
                reject();
                return true;
            default:
                break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

void CommandPalette::onSearchTextChanged(const QString& text)
{
    m_proxy->setQuery(text);
    if (m_proxy->rowCount() > 0) {
        m_list->setCurrentIndex(m_proxy->index(0, 0));
    }
}

void CommandPalette::onItemActivated()
{
    triggerSelected();
}

void CommandPalette::triggerSelected()
{
    const QModelIndex proxyIdx = m_list->currentIndex();
    if (!proxyIdx.isValid())
        return;

    const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    if (!srcIdx.isValid())
        return;

    const bool enabled = m_model->data(srcIdx, kRoleEnabled).toBool();
    if (!enabled)
        return;

    const quintptr raw = m_model->data(srcIdx, kRoleActionPtr).value<quintptr>();
    auto* action = reinterpret_cast<QAction*>(raw);
    if (!action)
        return;

    // Close before triggering so the action runs against the underlying window.
    accept();
    action->trigger();
}

void CommandPalette::moveSelection(int delta)
{
    const int rows = m_proxy->rowCount();
    if (rows <= 0)
        return;

    const QModelIndex cur = m_list->currentIndex();
    int row = cur.isValid() ? cur.row() : 0;
    row += delta;
    if (row < 0)            row = 0;
    if (row >= rows)        row = rows - 1;

    const QModelIndex target = m_proxy->index(row, 0);
    m_list->setCurrentIndex(target);
    m_list->scrollTo(target, QAbstractItemView::EnsureVisible);
}

QString CommandPalette::buildDisplayText(QAction* action) const
{
    if (!action)
        return {};

    // Strip Qt mnemonic ampersands ("&File" -> "File"), but preserve "&&".
    auto cleanText = [](QString s) {
        QString out;
        out.reserve(s.size());
        for (int i = 0; i < s.size(); ++i) {
            const QChar c = s[i];
            if (c == QLatin1Char('&')) {
                if (i + 1 < s.size() && s[i + 1] == QLatin1Char('&')) {
                    out.append(QLatin1Char('&'));
                    ++i;
                }
                // else: skip the mnemonic ampersand
            } else {
                out.append(c);
            }
        }
        return out;
    };

    QStringList chain; // collected from leaf -> root, reversed at the end.

    // Walk up through associated objects / parents to find enclosing QMenu titles.
    QSet<QObject*> visited;
    QObject* cursor = nullptr;

    // Prefer associatedObjects (Qt 6) as the starting set; fall back to parent().
    const auto assoc = action->associatedObjects();
    for (QObject* o : assoc) {
        if (auto* m = qobject_cast<QMenu*>(o)) {
            cursor = m;
            break;
        }
    }
    if (!cursor) {
        cursor = action->parent();
    }

    while (cursor && !visited.contains(cursor)) {
        visited.insert(cursor);

        if (auto* menu = qobject_cast<QMenu*>(cursor)) {
            const QString title = cleanText(menu->title());
            if (!title.isEmpty())
                chain.append(title);

            // Move further up: a QMenu can be a submenu of another QMenu via its
            // menuAction()'s associatedObjects, or via parentWidget().
            QObject* next = nullptr;
            if (QAction* menuAct = menu->menuAction()) {
                const auto a2 = menuAct->associatedObjects();
                for (QObject* o : a2) {
                    if (o == menu)
                        continue;
                    if (qobject_cast<QMenu*>(o) || qobject_cast<QMenuBar*>(o)) {
                        next = o;
                        break;
                    }
                }
            }
            if (!next) {
                next = menu->parentWidget();
            }
            cursor = next;
            continue;
        }

        if (qobject_cast<QMenuBar*>(cursor)) {
            // Menubar contributes no title.
            break;
        }

        // Some other QObject — try to climb to a QMenu ancestor.
        cursor = cursor->parent();
    }

    // chain currently goes leaf-menu -> root-menu; we want root -> leaf.
    QStringList ordered;
    for (int i = chain.size() - 1; i >= 0; --i)
        ordered.append(chain[i]);

    const QString actionText = cleanText(action->text());
    if (actionText.isEmpty())
        return {};

    if (ordered.isEmpty())
        return actionText;

    ordered.append(actionText);
    return ordered.join(QStringLiteral(" > "));
}
