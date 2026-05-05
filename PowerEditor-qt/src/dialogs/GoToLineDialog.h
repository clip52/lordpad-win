#pragma once
#include <QDialog>
class ScintillaEdit;
class QSpinBox;
class QLabel;

class GoToLineDialog : public QDialog {
    Q_OBJECT
public:
    explicit GoToLineDialog(ScintillaEdit* editor, QWidget* parent = nullptr);

private:
    ScintillaEdit* m_editor;
    QSpinBox* m_spinBox;
    QLabel* m_infoLabel;

private slots:
    void onAccept();
};
