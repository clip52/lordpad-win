#pragma once
#include <QDialog>
#include <QString>

class ScintillaEdit;
class QLabel;
class QPushButton;

class WordCountDialog : public QDialog {
    Q_OBJECT
public:
    explicit WordCountDialog(QWidget* parent = nullptr);

    // Populate stats from the editor. If selection is empty, the "Selection" rows show 0.
    void load(ScintillaEdit* editor, const QString& sourceTitle = QString());

private:
    struct Stats {
        int charsWithWs = 0;
        int charsNoWs   = 0;
        int words       = 0;
        int lines       = 0;
        int paragraphs  = 0;
    };

    static Stats computeStats(const QString& text);
    void setRow(QLabel* docLabel, QLabel* selLabel, int docVal, int selVal);
    void onRefresh();

    ScintillaEdit* m_editor = nullptr;
    QString        m_title;

    QLabel* m_docCharsWs   = nullptr;
    QLabel* m_docCharsNoWs = nullptr;
    QLabel* m_docWords     = nullptr;
    QLabel* m_docLines     = nullptr;
    QLabel* m_docParas     = nullptr;

    QLabel* m_selCharsWs   = nullptr;
    QLabel* m_selCharsNoWs = nullptr;
    QLabel* m_selWords     = nullptr;
    QLabel* m_selLines     = nullptr;
    QLabel* m_selParas     = nullptr;

    QPushButton* m_refreshBtn = nullptr;
};
