// CodeFormatter — runs an external formatter on the editor selection (or the
// whole document) and writes the result back. Async via QProcess so the UI
// stays responsive. Errors are surfaced through QMessageBox + formatFailed().
// See CodeFormatter.h for the public contract.

#include "CodeFormatter.h"

#include "ScintillaEdit.h"
#include "ScintillaMessages.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QFileInfo>

using namespace Scintilla;

namespace {

constexpr const char* kOrg  = "clip52";
constexpr const char* kApp  = "notepadpp-qt";
constexpr const char* kGrp  = "Formatter";

QString installHintFor(const QString& tool)
{
    // Fedora-friendly hints (the user runs Fedora 43).
    if (tool == "clang-format") return QStringLiteral("dnf install clang-tools-extra");
    if (tool == "prettier")     return QStringLiteral("npm install -g prettier");
    if (tool == "black")        return QStringLiteral("dnf install python3-black  (ou pip install black)");
    if (tool == "gofmt")        return QStringLiteral("dnf install golang");
    if (tool == "rustfmt")      return QStringLiteral("dnf install rustfmt  (ou rustup component add rustfmt)");
    if (tool == "sqlformat")    return QStringLiteral("pip install sqlparse");
    return QStringLiteral("verifique se '%1' está no PATH").arg(tool);
}

} // namespace

CodeFormatter::CodeFormatter(QObject* parent)
    : QObject(parent)
{
    loadOverrides();
}

CodeFormatter::~CodeFormatter() = default;

// Default lexer -> tool mapping ---------------------------------------------
CodeFormatter::ToolSpec CodeFormatter::defaultSpec(const QString& lexerName)
{
    const QString lx = lexerName.toLower();
    ToolSpec s;

    if (lx == "cpp" || lx == "c") {
        s.tool = "clang-format";
        s.args = QStringList() << "--assume-filename=buffer.cpp";
    } else if (lx == "python") {
        s.tool = "black";
        s.args = QStringList() << "-" << "-q";
    } else if (lx == "javascript" || lx == "js") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.js";
    } else if (lx == "typescript" || lx == "ts") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.ts";
    } else if (lx == "phpscript" || lx == "php") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.php";
    } else if (lx == "json") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.json";
    } else if (lx == "html" || lx == "hypertext") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.html";
    } else if (lx == "xml") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.xml";
    } else if (lx == "css" || lx == "scss") {
        s.tool = "prettier";
        s.args = QStringList() << "--stdin-filepath" << "buffer.css";
    } else if (lx == "go") {
        s.tool = "gofmt";
        s.args = QStringList(); // reads stdin, writes stdout by default
    } else if (lx == "rust") {
        s.tool = "rustfmt";
        s.args = QStringList() << "--emit" << "stdout" << "--quiet";
    } else if (lx == "sql") {
        s.tool = "sqlformat";
        s.args = QStringList() << "-";
    }
    return s;
}

// Public lookup / override API ----------------------------------------------
QString CodeFormatter::toolForLexer(const QString& lexerName) const
{
    ToolSpec s;
    if (!resolveSpec(lexerName, s)) return QString();
    return s.tool;
}

void CodeFormatter::setToolForLexer(const QString& lexerName,
                                    const QString& toolPath,
                                    const QStringList& extraArgs)
{
    ToolSpec s;
    s.tool = toolPath;
    s.args = extraArgs;
    m_overrides.insert(lexerName.toLower(), s);
    saveOverride(lexerName.toLower(), s);
}

bool CodeFormatter::resolveSpec(const QString& lexerName, ToolSpec& out) const
{
    const QString key = lexerName.toLower();
    if (m_overrides.contains(key)) {
        out = m_overrides.value(key);
        return !out.tool.isEmpty();
    }
    out = defaultSpec(key);
    return !out.tool.isEmpty();
}

// Settings persistence ------------------------------------------------------
void CodeFormatter::loadOverrides()
{
    QSettings s(kOrg, kApp);
    s.beginGroup(kGrp);
    const QStringList lexers = s.childGroups();
    for (const QString& lx : lexers) {
        s.beginGroup(lx);
        ToolSpec spec;
        spec.tool = s.value("tool").toString();
        spec.args = s.value("args").toStringList();
        s.endGroup();
        if (!spec.tool.isEmpty())
            m_overrides.insert(lx.toLower(), spec);
    }
    s.endGroup();
}

void CodeFormatter::saveOverride(const QString& lexerName, const ToolSpec& spec)
{
    QSettings s(kOrg, kApp);
    s.beginGroup(kGrp);
    s.beginGroup(lexerName);
    s.setValue("tool", spec.tool);
    s.setValue("args", spec.args);
    s.endGroup();
    s.endGroup();
}

// Editor I/O helpers --------------------------------------------------------
QString CodeFormatter::lexerNameOf(ScintillaEdit* editor)
{
    if (!editor) return QString();
    QByteArray name(8192, 0);
    editor->send(SCI_GETLEXERLANGUAGE, 0,
                 reinterpret_cast<sptr_t>(name.data()));
    return QString::fromUtf8(name.constData()); // C-string in buffer

}

QByteArray CodeFormatter::readTargetText(ScintillaEdit* editor, bool& wholeDoc)
{
    wholeDoc = false;
    const sptr_t selStart = editor->send(SCI_GETSELECTIONSTART);
    const sptr_t selEnd   = editor->send(SCI_GETSELECTIONEND);

    if (selEnd > selStart) {
        const sptr_t len = editor->send(SCI_GETSELTEXT, 0, 0);
        QByteArray buf(static_cast<int>(len) + 1, 0);
        editor->send(SCI_GETSELTEXT, 0, reinterpret_cast<sptr_t>(buf.data()));
        if (!buf.isEmpty() && buf.endsWith('\0')) buf.chop(1);
        return buf;
    }

    wholeDoc = true;
    const sptr_t total = editor->send(SCI_GETLENGTH);
    QByteArray buf(static_cast<int>(total) + 1, 0);
    editor->send(SCI_GETTEXT, static_cast<uptr_t>(total + 1),
                 reinterpret_cast<sptr_t>(buf.data()));
    if (!buf.isEmpty() && buf.endsWith('\0')) buf.chop(1);
    return buf;
}

void CodeFormatter::writeFormattedText(ScintillaEdit* editor,
                                       const QByteArray& text,
                                       bool wholeDoc)
{
    // Single undo step so Ctrl+Z reverts the whole format.
    editor->send(SCI_BEGINUNDOACTION);
    if (wholeDoc) {
        editor->send(SCI_SETTEXT, 0,
                     reinterpret_cast<sptr_t>(text.constData()));
    } else {
        editor->send(SCI_REPLACESEL, 0,
                     reinterpret_cast<sptr_t>(text.constData()));
    }
    editor->send(SCI_ENDUNDOACTION);
}

// formatActiveEditor — the entry point --------------------------------------
void CodeFormatter::formatActiveEditor(ScintillaEdit* editor)
{
    if (!editor) {
        emit formatFailed(QString(), tr("Nenhum editor ativo."));
        return;
    }

    const QString lexer = lexerNameOf(editor);
    ToolSpec spec;
    if (!resolveSpec(lexer, spec)) {
        const QString msg = tr("Nenhum formatador mapeado para o lexer '%1'.")
                                .arg(lexer.isEmpty() ? tr("(nenhum)") : lexer);
        QMessageBox::information(nullptr, tr("Formatar Código"), msg);
        emit formatFailed(QString(), msg);
        return;
    }

    // Verify the executable exists before spawning.
    const QString exe = QFileInfo(spec.tool).isAbsolute()
        ? spec.tool
        : QStandardPaths::findExecutable(spec.tool);
    if (exe.isEmpty()) {
        const QString msg = tr("Ferramenta '%1' não encontrada — instale com: %2")
                                .arg(spec.tool, installHintFor(spec.tool));
        QMessageBox::warning(nullptr, tr("Formatar Código"), msg);
        emit formatFailed(spec.tool, msg);
        return;
    }

    bool wholeDoc = false;
    const QByteArray input = readTargetText(editor, wholeDoc);
    if (input.isEmpty()) {
        emit formatFailed(spec.tool, tr("Nada para formatar."));
        return;
    }

    // QPointer guards against the editor going away during the async run.
    QPointer<ScintillaEdit> editorGuard(editor);

    auto* proc = new QProcess(this);
    proc->setProgram(exe);
    proc->setArguments(spec.args);
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    // Buffer stdout incrementally; finished() drains the rest.
    QByteArray* outBuf = new QByteArray();
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, outBuf]() {
        outBuf->append(proc->readAllStandardOutput());
    });

    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, outBuf, spec](QProcess::ProcessError err) {
        Q_UNUSED(err);
        const QString msg = tr("Falha ao executar '%1': %2")
                                .arg(spec.tool, proc->errorString());
        QMessageBox::warning(nullptr, tr("Formatar Código"), msg);
        emit formatFailed(spec.tool, msg);
        delete outBuf;
        proc->deleteLater();
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, outBuf, spec, editorGuard, wholeDoc]
                  (int code, QProcess::ExitStatus status) {
        outBuf->append(proc->readAllStandardOutput());
        const QByteArray err = proc->readAllStandardError();

        if (status != QProcess::NormalExit || code != 0) {
            const QString msg = tr("'%1' retornou código %2.\n%3")
                                    .arg(spec.tool)
                                    .arg(code)
                                    .arg(QString::fromUtf8(err));
            QMessageBox::warning(nullptr, tr("Formatar Código"), msg);
            emit formatFailed(spec.tool, msg);
        } else if (editorGuard) {
            writeFormattedText(editorGuard.data(), *outBuf, wholeDoc);
            emit formatCompleted(spec.tool);
        } else {
            emit formatFailed(spec.tool, tr("Editor foi destruído antes do término."));
        }

        delete outBuf;
        proc->deleteLater();
    });

    proc->start();
    if (!proc->waitForStarted(3000)) {
        const QString msg = tr("Não foi possível iniciar '%1': %2")
                                .arg(spec.tool, proc->errorString());
        QMessageBox::warning(nullptr, tr("Formatar Código"), msg);
        emit formatFailed(spec.tool, msg);
        delete outBuf;
        proc->deleteLater();
        return;
    }

    proc->write(input);
    proc->closeWriteChannel(); // EOF lets the formatter wrap up.
}
