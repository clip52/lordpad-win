#pragma once
#include <QDialog>
#include <QMetaObject>

class QTextBrowser;
class QPushButton;
class QCheckBox;
class ScintillaEdit;

class MarkdownPreviewPane : public QDialog {
    Q_OBJECT
public:
    explicit MarkdownPreviewPane(QWidget* parent = nullptr);

    void bindToEditor(ScintillaEdit* editor);

public slots:
    void refresh();

private slots:
    void onEditorModified();

private:
    QTextBrowser* m_browser    = nullptr;
    QPushButton*  m_refreshBtn = nullptr;
    QCheckBox*    m_liveCheck  = nullptr;

    ScintillaEdit*           m_editor = nullptr;
    QMetaObject::Connection  m_editorConn;

    bool m_refreshScheduled = false;
};
