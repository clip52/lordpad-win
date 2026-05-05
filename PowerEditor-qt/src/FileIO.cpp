#include "FileIO.h"

#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <QStringConverter>
#include <QStringList>

#include <uchardet/uchardet.h>

namespace FileIO {

namespace {

// Strip any leading BOM bytes for the given encoding name.
// Returns the byte offset to start reading from.
int bomOffsetFor(const QString& encoding)
{
    const QString e = encoding.toUpper();
    if (e == QStringLiteral("UTF-8")) return 3;
    if (e == QStringLiteral("UTF-16LE") || e == QStringLiteral("UTF-16BE")) return 2;
    if (e == QStringLiteral("UTF-32LE") || e == QStringLiteral("UTF-32BE")) return 4;
    return 0;
}

// Detect a BOM at the start of `bytes`. Returns the canonical encoding name,
// or empty string if no BOM was found. Note: UTF-32 BOMs must be checked
// before UTF-16LE because of the 0xFF 0xFE prefix overlap.
QString detectBOM(const QByteArray& bytes)
{
    const int n = bytes.size();
    const auto u = reinterpret_cast<const unsigned char*>(bytes.constData());

    // UTF-32LE: FF FE 00 00 (must be checked before UTF-16LE)
    if (n >= 4 && u[0] == 0xFF && u[1] == 0xFE && u[2] == 0x00 && u[3] == 0x00) {
        return QStringLiteral("UTF-32LE");
    }
    // UTF-32BE: 00 00 FE FF
    if (n >= 4 && u[0] == 0x00 && u[1] == 0x00 && u[2] == 0xFE && u[3] == 0xFF) {
        return QStringLiteral("UTF-32BE");
    }
    // UTF-8: EF BB BF
    if (n >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) {
        return QStringLiteral("UTF-8");
    }
    // UTF-16LE: FF FE
    if (n >= 2 && u[0] == 0xFF && u[1] == 0xFE) {
        return QStringLiteral("UTF-16LE");
    }
    // UTF-16BE: FE FF
    if (n >= 2 && u[0] == 0xFE && u[1] == 0xFF) {
        return QStringLiteral("UTF-16BE");
    }
    return QString();
}

// Map a (possibly all-caps / uchardet-style) encoding name to a form that
// QStringDecoder / QStringConverter is likely to recognise.
QString canonicaliseEncoding(const QString& enc)
{
    QString e = enc.trimmed();
    if (e.isEmpty()) return e;

    const QString upper = e.toUpper();

    // Direct mappings for common names produced by uchardet.
    if (upper == "UTF-8" || upper == "UTF8")                return QStringLiteral("UTF-8");
    if (upper == "UTF-16" || upper == "UTF16")              return QStringLiteral("UTF-16");
    if (upper == "UTF-16LE" || upper == "UTF16LE")          return QStringLiteral("UTF-16LE");
    if (upper == "UTF-16BE" || upper == "UTF16BE")          return QStringLiteral("UTF-16BE");
    if (upper == "UTF-32" || upper == "UTF32")              return QStringLiteral("UTF-32");
    if (upper == "UTF-32LE" || upper == "UTF32LE")          return QStringLiteral("UTF-32LE");
    if (upper == "UTF-32BE" || upper == "UTF32BE")          return QStringLiteral("UTF-32BE");
    if (upper == "ASCII" || upper == "US-ASCII")            return QStringLiteral("UTF-8");
    if (upper == "ISO-8859-1" || upper == "ISO8859-1" ||
        upper == "LATIN1"     || upper == "LATIN-1")        return QStringLiteral("ISO-8859-1");
    if (upper == "WINDOWS-1252" || upper == "CP1252" ||
        upper == "CP-1252")                                 return QStringLiteral("windows-1252");

    return e; // pass-through; QStringConverter will decide if it knows it.
}

// Try to obtain a QStringDecoder for the given encoding. Returns a default
// (invalid) decoder if the encoding is not recognised.
QStringDecoder makeDecoder(const QString& encoding)
{
    const QString canon = canonicaliseEncoding(encoding);
    if (canon.isEmpty()) {
        return QStringDecoder();
    }

    const QByteArray name = canon.toLatin1();

    // Try by name first (Qt6 accepts strings like "UTF-8", "ISO-8859-1",
    // "windows-1252", "UTF-16LE", etc.).
    QStringDecoder dec(name.constData());
    if (dec.isValid()) {
        return dec;
    }

    // Match against the list of available codecs case-insensitively.
    const auto codecs = QStringConverter::availableCodecs();
    for (const QString& c : codecs) {
        if (c.compare(canon, Qt::CaseInsensitive) == 0) {
            QStringDecoder named(c.toLatin1().constData());
            if (named.isValid()) return named;
        }
    }

    return QStringDecoder();
}

// Strict UTF-8 validator. Returns true iff every byte sequence in `bytes`
// matches a valid UTF-8 code point (no overlong forms, no surrogate halves,
// no trailing continuation bytes). uchardet sometimes mis-flags emoji-rich
// or 4-byte UTF-8 as Windows-1250 / Windows-1252 — this check lets us
// short-circuit detection when the file is clearly already valid UTF-8.
bool isStrictUtf8(const QByteArray& bytes)
{
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.constData());
    const auto* end = p + bytes.size();
    while (p < end) {
        unsigned char c = *p;
        if (c < 0x80) { ++p; continue; }
        int extra;
        unsigned int minCp;
        if      ((c & 0xE0) == 0xC0) { extra = 1; minCp = 0x80;    }
        else if ((c & 0xF0) == 0xE0) { extra = 2; minCp = 0x800;   }
        else if ((c & 0xF8) == 0xF0) { extra = 3; minCp = 0x10000; }
        else                         { return false; }
        if (p + extra >= end) return false;
        unsigned int cp = c & (0x7F >> (extra + 1));
        for (int i = 1; i <= extra; ++i) {
            unsigned char cc = p[i];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (cp < minCp) return false;                       // overlong
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;     // surrogate
        if (cp > 0x10FFFF) return false;                    // out of range
        p += extra + 1;
    }
    return true;
}

} // namespace

QString detectEncoding(const QByteArray& sample)
{
    if (sample.isEmpty()) {
        return QString();
    }

    uchardet_t ud = uchardet_new();
    if (!ud) {
        return QString();
    }

    QString result;
    const int rc = uchardet_handle_data(ud, sample.constData(), static_cast<size_t>(sample.size()));
    if (rc == 0) {
        uchardet_data_end(ud);
        const char* enc = uchardet_get_charset(ud);
        if (enc && *enc) {
            result = QString::fromLatin1(enc);
        }
    }
    uchardet_delete(ud);
    return result;
}

QByteArray transcodeToUtf8(const QByteArray& bytes, const QString& fromEncoding)
{
    if (bytes.isEmpty()) {
        return QByteArray();
    }

    const QString canon = canonicaliseEncoding(fromEncoding);

    // Skip any BOM that matches the source encoding so the decoder doesn't
    // emit a U+FEFF code point into the output.
    const int off = bomOffsetFor(canon);
    QByteArray payload = bytes;
    if (off > 0 && payload.size() >= off) {
        // Verify the BOM actually present (paranoia: caller may already have
        // stripped it). Strip only if the leading bytes match the expected BOM.
        const QString detected = detectBOM(payload);
        if (!detected.isEmpty() && canonicaliseEncoding(detected).compare(canon, Qt::CaseInsensitive) == 0) {
            payload = payload.mid(off);
        }
    }

    if (canon.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return payload;
    }

    QStringDecoder decoder = makeDecoder(canon);
    if (!decoder.isValid()) {
        // Unknown encoding: best-effort, treat as UTF-8.
        return payload;
    }

    QString decoded = decoder.decode(payload);
    return decoded.toUtf8();
}

LoadResult readFile(const QString& path)
{
    LoadResult r;
    r.hadBOM = false;
    r.ok = false;

    QFile f(path);
    if (!f.exists()) {
        r.error = QStringLiteral("File does not exist: %1").arg(path);
        return r;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("Cannot open '%1': %2").arg(path, f.errorString());
        return r;
    }

    const QByteArray raw = f.readAll();
    if (f.error() != QFileDevice::NoError) {
        r.error = QStringLiteral("Read error on '%1': %2").arg(path, f.errorString());
        f.close();
        return r;
    }
    f.close();

    // Empty file: well-defined result.
    if (raw.isEmpty()) {
        r.utf8 = QByteArray();
        r.detectedEncoding = QStringLiteral("UTF-8");
        r.hadBOM = false;
        r.ok = true;
        return r;
    }

    // Step 1: BOM detection.
    QString encoding = detectBOM(raw);
    if (!encoding.isEmpty()) {
        r.hadBOM = true;
    } else if (isStrictUtf8(raw)) {
        // Step 2a: if the file is already strictly valid UTF-8, trust it.
        // uchardet often mis-flags emoji-rich UTF-8 as Windows-1250/1252.
        encoding = QStringLiteral("UTF-8");
    } else {
        // Step 2b: uchardet fallback for non-UTF-8 content.
        const QString detected = detectEncoding(raw);
        const QString upper = detected.toUpper();
        if (detected.isEmpty() || upper == QStringLiteral("ASCII") || upper == QStringLiteral("US-ASCII")) {
            encoding = QStringLiteral("UTF-8");
        } else {
            encoding = detected;
        }
    }

    r.detectedEncoding = encoding;

    // Step 3: transcode (or strip BOM) to UTF-8.
    if (r.hadBOM) {
        const int off = bomOffsetFor(encoding);
        QByteArray body = (off > 0 && raw.size() >= off) ? raw.mid(off) : raw;

        if (encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
            r.utf8 = body;
        } else {
            QStringDecoder decoder = makeDecoder(encoding);
            if (decoder.isValid()) {
                QString decoded = decoder.decode(body);
                r.utf8 = decoded.toUtf8();
            } else {
                // Unknown BOM-marked encoding: best-effort treat as UTF-8.
                r.utf8 = body;
            }
        }
    } else {
        if (encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
            r.utf8 = raw;
        } else {
            r.utf8 = transcodeToUtf8(raw, encoding);
        }
    }

    r.ok = true;
    return r;
}

bool writeFile(const QString& path, const QByteArray& utf8, QString* error)
{
    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot open '%1' for writing: %2").arg(path, sf.errorString());
        }
        return false;
    }

    const qint64 want = utf8.size();
    const qint64 wrote = sf.write(utf8);
    if (wrote != want) {
        if (error) {
            *error = QStringLiteral("Short write on '%1': %2 of %3 bytes (%4)")
                         .arg(path)
                         .arg(wrote)
                         .arg(want)
                         .arg(sf.errorString());
        }
        sf.cancelWriting();
        return false;
    }

    if (!sf.commit()) {
        if (error) {
            *error = QStringLiteral("Failed to commit '%1': %2").arg(path, sf.errorString());
        }
        return false;
    }

    return true;
}

} // namespace FileIO
