#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    bool restoreOnStartup() const;
    void setRestoreOnStartup(bool b);

    // Save / load. Caller passes the current state.
    void saveSession(const QStringList& openFilePaths, int activeIndex);

    // Returns the previously saved file list. activeIndex is written via out parameter.
    QStringList loadSession(int* activeIndex = nullptr) const;

    void clearSession();

signals:
    void sessionChanged();
};
