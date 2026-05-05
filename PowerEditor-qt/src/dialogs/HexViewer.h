#pragma once
#include <QDialog>
#include <QByteArray>

class QPlainTextEdit;
class QLabel;
class QPushButton;

class HexViewer : public QDialog {
    Q_OBJECT
public:
    explicit HexViewer(QWidget* parent = nullptr);

    // Load bytes to display. Title is shown in the window title bar.
    void load(const QByteArray& bytes, const QString& sourceTitle = QString());

private slots:
    void copyAsHex();

private:
    QPlainTextEdit* m_view = nullptr;
    QLabel*         m_status = nullptr;
    QPushButton*    m_copyHexBtn = nullptr;
    QByteArray      m_bytes;
};
