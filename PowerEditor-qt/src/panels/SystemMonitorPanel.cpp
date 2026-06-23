#include "SystemMonitorPanel.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

namespace {

QString readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

struct ProcInfo {
    int pid = 0;
    QString comm;
    QString user;
    long rssKb = 0;
    long vmKb = 0;
    QString state;
};

QString uidToUser(const QString& uid)
{
    static QHash<QString, QString> cache;
    if (auto it = cache.find(uid); it != cache.end()) return *it;
    QFile pw(QStringLiteral("/etc/passwd"));
    if (pw.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&pw);
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            const QStringList p = line.split(':');
            if (p.size() >= 3 && p[2] == uid) {
                cache.insert(uid, p[0]);
                return p[0];
            }
        }
    }
    cache.insert(uid, uid);
    return uid;
}

ProcInfo readProc(int pid)
{
    ProcInfo p; p.pid = pid;
    const QString base = QStringLiteral("/proc/%1").arg(pid);
    const QString status = readFile(base + QStringLiteral("/status"));
    if (status.isEmpty()) return p;
    for (const QString& line : status.split('\n')) {
        if (line.startsWith(QStringLiteral("Name:")))     p.comm  = line.section('\t', 1).trimmed();
        else if (line.startsWith(QStringLiteral("State:"))) p.state = line.section('\t', 1).trimmed().left(1);
        else if (line.startsWith(QStringLiteral("VmRSS:"))) p.rssKb = line.section(QRegularExpression("\\s+"), 1, 1).toLong();
        else if (line.startsWith(QStringLiteral("VmSize:"))) p.vmKb = line.section(QRegularExpression("\\s+"), 1, 1).toLong();
        else if (line.startsWith(QStringLiteral("Uid:"))) {
            const QString uid = line.section('\t', 1).section('\t', 0, 0);
            p.user = uidToUser(uid);
        }
    }
    return p;
}

QString humanKb(long kb)
{
    if (kb < 1024) return QString::number(kb) + " KB";
    if (kb < 1024L * 1024L) return QString::number(kb / 1024.0, 'f', 1) + " MB";
    return QString::number(kb / 1024.0 / 1024.0, 'f', 2) + " GB";
}

} // namespace

SystemMonitorPanel::SystemMonitorPanel(QWidget* parent) : QDockWidget(tr("Sistema (top)"), parent)
{
    setObjectName(QStringLiteral("SystemMonitorPanel"));
    setAllowedAreas(Qt::AllDockWidgetAreas);

    auto* root = new QWidget(this);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    m_summary = new QLabel(root); m_summary->setFont(mono);
    m_filter  = new QLineEdit(root); m_filter->setPlaceholderText(tr("filtro (nome ou usuário)"));
    m_procs   = new QTreeWidget(root);
    m_procs->setHeaderLabels({ tr("PID"), tr("Usuário"), tr("RSS"), tr("VM"), tr("S"), tr("Comando") });
    m_procs->setRootIsDecorated(false);
    m_procs->setUniformRowHeights(true);
    m_procs->setSortingEnabled(true);
    m_killBtn = new QPushButton(tr("kill -TERM"), root);
    m_pollBtn = new QPushButton(tr("Polling 2s"), root);
    m_pollBtn->setCheckable(true);
    m_refreshBtn = new QPushButton(tr("Refresh"), root);

    auto* row = new QHBoxLayout();
    row->addWidget(m_filter, 1);
    row->addWidget(m_refreshBtn);
    row->addWidget(m_pollBtn);
    row->addWidget(m_killBtn);

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(4,4,4,4);
    lay->addWidget(m_summary);
    lay->addLayout(row);
    lay->addWidget(m_procs, 1);
    setWidget(root);

    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitorPanel::refresh);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SystemMonitorPanel::refresh);
    connect(m_killBtn, &QPushButton::clicked, this, &SystemMonitorPanel::onKill);
    connect(m_pollBtn, &QPushButton::clicked, this, &SystemMonitorPanel::onTogglePoll);
    connect(m_filter, &QLineEdit::textChanged, this, &SystemMonitorPanel::refresh);

    refresh();
}

void SystemMonitorPanel::refresh()
{
    // CPU info
    const QString stat = readFile(QStringLiteral("/proc/loadavg"));
    const QString meminfo = readFile(QStringLiteral("/proc/meminfo"));
    long memTotal = 0, memFree = 0, memAvail = 0;
    for (const QString& line : meminfo.split('\n')) {
        if (line.startsWith(QStringLiteral("MemTotal:")))     memTotal = line.section(QRegularExpression("\\s+"), 1, 1).toLong();
        else if (line.startsWith(QStringLiteral("MemFree:"))) memFree  = line.section(QRegularExpression("\\s+"), 1, 1).toLong();
        else if (line.startsWith(QStringLiteral("MemAvailable:"))) memAvail = line.section(QRegularExpression("\\s+"), 1, 1).toLong();
    }
    m_summary->setText(tr("Load: %1   Mem: %2 livre / %3 disponível / %4 total")
                          .arg(stat.section(' ', 0, 2),
                               humanKb(memFree),
                               humanKb(memAvail),
                               humanKb(memTotal)));

    const QString filter = m_filter->text().trimmed();
    m_procs->setSortingEnabled(false);
    m_procs->clear();
    QDir proc(QStringLiteral("/proc"));
    bool ok;
    for (const QString& entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const int pid = entry.toInt(&ok);
        if (!ok) continue;
        const ProcInfo p = readProc(pid);
        if (p.comm.isEmpty()) continue;
        if (!filter.isEmpty() && !p.comm.contains(filter, Qt::CaseInsensitive)
                              && !p.user.contains(filter, Qt::CaseInsensitive)) continue;
        auto* it = new QTreeWidgetItem(m_procs);
        it->setData(0, Qt::DisplayRole, p.pid);
        it->setText(1, p.user);
        it->setData(2, Qt::DisplayRole, qlonglong(p.rssKb));
        it->setText(2, humanKb(p.rssKb));
        it->setData(3, Qt::DisplayRole, qlonglong(p.vmKb));
        it->setText(3, humanKb(p.vmKb));
        it->setText(4, p.state);
        it->setText(5, p.comm);
    }
    m_procs->setSortingEnabled(true);
    m_procs->sortByColumn(2, Qt::DescendingOrder);
    for (int c = 0; c < 6; ++c) m_procs->resizeColumnToContents(c);
}

void SystemMonitorPanel::onKill()
{
    auto* it = m_procs->currentItem();
    if (!it) return;
    const int pid = it->data(0, Qt::DisplayRole).toInt();
#ifdef _WIN32
    if (pid > 0) {
        HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (h) { ::TerminateProcess(h, 1); ::CloseHandle(h); }
    }
#else
    if (pid > 0) ::kill(pid, SIGTERM);
#endif
    QTimer::singleShot(500, this, &SystemMonitorPanel::refresh);
}

void SystemMonitorPanel::onTogglePoll()
{
    if (m_pollBtn->isChecked()) m_timer->start();
    else                        m_timer->stop();
}
