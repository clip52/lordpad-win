#pragma once
#include <QDockWidget>
#include <QPointer>
#include <QMetaObject>

class ScintillaEdit;

class DocumentMapPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit DocumentMapPanel(QWidget* parent = nullptr);
    ~DocumentMapPanel() override;

    // Bind the map to follow this editor. Pass nullptr to detach.
    void setActiveEditor(ScintillaEdit* editor);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void configureMapEditor();
    void detachFromTarget();
    void updateViewportMarker();
    void syncMapScroll();

    ScintillaEdit*           m_map = nullptr;
    QPointer<ScintillaEdit>  m_target;

    QMetaObject::Connection  m_updateUiConn;
    QMetaObject::Connection  m_destroyedConn;

    static constexpr int kViewportMarker = 0;
};
