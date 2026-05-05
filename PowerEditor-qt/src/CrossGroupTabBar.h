#pragma once

#include <QTabBar>
#include <QPoint>

class QMouseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class CrossGroupTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit CrossGroupTabBar(QWidget* parent = nullptr);

    // MIME type used by drags between two CrossGroupTabBar instances.
    // Format of payload: "<barPtrAsHex>:<sourceIndex>".
    static const char* kMimeType;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    // Emitted when a drop from another CrossGroupTabBar lands on this bar.
    // Receiver should: take widget+title+icon+tooltip from sourceBar's tab at sourceIndex,
    // remove it from source, and insert it on this bar's QTabWidget at insertIndex.
    void tabDroppedFromOther(CrossGroupTabBar* sourceBar, int sourceIndex, int insertIndex);

private:
    QPoint m_dragStart;
    bool   m_dragActive = false;
};
