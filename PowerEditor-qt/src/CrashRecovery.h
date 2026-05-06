#pragma once
// CrashRecovery - snapshot periódico de buffers modificados em disco.
// Na próxima inicialização detecta arquivos órfãos (PID dono morreu sem
// shutdown limpo) e oferece restauração ao usuário.

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <functional>

class QTimer;

struct RecoveryRecord {
    QString   recoveryFile;   // path absoluto do .recovery
    QString   originalPath;   // pode estar vazio (untitled)
    QString   encoding;       // "UTF-8", "UTF-16LE", etc.
    qint64    pid = 0;        // PID que escreveu o snapshot
    QDateTime modified;       // mtime do arquivo
};

class CrashRecovery : public QObject {
    Q_OBJECT
public:
    explicit CrashRecovery(QObject* parent = nullptr);
    ~CrashRecovery() override;

    // Cria diretório de recovery, escreve lockfile da sessão e dispara
    // o timer de snapshot (10s). Idempotente.
    void start();

    // Registra (ou reconfigura) um buffer pra ser snapshotado a cada tick.
    // bufferId deve ser estável durante toda a sessão.
    using ContentGetter = std::function<QByteArray()>;
    void registerBuffer(int bufferId,
                        const QString& originalPath,
                        const QString& encoding,
                        ContentGetter getter);

    // Remove um buffer do conjunto monitorado e apaga seu .recovery local.
    void unregisterBuffer(int bufferId);

    // Apaga lockfile e todos os snapshots desta sessão.
    // Chamado no fechamento limpo do app.
    void shutdownClean();

    // Retorna todos os .recovery encontrados cujo PID dono não está mais vivo.
    QList<RecoveryRecord> findOrphanRecoveries();

    // Após o usuário decidir (restaurar/descartar), apaga o .recovery.
    void consume(const RecoveryRecord& rec);

    // Caminho absoluto do diretório de recovery (criado se necessário).
    static QString recoveryDir();

private slots:
    void onTick();

private:
    struct Entry {
        QString       originalPath;
        QString       encoding;
        ContentGetter getter;
        uint          lastHash = 0;
        bool          everWritten = false;
    };

    QString recoveryFilePath(int bufferId) const;
    QString lockFilePath(qint64 pid) const;
    bool    writeAtomic(const QString& path, const QByteArray& data) const;
    bool    pidAlive(qint64 pid) const;
    void    snapshotOne(int bufferId, Entry& entry);
    bool    parseRecoveryHeader(const QString& path, RecoveryRecord& out) const;

    QTimer*           m_timer = nullptr;
    QHash<int, Entry> m_buffers;
    qint64            m_pid = 0;
    QString           m_lockPath;
    bool              m_started = false;
};
