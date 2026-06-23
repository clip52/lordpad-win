#include "TerminalPanel.h"

#include <QCloseEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#ifdef _WIN32
// O terminal embutido depende de forkpty(3)/pty.h, que não existem no Windows.
// Fica stubado até um backend ConPTY ser portado; a classe segue existindo para
// não exigir cirurgia no MainWindow.
#else
extern "C" {
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
}
#endif

TerminalPanel::TerminalPanel(QWidget* parent)
    : QDockWidget(tr("Terminal"), parent)
{
    setObjectName(QStringLiteral("TerminalPanel"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    auto* root = new QWidget(this);
    setWidget(root);

    m_view = new QPlainTextEdit(root);
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(5000);   // bounded scrollback so we don't bloat
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    m_view->setFont(mono);

    m_input = new QLineEdit(root);
    m_input->setPlaceholderText(tr("Comando — Enter para enviar"));
    m_input->setFont(mono);

    m_clear = new QToolButton(root);
    m_clear->setText(tr("Limpar"));
    m_restart = new QToolButton(root);
    m_restart->setText(tr("Reiniciar"));

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_input, 1);
    row->addWidget(m_clear);
    row->addWidget(m_restart);

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->addWidget(m_view, 1);
    lay->addLayout(row);

    connect(m_input,   &QLineEdit::returnPressed, this, &TerminalPanel::onSubmit);
    connect(m_clear,   &QToolButton::clicked,    this, &TerminalPanel::onClearClicked);
    connect(m_restart, &QToolButton::clicked,    this, &TerminalPanel::onRestartClicked);

    startShell();
}

TerminalPanel::~TerminalPanel() {
    stopShell();
}

void TerminalPanel::closeEvent(QCloseEvent* e) {
    // QDockWidget closeEvent fires when the user toggles it off — keep the
    // shell running so the next open keeps history. Nothing to do here.
    QDockWidget::closeEvent(e);
}

void TerminalPanel::startShell(const QString& shellPath)
{
    QString cmd = shellPath;
    if (cmd.isEmpty()) {
        cmd = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SHELL"));
        if (cmd.isEmpty()) cmd = QStringLiteral("/bin/bash");
    }
    startCommand(cmd, { QStringLiteral("-i") }, QString());
}

void TerminalPanel::startCommand(const QString& executable, const QStringList& args,
                                 const QString& windowTitle)
{
#ifdef _WIN32
    if (!windowTitle.isEmpty()) setWindowTitle(windowTitle);
    appendOutput(QByteArray("[Terminal embutido ainda não suportado no Windows "
                            "(ConPTY pendente). Use o painel CLI/Shell.]\n"));
    Q_UNUSED(executable);
    Q_UNUSED(args);
#else
    if (m_masterFd >= 0) stopShell();

    int master = -1;
    pid_t pid = forkpty(&master, nullptr, nullptr, nullptr);
    if (pid < 0) {
        appendOutput(QByteArray("forkpty failed: ") + strerror(errno) + "\n");
        return;
    }
    if (pid == 0) {
        // Child: exec the requested executable. We build a NULL-terminated
        // argv vector, with argv[0] = basename of the executable.
        signal(SIGPIPE, SIG_DFL);
        const QByteArray exeUtf8 = executable.toUtf8();
        QList<QByteArray> argvBufs;
        argvBufs.reserve(args.size() + 1);
        argvBufs.append(exeUtf8);
        for (const QString& a : args) argvBufs.append(a.toUtf8());
        std::vector<char*> argv;
        argv.reserve(argvBufs.size() + 1);
        for (auto& b : argvBufs) argv.push_back(b.data());
        argv.push_back(nullptr);
        execvp(exeUtf8.constData(), argv.data());
        _exit(127);
    }

    m_masterFd = master;
    m_childPid = pid;

    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier.data(), &QSocketNotifier::activated,
            this, &TerminalPanel::onPtyReadable);

    if (!windowTitle.isEmpty()) setWindowTitle(windowTitle);
    appendOutput(("$ " + executable + " " + args.join(' ') + "\n").toUtf8());
#endif
}

void TerminalPanel::stopShell()
{
#ifndef _WIN32
    if (m_notifier) { m_notifier->setEnabled(false); m_notifier->deleteLater(); }
    if (m_masterFd >= 0) { ::close(m_masterFd); m_masterFd = -1; }
    if (m_childPid > 0) {
        ::kill(m_childPid, SIGHUP);
        int status = 0;
        // Reap; non-blocking so we don't stall the UI thread when the child
        // is unresponsive — leftover zombies will be cleared at app exit.
        ::waitpid(m_childPid, &status, WNOHANG);
        m_childPid = -1;
    }
#endif
}

void TerminalPanel::sendLine(const QString& text)
{
#ifdef _WIN32
    Q_UNUSED(text);
#else
    if (m_masterFd < 0) return;
    QByteArray buf = text.toUtf8();
    buf.append('\n');
    ::write(m_masterFd, buf.constData(), buf.size());
#endif
}

void TerminalPanel::onSubmit()
{
    const QString text = m_input->text();
    m_input->clear();
    // Echo locally so the user can see what they typed; the shell will also
    // echo it back via the pty, but local echo keeps the display reactive.
    appendOutput(("$ " + text + "\n").toUtf8());
    sendLine(text);
}

void TerminalPanel::onClearClicked()    { m_view->clear(); }
void TerminalPanel::onRestartClicked()  { stopShell(); startShell(); }

void TerminalPanel::onPtyReadable()
{
#ifndef _WIN32
    if (m_masterFd < 0) return;
    char chunk[4096];
    while (true) {
        ssize_t n = ::read(m_masterFd, chunk, sizeof(chunk));
        if (n > 0) {
            appendOutput(QByteArray(chunk, static_cast<int>(n)));
            continue;
        }
        if (n == 0 || (n < 0 && (errno == EIO))) {
            // Shell exited — close the side without restarting (user can
            // press the Restart button explicitly).
            appendOutput(QByteArray("\n[shell terminou]\n"));
            stopShell();
            return;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0 && errno != EINTR) {
            appendOutput(QByteArray("read error: ") + strerror(errno) + "\n");
            return;
        }
    }
#endif
}

void TerminalPanel::appendOutput(const QByteArray& bytes)
{
    const QByteArray clean = stripAnsi(bytes);
    if (clean.isEmpty()) return;
    QTextCursor tc(m_view->document());
    tc.movePosition(QTextCursor::End);
    tc.insertText(QString::fromUtf8(clean));
    // Auto-scroll to keep the latest output visible.
    auto* sb = m_view->verticalScrollBar();
    sb->setValue(sb->maximum());
}

QByteArray TerminalPanel::stripAnsi(const QByteArray& in)
{
    // Drops:
    //   - CSI sequences: ESC '[' ... letter
    //   - OSC sequences: ESC ']' ... BEL or ESC '\\'
    //   - Bare ESC followed by single intro byte (non-CSI/OSC)
    //   - DEL / BEL bytes
    QByteArray out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        if (c == 0x07 || c == 0x7F) continue;          // BEL / DEL
        if (c != 0x1B) { out += static_cast<char>(c); continue; }
        // ESC seen — peek next byte.
        if (i + 1 >= in.size()) break;
        char next = in[i + 1];
        if (next == '[') {
            // CSI ... letter (final byte in 0x40..0x7E).
            i += 2;
            while (i < in.size() && (static_cast<unsigned char>(in[i]) < 0x40
                                  || static_cast<unsigned char>(in[i]) > 0x7E)) ++i;
            // i now points at the final byte; the loop's ++i will skip it.
        } else if (next == ']') {
            // OSC ... terminated by BEL or ESC '\\'.
            i += 2;
            while (i < in.size()) {
                char b = in[i];
                if (b == 0x07) break;
                if (b == 0x1B && i + 1 < in.size() && in[i + 1] == '\\') { ++i; break; }
                ++i;
            }
        } else {
            // ESC + single byte (e.g. ESC = swap charsets) — skip both.
            ++i;
        }
    }
    return out;
}
