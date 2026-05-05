#pragma once
#include <QDialog>
#include <QString>

class ScintillaEdit;
class QSplitter;
class QLabel;

class ComparePanel : public QDialog {
    Q_OBJECT
public:
    explicit ComparePanel(QWidget* parent = nullptr);

    void setLeft(const QString& title, const QString& utf8Text);
    void setRight(const QString& title, const QString& utf8Text);
    void runDiff();   // re-compute diff and re-mark both sides

private:
    enum LineKind { Same, Added, Removed, Modified };

    void configureEditor(ScintillaEdit* ed);
    void clearAllMarkers(ScintillaEdit* ed);
    void syncScroll(ScintillaEdit* from, ScintillaEdit* to);
    void updateStatus(int added, int removed, int modified);

    QSplitter*     m_splitter   = nullptr;
    ScintillaEdit* m_left       = nullptr;
    ScintillaEdit* m_right      = nullptr;
    QLabel*        m_leftLabel  = nullptr;
    QLabel*        m_rightLabel = nullptr;
    QLabel*        m_status     = nullptr;

    bool m_hasLeft  = false;
    bool m_hasRight = false;

    bool m_syncing = false;   // re-entrancy guard for synchronized scrolling
};
