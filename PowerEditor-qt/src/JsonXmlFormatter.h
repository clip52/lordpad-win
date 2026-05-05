#pragma once

#include <QString>

class ScintillaEdit;

namespace JsonXmlFormatter {

enum class Result { Ok, Empty, ParseError };

// Pretty-print JSON in selection (or whole buffer if no selection).
// Note: Qt6's QJsonDocument emits 4-space indentation; that is preserved here.
Result jsonPretty(ScintillaEdit* editor, QString* errorOut = nullptr);

// Minify JSON in selection (or whole buffer if no selection).
Result jsonMinify(ScintillaEdit* editor, QString* errorOut = nullptr);

// Pretty-print XML in selection (or whole buffer). Indent = 2 spaces.
Result xmlPretty(ScintillaEdit* editor, QString* errorOut = nullptr);

// Minify XML (collapse whitespace between tags) in selection (or whole buffer).
Result xmlMinify(ScintillaEdit* editor, QString* errorOut = nullptr);

} // namespace JsonXmlFormatter
