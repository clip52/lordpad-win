#include "ExternalFileWatcher.h"

#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QTimer>
#include <QDateTime>

#include <limits>

namespace {
// Window during which a change notification is attributed to our own save.
constexpr qint64 kOurWriteSilenceMsec = 1500;
// Verify pass cadence: re-add watches that QFileSystemWatcher dropped (e.g. save-by-rename).
constexpr int kVerifyIntervalMsec = 5000;
// Delay before re-adding a path that briefly vanished.
constexpr int kReAddDelayMsec = 500;
} // namespace

ExternalFileWatcher::ExternalFileWatcher(QObject* parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_verifyTimer(new QTimer(this))
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ExternalFileWatcher::onFileChanged);

    m_verifyTimer->setInterval(kVerifyIntervalMsec);
    m_verifyTimer->setSingleShot(false);
    connect(m_verifyTimer, &QTimer::timeout,
            this, &ExternalFileWatcher::onVerifyTick);
    m_verifyTimer->start();
}

void ExternalFileWatcher::watch(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    // Idempotent: if QFileSystemWatcher already tracks it, leave bookkeeping alone.
    const QStringList currentlyWatched = m_watcher->files();
    if (currentlyWatched.contains(path)) {
        return;
    }

    if (m_watcher->addPath(path)) {
        recordSnapshot(path);
    } else {
        // addPath can fail if the file doesn't exist yet; still keep bookkeeping
        // so the verify tick can pick it up once it appears. We mark its presence
        // by inserting default snapshot values.
        if (!m_lastMtime.contains(path)) {
            m_lastMtime.insert(path, 0);
            m_lastSize.insert(path, 0);
        }
    }
}

void ExternalFileWatcher::unwatch(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    if (m_watcher->files().contains(path)) {
        m_watcher->removePath(path);
    }
    m_lastMtime.remove(path);
    m_lastSize.remove(path);
    m_lastWriteOurs.remove(path);
}

void ExternalFileWatcher::notifyOurWrite(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    m_lastWriteOurs.insert(path, QDateTime::currentMSecsSinceEpoch());

    // Refresh stored snapshot so subsequent external changes are detected
    // against the post-save state rather than the pre-save state.
    QFileInfo fi(path);
    if (fi.exists()) {
        m_lastMtime.insert(path, fi.lastModified().toMSecsSinceEpoch());
        m_lastSize.insert(path, fi.size());
    }
}

void ExternalFileWatcher::onFileChanged(const QString& path)
{
    QFileInfo fi(path);

    if (!fi.exists()) {
        emit fileRemovedExternally(path);
        // QFileSystemWatcher auto-removes vanished paths; some editors recreate
        // the file shortly after a save-by-rename, so try to re-add it.
        QTimer::singleShot(kReAddDelayMsec, this, [this, path]() {
            QFileInfo recheck(path);
            if (recheck.exists() && !m_watcher->files().contains(path)) {
                if (m_watcher->addPath(path)) {
                    recordSnapshot(path);
                }
            }
        });
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ourLast = m_lastWriteOurs.value(path, std::numeric_limits<qint64>::min() / 2);
    if ((now - ourLast) < kOurWriteSilenceMsec) {
        // Refresh snapshot to reflect our own write so a later real external
        // change is still detected as "different".
        m_lastMtime.insert(path, fi.lastModified().toMSecsSinceEpoch());
        m_lastSize.insert(path, fi.size());
        return;
    }

    const qint64 curMtime = fi.lastModified().toMSecsSinceEpoch();
    const qint64 curSize  = fi.size();

    const qint64 prevMtime = m_lastMtime.value(path, -1);
    const qint64 prevSize  = m_lastSize.value(path, -1);

    if (curMtime == prevMtime && curSize == prevSize) {
        // Spurious / duplicate notification (some editors trigger 2 events on save-as-replace).
        return;
    }

    m_lastMtime.insert(path, curMtime);
    m_lastSize.insert(path, curSize);

    emit fileChangedExternally(path);
}

void ExternalFileWatcher::onVerifyTick()
{
    // Save-by-rename causes QFileSystemWatcher to silently drop the path. Walk
    // every path we still consider watched (tracked in m_lastMtime) and re-add
    // any that have fallen off the watcher but still exist on disk.
    const QStringList watcherFiles = m_watcher->files();
    const QList<QString> tracked = m_lastMtime.keys();
    for (const QString& path : tracked) {
        if (watcherFiles.contains(path)) {
            continue;
        }
        QFileInfo fi(path);
        if (!fi.exists()) {
            continue;
        }
        if (m_watcher->addPath(path)) {
            // After a rename-replace, the new inode may differ; refresh snapshot
            // and surface the change to the caller if it actually differs.
            const qint64 curMtime = fi.lastModified().toMSecsSinceEpoch();
            const qint64 curSize  = fi.size();
            const qint64 prevMtime = m_lastMtime.value(path, -1);
            const qint64 prevSize  = m_lastSize.value(path, -1);

            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 ourLast = m_lastWriteOurs.value(path, std::numeric_limits<qint64>::min() / 2);
            const bool ourOwnWrite = (now - ourLast) < kOurWriteSilenceMsec;

            m_lastMtime.insert(path, curMtime);
            m_lastSize.insert(path, curSize);

            if (!ourOwnWrite && (curMtime != prevMtime || curSize != prevSize)) {
                emit fileChangedExternally(path);
            }
        }
    }
}

void ExternalFileWatcher::recordSnapshot(const QString& path)
{
    QFileInfo fi(path);
    if (fi.exists()) {
        m_lastMtime.insert(path, fi.lastModified().toMSecsSinceEpoch());
        m_lastSize.insert(path, fi.size());
    } else {
        m_lastMtime.insert(path, 0);
        m_lastSize.insert(path, 0);
    }
}
