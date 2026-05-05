#include "HashDialog.h"

#include <QDialog>
#include <QLineEdit>
#include <QToolButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QClipboard>
#include <QApplication>
#include <QCryptographicHash>
#include <QFont>
#include <QByteArray>
#include <QWidget>

namespace {

QFont monospaceFont()
{
    QFont f(QStringLiteral("monospace"));
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

struct HashRow {
    QLineEdit*   edit = nullptr;
    QToolButton* copy = nullptr;
};

HashRow makeHashRow(QWidget* parent)
{
    HashRow row;
    row.edit = new QLineEdit(parent);
    row.edit->setReadOnly(true);
    row.edit->setFont(monospaceFont());
    row.edit->setCursorPosition(0);

    row.copy = new QToolButton(parent);
    row.copy->setText(QObject::tr("Copy"));
    row.copy->setToolTip(QObject::tr("Copy to clipboard"));
    row.copy->setAutoRaise(false);
    return row;
}

QWidget* wrapRow(QLineEdit* edit, QToolButton* copyBtn, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* h = new QHBoxLayout(container);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    h->addWidget(edit, 1);
    h->addWidget(copyBtn, 0);
    return container;
}

} // namespace

HashDialog::HashDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Checksums"));
    resize(700, 300);

    auto* mainLayout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    HashRow md5Row    = makeHashRow(this);
    HashRow sha1Row   = makeHashRow(this);
    HashRow sha256Row = makeHashRow(this);
    HashRow sha512Row = makeHashRow(this);

    m_md5Edit    = md5Row.edit;
    m_sha1Edit   = sha1Row.edit;
    m_sha256Edit = sha256Row.edit;
    m_sha512Edit = sha512Row.edit;

    m_md5Copy    = md5Row.copy;
    m_sha1Copy   = sha1Row.copy;
    m_sha256Copy = sha256Row.copy;
    m_sha512Copy = sha512Row.copy;

    form->addRow(tr("MD5:"),     wrapRow(m_md5Edit,    m_md5Copy,    this));
    form->addRow(tr("SHA-1:"),   wrapRow(m_sha1Edit,   m_sha1Copy,   this));
    form->addRow(tr("SHA-256:"), wrapRow(m_sha256Edit, m_sha256Copy, this));
    form->addRow(tr("SHA-512:"), wrapRow(m_sha512Edit, m_sha512Copy, this));

    mainLayout->addLayout(form);

    m_statusLabel = new QLabel(tr("Bytes hashed: %1").arg(0), this);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch(1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_recomputeBtn = buttonBox->addButton(tr("Recompute"), QDialogButtonBox::ActionRole);
    m_closeBtn     = buttonBox->button(QDialogButtonBox::Close);

    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (m_closeBtn) {
        connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    }
    if (m_recomputeBtn) {
        connect(m_recomputeBtn, &QPushButton::clicked, this, [this]() {
            load(m_bytes, m_sourceLabel);
        });
    }

    connect(m_md5Copy,    &QToolButton::clicked, this, [this]() { copyToClipboard(m_md5Edit); });
    connect(m_sha1Copy,   &QToolButton::clicked, this, [this]() { copyToClipboard(m_sha1Edit); });
    connect(m_sha256Copy, &QToolButton::clicked, this, [this]() { copyToClipboard(m_sha256Edit); });
    connect(m_sha512Copy, &QToolButton::clicked, this, [this]() { copyToClipboard(m_sha512Edit); });
}

void HashDialog::load(const QByteArray& bytes, const QString& sourceLabel)
{
    m_bytes       = bytes;
    m_sourceLabel = sourceLabel;

    if (sourceLabel.isEmpty()) {
        setWindowTitle(tr("Checksums"));
    } else {
        setWindowTitle(tr("Checksums \xE2\x80\x94 %1").arg(sourceLabel));
    }

    const QString placeholder = tr("(empty)");

    if (bytes.isEmpty()) {
        m_md5Edit->clear();
        m_sha1Edit->clear();
        m_sha256Edit->clear();
        m_sha512Edit->clear();

        m_md5Edit->setPlaceholderText(placeholder);
        m_sha1Edit->setPlaceholderText(placeholder);
        m_sha256Edit->setPlaceholderText(placeholder);
        m_sha512Edit->setPlaceholderText(placeholder);

        m_statusLabel->setText(tr("Bytes hashed: %1").arg(0));
        return;
    }

    const QString md5Hex    = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Md5).toHex());
    const QString sha1Hex   = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
    const QString sha256Hex = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString sha512Hex = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha512).toHex());

    m_md5Edit->setText(md5Hex);
    m_sha1Edit->setText(sha1Hex);
    m_sha256Edit->setText(sha256Hex);
    m_sha512Edit->setText(sha512Hex);

    m_md5Edit->setCursorPosition(0);
    m_sha1Edit->setCursorPosition(0);
    m_sha256Edit->setCursorPosition(0);
    m_sha512Edit->setCursorPosition(0);

    m_statusLabel->setText(tr("Bytes hashed: %1").arg(bytes.size()));
}

void HashDialog::copyToClipboard(QLineEdit* edit)
{
    if (!edit) {
        return;
    }
    if (QClipboard* cb = QApplication::clipboard()) {
        cb->setText(edit->text());
    }
}
