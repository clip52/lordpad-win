#pragma once
#include <QObject>
#include <QString>
#include <QHash>

class QFileSystemWatcher;
class QTimer;

class ExternalFileWatcher : public QObject {
    Q_OBJECT
public:
    explicit ExternalFileWatcher(QObject* parent = nullptr);

    // Start watching a file path. The token returned identifies this watch entry
    // (use it to stop watching). If the same path is added again, returns the
    // existing token without re-adding.
    void watch(const QString& path);

    // Stop watching. No-op if not currently watched.
    void unwatch(const QString& path);

    // Call when our app saves the file: silences the next change notification
    // so we don't prompt the user on our own writes.
    void notifyOurWrite(const QString& path);

signals:
    // Fired when a watched file changes on disk and the change wasn't from our save.
    void fileChangedExternally(const QString& path);

    // Fired when a watched file disappears (unlink, rename out).
    void fileRemovedExternally(const QString& path);

private slots:
    void onFileChanged(const QString& path);
    void onVerifyTick();

private:
    void recordSnapshot(const QString& path);

    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_verifyTimer = nullptr;

    // Per-path bookkeeping.
    QHash<QString, qint64> m_lastMtime;   // last known modification time (msecs since epoch)
    QHash<QString, qint64> m_lastSize;    // last known file size in bytes
    QHash<QString, qint64> m_lastWriteOurs; // msecs-since-epoch of our own most recent save
};
