#include "ColorPickerHelper.h"

#include <QColor>
#include <QColorDialog>
#include <QString>
#include <QRegularExpression>

#include "ScintillaEdit.h"

namespace ColorPickerHelper {

namespace {

enum class Format {
    HexRgb,    // #RRGGBB
    HexRgba,   // #RRGGBBAA
    RgbFunc,   // rgb(R, G, B)
    RgbaFunc,  // rgba(R, G, B, A)
};

// Clamp helper.
inline int clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

// Parse a single channel token from rgb()/rgba() — supports "255", "100%", and tolerates whitespace.
bool parseChannel(const QString& token, int& outValue /*0..255*/) {
    QString t = token.trimmed();
    if (t.isEmpty()) return false;
    bool isPercent = t.endsWith(QLatin1Char('%'));
    if (isPercent) t.chop(1);
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok) return false;
    if (isPercent) {
        v = (v / 100.0) * 255.0;
    }
    int iv = static_cast<int>(v + 0.5);
    outValue = clamp255(iv);
    return true;
}

// Parse alpha from rgba() — accepts "0..1" float, "0..100%", or 0..255 integer (we treat ambiguous
// integers >1 as 0..255 only if they are an integer with no decimal; safer: per CSS, alpha is 0..1
// or percent. We'll follow that strictly: float 0..1, or percent.)
bool parseAlpha(const QString& token, int& outAlpha /*0..255*/) {
    QString t = token.trimmed();
    if (t.isEmpty()) return false;
    bool isPercent = t.endsWith(QLatin1Char('%'));
    if (isPercent) t.chop(1);
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok) return false;
    if (isPercent) {
        v = v / 100.0;
    }
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    outAlpha = clamp255(static_cast<int>(v * 255.0 + 0.5));
    return true;
}

bool tryParseColor(const QString& sIn, QColor& out, Format& format) {
    const QString s = sIn.trimmed();
    if (s.isEmpty()) return false;

    // Hex forms.
    if (s.startsWith(QLatin1Char('#'))) {
        const QString hex = s.mid(1);
        // Validate hex chars.
        static const QRegularExpression hexOnly(QStringLiteral("^[0-9A-Fa-f]+$"));
        if (!hexOnly.match(hex).hasMatch()) return false;

        if (hex.length() == 3) {
            // #RGB shorthand → expand.
            const QChar r = hex.at(0);
            const QChar g = hex.at(1);
            const QChar b = hex.at(2);
            const QString expanded = QStringLiteral("#%1%1%2%2%3%3").arg(r).arg(g).arg(b);
            QColor c(expanded);
            if (!c.isValid()) return false;
            out = c;
            format = Format::HexRgb;
            return true;
        }
        if (hex.length() == 6) {
            QColor c(s);
            if (!c.isValid()) return false;
            out = c;
            format = Format::HexRgb;
            return true;
        }
        if (hex.length() == 8) {
            // #RRGGBBAA — Qt's QColor accepts #AARRGGBB style with setNamedColor for "#AARRGGBB",
            // but our spec is RR GG BB AA. Parse manually.
            bool okR = false, okG = false, okB = false, okA = false;
            int r = hex.mid(0, 2).toInt(&okR, 16);
            int g = hex.mid(2, 2).toInt(&okG, 16);
            int b = hex.mid(4, 2).toInt(&okB, 16);
            int a = hex.mid(6, 2).toInt(&okA, 16);
            if (!(okR && okG && okB && okA)) return false;
            QColor c(r, g, b, a);
            if (!c.isValid()) return false;
            out = c;
            format = Format::HexRgba;
            return true;
        }
        return false;
    }

    // rgb(...) / rgba(...) forms.
    static const QRegularExpression rgbRe(
        QStringLiteral("^rgb\\s*\\(\\s*([^,]+)\\s*,\\s*([^,]+)\\s*,\\s*([^,\\)]+)\\s*\\)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rgbaRe(
        QStringLiteral("^rgba\\s*\\(\\s*([^,]+)\\s*,\\s*([^,]+)\\s*,\\s*([^,]+)\\s*,\\s*([^,\\)]+)\\s*\\)$"),
        QRegularExpression::CaseInsensitiveOption);

    {
        const auto m = rgbaRe.match(s);
        if (m.hasMatch()) {
            int r = 0, g = 0, b = 0, a = 255;
            if (!parseChannel(m.captured(1), r)) return false;
            if (!parseChannel(m.captured(2), g)) return false;
            if (!parseChannel(m.captured(3), b)) return false;
            if (!parseAlpha(m.captured(4), a)) return false;
            QColor c(r, g, b, a);
            if (!c.isValid()) return false;
            out = c;
            format = Format::RgbaFunc;
            return true;
        }
    }
    {
        const auto m = rgbRe.match(s);
        if (m.hasMatch()) {
            int r = 0, g = 0, b = 0;
            if (!parseChannel(m.captured(1), r)) return false;
            if (!parseChannel(m.captured(2), g)) return false;
            if (!parseChannel(m.captured(3), b)) return false;
            QColor c(r, g, b);
            if (!c.isValid()) return false;
            out = c;
            format = Format::RgbFunc;
            return true;
        }
    }

    return false;
}

QString formatColor(const QColor& c, Format format) {
    const int r = c.red();
    const int g = c.green();
    const int b = c.blue();
    const int a = c.alpha();
    switch (format) {
        case Format::HexRgb:
            return QStringLiteral("#%1%2%3")
                .arg(r, 2, 16, QLatin1Char('0'))
                .arg(g, 2, 16, QLatin1Char('0'))
                .arg(b, 2, 16, QLatin1Char('0'))
                .toUpper();
        case Format::HexRgba:
            return QStringLiteral("#%1%2%3%4")
                .arg(r, 2, 16, QLatin1Char('0'))
                .arg(g, 2, 16, QLatin1Char('0'))
                .arg(b, 2, 16, QLatin1Char('0'))
                .arg(a, 2, 16, QLatin1Char('0'))
                .toUpper();
        case Format::RgbFunc:
            return QStringLiteral("rgb(%1, %2, %3)").arg(r).arg(g).arg(b);
        case Format::RgbaFunc: {
            const double af = a / 255.0;
            return QStringLiteral("rgba(%1, %2, %3, %4)")
                .arg(r)
                .arg(g)
                .arg(b)
                .arg(QString::number(af, 'f', 2));
        }
    }
    // Unreachable.
    return QStringLiteral("#%1%2%3")
        .arg(r, 2, 16, QLatin1Char('0'))
        .arg(g, 2, 16, QLatin1Char('0'))
        .arg(b, 2, 16, QLatin1Char('0'))
        .toUpper();
}

} // namespace

bool pickAndReplace(ScintillaEdit* editor, QWidget* dialogParent) {
    if (!editor) return false;

    // Read selection.
    const QByteArray selBytes = editor->getSelText();
    const QString selText = QString::fromUtf8(selBytes);
    const bool hasSelection = !editor->selectionEmpty();

    // Determine initial color and format.
    QColor initial(0xFFFFFF);
    Format format = Format::HexRgb;

    if (hasSelection) {
        QColor parsed;
        Format parsedFormat = Format::HexRgb;
        if (tryParseColor(selText, parsed, parsedFormat)) {
            initial = parsed;
            format = parsedFormat;
        }
        // If parse failed, keep defaults (white, HexRgb) but still treat selection
        // as the replacement target.
    }

    // Open dialog.
    QColorDialog::ColorDialogOptions options = QColorDialog::ShowAlphaChannel;
    const QColor picked = QColorDialog::getColor(initial,
                                                 dialogParent,
                                                 QStringLiteral("Pick Color"),
                                                 options);
    if (!picked.isValid()) {
        // User cancelled.
        return false;
    }

    const QString replacement = formatColor(picked, format);
    const QByteArray replacementUtf8 = replacement.toUtf8();

    if (hasSelection) {
        // Replace selection and re-select the inserted text.
        const sptr_t selStart = editor->selectionStart();
        editor->beginUndoAction();
        editor->replaceSel(replacementUtf8.constData());
        editor->endUndoAction();

        // Re-select the inserted text (length in bytes for Scintilla).
        const sptr_t newEnd = selStart + static_cast<sptr_t>(replacementUtf8.size());
        editor->setSel(selStart, newEnd);
    } else {
        // Insert at caret; leave caret after inserted text.
        const sptr_t caret = editor->currentPos();
        editor->beginUndoAction();
        editor->insertText(caret, replacementUtf8.constData());
        editor->endUndoAction();
        const sptr_t newCaret = caret + static_cast<sptr_t>(replacementUtf8.size());
        editor->gotoPos(newCaret);
    }

    return true;
}

} // namespace ColorPickerHelper
