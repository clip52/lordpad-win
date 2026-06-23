// CrashRecovery - implementação.
//
// Layout em disco (sob QStandardPaths::AppDataLocation + "/recovery/"):
//   <pid>.lock                 marcador de sessão viva
//   <pid>_<bufferId>.recovery  snapshot de um buffer
//
// Formato de cada .recovery:
//   linha 1: caminho original (pode estar em branco)
//   linha 2: encoding (ex.: "UTF-8")
//   linha 3: "---END-META---"
//   resto:  conteúdo bruto em UTF-8 (a aplicação converte na restauração)

#include "CrashRecovery.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QHash>
#include <QSaveFile>
#include <QRegularExpression>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>   // kill()
#include <errno.h>
#endif

namespace {
constexpr int    kSnapshotIntervalMs = 10 * 1000;
constexpr char   kMetaEndMarker[]    = "---END-META---";
constexpr char   kRecoverySuffix[]   = ".recovery";
constexpr char   kLockSuffix[]       = ".lock";
} // namespace

CrashRecovery::CrashRecovery(QObject* parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_pid(static_cast<qint64>(QCoreApplication::applicationPid()))
{
    m_timer->setInterval(kSnapshotIntervalMs);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &CrashRecovery::onTick);
}

CrashRecovery::~CrashRecovery()
{
    // Não chamamos shutdownClean() aqui de propósito — se o destrutor roda
    // por crash, queremos preservar os snapshots pra próxima execução.
    if (m_timer) m_timer->stop();
}

QString CrashRecovery::recoveryDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    const QString sub = "recovery";
    if (!dir.exists(sub)) {
        dir.mkpath(sub);
    }
    return dir.filePath(sub);
}

QString CrashRecovery::recoveryFilePath(int bufferId) const
{
    return recoveryDir() + QDir::separator() +
           QString::number(m_pid) + "_" + QString::number(bufferId) + kRecoverySuffix;
}

QString CrashRecovery::lockFilePath(qint64 pid) const
{
    return recoveryDir() + QDir::separator() + QString::number(pid) + kLockSuffix;
}

void CrashRecovery::start()
{
    if (m_started) return;
    m_started = true;

    // Garante diretório.
    (void)recoveryDir();

    // Escreve lockfile da sessão. Conteúdo: PID + timestamp (para debug).
    m_lockPath = lockFilePath(m_pid);
    QFile lock(m_lockPath);
    if (lock.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QByteArray payload =
            QByteArray::number(m_pid) + "\n" +
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8() + "\n";
        lock.write(payload);
        lock.close();
    } else {
        qWarning() << "[CrashRecovery] não foi possível criar lockfile:" << m_lockPath;
    }

    m_timer->start();
}

void CrashRecovery::registerBuffer(int bufferId,
                                   const QString& originalPath,
                                   const QString& encoding,
                                   ContentGetter getter)
{
    Entry e;
    auto it = m_buffers.find(bufferId);
    if (it != m_buffers.end()) {
        e = it.value();      // preserva lastHash/everWritten se já existia
    }
    e.originalPath = originalPath;
    e.encoding     = encoding.isEmpty() ? QStringLiteral("UTF-8") : encoding;
    e.getter       = std::move(getter);
    m_buffers.insert(bufferId, e);
}

void CrashRecovery::unregisterBuffer(int bufferId)
{
    auto it = m_buffers.find(bufferId);
    if (it == m_buffers.end()) return;
    QFile::remove(recoveryFilePath(bufferId));
    m_buffers.erase(it);
}

void CrashRecovery::shutdownClean()
{
    if (m_timer) m_timer->stop();

    // Apaga snapshots desta sessão.
    const QString dir = recoveryDir();
    const QString prefix = QString::number(m_pid) + "_";
    QDir d(dir);
    const auto entries = d.entryList(QDir::Files);
    for (const QString& name : entries) {
        if (name.startsWith(prefix) && name.endsWith(kRecoverySuffix)) {
            QFile::remove(d.filePath(name));
        }
    }

    // Apaga lockfile por último.
    if (!m_lockPath.isEmpty()) {
        QFile::remove(m_lockPath);
    }

    m_buffers.clear();
    m_started = false;
}

bool CrashRecovery::pidAlive(qint64 pid) const
{
    if (pid <= 0) return false;
    if (pid == m_pid) return true;
#ifdef _WIN32
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                             static_cast<DWORD>(pid));
    if (!h) return false;
    DWORD code = 0;
    const bool alive = ::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    ::CloseHandle(h);
    return alive;
#else
    // kill(pid, 0) → 0 vivo; -1 + EPERM ainda significa que existe.
    if (::kill(static_cast<pid_t>(pid), 0) == 0) return true;
    return errno == EPERM;
#endif
}

bool CrashRecovery::writeAtomic(const QString& path, const QByteArray& data) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[CrashRecovery] open falhou:" << path << file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        qWarning() << "[CrashRecovery] write incompleto:" << path;
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        qWarning() << "[CrashRecovery] commit falhou:" << path << file.errorString();
        return false;
    }
    return true;
}

void CrashRecovery::snapshotOne(int bufferId, Entry& entry)
{
    if (!entry.getter) return;

    QByteArray content;
    try {
        content = entry.getter();
    } catch (...) {
        qWarning() << "[CrashRecovery] getter lançou exceção para buffer" << bufferId;
        return;
    }

    const uint h = qHash(content);
    if (entry.everWritten && h == entry.lastHash) {
        return; // sem mudanças desde o último snapshot
    }

    QByteArray payload;
    payload.reserve(content.size() + 128);
    payload.append(entry.originalPath.toUtf8());
    payload.append('\n');
    payload.append(entry.encoding.toUtf8());
    payload.append('\n');
    payload.append(kMetaEndMarker);
    payload.append('\n');
    payload.append(content);

    const QString target = recoveryFilePath(bufferId);
    if (writeAtomic(target, payload)) {
        entry.lastHash    = h;
        entry.everWritten = true;
    }
}

void CrashRecovery::onTick()
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        snapshotOne(it.key(), it.value());
    }
}

bool CrashRecovery::parseRecoveryHeader(const QString& path, RecoveryRecord& out) const
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    // Lê só até o marcador de fim de meta — não carrega o corpo.
    const QByteArray l1 = f.readLine();
    const QByteArray l2 = f.readLine();
    const QByteArray l3 = f.readLine();
    f.close();

    auto stripNl = [](QByteArray b) {
        while (b.endsWith('\n') || b.endsWith('\r')) b.chop(1);
        return b;
    };

    if (stripNl(l3) != QByteArray(kMetaEndMarker)) {
        return false;
    }

    out.recoveryFile = path;
    out.originalPath = QString::fromUtf8(stripNl(l1));
    out.encoding     = QString::fromUtf8(stripNl(l2));

    // Extrai PID do nome (<pid>_<bufferId>.recovery).
    const QString base = QFileInfo(path).completeBaseName(); // sem ".recovery"
    const int us = base.indexOf('_');
    out.pid = (us > 0) ? base.left(us).toLongLong() : 0;

    out.modified = QFileInfo(path).lastModified();
    return true;
}

QList<RecoveryRecord> CrashRecovery::findOrphanRecoveries()
{
    QList<RecoveryRecord> result;
    const QString dir = recoveryDir();
    QDir d(dir);

    const auto files = d.entryInfoList(
        QStringList() << ("*" + QString(kRecoverySuffix)),
        QDir::Files, QDir::Time);

    for (const QFileInfo& fi : files) {
        RecoveryRecord rec;
        if (!parseRecoveryHeader(fi.absoluteFilePath(), rec)) continue;

        // Ignora arquivos da própria sessão.
        if (rec.pid == m_pid) continue;

        // Lock do dono ainda existe? Então o dono está (provavelmente) vivo.
        const QString ownerLock = lockFilePath(rec.pid);
        if (QFile::exists(ownerLock) && pidAlive(rec.pid)) {
            continue;
        }

        // Lockfile órfão também — marcamos para limpeza posterior.
        result.push_back(rec);
    }

    // Limpa lockfiles de PIDs mortos para não acumular lixo.
    const auto locks = d.entryInfoList(
        QStringList() << ("*" + QString(kLockSuffix)),
        QDir::Files);
    for (const QFileInfo& lf : locks) {
        const qint64 lockPid = lf.completeBaseName().toLongLong();
        if (lockPid > 0 && lockPid != m_pid && !pidAlive(lockPid)) {
            QFile::remove(lf.absoluteFilePath());
        }
    }

    return result;
}

void CrashRecovery::consume(const RecoveryRecord& rec)
{
    if (rec.recoveryFile.isEmpty()) return;
    QFile::remove(rec.recoveryFile);
}
