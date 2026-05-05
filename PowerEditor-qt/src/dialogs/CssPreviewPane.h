#pragma once
#include <QDialog>
#include <QString>
#include <QMetaObject>

class QTextBrowser;
class QCheckBox;
class QPushButton;
class QLabel;
class ScintillaEdit;

class CssPreviewPane : public QDialog {
    Q_OBJECT
public:
    explicit CssPreviewPane(QWidget* parent = nullptr);

    // Bind the pane to a ScintillaEdit. Live-updates the preview when the editor's
    // text changes (via the modified signal).
    void bindToEditor(ScintillaEdit* editor);

    // Set the sample HTML used as the preview body. If not set, uses a default sample.
    void setSampleHtml(const QString& html);

public slots:
    // Manually refresh the preview (re-fetches CSS from the bound editor).
    void refresh();

private slots:
    void onEditorModified();

private:
    QString composeHtml(const QString& css) const;

    QTextBrowser*  m_browser   = nullptr;
    QPushButton*   m_refreshBtn = nullptr;
    QCheckBox*     m_liveCheck = nullptr;
    QLabel*        m_noteLabel = nullptr;

    ScintillaEdit* m_editor = nullptr;
    QMetaObject::Connection m_editorConn;

    QString m_sampleHtml;
    bool    m_refreshScheduled = false;
};
