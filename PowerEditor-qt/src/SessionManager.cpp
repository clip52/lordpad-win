#include "SessionManager.h"

#include <QFileInfo>
#include <QSettings>

namespace {
constexpr const char* kRestoreKey    = "session/restoreOnStartup";
constexpr const char* kOpenFilesKey  = "session/openFiles";
constexpr const char* kActiveIdxKey  = "session/activeIndex";
constexpr int         kMaxEntries    = 50;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
}

bool SessionManager::restoreOnStartup() const
{
    QSettings s;
    return s.value(QString::fromLatin1(kRestoreKey), true).toBool();
}

void SessionManager::setRestoreOnStartup(bool b)
{
    QSettings s;
    s.setValue(QString::fromLatin1(kRestoreKey), b);
    s.sync();
    emit sessionChanged();
}

void SessionManager::saveSession(const QStringList& openFilePaths, int activeIndex)
{
    QStringList filtered;
    filtered.reserve(openFilePaths.size());
    for (const QString& path : openFilePaths) {
        if (path.isEmpty())
            continue;
        if (!QFileInfo::exists(path))
            continue;
        filtered.append(path);
        if (filtered.size() >= kMaxEntries)
            break;
    }

    int clampedIndex = 0;
    if (!filtered.isEmpty()) {
        if (activeIndex < 0)
            clampedIndex = 0;
        else if (activeIndex >= filtered.size())
            clampedIndex = filtered.size() - 1;
        else
            clampedIndex = activeIndex;
    }

    QSettings s;
    s.setValue(QString::fromLatin1(kOpenFilesKey), filtered);
    s.setValue(QString::fromLatin1(kActiveIdxKey), clampedIndex);
    s.sync();

    emit sessionChanged();
}

QStringList SessionManager::loadSession(int* activeIndex) const
{
    QSettings s;
    const QStringList list = s.value(QString::fromLatin1(kOpenFilesKey)).toStringList();
    if (activeIndex) {
        *activeIndex = s.value(QString::fromLatin1(kActiveIdxKey), 0).toInt();
    }
    return list;
}

void SessionManager::clearSession()
{
    QSettings s;
    s.remove(QString::fromLatin1(kOpenFilesKey));
    s.remove(QString::fromLatin1(kActiveIdxKey));
    s.sync();
    emit sessionChanged();
}
