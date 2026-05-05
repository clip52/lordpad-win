#include "AutoSavePolicy.h"

#include <QObject>
#include <QTimer>
#include <QSettings>

namespace {
constexpr int kMinIntervalSec = 10;
constexpr int kMaxIntervalSec = 3600;
constexpr int kDefaultIntervalSec = 60;

constexpr const char* kKeyEnabled    = "autoSave/enabled";
constexpr const char* kKeyIntervalSec = "autoSave/intervalSec";
constexpr const char* kKeyOnlyNamed  = "autoSave/onlyNamed";

int clampInterval(int seconds) {
    if (seconds < kMinIntervalSec) return kMinIntervalSec;
    if (seconds > kMaxIntervalSec) return kMaxIntervalSec;
    return seconds;
}
} // namespace

AutoSavePolicy::AutoSavePolicy(QObject* parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_enabled(false),
      m_intervalSec(kDefaultIntervalSec),
      m_onlyNamed(true)
{
    QSettings settings;
    m_enabled     = settings.value(kKeyEnabled, false).toBool();
    m_intervalSec = clampInterval(settings.value(kKeyIntervalSec, kDefaultIntervalSec).toInt());
    m_onlyNamed   = settings.value(kKeyOnlyNamed, true).toBool();

    m_timer->setInterval(m_intervalSec * 1000);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &AutoSavePolicy::onTimerTimeout);

    if (m_enabled) {
        m_timer->start();
    }
}

bool AutoSavePolicy::isEnabled() const {
    return m_enabled;
}

int AutoSavePolicy::intervalSeconds() const {
    return m_intervalSec;
}

bool AutoSavePolicy::saveOnlyNamedFiles() const {
    return m_onlyNamed;
}

void AutoSavePolicy::setEnabled(bool b) {
    m_enabled = b;

    QSettings settings;
    settings.setValue(kKeyEnabled, m_enabled);
    settings.sync();

    if (m_enabled) {
        if (!m_timer->isActive()) {
            m_timer->start();
        }
    } else {
        if (m_timer->isActive()) {
            m_timer->stop();
        }
    }
}

void AutoSavePolicy::setIntervalSeconds(int seconds) {
    m_intervalSec = clampInterval(seconds);

    QSettings settings;
    settings.setValue(kKeyIntervalSec, m_intervalSec);
    settings.sync();

    m_timer->setInterval(m_intervalSec * 1000);
}

void AutoSavePolicy::setSaveOnlyNamedFiles(bool b) {
    m_onlyNamed = b;

    QSettings settings;
    settings.setValue(kKeyOnlyNamed, m_onlyNamed);
    settings.sync();
}

void AutoSavePolicy::runNow() {
    emit autoSaveTick();
}

void AutoSavePolicy::onTimerTimeout() {
    emit autoSaveTick();
}
