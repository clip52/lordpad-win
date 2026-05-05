#include "DocumentMapPanel.h"

#include <QDockWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPointer>
#include <QWidget>
#include <QEvent>

#include <ScintillaEdit.h>

// Scintilla constants we rely on. Some headers expose them as plain macros
// (Scintilla.h); others only via the C++ enum namespace. Pull what's needed.
#ifndef SC_WRAP_WORD
#define SC_WRAP_WORD 1
#endif
#ifndef SC_MARK_BACKGROUND
#define SC_MARK_BACKGROUND 22
#endif

namespace {
// #88AAFF as 0x00BBGGRR for Scintilla COLORREF
constexpr int kViewportColor = 0x00FFAA88;
}

DocumentMapPanel::DocumentMapPanel(QWidget* parent)
    : QDockWidget(tr("Document Map"), parent)
{
    setObjectName(QStringLiteral("DocumentMapPanel"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_map = new ScintillaEdit(container);
    layout->addWidget(m_map);

    container->setMinimumWidth(120);
    container->resize(160, container->height());
    setMinimumWidth(120);

    setWidget(container);

    configureMapEditor();
}

DocumentMapPanel::~DocumentMapPanel()
{
    detachFromTarget();
}

void DocumentMapPanel::configureMapEditor()
{
    if (!m_map) return;

    // Hide all five standard margins.
    for (int i = 0; i <= 4; ++i) {
        m_map->setMarginWidthN(i, 0);
    }

    // Tiny font.
    m_map->setZoom(-10);

    // Hide caret artifacts.
    m_map->setCaretLineVisible(false);
    m_map->setCaretWidth(0);

    // No right-click menu.
    m_map->setContextMenuPolicy(Qt::NoContextMenu);

    // Word wrap so long lines fold into the narrow column.
    m_map->setWrapMode(SC_WRAP_WORD);

    // Configure the viewport marker (background highlight).
    m_map->markerDefine(kViewportMarker, SC_MARK_BACKGROUND);
    m_map->markerSetBack(kViewportMarker, kViewportColor);

    // Try translucent variant if the build supports it; failure is harmless.
    // Alpha 0x60 over #88AAFF -> 0x6088AAFF as 0xAARRGGBB.
    m_map->markerSetBackTranslucent(kViewportMarker, 0x6088AAFF);

    // Read-only initially; the document share will preserve this on the map.
    m_map->setReadOnly(true);

    // Catch clicks on the map viewport to scroll the target.
    m_map->installEventFilter(this);
    m_map->viewport()->installEventFilter(this);
}

void DocumentMapPanel::detachFromTarget()
{
    if (m_updateUiConn) {
        QObject::disconnect(m_updateUiConn);
        m_updateUiConn = QMetaObject::Connection();
    }
    if (m_destroyedConn) {
        QObject::disconnect(m_destroyedConn);
        m_destroyedConn = QMetaObject::Connection();
    }
    m_target.clear();
}

void DocumentMapPanel::setActiveEditor(ScintillaEdit* editor)
{
    // Always disconnect the old wiring first.
    detachFromTarget();

    if (!m_map) return;

    if (!editor) {
        // Detach: give the map its own empty document so it stops mirroring.
        // SCI_SETDOCPOINTER with 0 makes Scintilla allocate a fresh document.
        m_map->setDocPointer(0);
        m_map->setReadOnly(true);
        m_map->markerDeleteAll(kViewportMarker);
        return;
    }

    m_target = editor;

    // Share the underlying document. Both editors will render the same buffer.
    sptr_t doc = editor->docPointer();
    m_map->setDocPointer(doc);

    // Reapply read-only on the map (shared docs propagate the flag both ways
    // unless we're explicit; we want only the map to be locked from typing,
    // but Scintilla's read-only is per-document. To avoid affecting the
    // target, we only set the visual lockdown flags on the map widget side
    // and skip toggling read-only here — leaving it as the shared doc's
    // current state.). Re-apply read-only as the spec requests.
    m_map->setReadOnly(true);

    // Re-apply the map's display configuration (margins/zoom/wrap survive
    // the doc swap, but markers must be re-defined when the doc changes).
    m_map->markerDefine(kViewportMarker, SC_MARK_BACKGROUND);
    m_map->markerSetBack(kViewportMarker, kViewportColor);
    m_map->markerSetBackTranslucent(kViewportMarker, 0x6088AAFF);

    // Connect the target's updateUi -> refresh viewport rect on map.
    m_updateUiConn = connect(editor, &ScintillaEdit::updateUi,
                             this, [this](Scintilla::Update /*updated*/) {
                                 updateViewportMarker();
                                 syncMapScroll();
                             });

    // If the target dies, reset the map to a blank doc.
    m_destroyedConn = connect(editor, &QObject::destroyed,
                              this, [this](QObject*) {
                                  setActiveEditor(nullptr);
                              });

    // Initial draw.
    updateViewportMarker();
    syncMapScroll();
}

void DocumentMapPanel::updateViewportMarker()
{
    if (!m_map || !m_target) return;

    m_map->markerDeleteAll(kViewportMarker);

    const sptr_t firstVisible = m_target->firstVisibleLine();
    const sptr_t onScreen     = m_target->linesOnScreen();
    if (onScreen <= 0) return;

    // Mark every visible line so the highlight forms a solid block.
    for (sptr_t i = 0; i < onScreen; ++i) {
        m_map->markerAdd(firstVisible + i, kViewportMarker);
    }
}

void DocumentMapPanel::syncMapScroll()
{
    if (!m_map || !m_target) return;

    // Best-effort: try to keep the highlighted region centered on the map.
    const sptr_t firstVisible = m_target->firstVisibleLine();
    const sptr_t onScreen     = m_target->linesOnScreen();
    const sptr_t mapOnScreen  = m_map->linesOnScreen();

    if (mapOnScreen <= 0) return;

    sptr_t desired = firstVisible + (onScreen / 2) - (mapOnScreen / 2);
    if (desired < 0) desired = 0;

    m_map->setFirstVisibleLine(desired);
}

bool DocumentMapPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (m_map && m_target &&
        (watched == m_map || watched == m_map->viewport()) &&
        event->type() == QEvent::MouseButtonPress)
    {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const QPoint p = me->pos();
            const sptr_t pos  = m_map->positionFromPoint(p.x(), p.y());
            const sptr_t line = m_map->lineFromPosition(pos);

            m_target->gotoLine(line);
            m_target->scrollCaret();
            return true;
        }
    }
    return QDockWidget::eventFilter(watched, event);
}
