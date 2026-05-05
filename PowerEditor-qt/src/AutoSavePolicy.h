#pragma once
#include <QObject>
class QTimer;

class AutoSavePolicy : public QObject {
    Q_OBJECT
public:
    explicit AutoSavePolicy(QObject* parent = nullptr);

    bool isEnabled() const;
    int  intervalSeconds() const;
    bool saveOnlyNamedFiles() const;   // if true (default), tabs with empty path are skipped

    void setEnabled(bool b);
    void setIntervalSeconds(int seconds);   // clamped to [10, 3600]
    void setSaveOnlyNamedFiles(bool b);

    // Trigger an immediate save sweep (idempotent).
    void runNow();

signals:
    // Caller (MainWindow) connects this to its tab-save-each-dirty action.
    // Emitted on each timer tick (and from runNow()). Caller iterates tabs and saves.
    void autoSaveTick();

private slots:
    void onTimerTimeout();

private:
    QTimer* m_timer;
    bool    m_enabled;
    int     m_intervalSec;
    bool    m_onlyNamed;
};
