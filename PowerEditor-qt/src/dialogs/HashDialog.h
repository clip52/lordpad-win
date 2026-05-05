#pragma once
#include <QDialog>
#include <QByteArray>

class QLineEdit;
class QToolButton;
class QLabel;
class QPushButton;

class HashDialog : public QDialog {
    Q_OBJECT
public:
    explicit HashDialog(QWidget* parent = nullptr);

    // bytes = data to hash (UTF-8 buffer if from text editor).
    // sourceLabel = e.g. "document" or "selection: 123 chars" (optional title).
    void load(const QByteArray& bytes, const QString& sourceLabel = QString());

private:
    void copyToClipboard(QLineEdit* edit);

    QByteArray  m_bytes;
    QString     m_sourceLabel;

    QLineEdit*  m_md5Edit    = nullptr;
    QLineEdit*  m_sha1Edit   = nullptr;
    QLineEdit*  m_sha256Edit = nullptr;
    QLineEdit*  m_sha512Edit = nullptr;

    QToolButton* m_md5Copy    = nullptr;
    QToolButton* m_sha1Copy   = nullptr;
    QToolButton* m_sha256Copy = nullptr;
    QToolButton* m_sha512Copy = nullptr;

    QLabel*      m_statusLabel  = nullptr;
    QPushButton* m_recomputeBtn = nullptr;
    QPushButton* m_closeBtn     = nullptr;
};
