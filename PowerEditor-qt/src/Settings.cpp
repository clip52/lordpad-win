#include "Settings.h"

namespace {
constexpr int kMaxRecentFiles = 10;

constexpr auto kKeyFontFamily       = "editor/fontFamily";
constexpr auto kKeyFontSize         = "editor/fontSize";
constexpr auto kKeyTabWidth         = "editor/tabWidth";
constexpr auto kKeyUseSpaces        = "editor/useSpaces";
constexpr auto kKeyShowLineNumbers  = "editor/showLineNumbers";
constexpr auto kKeyWordWrap         = "editor/wordWrap";
constexpr auto kKeyDarkTheme        = "ui/darkTheme";
constexpr auto kKeyRecentFiles      = "ui/recentFiles";
constexpr auto kKeyWindowGeometry   = "ui/windowGeometry";
constexpr auto kKeyWindowState      = "ui/windowState";
} // namespace

Settings::Settings() = default;

Settings& Settings::instance() {
    static Settings s;
    return s;
}

// Editor preferences -----------------------------------------------------------

QString Settings::fontFamily() const {
    return m_settings.value(kKeyFontFamily, QStringLiteral("Monospace")).toString();
}

void Settings::setFontFamily(const QString& v) {
    m_settings.setValue(kKeyFontFamily, v);
}

int Settings::fontSize() const {
    return m_settings.value(kKeyFontSize, 11).toInt();
}

void Settings::setFontSize(int v) {
    m_settings.setValue(kKeyFontSize, v);
}

int Settings::tabWidth() const {
    return m_settings.value(kKeyTabWidth, 4).toInt();
}

void Settings::setTabWidth(int v) {
    m_settings.setValue(kKeyTabWidth, v);
}

bool Settings::useSpaces() const {
    return m_settings.value(kKeyUseSpaces, true).toBool();
}

void Settings::setUseSpaces(bool v) {
    m_settings.setValue(kKeyUseSpaces, v);
}

bool Settings::showLineNumbers() const {
    return m_settings.value(kKeyShowLineNumbers, true).toBool();
}

void Settings::setShowLineNumbers(bool v) {
    m_settings.setValue(kKeyShowLineNumbers, v);
}

bool Settings::wordWrap() const {
    return m_settings.value(kKeyWordWrap, false).toBool();
}

void Settings::setWordWrap(bool v) {
    m_settings.setValue(kKeyWordWrap, v);
}

bool Settings::darkTheme() const {
    return m_settings.value(kKeyDarkTheme, false).toBool();
}

void Settings::setDarkTheme(bool v) {
    m_settings.setValue(kKeyDarkTheme, v);
}

// Recent files ----------------------------------------------------------------

QStringList Settings::recentFiles() const {
    return m_settings.value(kKeyRecentFiles, QStringList{}).toStringList();
}

void Settings::addRecentFile(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    m_settings.setValue(kKeyRecentFiles, files);
}

void Settings::clearRecentFiles() {
    m_settings.setValue(kKeyRecentFiles, QStringList{});
}

// Window state ----------------------------------------------------------------

QByteArray Settings::windowGeometry() const {
    return m_settings.value(kKeyWindowGeometry, QByteArray{}).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& v) {
    m_settings.setValue(kKeyWindowGeometry, v);
}

QByteArray Settings::windowState() const {
    return m_settings.value(kKeyWindowState, QByteArray{}).toByteArray();
}

void Settings::setWindowState(const QByteArray& v) {
    m_settings.setValue(kKeyWindowState, v);
}

// Persistence ------------------------------------------------------------------

void Settings::save() {
    m_settings.sync();
}
