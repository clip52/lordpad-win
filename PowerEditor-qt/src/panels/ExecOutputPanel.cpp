#include "ExecOutputPanel.h"

#include <QDockWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <QTimer>
#include <QFontMetrics>
#include <QWidget>
#include <QFont>
#include <QPointer>
#include <QStringList>
#include <QByteArray>

#include "ScintillaEdit.h"

ExecOutputPanel::ExecOutputPanel(QWidget* parent)
    : QDockWidget(tr("Exec Output"), parent)
{
    setObjectName(QStringLiteral("ExecOutputPanel"));
    setAllowedAreas(Qt::AllDockWidgetAreas);

    auto* root = new QWidget(this);
    auto* vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Top toolbar row
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);

    m_cwdLabel = new QLabel(root);
    m_cwdLabel->setMaximumWidth(300);
    m_cwdLabel->setMinimumWidth(80);
    m_cwdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cwdLabel->setToolTip(tr("Working directory"));

    m_cdBtn = new QToolButton(root);
    m_cdBtn->setText(tr("CD..."));
    m_cdBtn->setToolTip(tr("Change working directory"));

    m_cmdEdit = new QLineEdit(root);
    m_cmdEdit->setPlaceholderText(tr("Command (Enter to run)"));

    m_runBtn = new QToolButton(root);
    m_runBtn->setText(tr("Run"));

    m_stopBtn = new QToolButton(root);
    m_stopBtn->setText(tr("Stop"));
    m_stopBtn->setEnabled(false);

    m_clearBtn = new QToolButton(root);
    m_clearBtn->setText(tr("Clear"));

    topRow->addWidget(m_cwdLabel);
    topRow->addWidget(m_cdBtn);
    topRow->addWidget(m_cmdEdit, /*stretch=*/1);
    topRow->addWidget(m_runBtn);
    topRow->addWidget(m_stopBtn);
    topRow->addWidget(m_clearBtn);

    vbox->addLayout(topRow);

    // Output Scintilla
    m_out = new ScintillaEdit(root);
    m_out->setCodePage(SC_CP_UTF8);
    m_out->setReadOnly(true);
    m_out->setMarginWidthN(0, 0);
    m_out->setMarginWidthN(1, 0);
    m_out->setMarginWidthN(2, 0);
    m_out->setWrapMode(SC_WRAP_NONE);

    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    m_out->setFont(monoFont);

    vbox->addWidget(m_out, /*stretch=*/1);

    setWidget(root);

    // Restore persisted CWD or default to home
    QSettings s;
    const QString saved = s.value(QStringLiteral("execPanel/cwd"), QDir::homePath()).toString();
    m_workingDir = saved.isEmpty() ? QDir::homePath() : saved;
    updateCwdLabel();

    // Wire signals
    connect(m_cdBtn,    &QToolButton::clicked, this, &ExecOutputPanel::onCdClicked);
    connect(m_runBtn,   &QToolButton::clicked, this, &ExecOutputPanel::onRunClicked);
    connect(m_stopBtn,  &QToolButton::clicked, this, &ExecOutputPanel::onStopClicked);
    connect(m_clearBtn, &QToolButton::clicked, this, &ExecOutputPanel::onClearClicked);
    connect(m_cmdEdit,  &QLineEdit::returnPressed, this, &ExecOutputPanel::onRunClicked);
}

ExecOutputPanel::~ExecOutputPanel()
{
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            m_proc->waitForFinished(500);
        }
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void ExecOutputPanel::setWorkingDirectory(const QString& path)
{
    if (path.isEmpty())
        return;
    m_workingDir = path;
    updateCwdLabel();

    QSettings s;
    s.setValue(QStringLiteral("execPanel/cwd"), m_workingDir);
}

QString ExecOutputPanel::workingDirectory() const
{
    return m_workingDir;
}

void ExecOutputPanel::updateCwdLabel()
{
    if (!m_cwdLabel)
        return;
    QFontMetrics fm(m_cwdLabel->font());
    const int w = m_cwdLabel->maximumWidth() > 0 ? m_cwdLabel->maximumWidth() : 300;
    const QString elided = fm.elidedText(m_workingDir, Qt::ElideMiddle, w);
    m_cwdLabel->setText(elided);
    m_cwdLabel->setToolTip(m_workingDir);
}

void ExecOutputPanel::setStopEnabled(bool enabled)
{
    if (m_stopBtn)
        m_stopBtn->setEnabled(enabled);
}

void ExecOutputPanel::onCdClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose working directory"), m_workingDir);
    if (!dir.isEmpty())
        setWorkingDirectory(dir);
}

void ExecOutputPanel::onRunClicked()
{
    runCommand(m_cmdEdit ? m_cmdEdit->text() : QString());
}

void ExecOutputPanel::onStopClicked()
{
    stopRunning();
}

void ExecOutputPanel::onClearClicked()
{
    if (!m_out)
        return;
    m_out->setReadOnly(false);
    m_out->clearAll();
    m_out->setReadOnly(true);
}

void ExecOutputPanel::runCommand(const QString& cmdlineIn)
{
    const QString cmdline = cmdlineIn.trimmed();
    if (cmdline.isEmpty())
        return;

    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        appendOutput(QByteArray("[busy]\n"));
        return;
    }

    // Banner
    {
        const QByteArray banner = QByteArrayLiteral("$ ") + cmdline.toUtf8() + QByteArrayLiteral("\n");
        appendOutput(banner);
    }

    // Recreate process for each run for a clean slate
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = nullptr;
    }

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setWorkingDirectory(m_workingDir);

    connect(m_proc, &QProcess::readyReadStandardOutput,
            this,   &ExecOutputPanel::onReadyReadStdout);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,   [this](int exitCode, QProcess::ExitStatus status) {
                onProcessFinished(exitCode, static_cast<int>(status));
            });
    connect(m_proc, &QProcess::errorOccurred,
            this,   [this](QProcess::ProcessError error) {
                onProcessError(static_cast<int>(error));
            });

    setStopEnabled(true);

    const QStringList args{ QStringLiteral("-c"), cmdline };
    m_proc->start(QStringLiteral("/bin/sh"), args);
}

void ExecOutputPanel::stopRunning()
{
    if (!m_proc)
        return;
    if (m_proc->state() == QProcess::NotRunning)
        return;

    m_proc->terminate();
    QPointer<QProcess> guard(m_proc);
    QTimer::singleShot(2000, this, [guard]() {
        if (guard && guard->state() != QProcess::NotRunning)
            guard->kill();
    });
}

void ExecOutputPanel::onReadyReadStdout()
{
    if (!m_proc)
        return;
    const QByteArray data = m_proc->readAllStandardOutput();
    if (!data.isEmpty())
        appendOutput(data);
}

void ExecOutputPanel::onProcessFinished(int exitCode, int /*exitStatus*/)
{
    const QByteArray msg =
        QByteArrayLiteral("[exited with code ") +
        QByteArray::number(exitCode) +
        QByteArrayLiteral("]\n");
    appendOutput(msg);

    setStopEnabled(false);
    emit commandFinished(exitCode);
}

void ExecOutputPanel::onProcessError(int error)
{
    QString name;
    switch (static_cast<QProcess::ProcessError>(error)) {
        case QProcess::FailedToStart: name = QStringLiteral("FailedToStart"); break;
        case QProcess::Crashed:       name = QStringLiteral("Crashed"); break;
        case QProcess::Timedout:      name = QStringLiteral("Timedout"); break;
        case QProcess::WriteError:    name = QStringLiteral("WriteError"); break;
        case QProcess::ReadError:     name = QStringLiteral("ReadError"); break;
        case QProcess::UnknownError:  name = QStringLiteral("UnknownError"); break;
        default:                      name = QStringLiteral("Error"); break;
    }
    const QByteArray msg =
        QByteArrayLiteral("[process error: ") +
        name.toUtf8() +
        QByteArrayLiteral("]\n");
    appendOutput(msg);
}

void ExecOutputPanel::appendOutput(const QByteArray& bytes)
{
    if (!m_out || bytes.isEmpty())
        return;
    m_out->setReadOnly(false);
    m_out->appendText(bytes.size(), bytes.constData());
    m_out->setReadOnly(true);
    m_out->gotoPos(m_out->length());
}
