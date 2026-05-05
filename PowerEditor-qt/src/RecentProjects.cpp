#include "RecentProjects.h"

#include <QFileInfo>
#include <QSettings>

namespace {
constexpr int kMaxEntries = 10;
const char* const kSettingsKey = "recentProjects/list";
} // namespace

RecentProjects::RecentProjects(QObject* parent)
    : QObject(parent)
{
}

QStringList RecentProjects::list() const
{
    QSettings settings;
    QStringList stored = settings.value(QString::fromLatin1(kSettingsKey)).toStringList();

    QStringList surviving;
    surviving.reserve(stored.size());
    bool pruned = false;
    for (const QString& entry : stored) {
        if (!entry.isEmpty() && QFileInfo::exists(entry)) {
            surviving.append(entry);
        } else {
            pruned = true;
        }
    }

    if (surviving.size() > kMaxEntries) {
        surviving = surviving.mid(0, kMaxEntries);
        pruned = true;
    }

    if (pruned) {
        QSettings writable;
        writable.setValue(QString::fromLatin1(kSettingsKey), surviving);
    }

    return surviving;
}

void RecentProjects::use(const QString& folderPath)
{
    if (folderPath.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(folderPath)) {
        return;
    }

    QSettings settings;
    QStringList current = settings.value(QString::fromLatin1(kSettingsKey)).toStringList();

    // Remove existing instances (case-sensitive on Linux).
    current.removeAll(folderPath);

    // Prepend.
    current.prepend(folderPath);

    // Truncate to cap.
    if (current.size() > kMaxEntries) {
        current = current.mid(0, kMaxEntries);
    }

    settings.setValue(QString::fromLatin1(kSettingsKey), current);
    emit changed();
}

void RecentProjects::remove(const QString& folderPath)
{
    QSettings settings;
    QStringList current = settings.value(QString::fromLatin1(kSettingsKey)).toStringList();
    if (current.removeAll(folderPath) > 0) {
        settings.setValue(QString::fromLatin1(kSettingsKey), current);
        emit changed();
    } else {
        // Persist to keep behavior consistent even when nothing matched.
        settings.setValue(QString::fromLatin1(kSettingsKey), current);
        emit changed();
    }
}

void RecentProjects::clear()
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), QStringList{});
    emit changed();
}
