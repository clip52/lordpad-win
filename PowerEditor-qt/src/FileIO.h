#pragma once

#include <QString>
#include <QByteArray>

namespace FileIO {

struct LoadResult {
    QByteArray utf8;            // file contents converted to UTF-8 (BOM stripped)
    QString detectedEncoding;   // e.g. "UTF-8", "ISO-8859-1", "WINDOWS-1252", "UTF-16LE", "ASCII"
    bool hadBOM = false;
    bool ok = false;
    QString error;              // non-empty on failure
};

LoadResult readFile(const QString& path);
bool writeFile(const QString& path, const QByteArray& utf8, QString* error = nullptr);
QString detectEncoding(const QByteArray& sample);    // uses uchardet; "" if unable
QByteArray transcodeToUtf8(const QByteArray& bytes, const QString& fromEncoding);

} // namespace FileIO
