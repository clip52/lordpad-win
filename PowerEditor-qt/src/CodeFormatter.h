// CodeFormatter — runs external formatters (clang-format, prettier, black, gofmt,
// rustfmt, ...) over the current selection (or the whole document when empty)
// and replaces the editor content with the tool's stdout. Non-blocking: uses
// QProcess with start() + finished() so the UI stays responsive.
//
// Part of the notepadpp-qt port. Persistence lives in QSettings under
// ("clip52", "notepadpp-qt"), group "Formatter".

#ifndef CODEFORMATTER_H
#define CODEFORMATTER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QPointer>

class ScintillaEdit;
class QProcess;

class CodeFormatter : public QObject
{
    Q_OBJECT
public:
    explicit CodeFormatter(QObject* parent = nullptr);
    ~CodeFormatter() override;

    // Format the active editor's selection (or the full document when there is
    // no selection) using the default tool mapped to the current lexer.
    void formatActiveEditor(ScintillaEdit* editor);

    // Returns the executable name (or absolute path) configured for the given
    // lexer, or an empty QString if no mapping exists.
    QString toolForLexer(const QString& lexerName) const;

    // Override the tool/args for a lexer. Persists in QSettings.
    void setToolForLexer(const QString& lexerName,
                         const QString& toolPath,
                         const QStringList& extraArgs);

signals:
    void formatFailed(const QString& tool, const QString& message);
    void formatCompleted(const QString& tool);

private:
    struct ToolSpec {
        QString tool;          // executable name or absolute path
        QStringList args;      // arguments (may contain placeholders such as
                               // "%FILEPATH%" replaced with a synthetic name)
    };

    // Resolve the ToolSpec for a lexer: user override (QSettings) wins over
    // the built-in default table. Returns false if nothing is mapped.
    bool resolveSpec(const QString& lexerName, ToolSpec& out) const;

    // Built-in default mapping (lexer name -> tool + args).
    static ToolSpec defaultSpec(const QString& lexerName);

    // Returns the lexer name from the editor (SCI_GETLEXERLANGUAGE).
    static QString lexerNameOf(ScintillaEdit* editor);

    // Reads the current selection text. If empty, falls back to the whole doc;
    // 'wholeDoc' is set accordingly so we know how to write results back.
    static QByteArray readTargetText(ScintillaEdit* editor, bool& wholeDoc);

    // Writes the formatted text back to the editor.
    static void writeFormattedText(ScintillaEdit* editor,
                                   const QByteArray& text,
                                   bool wholeDoc);

    // Settings helpers.
    void loadOverrides();
    void saveOverride(const QString& lexerName, const ToolSpec& spec);

    // Per-lexer overrides loaded at construction (and refreshed on writes).
    QHash<QString, ToolSpec> m_overrides;
};

#endif // CODEFORMATTER_H
