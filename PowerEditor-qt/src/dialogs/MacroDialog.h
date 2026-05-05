#pragma once
#include <QDialog>

class MacroRecorder;
class ScintillaEdit;
class QListWidget;
class QPushButton;
class QLabel;

class MacroDialog : public QDialog {
    Q_OBJECT
public:
    explicit MacroDialog(MacroRecorder* recorder, ScintillaEdit* editor, QWidget* parent = nullptr);

private slots:
    void refreshList();
    void updateStatus();
    void onRecord();
    void onStop();
    void onPlay();
    void onSaveAs();
    void onLoad();
    void onDelete();

private:
    MacroRecorder* m_recorder = nullptr;
    ScintillaEdit* m_editor   = nullptr;

    QListWidget*   m_list     = nullptr;
    QPushButton*   m_btnRecord = nullptr;
    QPushButton*   m_btnStop   = nullptr;
    QPushButton*   m_btnPlay   = nullptr;
    QPushButton*   m_btnSaveAs = nullptr;
    QPushButton*   m_btnLoad   = nullptr;
    QPushButton*   m_btnDelete = nullptr;
    QLabel*        m_status    = nullptr;
};
