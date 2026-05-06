#include "GitStatusService.h"

#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStringList>
#include <QRegularExpression>

namespace {
constexpr int kProcessTimeoutMsec = 1500;
constexpr qint64 kCacheTtlMsec = 5000;
const QString kGitProgram = QStringLiteral("git");
} // namespace

GitStatusService::GitStatusService(QObject* parent)
    : QObject(parent)
{
}

GitStatusService::~GitStatusService()
{
    // Mata e libera todas as queries pendentes.
    for (PendingQuery* q : m_inFlight) {
        if (q->proc) {
            q->proc->disconnect(this);
            q->proc->kill();
            q->proc->waitForFinished(100);
            q->proc->deleteLater();
        }
        if (q->timeout) {
            q->timeout->stop();
            q->timeout->deleteLater();
        }
        delete q;
    }
    m_inFlight.clear();
}

void GitStatusService::queryStatus(const QString& filePath)
{
    if (filePath.isEmpty()) return;

    // Coalescência: ignora nova request se já há query em voo.
    if (m_inFlight.contains(filePath)) return;

    // Cache hit fresco -> emite assincronamente sem fazer nova query.
    const auto it = m_cache.find(filePath);
    if (it != m_cache.end()
        && QDateTime::currentMSecsSinceEpoch() - it->fetchedAtMsec < kCacheTtlMsec) {
        const GitStatus status = it->status;
        QTimer::singleShot(0, this, [this, filePath, status]() {
            emit statusReady(filePath, status);
        });
        return;
    }

    auto* q = new PendingQuery;
    q->filePath = filePath;
    QFileInfo fi(filePath);
    q->workingDir = fi.absolutePath();
    if (q->workingDir.isEmpty() || !QFileInfo(q->workingDir).isDir()) {
        q->workingDir = QDir::currentPath();
    }
    m_inFlight.insert(filePath, q);
    startStage(q);
}

GitStatus GitStatusService::cachedStatus(const QString& filePath) const
{
    const auto it = m_cache.find(filePath);
    return (it == m_cache.end()) ? GitStatus{} : it->status;
}

void GitStatusService::refreshAll()
{
    m_cache.clear();
}

void GitStatusService::startStage(PendingQuery* q)
{
    // Monta argumentos da etapa atual do pipeline:
    // 0) rev-parse --show-toplevel  (descobrir repo root)
    // 1) status --porcelain=v1 -- <relpath>
    // 2) rev-parse --abbrev-ref HEAD
    // 3) rev-list --left-right --count <branch>...origin/<branch>
    QStringList args;
    QString cwd;

    switch (q->stage) {
    case 0:
        cwd = q->workingDir;
        args << QStringLiteral("-C") << q->workingDir
             << QStringLiteral("rev-parse") << QStringLiteral("--show-toplevel");
        break;
    case 1:
        cwd = q->repoRoot;
        args << QStringLiteral("-C") << q->repoRoot
             << QStringLiteral("status") << QStringLiteral("--porcelain=v1")
             << QStringLiteral("--") << q->relPath;
        break;
    case 2:
        cwd = q->repoRoot;
        args << QStringLiteral("-C") << q->repoRoot
             << QStringLiteral("rev-parse") << QStringLiteral("--abbrev-ref")
             << QStringLiteral("HEAD");
        break;
    case 3:
        cwd = q->repoRoot;
        args << QStringLiteral("-C") << q->repoRoot
             << QStringLiteral("rev-list") << QStringLiteral("--left-right")
             << QStringLiteral("--count")
             << QStringLiteral("%1...origin/%1").arg(q->branch);
        break;
    default:
        finalize(q, GitStatus{});
        return;
    }

    q->proc = new QProcess(this);
    q->proc->setProgram(kGitProgram);
    q->proc->setArguments(args);
    q->proc->setProcessChannelMode(QProcess::MergedChannels);
    q->proc->setWorkingDirectory(cwd);

    connect(q->proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, q](int exitCode, QProcess::ExitStatus exitStatus) {
                onProcessFinished(q, exitCode, static_cast<int>(exitStatus));
            });
    // Falha de start (git ausente, etc.) -> NotInRepo gracioso.
    connect(q->proc, &QProcess::errorOccurred, this,
            [this, q](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart) {
                    GitStatus s;
                    s.state = GitStatus::State::NotInRepo;
                    finalize(q, s);
                }
            });

    q->timeout = new QTimer(this);
    q->timeout->setSingleShot(true);
    q->timeout->setInterval(kProcessTimeoutMsec);
    connect(q->timeout, &QTimer::timeout, this, [this, q]() { onTimeout(q); });
    q->timeout->start();

    q->proc->start();
}

void GitStatusService::onProcessFinished(PendingQuery* q, int exitCode, int exitStatus)
{
    if (!q || !q->proc) return;

    if (q->timeout) {
        q->timeout->stop();
        q->timeout->deleteLater();
        q->timeout = nullptr;
    }

    const QString output = QString::fromUtf8(q->proc->readAllStandardOutput()).trimmed();
    q->proc->deleteLater();
    q->proc = nullptr;

    const bool ok = (exitStatus == static_cast<int>(QProcess::NormalExit) && exitCode == 0);

    switch (q->stage) {
    case 0: {
        if (!ok || output.isEmpty()) {
            GitStatus s;
            s.state = GitStatus::State::NotInRepo;
            finalize(q, s);
            return;
        }
        q->repoRoot = output;
        q->relPath = QDir(q->repoRoot).relativeFilePath(q->filePath);
        q->stage = 1;
        startStage(q);
        return;
    }
    case 1:
        // Mesmo se exit != 0, saída vazia = Clean.
        q->state = ok ? parsePorcelain(output) : GitStatus::State::Clean;
        q->stage = 2;
        startStage(q);
        return;
    case 2:
        if (ok) {
            q->branch = output;
            // "HEAD" indica detached: pula etapa de ahead/behind.
            if (q->branch == QStringLiteral("HEAD") || q->branch.isEmpty()) {
                GitStatus s;
                s.state = q->state;
                s.branch = q->branch;
                s.repoRoot = q->repoRoot;
                finalize(q, s);
                return;
            }
        }
        q->stage = 3;
        startStage(q);
        return;
    case 3: {
        GitStatus s;
        s.state = q->state;
        s.branch = q->branch;
        s.repoRoot = q->repoRoot;
        if (ok) {
            int ahead = 0, behind = 0;
            if (parseAheadBehind(output, ahead, behind)) {
                s.ahead = ahead;
                s.behind = behind;
            }
        }
        // Sem upstream -> ahead/behind ficam zerados (fallback gracioso).
        finalize(q, s);
        return;
    }
    }
}

void GitStatusService::onTimeout(PendingQuery* q)
{
    if (!q) return;

    if (q->proc) {
        q->proc->disconnect(this);
        q->proc->kill();
        q->proc->waitForFinished(100);
        q->proc->deleteLater();
        q->proc = nullptr;
    }
    if (q->timeout) {
        q->timeout->deleteLater();
        q->timeout = nullptr;
    }

    GitStatus s;
    if (q->stage == 0) {
        s.state = GitStatus::State::NotInRepo;
    } else {
        s.state = q->state;
        s.branch = q->branch;
        s.repoRoot = q->repoRoot;
    }
    finalize(q, s);
}

void GitStatusService::finalize(PendingQuery* q, const GitStatus& status)
{
    if (!q) return;

    const QString path = q->filePath;

    // Atualiza cache + remove de in-flight ANTES de emitir, assim listeners
    // podem chamar cachedStatus dentro do slot.
    CacheEntry entry;
    entry.status = status;
    entry.fetchedAtMsec = QDateTime::currentMSecsSinceEpoch();
    m_cache.insert(path, entry);
    m_inFlight.remove(path);

    if (q->proc) {
        q->proc->disconnect(this);
        q->proc->deleteLater();
    }
    if (q->timeout) {
        q->timeout->stop();
        q->timeout->deleteLater();
    }
    delete q;

    emit statusReady(path, status);
}

GitStatus::State GitStatusService::parsePorcelain(const QString& output)
{
    if (output.isEmpty()) return GitStatus::State::Clean;

    // Porcelain v1: cada linha começa com 2 chars XY (X=index, Y=worktree).
    const QString firstLine = output.section(QChar('\n'), 0, 0);
    if (firstLine.size() < 2) return GitStatus::State::Clean;

    const QString xy = firstLine.left(2);
    const QChar X = xy.at(0);
    const QChar Y = xy.at(1);

    // Conflitos: qualquer 'U', ou AA/DD.
    if (X == QLatin1Char('U') || Y == QLatin1Char('U')
        || (X == QLatin1Char('A') && Y == QLatin1Char('A'))
        || (X == QLatin1Char('D') && Y == QLatin1Char('D'))) {
        return GitStatus::State::Conflicted;
    }
    if (xy == QStringLiteral("??")) return GitStatus::State::Untracked;
    if (xy == QStringLiteral("!!")) return GitStatus::State::NotInRepo;

    // Rename tem prioridade sobre as flags genéricas.
    if (X == QLatin1Char('R') || Y == QLatin1Char('R')) return GitStatus::State::Renamed;
    if (X == QLatin1Char('A'))                          return GitStatus::State::Added;
    if (X == QLatin1Char('D') || Y == QLatin1Char('D')) return GitStatus::State::Deleted;
    if (X == QLatin1Char('M') || Y == QLatin1Char('M')) return GitStatus::State::Modified;

    return GitStatus::State::Modified; // fallback "sujo" genérico
}

bool GitStatusService::parseAheadBehind(const QString& output, int& ahead, int& behind)
{
    // Formato esperado: "<ahead>\t<behind>".
    static const QRegularExpression sep(QStringLiteral("\\s+"));
    const QStringList parts = output.split(sep, Qt::SkipEmptyParts);
    if (parts.size() < 2) return false;

    bool okA = false, okB = false;
    const int a = parts.at(0).toInt(&okA);
    const int b = parts.at(1).toInt(&okB);
    if (!okA || !okB) return false;

    ahead = a;
    behind = b;
    return true;
}

QColor GitStatusService::stateColor(GitStatus::State s)
{
    switch (s) {
    case GitStatus::State::Clean:      return QColor(QStringLiteral("#2ea043"));
    case GitStatus::State::Untracked:  return QColor(QStringLiteral("#8b949e"));
    case GitStatus::State::Modified:   return QColor(QStringLiteral("#d29922"));
    case GitStatus::State::Added:      return QColor(QStringLiteral("#3fb950"));
    case GitStatus::State::Deleted:    return QColor(QStringLiteral("#f85149"));
    case GitStatus::State::Renamed:    return QColor(QStringLiteral("#a371f7"));
    case GitStatus::State::Conflicted: return QColor(QStringLiteral("#db6d28"));
    case GitStatus::State::NotInRepo:
    default:                           return QColor(Qt::transparent);
    }
}

QString GitStatusService::stateGlyph(GitStatus::State s)
{
    switch (s) {
    case GitStatus::State::Clean:      return QStringLiteral("○");
    case GitStatus::State::Untracked:  return QStringLiteral("?");
    case GitStatus::State::Modified:   return QStringLiteral("M");
    case GitStatus::State::Added:      return QStringLiteral("+");
    case GitStatus::State::Deleted:    return QStringLiteral("-");
    case GitStatus::State::Renamed:    return QStringLiteral("R");
    case GitStatus::State::Conflicted: return QStringLiteral("!");
    case GitStatus::State::NotInRepo:
    default:                           return QString();
    }
}
