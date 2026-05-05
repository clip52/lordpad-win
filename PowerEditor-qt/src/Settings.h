#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>

class Settings {
public:
    static Settings& instance();

    // Editor preferences
    QString fontFamily() const;          void setFontFamily(const QString&);
    int fontSize() const;                void setFontSize(int);
    int tabWidth() const;                void setTabWidth(int);
    bool useSpaces() const;              void setUseSpaces(bool);
    bool showLineNumbers() const;        void setShowLineNumbers(bool);
    bool wordWrap() const;               void setWordWrap(bool);
    bool darkTheme() const;              void setDarkTheme(bool);

    // Recent files (most recent first; capped at 10)
    QStringList recentFiles() const;
    void addRecentFile(const QString& path);
    void clearRecentFiles();

    // Window state persistence
    QByteArray windowGeometry() const;   void setWindowGeometry(const QByteArray&);
    QByteArray windowState() const;      void setWindowState(const QByteArray&);

    void save();   // forces sync to backing store

private:
    Settings();
    ~Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    mutable QSettings m_settings;
};
