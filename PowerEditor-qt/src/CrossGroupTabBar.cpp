#include "CrossGroupTabBar.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QString>

const char* CrossGroupTabBar::kMimeType = "application/x-notepadpp-qt-tab";

CrossGroupTabBar::CrossGroupTabBar(QWidget* parent)
    : QTabBar(parent)
{
    setAcceptDrops(true);
}

void CrossGroupTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart  = event->pos();
        m_dragActive = false;
    }
    QTabBar::mousePressEvent(event);
}

void CrossGroupTabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    if (m_dragActive) {
        // A QDrag is already in flight (shouldn't really get here because exec blocks,
        // but be safe and don't double-forward).
        return;
    }

    const int idx = tabAt(m_dragStart);
    if (idx < 0) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    const int moved = (event->pos() - m_dragStart).manhattanLength();
    if (moved < QApplication::startDragDistance()) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    // While the cursor is still inside the bar, let QTabBar's native (movable)
    // intra-bar reordering handle things. Only escalate to a cross-group QDrag
    // once the user has dragged the cursor outside the bar's geometry.
    if (rect().contains(event->pos())) {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    // Begin cross-group QDrag.
    m_dragActive = true;

    auto* mime = new QMimeData();
    const QByteArray payload =
        QString("%1:%2")
            .arg(reinterpret_cast<quintptr>(this), 0, 16)
            .arg(idx)
            .toUtf8();
    mime->setData(QString::fromLatin1(kMimeType), payload);

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);

    // Visual feedback: snapshot of the dragged tab.
    const QRect tr = tabRect(idx);
    if (tr.isValid()) {
        QPixmap pm = grab(tr);
        if (!pm.isNull()) {
            drag->setPixmap(pm);
            drag->setHotSpot(QPoint(pm.width() / 2, pm.height() / 2));
        }
    }

    drag->exec(Qt::MoveAction);

    m_dragActive = false;
    // Do NOT forward to base while a drag is/has been active — Qt's QDrag
    // owns the gesture from this point.
}

void CrossGroupTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragStart  = QPoint();
    m_dragActive = false;
    QTabBar::mouseReleaseEvent(event);
}

void CrossGroupTabBar::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMimeType))) {
        QTabBar::dragEnterEvent(event);
        return;
    }

    auto* sourceBar = qobject_cast<CrossGroupTabBar*>(event->source());
    if (!sourceBar || sourceBar == this) {
        // Same-bar moves are handled by QTabBar's native movable behaviour.
        event->ignore();
        return;
    }

    event->acceptProposedAction();
}

void CrossGroupTabBar::dragMoveEvent(QDragMoveEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMimeType))) {
        QTabBar::dragMoveEvent(event);
        return;
    }

    auto* sourceBar = qobject_cast<CrossGroupTabBar*>(event->source());
    if (!sourceBar || sourceBar == this) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();
}

void CrossGroupTabBar::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMimeType))) {
        QTabBar::dropEvent(event);
        return;
    }

    auto* sourceBar = qobject_cast<CrossGroupTabBar*>(event->source());
    if (!sourceBar || sourceBar == this) {
        event->ignore();
        return;
    }

    // Parse payload: "<hexPtr>:<sourceIndex>". We trust the cast above and
    // primarily use the payload for the source index (the pointer part is
    // useful for cross-checking / future cross-window drops).
    const QByteArray payload = mime->data(QString::fromLatin1(kMimeType));
    const QString    s       = QString::fromUtf8(payload);
    const int        colon   = s.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        event->ignore();
        return;
    }

    bool      okPtr  = false;
    bool      okIdx  = false;
    const quintptr ptrVal = s.left(colon).toULongLong(&okPtr, 16);
    const int      srcIdx = s.mid(colon + 1).toInt(&okIdx);
    if (!okPtr || !okIdx || srcIdx < 0) {
        event->ignore();
        return;
    }

    // Cross-check pointer with sender (defensive — same process, same MultiView).
    if (reinterpret_cast<quintptr>(sourceBar) != ptrVal) {
        // Pointer mismatch: payload didn't come from event->source(). Bail.
        event->ignore();
        return;
    }

    // Compute insert index on this bar.
    const QPoint dropPos = event->position().toPoint();
    int insertIndex = tabAt(dropPos);
    if (insertIndex < 0) {
        // Dropped past the last tab.
        insertIndex = count();
    } else {
        // Left half -> insert before, right half -> insert after.
        const QRect tr = tabRect(insertIndex);
        if (tr.isValid() && dropPos.x() > tr.center().x()) {
            insertIndex += 1;
        }
    }

    emit tabDroppedFromOther(sourceBar, srcIdx, insertIndex);
    event->acceptProposedAction();
}
