#include "HexViewer.h"

#include <QDialog>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QApplication>
#include <QFont>

namespace {

constexpr qsizetype kMaxRenderBytes = 1024 * 1024; // 1 MiB
constexpr int kBytesPerLine = 16;

inline char hexNibble(int v) {
    return static_cast<char>(v < 10 ? ('0' + v) : ('a' + (v - 10)));
}

// Build the classic hex dump for the supplied range of bytes.
QString buildHexDump(const QByteArray& bytes, qsizetype renderLen) {
    const qsizetype lines = (renderLen + kBytesPerLine - 1) / kBytesPerLine;

    // Per-line size approx: 8 (offset) + 2 (gap) + 16*3 (hex) + 1 (extra space) + 3 (gap) + 16 (ascii) + 1 (\n) = ~79
    QString out;
    out.reserve(static_cast<int>(lines * 80 + 64));

    const uchar* data = reinterpret_cast<const uchar*>(bytes.constData());

    for (qsizetype lineStart = 0; lineStart < renderLen; lineStart += kBytesPerLine) {
        const qsizetype lineLen = qMin<qsizetype>(kBytesPerLine, renderLen - lineStart);

        // Offset: 8 hex digits, 0-padded.
        char offBuf[9];
        quint32 off = static_cast<quint32>(lineStart);
        for (int i = 7; i >= 0; --i) {
            offBuf[i] = hexNibble(off & 0xF);
            off >>= 4;
        }
        offBuf[8] = '\0';
        out.append(QLatin1String(offBuf, 8));
        out.append(QLatin1String("  ")); // 2 spaces after offset

        // Hex columns (16 bytes). Extra space between byte 8 and 9 (i.e. before index 8).
        for (int i = 0; i < kBytesPerLine; ++i) {
            if (i == 8) {
                out.append(QLatin1Char(' ')); // extra space before second half
            }
            if (i < lineLen) {
                uchar b = data[lineStart + i];
                char pair[2] = { hexNibble((b >> 4) & 0xF), hexNibble(b & 0xF) };
                out.append(QLatin1String(pair, 2));
            } else {
                out.append(QLatin1String("  "));
            }
            if (i != kBytesPerLine - 1) {
                out.append(QLatin1Char(' '));
            }
        }

        // 3 spaces gap before ASCII gutter.
        out.append(QLatin1String("   "));

        // ASCII gutter: 16 chars.
        for (int i = 0; i < kBytesPerLine; ++i) {
            if (i < lineLen) {
                uchar b = data[lineStart + i];
                if (b >= 0x20 && b <= 0x7E) {
                    out.append(QLatin1Char(static_cast<char>(b)));
                } else {
                    out.append(QLatin1Char('.'));
                }
            } else {
                out.append(QLatin1Char(' '));
            }
        }

        out.append(QLatin1Char('\n'));
    }

    return out;
}

// Build a continuous hex string of the entire buffer (no spaces).
QString buildContinuousHex(const QByteArray& bytes) {
    QString out;
    out.reserve(static_cast<int>(bytes.size() * 2));
    const uchar* data = reinterpret_cast<const uchar*>(bytes.constData());
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        uchar b = data[i];
        char pair[2] = { hexNibble((b >> 4) & 0xF), hexNibble(b & 0xF) };
        out.append(QLatin1String(pair, 2));
    }
    return out;
}

} // namespace

HexViewer::HexViewer(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hex Viewer"));
    resize(900, 600);
    setSizeGripEnabled(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Top toolbar with status label.
    auto* topBar = new QToolBar(this);
    topBar->setMovable(false);
    topBar->setFloatable(false);
    m_status = new QLabel(tr("Bytes: %1  |  Hex: %2  |  Lines: %3").arg(0).arg(0).arg(0), topBar);
    topBar->addWidget(m_status);
    mainLayout->addWidget(topBar);

    // Monospace, fixed-width, read-only viewer.
    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setFixedPitch(true);
    mono.setPointSize(10);
    m_view->setFont(mono);
    mainLayout->addWidget(m_view, 1);

    // Bottom button box: Close + Copy as Hex.
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_copyHexBtn = new QPushButton(tr("Copy as Hex"), this);
    buttons->addButton(m_copyHexBtn, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_copyHexBtn, &QPushButton::clicked, this, &HexViewer::copyAsHex);
    mainLayout->addWidget(buttons);
}

void HexViewer::load(const QByteArray& bytes, const QString& sourceTitle) {
    m_bytes = bytes;

    if (sourceTitle.isEmpty()) {
        setWindowTitle(tr("Hex Viewer"));
    } else {
        setWindowTitle(tr("Hex Viewer — %1").arg(sourceTitle));
    }

    const qsizetype total = bytes.size();
    const qsizetype renderLen = qMin<qsizetype>(total, kMaxRenderBytes);

    QString dump = buildHexDump(bytes, renderLen);

    if (total > renderLen) {
        const qsizetype remaining = total - renderLen;
        dump.append(QStringLiteral("\n"));
        dump.append(tr("(truncated — %1 more bytes not shown)").arg(remaining));
        dump.append(QLatin1Char('\n'));
    }

    m_view->setPlainText(dump);

    const qsizetype hexChars = total * 2;
    const qsizetype lineCount = (renderLen + kBytesPerLine - 1) / kBytesPerLine;
    m_status->setText(tr("Bytes: %1  |  Hex: %2  |  Lines: %3")
                          .arg(total)
                          .arg(hexChars)
                          .arg(lineCount));
}

void HexViewer::copyAsHex() {
    QClipboard* cb = QApplication::clipboard();
    if (!cb) {
        return;
    }
    cb->setText(buildContinuousHex(m_bytes));
}
