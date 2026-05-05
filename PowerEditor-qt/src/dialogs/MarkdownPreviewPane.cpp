#include "MarkdownPreviewPane.h"

#include <QDialog>
#include <QTextBrowser>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QDialogButtonBox>
#include <QTimer>
#include <QRegularExpression>
#include <QStringList>
#include <QByteArray>
#include <QString>
#include <QChar>

#include "ScintillaEdit.h"

namespace {

// ---------------------------------------------------------------------------
// HTML escaping for plain text and inline-code content.
// ---------------------------------------------------------------------------
QString htmlEscape(const QString& in)
{
    QString out;
    out.reserve(in.size());
    for (QChar c : in) {
        if      (c == QLatin1Char('&')) out += QStringLiteral("&amp;");
        else if (c == QLatin1Char('<')) out += QStringLiteral("&lt;");
        else if (c == QLatin1Char('>')) out += QStringLiteral("&gt;");
        else                            out += c;
    }
    return out;
}

// Validate URL: only http(s) or relative (no scheme, or no scheme followed by ":").
bool isAllowedUrl(const QString& url)
{
    // Allow empty? treat as relative.
    if (url.isEmpty()) return true;

    // Find first colon before any '/', '?', '#'. If there is one, it's a scheme.
    int colon = -1;
    for (int i = 0; i < url.size(); ++i) {
        QChar ch = url.at(i);
        if (ch == QLatin1Char(':')) { colon = i; break; }
        if (ch == QLatin1Char('/') || ch == QLatin1Char('?') ||
            ch == QLatin1Char('#') || ch == QLatin1Char('\\')) {
            // No scheme delimiter encountered -> relative.
            return true;
        }
    }
    if (colon < 0) {
        // No colon at all -> relative.
        return true;
    }
    const QString scheme = url.left(colon).toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

// ---------------------------------------------------------------------------
// Inline transformations applied to a single line/segment of text.
// Order: extract code spans first, then images, links, bold, italic.
// We use placeholders to protect already-transformed segments from further
// processing (e.g. so bold/italic don't break inside code spans).
// ---------------------------------------------------------------------------
QString applyInline(const QString& input)
{
    // Step 0: capture inline code spans -> placeholders.
    QStringList codePlaceholders;
    QString text;
    text.reserve(input.size());
    {
        int i = 0;
        const int n = input.size();
        while (i < n) {
            QChar ch = input.at(i);
            if (ch == QLatin1Char('`')) {
                // Find closing backtick on the same segment.
                int end = input.indexOf(QLatin1Char('`'), i + 1);
                if (end > i) {
                    QString inner = input.mid(i + 1, end - i - 1);
                    QString placeholder =
                        QStringLiteral("\x01CODE%1\x02").arg(codePlaceholders.size());
                    codePlaceholders.append(
                        QStringLiteral("<code>") + htmlEscape(inner) + QStringLiteral("</code>"));
                    text += placeholder;
                    i = end + 1;
                    continue;
                }
            }
            text += ch;
            ++i;
        }
    }

    // Step 1: escape the remaining (non-code) text first.
    text = htmlEscape(text);

    // Step 2: images ![alt](url)  -- run BEFORE links so link regex doesn't grab them.
    {
        static const QRegularExpression rx(
            QStringLiteral("!\\[([^\\]]*)\\]\\(([^)\\s]+)(?:\\s+\"[^\"]*\")?\\)"));
        QString out;
        out.reserve(text.size());
        int last = 0;
        auto it = rx.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            out += text.mid(last, m.capturedStart() - last);
            const QString alt = m.captured(1);
            const QString url = m.captured(2);
            if (isAllowedUrl(url)) {
                out += QStringLiteral("<img src=\"") + url
                     + QStringLiteral("\" alt=\"") + alt
                     + QStringLiteral("\"/>");
            } else {
                // Skip unknown scheme: emit literal markdown.
                out += m.captured(0);
            }
            last = m.capturedEnd();
        }
        out += text.mid(last);
        text = out;
    }

    // Step 3: links [text](url)
    {
        static const QRegularExpression rx(
            QStringLiteral("\\[([^\\]]+)\\]\\(([^)\\s]+)(?:\\s+\"[^\"]*\")?\\)"));
        QString out;
        out.reserve(text.size());
        int last = 0;
        auto it = rx.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            out += text.mid(last, m.capturedStart() - last);
            const QString label = m.captured(1);
            const QString url   = m.captured(2);
            if (isAllowedUrl(url)) {
                out += QStringLiteral("<a href=\"") + url
                     + QStringLiteral("\">") + label + QStringLiteral("</a>");
            } else {
                out += m.captured(0);
            }
            last = m.capturedEnd();
        }
        out += text.mid(last);
        text = out;
    }

    // Step 4: bold (**...** or __...__) -- non-greedy.
    {
        static const QRegularExpression rxStar(QStringLiteral("\\*\\*(.+?)\\*\\*"));
        text.replace(rxStar, QStringLiteral("<strong>\\1</strong>"));
        static const QRegularExpression rxUnder(QStringLiteral("__(.+?)__"));
        text.replace(rxUnder, QStringLiteral("<strong>\\1</strong>"));
    }

    // Step 5: italic (*...* or _..._) -- non-greedy.
    {
        // Avoid matching across already-emitted tags by requiring inner not to start/end with space.
        static const QRegularExpression rxStar(QStringLiteral("\\*(?!\\s)([^*\\n]+?)(?<!\\s)\\*"));
        text.replace(rxStar, QStringLiteral("<em>\\1</em>"));
        static const QRegularExpression rxUnder(QStringLiteral("_(?!\\s)([^_\\n]+?)(?<!\\s)_"));
        text.replace(rxUnder, QStringLiteral("<em>\\1</em>"));
    }

    // Step 6: re-substitute code placeholders.
    for (int i = 0; i < codePlaceholders.size(); ++i) {
        const QString placeholder = QStringLiteral("\x01CODE%1\x02").arg(i);
        text.replace(placeholder, codePlaceholders.at(i));
    }

    return text;
}

// ---------------------------------------------------------------------------
// Helpers for line classification.
// ---------------------------------------------------------------------------
bool isHorizontalRule(const QString& line)
{
    QString t = line.trimmed();
    if (t.size() < 3) return false;
    QChar c = t.at(0);
    if (c != QLatin1Char('-') && c != QLatin1Char('*') && c != QLatin1Char('_'))
        return false;
    for (QChar ch : t) {
        if (ch != c) return false;
    }
    return true;
}

int atxHeadingLevel(const QString& line, QString* rest = nullptr)
{
    int i = 0;
    const int n = line.size();
    while (i < n && i < 6 && line.at(i) == QLatin1Char('#')) ++i;
    if (i == 0 || i > 6) return 0;
    if (i >= n) return 0;
    if (line.at(i) != QLatin1Char(' ')) return 0;
    if (rest) {
        // Strip trailing # padding too (CommonMark-ish, optional).
        QString r = line.mid(i + 1);
        // Trim trailing whitespace.
        while (!r.isEmpty() && r.back().isSpace()) r.chop(1);
        // Trim trailing closing #s if preceded by space.
        int trailing = 0;
        while (trailing < r.size() &&
               r.at(r.size() - 1 - trailing) == QLatin1Char('#'))
            ++trailing;
        if (trailing > 0 && r.size() > trailing &&
            r.at(r.size() - 1 - trailing) == QLatin1Char(' ')) {
            r.chop(trailing);
            while (!r.isEmpty() && r.back().isSpace()) r.chop(1);
        }
        *rest = r;
    }
    return i;
}

bool isBlockquote(const QString& line)
{
    return line.startsWith(QLatin1String("> ")) || line == QLatin1String(">");
}

bool isUnorderedItem(const QString& line, QString* rest = nullptr)
{
    if (line.size() < 2) return false;
    QChar c = line.at(0);
    if ((c == QLatin1Char('-') || c == QLatin1Char('*')) &&
        line.at(1) == QLatin1Char(' ')) {
        if (rest) *rest = line.mid(2);
        return true;
    }
    return false;
}

bool isOrderedItem(const QString& line, QString* rest = nullptr)
{
    int i = 0;
    const int n = line.size();
    while (i < n && line.at(i).isDigit()) ++i;
    if (i == 0) return false;
    if (i + 1 >= n) return false;
    if (line.at(i) != QLatin1Char('.')) return false;
    if (line.at(i + 1) != QLatin1Char(' ')) return false;
    if (rest) *rest = line.mid(i + 2);
    return true;
}

bool isFence(const QString& line)
{
    QString t = line.trimmed();
    return t.startsWith(QLatin1String("```"));
}

// ---------------------------------------------------------------------------
// GFM table helpers.
// ---------------------------------------------------------------------------

// Split a table row line into trimmed cell strings, honoring escaped pipes (\|).
// Strips a single optional leading and trailing pipe.
QStringList splitTableRow(const QString& line)
{
    QStringList cells;
    QString cur;
    cur.reserve(line.size());
    const int n = line.size();
    for (int i = 0; i < n; ++i) {
        QChar ch = line.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < n && line.at(i + 1) == QLatin1Char('|')) {
            // Escaped pipe -> literal '|', not a separator.
            cur += QLatin1Char('|');
            ++i;
            continue;
        }
        if (ch == QLatin1Char('|')) {
            cells.append(cur.trimmed());
            cur.clear();
            continue;
        }
        cur += ch;
    }
    cells.append(cur.trimmed());

    // Strip a single empty leading cell from an optional leading pipe.
    if (!cells.isEmpty() && cells.first().isEmpty())
        cells.removeFirst();
    // Strip a single empty trailing cell from an optional trailing pipe.
    if (!cells.isEmpty() && cells.last().isEmpty())
        cells.removeLast();

    return cells;
}

// Detect a separator row: each cell is /^:?-{3,}:?$/, optional leading/trailing pipe.
// At least one '|' must be present on the line.
bool isTableSeparator(const QString& line)
{
    QString t = line.trimmed();
    if (!t.contains(QLatin1Char('|'))) return false;

    const QStringList cells = splitTableRow(t);
    if (cells.isEmpty()) return false;

    for (const QString& raw : cells) {
        QString c = raw.trimmed();
        if (c.size() < 3) return false;
        int idx = 0;
        const int len = c.size();
        if (c.at(idx) == QLatin1Char(':')) ++idx;
        int dashCount = 0;
        while (idx < len && c.at(idx) == QLatin1Char('-')) { ++idx; ++dashCount; }
        if (dashCount < 3) return false;
        if (idx < len && c.at(idx) == QLatin1Char(':')) ++idx;
        if (idx != len) return false;
    }
    return true;
}

// Returns "left", "right", "center", or empty string for no alignment.
QString tableAlignFromCell(const QString& cellRaw)
{
    QString c = cellRaw.trimmed();
    if (c.isEmpty()) return QString();
    const bool left  = c.startsWith(QLatin1Char(':'));
    const bool right = c.endsWith(QLatin1Char(':'));
    if (left && right) return QStringLiteral("center");
    if (right)         return QStringLiteral("right");
    if (left)          return QStringLiteral("left");
    return QString();
}

// A line is a candidate table row if it contains an unescaped pipe.
bool lineHasUnescapedPipe(const QString& line)
{
    const int n = line.size();
    for (int i = 0; i < n; ++i) {
        QChar ch = line.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < n && line.at(i + 1) == QLatin1Char('|')) {
            ++i;
            continue;
        }
        if (ch == QLatin1Char('|')) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Main converter.
// ---------------------------------------------------------------------------
QString markdownToHtml(const QString& md)
{
    // Normalize newlines.
    QString src = md;
    src.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    src.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QStringList lines = src.split(QLatin1Char('\n'));
    QString out;
    out.reserve(src.size() * 2 + 1024);

    int i = 0;
    const int n = lines.size();

    auto flushParagraph = [&](QStringList& buf) {
        if (buf.isEmpty()) return;
        QString joined = buf.join(QLatin1Char('\n'));
        out += QStringLiteral("<p>") + applyInline(joined) + QStringLiteral("</p>\n");
        buf.clear();
    };

    QStringList paragraphBuf;

    while (i < n) {
        const QString& line = lines.at(i);

        // Blank line -> end paragraph.
        if (line.trimmed().isEmpty()) {
            flushParagraph(paragraphBuf);
            ++i;
            continue;
        }

        // Fenced code block.
        if (isFence(line)) {
            flushParagraph(paragraphBuf);
            ++i;
            QStringList codeLines;
            while (i < n && !isFence(lines.at(i))) {
                codeLines.append(lines.at(i));
                ++i;
            }
            // Skip closing fence if present.
            if (i < n && isFence(lines.at(i))) ++i;
            QString codeText = codeLines.join(QLatin1Char('\n'));
            out += QStringLiteral("<pre><code>") + htmlEscape(codeText)
                 + QStringLiteral("</code></pre>\n");
            continue;
        }

        // Horizontal rule.
        if (isHorizontalRule(line)) {
            flushParagraph(paragraphBuf);
            out += QStringLiteral("<hr/>\n");
            ++i;
            continue;
        }

        // ATX heading.
        {
            QString rest;
            int lvl = atxHeadingLevel(line, &rest);
            if (lvl > 0) {
                flushParagraph(paragraphBuf);
                out += QStringLiteral("<h%1>").arg(lvl)
                     + applyInline(rest)
                     + QStringLiteral("</h%1>\n").arg(lvl);
                ++i;
                continue;
            }
        }

        // Blockquote group.
        if (isBlockquote(line)) {
            flushParagraph(paragraphBuf);
            QStringList quoteParts;
            while (i < n && isBlockquote(lines.at(i))) {
                const QString& l = lines.at(i);
                QString stripped = l.size() >= 2 ? l.mid(2) : QString();
                if (l == QLatin1String(">")) stripped = QString();
                quoteParts.append(stripped);
                ++i;
            }
            QString joined = quoteParts.join(QLatin1Char('\n'));
            out += QStringLiteral("<blockquote>")
                 + applyInline(joined)
                 + QStringLiteral("</blockquote>\n");
            continue;
        }

        // Unordered list group.
        {
            QString rest;
            if (isUnorderedItem(line, &rest)) {
                flushParagraph(paragraphBuf);
                out += QStringLiteral("<ul>\n");
                while (i < n && isUnorderedItem(lines.at(i), &rest)) {
                    out += QStringLiteral("  <li>") + applyInline(rest)
                         + QStringLiteral("</li>\n");
                    ++i;
                }
                out += QStringLiteral("</ul>\n");
                continue;
            }
        }

        // Ordered list group.
        {
            QString rest;
            if (isOrderedItem(line, &rest)) {
                flushParagraph(paragraphBuf);
                out += QStringLiteral("<ol>\n");
                while (i < n && isOrderedItem(lines.at(i), &rest)) {
                    out += QStringLiteral("  <li>") + applyInline(rest)
                         + QStringLiteral("</li>\n");
                    ++i;
                }
                out += QStringLiteral("</ol>\n");
                continue;
            }
        }

        // GFM table: header row + separator row + zero-or-more data rows.
        // Note: we are NOT inside a fenced code block here (that branch
        // continues above), so it's safe to detect tables in this context.
        if (lineHasUnescapedPipe(line) &&
            i + 1 < n && isTableSeparator(lines.at(i + 1))) {
            flushParagraph(paragraphBuf);

            const QStringList headerCells    = splitTableRow(line);
            const QStringList separatorCells = splitTableRow(lines.at(i + 1));

            QStringList aligns;
            aligns.reserve(separatorCells.size());
            for (const QString& s : separatorCells)
                aligns.append(tableAlignFromCell(s));

            const int colCount = headerCells.size();

            auto openCellTag = [&](const QString& tag, int colIdx) {
                const QString align =
                    (colIdx < aligns.size()) ? aligns.at(colIdx) : QString();
                if (align.isEmpty())
                    return QStringLiteral("<") + tag + QStringLiteral(">");
                return QStringLiteral("<") + tag
                     + QStringLiteral(" align=\"") + align
                     + QStringLiteral("\">");
            };

            out += QStringLiteral("<table>\n<thead>\n<tr>");
            for (int c = 0; c < colCount; ++c) {
                out += openCellTag(QStringLiteral("th"), c)
                     + applyInline(headerCells.at(c))
                     + QStringLiteral("</th>");
            }
            out += QStringLiteral("</tr>\n</thead>\n<tbody>\n");

            i += 2; // consume header + separator

            while (i < n) {
                const QString& rowLine = lines.at(i);
                if (rowLine.trimmed().isEmpty()) break;
                if (!lineHasUnescapedPipe(rowLine)) break;
                // Don't let a fence inside a table sneak through.
                if (isFence(rowLine)) break;

                QStringList cells = splitTableRow(rowLine);
                // Pad/truncate to header column count.
                while (cells.size() < colCount) cells.append(QString());
                if (cells.size() > colCount) {
                    QStringList trimmed = cells.mid(0, colCount);
                    cells = trimmed;
                }

                out += QStringLiteral("<tr>");
                for (int c = 0; c < colCount; ++c) {
                    out += openCellTag(QStringLiteral("td"), c)
                         + applyInline(cells.at(c))
                         + QStringLiteral("</td>");
                }
                out += QStringLiteral("</tr>\n");
                ++i;
            }

            out += QStringLiteral("</tbody>\n</table>\n");
            continue;
        }

        // Default: accumulate into paragraph buffer.
        paragraphBuf.append(line);
        ++i;
    }

    flushParagraph(paragraphBuf);

    // Wrap with styled HTML body.
    QString html;
    html.reserve(out.size() + 1024);
    html += QStringLiteral(
        "<html><head><style>"
        "body { font-family: sans-serif; font-size: 11pt; line-height: 1.5; padding: 12px; }"
        "h1, h2, h3 { color: #333; }"
        "code { background: #F0F0F0; padding: 1px 4px; border-radius: 2px; font-family: monospace; }"
        "pre code { display: block; padding: 8px; background: #F5F5F5; border: 1px solid #DDD; }"
        "blockquote { border-left: 4px solid #DDD; padding-left: 12px; color: #555; }"
        "img { max-width: 100%; }"
        "table { border-collapse: collapse; margin: 8px 0; }"
        "th, td { border: 1px solid #DDD; padding: 6px 12px; text-align: left; }"
        "th { background: #F5F5F5; font-weight: 600; }"
        "</style></head><body>\n");
    html += out;
    html += QStringLiteral("</body></html>");
    return html;
}

} // anonymous namespace

// ===========================================================================
// MarkdownPreviewPane
// ===========================================================================
MarkdownPreviewPane::MarkdownPreviewPane(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Markdown Preview"));
    resize(800, 700);
    setSizeGripEnabled(true);

    auto* root = new QVBoxLayout(this);

    // Top toolbar with refresh button + live update checkbox.
    auto* toolbar = new QToolBar(this);
    m_refreshBtn = new QPushButton(tr("Refresh"), toolbar);
    m_liveCheck  = new QCheckBox(tr("Live update"), toolbar);
    m_liveCheck->setChecked(true);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addWidget(m_liveCheck);
    root->addWidget(toolbar);

    // Central preview.
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    root->addWidget(m_browser, /*stretch*/ 1);

    // Bottom button box with Close.
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    root->addWidget(buttons);

    connect(m_refreshBtn, &QPushButton::clicked, this, &MarkdownPreviewPane::refresh);

    refresh();
}

void MarkdownPreviewPane::bindToEditor(ScintillaEdit* editor)
{
    if (m_editorConn) {
        QObject::disconnect(m_editorConn);
        m_editorConn = QMetaObject::Connection();
    }

    m_editor = editor;

    if (m_editor) {
        m_editorConn = connect(m_editor, &ScintillaEdit::modified,
                               this, &MarkdownPreviewPane::onEditorModified);
    }

    refresh();
}

void MarkdownPreviewPane::onEditorModified()
{
    if (!m_liveCheck || !m_liveCheck->isChecked()) return;
    if (m_refreshScheduled) return;
    m_refreshScheduled = true;
    QTimer::singleShot(200, this, [this]() {
        m_refreshScheduled = false;
        refresh();
    });
}

void MarkdownPreviewPane::refresh()
{
    if (!m_browser) return;

    if (!m_editor) {
        m_browser->setHtml(QStringLiteral("<i>(no editor bound)</i>"));
        return;
    }

    const auto length = m_editor->textLength();
    QByteArray raw = m_editor->getText(length + 1);
    if (!raw.isEmpty() && raw.endsWith('\0')) {
        raw.chop(1);
    }
    const QString md = QString::fromUtf8(raw);

    m_browser->setHtml(markdownToHtml(md));
}
