#pragma once
#include <QDialog>

class ScintillaEdit;
class QLineEdit;
class QCheckBox;
class QTabWidget;
class QLabel;
class QPushButton;

class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    explicit FindReplaceDialog(ScintillaEdit* editor, QWidget* parent = nullptr);
    void setActiveEditor(ScintillaEdit* editor);   // called by MainWindow when active tab changes
    void showFind();         // show, focus search field, switch to Find tab
    void showReplace();      // show, focus search field, switch to Replace tab

private slots:
    void onFindNext();
    void onFindPrevious();
    void onReplace();
    void onReplaceAll();

private:
    int  buildSearchFlags() const;
    long long findOnce(long long startPos, long long endPos, const QByteArray& needle,
                       long long* matchStart, long long* matchEnd);
    bool findDirectional(bool forward);
    void setStatus(const QString& text);
    bool hasEditor() const { return m_editor != nullptr; }

    ScintillaEdit* m_editor = nullptr;

    QTabWidget*  m_tabs = nullptr;

    // Find tab widgets
    QLineEdit*   m_findEdit = nullptr;
    QPushButton* m_btnFindNext = nullptr;
    QPushButton* m_btnFindPrev = nullptr;
    QLabel*      m_findStatus = nullptr;

    // Replace tab widgets
    QLineEdit*   m_findEditR = nullptr;
    QLineEdit*   m_replaceEdit = nullptr;
    QPushButton* m_btnFindNextR = nullptr;
    QPushButton* m_btnFindPrevR = nullptr;
    QPushButton* m_btnReplace = nullptr;
    QPushButton* m_btnReplaceAll = nullptr;
    QLabel*      m_replaceStatus = nullptr;

    // Shared options
    QCheckBox*   m_chkMatchCase = nullptr;
    QCheckBox*   m_chkWholeWord = nullptr;
    QCheckBox*   m_chkRegex = nullptr;
    QCheckBox*   m_chkWrap = nullptr;

    // Cache of last successful match (used by Replace)
    long long    m_lastMatchStart = -1;
    long long    m_lastMatchEnd   = -1;
};
