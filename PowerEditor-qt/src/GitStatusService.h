#pragma once
//
// GitStatusService
// ----------------
// Serviço assíncrono que consulta `git` (via QProcess) para descobrir o estado
// de um arquivo no repositório (untracked, modified, clean, etc.) e o estado
// do branch corrente (ahead/behind do upstream).
//
// API pública resumida:
//   - queryStatus(path): dispara consulta assíncrona; o resultado vem em
//     statusReady(path, status).
//   - cachedStatus(path): retorna o último resultado conhecido sem refazer
//     a query (ou um GitStatus default com state==NotInRepo se nada foi
//     consultado ainda).
//   - refreshAll(): invalida o cache para forçar nova consulta.
//
// Notas de design:
//   - Coalescência: se já existe uma query em voo para o mesmo path, novas
//     chamadas a queryStatus são ignoradas até a primeira terminar.
//   - Timeout de 1500 ms por etapa; se um comando demorar demais ele é morto
//     e o resultado é reportado com fallback gracioso.
//   - Cache TTL de 5 s: entradas mais antigas são tratadas como inválidas.
//

#include <QObject>
#include <QString>
#include <QHash>
#include <QColor>
#include <QDateTime>

class QProcess;
class QTimer;

struct GitStatus {
    enum class State {
        NotInRepo,
        Clean,
        Untracked,
        Modified,
        Added,
        Deleted,
        Renamed,
        Conflicted
    };

    State state = State::NotInRepo;
    QString branch;       // nome do branch (ou vazio se detached/desconhecido)
    int ahead = 0;        // commits à frente do upstream
    int behind = 0;       // commits atrás do upstream
    QString repoRoot;     // toplevel do repo (vazio quando NotInRepo)
};

class GitStatusService : public QObject {
    Q_OBJECT
public:
    explicit GitStatusService(QObject* parent = nullptr);
    ~GitStatusService() override;

    // Solicita status assíncrono. Emite statusReady quando pronto.
    // Se já houver uma query em voo para o mesmo path, a chamada é ignorada.
    void queryStatus(const QString& filePath);

    // Lookup do último status conhecido sem refazer a query.
    // Retorna um GitStatus default (NotInRepo) se nada foi consultado ainda.
    GitStatus cachedStatus(const QString& filePath) const;

    // Invalida todo o cache (próximas chamadas a queryStatus farão fetch real).
    void refreshAll();

    // Helpers visuais para uso como badge na statusbar / tab icon.
    static QColor stateColor(GitStatus::State s);
    static QString stateGlyph(GitStatus::State s); // 1 char: ●○+M-?R!

signals:
    void statusReady(const QString& filePath, const GitStatus& status);

private:
    // Estado por consulta em andamento. Cada query passa por 3 etapas
    // sequenciais: rev-parse (toplevel) -> status --porcelain -> branch info.
    struct PendingQuery {
        QString filePath;     // path original solicitado
        QString workingDir;   // dirname(filePath) — usado em git -C
        QString repoRoot;     // toplevel resolvido na etapa 1
        QString relPath;      // path relativo ao repoRoot
        QString branch;       // resolvido na etapa 3a
        GitStatus::State state = GitStatus::State::NotInRepo;
        int stage = 0;        // 0=revparse, 1=status, 2=branchName, 3=aheadBehind
        QProcess* proc = nullptr;
        QTimer* timeout = nullptr;
    };

    void startStage(PendingQuery* q);
    void onProcessFinished(PendingQuery* q, int exitCode, int exitStatus);
    void onTimeout(PendingQuery* q);
    void finalize(PendingQuery* q, const GitStatus& status);

    static GitStatus::State parsePorcelain(const QString& output);
    static bool parseAheadBehind(const QString& output, int& ahead, int& behind);

    // Cache: path -> (status, timestamp do fetch).
    struct CacheEntry {
        GitStatus status;
        qint64 fetchedAtMsec = 0;
    };
    QHash<QString, CacheEntry> m_cache;

    // Conjunto de paths com query em voo (para coalescência).
    QHash<QString, PendingQuery*> m_inFlight;
};
