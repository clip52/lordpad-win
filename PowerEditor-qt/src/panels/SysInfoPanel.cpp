#include "SysInfoPanel.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#else
extern "C" {
#include <sys/sysinfo.h>
#include <unistd.h>
}
#endif

SysInfoPanel::SysInfoPanel(QWidget* parent) : QDockWidget(tr("Sistema"), parent)
{
    setObjectName(QStringLiteral("SysInfoPanel"));
    setAllowedAreas(Qt::AllDockWidgetAreas);

    auto* root = new QWidget(this);
    m_text = new QPlainTextEdit(root);
    m_text->setReadOnly(true);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_text->setFont(mono);
    m_refreshBtn = new QPushButton(tr("Atualizar agora"), root);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    row->addWidget(m_refreshBtn);

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addWidget(m_text, 1);
    lay->addLayout(row);
    setWidget(root);

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &SysInfoPanel::onRefresh);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SysInfoPanel::onRefresh);
    m_timer->start();
    onRefresh();
}

void SysInfoPanel::onRefresh()
{
    QStringList lines;
    lines << QStringLiteral("== Sistema ==");
    lines << QStringLiteral("OS:       %1 %2").arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion());
    lines << QStringLiteral("Kernel:   %1 %2 (%3)").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture());
    lines << QStringLiteral("Host:     %1").arg(QSysInfo::machineHostName());
    lines << QStringLiteral("CPUs:     %1 (idealThread=%2)").arg(QThread::idealThreadCount()).arg(QThread::idealThreadCount());

#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        const auto mb = [](DWORDLONG bytes) { return double(bytes) / (1024.0 * 1024.0); };
        lines << QStringLiteral("");
        lines << QStringLiteral("== Memória ==");
        lines << QStringLiteral("Total:    %1 MiB").arg(mb(ms.ullTotalPhys), 0, 'f', 0);
        lines << QStringLiteral("Livre:    %1 MiB").arg(mb(ms.ullAvailPhys), 0, 'f', 0);
        lines << QStringLiteral("Swap:     %1 / %2 MiB livres")
                     .arg(mb(ms.ullAvailPageFile), 0, 'f', 0)
                     .arg(mb(ms.ullTotalPageFile), 0, 'f', 0);
        lines << QStringLiteral("Em uso:   %1%").arg(ms.dwMemoryLoad);
    }
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        const auto mb = [&](unsigned long bytes) { return double(bytes) * si.mem_unit / (1024.0 * 1024.0); };
        lines << QStringLiteral("");
        lines << QStringLiteral("== Memória ==");
        lines << QStringLiteral("Total:    %1 MiB").arg(mb(si.totalram), 0, 'f', 0);
        lines << QStringLiteral("Livre:    %1 MiB").arg(mb(si.freeram),  0, 'f', 0);
        lines << QStringLiteral("Swap:     %1 / %2 MiB livres")
                     .arg(mb(si.freeswap), 0, 'f', 0)
                     .arg(mb(si.totalswap), 0, 'f', 0);
        lines << QStringLiteral("Uptime:   %1 h").arg(si.uptime / 3600.0, 0, 'f', 1);
        lines << QStringLiteral("Load avg: %1 / %2 / %3")
                     .arg(si.loads[0] / 65536.0, 0, 'f', 2)
                     .arg(si.loads[1] / 65536.0, 0, 'f', 2)
                     .arg(si.loads[2] / 65536.0, 0, 'f', 2);
    }
#endif

    lines << QStringLiteral("");
    lines << QStringLiteral("== Discos (montados) ==");
    for (const QStorageInfo& s : QStorageInfo::mountedVolumes()) {
        if (!s.isReady() || s.isReadOnly()) continue;
        lines << QStringLiteral("%1  %2 / %3 GiB livres")
                     .arg(s.rootPath().leftJustified(24, ' '))
                     .arg(s.bytesAvailable() / 1073741824.0, 0, 'f', 1)
                     .arg(s.bytesTotal()     / 1073741824.0, 0, 'f', 1);
    }

    lines << QStringLiteral("");
    lines << QStringLiteral("== Editor ==");
    lines << QStringLiteral("PID:      %1").arg(QCoreApplication::applicationPid());
    lines << QStringLiteral("Path:     %1").arg(QCoreApplication::applicationFilePath());

    m_text->setPlainText(lines.join('\n'));
}
