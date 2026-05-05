#include "CssPreviewPane.h"

#include <QDialog>
#include <QTextBrowser>
#include <QToolBar>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QTimer>
#include <QByteArray>
#include <QString>

#include "ScintillaEdit.h"

namespace {
constexpr const char* kDefaultSample =
    "<h1>Heading 1</h1>\n"
    "<h2>Heading 2</h2>\n"
    "<p>This is a paragraph with some <strong>bold</strong> and <em>italic</em> text. "
    "<a href=\"#\">A link</a> looks like this.</p>\n"
    "<ul><li>List item one</li><li>List item two</li><li>List item three</li></ul>\n"
    "<blockquote>A blockquote is shown like this.</blockquote>\n"
    "<pre><code>code { color: #888; }</code></pre>\n"
    "<p class=\"highlight\">A paragraph with class=\"highlight\".</p>\n"
    "<div class=\"card\"><h3>Card title</h3><p>Card body text.</p></div>\n"
    "<button>A button</button>\n"
    "<table border=\"1\"><tr><th>H1</th><th>H2</th></tr><tr><td>A</td><td>B</td></tr></table>\n";
}

CssPreviewPane::CssPreviewPane(QWidget* parent)
    : QDialog(parent)
    , m_sampleHtml(QString::fromLatin1(kDefaultSample))
{
    setWindowTitle(tr("CSS Preview"));
    resize(800, 600);
    setSizeGripEnabled(true);

    auto* root = new QVBoxLayout(this);

    // Top toolbar
    auto* toolbar = new QToolBar(this);
    m_refreshBtn = new QPushButton(tr("Refresh"), toolbar);
    m_liveCheck  = new QCheckBox(tr("Live update"), toolbar);
    m_liveCheck->setChecked(true);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addWidget(m_liveCheck);
    root->addWidget(toolbar);

    // Note label under the toolbar
    m_noteLabel = new QLabel(
        tr("Rendered with Qt rich-text engine \xE2\x80\x94 flexbox/grid/animations not supported."),
        this);
    m_noteLabel->setWordWrap(true);
    root->addWidget(m_noteLabel);

    // Central preview
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    root->addWidget(m_browser, /*stretch*/ 1);

    // Bottom button box
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    root->addWidget(buttons);

    connect(m_refreshBtn, &QPushButton::clicked, this, &CssPreviewPane::refresh);

    // Initial render (no editor bound yet)
    refresh();
}

void CssPreviewPane::setSampleHtml(const QString& html)
{
    m_sampleHtml = html;
    refresh();
}

void CssPreviewPane::bindToEditor(ScintillaEdit* editor)
{
    // Disconnect previous editor connection, if any.
    if (m_editorConn) {
        QObject::disconnect(m_editorConn);
        m_editorConn = QMetaObject::Connection();
    }

    m_editor = editor;

    if (m_editor) {
        m_editorConn = connect(m_editor, &ScintillaEdit::modified,
                               this, &CssPreviewPane::onEditorModified);
    }

    // Reflect current content immediately.
    refresh();
}

void CssPreviewPane::onEditorModified()
{
    if (!m_liveCheck || !m_liveCheck->isChecked()) {
        return;
    }
    if (m_refreshScheduled) {
        return;
    }
    m_refreshScheduled = true;
    QTimer::singleShot(150, this, [this]() {
        m_refreshScheduled = false;
        refresh();
    });
}

QString CssPreviewPane::composeHtml(const QString& css) const
{
    QString html;
    html.reserve(css.size() + m_sampleHtml.size() + 64);
    html += QStringLiteral("<html><head><style>");
    html += css;
    html += QStringLiteral("</style></head><body>");
    html += m_sampleHtml;
    html += QStringLiteral("</body></html>");
    return html;
}

void CssPreviewPane::refresh()
{
    if (!m_browser) {
        return;
    }

    if (!m_editor) {
        m_browser->setHtml(QStringLiteral("<html><body></body></html>"));
        setWindowTitle(tr("CSS Preview"));
        return;
    }

    const auto length = m_editor->textLength();
    QByteArray raw = m_editor->getText(length + 1);
    // Drop trailing NUL if present.
    if (!raw.isEmpty() && raw.endsWith('\0')) {
        raw.chop(1);
    }
    const QString css = QString::fromUtf8(raw);

    m_browser->setHtml(composeHtml(css));

    setWindowTitle(tr("CSS Preview \xE2\x80\x94 %1 bytes").arg(raw.size()));
}
