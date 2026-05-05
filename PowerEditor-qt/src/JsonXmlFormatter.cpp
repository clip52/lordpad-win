// Requires Qt6::Xml — add to target_link_libraries
#include "JsonXmlFormatter.h"

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtXml/QDomDocument>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

#include "ScintillaEdit.h"

namespace JsonXmlFormatter {

namespace {

struct Range {
    int from;
    int to;
};

// Determine the target range: selection if non-empty, otherwise whole document.
Range targetRange(ScintillaEdit* editor)
{
    if (!editor) {
        return {0, 0};
    }
    if (editor->selectionEmpty()) {
        return {0, static_cast<int>(editor->length())};
    }
    const int from = static_cast<int>(editor->selectionStart());
    const int to   = static_cast<int>(editor->selectionEnd());
    return {from, to};
}

// Read raw bytes from the editor for [from, to). The Scintilla buffer is UTF-8.
QByteArray fetchRange(ScintillaEdit* editor, int from, int to)
{
    if (!editor || to <= from) {
        return QByteArray();
    }
    return editor->textRange(from, to);
}

// Replace [from, to) with `replacement`, wrapped in a single undo action.
void replaceRange(ScintillaEdit* editor, int from, int to, const QByteArray& replacement)
{
    editor->beginUndoAction();
    editor->setTargetRange(from, to);
    editor->replaceTarget(static_cast<sptr_t>(replacement.size()), replacement.constData());
    editor->endUndoAction();
}

void setError(QString* errorOut, const QString& message)
{
    if (errorOut) {
        *errorOut = message;
    }
}

// Strip whitespace-only content between tags but skip CDATA sections.
// Returns the minified text. The input is assumed to be well-formed enough to
// have already been validated by QDomDocument::setContent().
QByteArray minifyXmlBytes(const QByteArray& input)
{
    const QString src = QString::fromUtf8(input);
    QString out;
    out.reserve(src.size());

    static const QRegularExpression betweenTags(QStringLiteral(">[\\s]+<"));

    int i = 0;
    const int n = src.size();
    while (i < n) {
        const int cdataStart = src.indexOf(QStringLiteral("<![CDATA["), i);
        if (cdataStart < 0) {
            // No more CDATA: collapse whitespace between tags in the rest.
            QString tail = src.mid(i);
            tail.replace(betweenTags, QStringLiteral("><"));
            out += tail;
            break;
        }
        // Process the chunk before the CDATA block.
        QString head = src.mid(i, cdataStart - i);
        head.replace(betweenTags, QStringLiteral("><"));
        out += head;

        // Locate the end of the CDATA block and copy it verbatim.
        const int cdataEnd = src.indexOf(QStringLiteral("]]>"), cdataStart + 9);
        if (cdataEnd < 0) {
            // Malformed CDATA — copy the remainder verbatim and stop.
            out += src.mid(cdataStart);
            break;
        }
        const int afterCdata = cdataEnd + 3;
        out += src.mid(cdataStart, afterCdata - cdataStart);
        i = afterCdata;
    }

    return out.trimmed().toUtf8();
}

} // namespace

Result jsonPretty(ScintillaEdit* editor, QString* errorOut)
{
    if (!editor) {
        setError(errorOut, QStringLiteral("Invalid editor"));
        return Result::ParseError;
    }
    const Range r = targetRange(editor);
    const QByteArray bytes = fetchRange(editor, r.from, r.to);
    if (bytes.trimmed().isEmpty()) {
        return Result::Empty;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(errorOut,
                 QStringLiteral("JSON parse error at offset %1: %2")
                     .arg(parseError.offset)
                     .arg(parseError.errorString()));
        return Result::ParseError;
    }

    // Qt6 emits 4-space indentation; documented as the chosen behaviour.
    const QByteArray pretty = doc.toJson(QJsonDocument::Indented);
    replaceRange(editor, r.from, r.to, pretty);
    return Result::Ok;
}

Result jsonMinify(ScintillaEdit* editor, QString* errorOut)
{
    if (!editor) {
        setError(errorOut, QStringLiteral("Invalid editor"));
        return Result::ParseError;
    }
    const Range r = targetRange(editor);
    const QByteArray bytes = fetchRange(editor, r.from, r.to);
    if (bytes.trimmed().isEmpty()) {
        return Result::Empty;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(errorOut,
                 QStringLiteral("JSON parse error at offset %1: %2")
                     .arg(parseError.offset)
                     .arg(parseError.errorString()));
        return Result::ParseError;
    }

    const QByteArray compact = doc.toJson(QJsonDocument::Compact);
    replaceRange(editor, r.from, r.to, compact);
    return Result::Ok;
}

Result xmlPretty(ScintillaEdit* editor, QString* errorOut)
{
    if (!editor) {
        setError(errorOut, QStringLiteral("Invalid editor"));
        return Result::ParseError;
    }
    const Range r = targetRange(editor);
    const QByteArray bytes = fetchRange(editor, r.from, r.to);
    if (bytes.trimmed().isEmpty()) {
        return Result::Empty;
    }

    QDomDocument doc;
    QString errMsg;
    int errLine = 0;
    int errCol = 0;
    if (!doc.setContent(bytes, &errMsg, &errLine, &errCol)) {
        setError(errorOut,
                 QStringLiteral("XML parse error at line %1, column %2: %3")
                     .arg(errLine)
                     .arg(errCol)
                     .arg(errMsg));
        return Result::ParseError;
    }

    const QString pretty = doc.toString(2);
    replaceRange(editor, r.from, r.to, pretty.toUtf8());
    return Result::Ok;
}

Result xmlMinify(ScintillaEdit* editor, QString* errorOut)
{
    if (!editor) {
        setError(errorOut, QStringLiteral("Invalid editor"));
        return Result::ParseError;
    }
    const Range r = targetRange(editor);
    const QByteArray bytes = fetchRange(editor, r.from, r.to);
    if (bytes.trimmed().isEmpty()) {
        return Result::Empty;
    }

    // Validate via QDomDocument first so we can report parse errors.
    QDomDocument doc;
    QString errMsg;
    int errLine = 0;
    int errCol = 0;
    if (!doc.setContent(bytes, &errMsg, &errLine, &errCol)) {
        setError(errorOut,
                 QStringLiteral("XML parse error at line %1, column %2: %3")
                     .arg(errLine)
                     .arg(errCol)
                     .arg(errMsg));
        return Result::ParseError;
    }

    // Minify the original bytes so that comments, the XML declaration, and
    // CDATA sections are preserved (QDomDocument round-trips would lose some
    // of these or normalise them). CDATA content is left untouched.
    const QByteArray minified = minifyXmlBytes(bytes);
    replaceRange(editor, r.from, r.to, minified);
    return Result::Ok;
}

} // namespace JsonXmlFormatter
