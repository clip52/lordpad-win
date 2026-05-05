#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class RecentProjects : public QObject {
    Q_OBJECT
public:
    explicit RecentProjects(QObject* parent = nullptr);

    // List of recent project folder paths (most recent first), capped at 10.
    QStringList list() const;

    // Bring a folder to the top of the list. Drops duplicates and non-existent paths.
    void use(const QString& folderPath);

    // Remove a single entry.
    void remove(const QString& folderPath);

    // Clear all entries.
    void clear();

signals:
    void changed();
};
