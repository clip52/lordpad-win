#include "WordCountDialog.h"

#include <QDialog>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QString>
#include <QFont>
#include <QFontDatabase>
#include <QByteArray>

#include "ScintillaEdit.h"

WordCountDialog::WordCountDialog(QWidget* parent)
    : QDialog(parent)
{
    setModal(true);
    setWindowTitle(tr("Word Count"));

    auto* root = new QVBoxLayout(this);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(6);

    auto makeHeader = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        return l;
    };

    auto makeCount = [this]() {
        auto* l = new QLabel(QStringLiteral("0"), this);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        l->setMinimumWidth(80);
        return l;
    };

    // Header row.
    grid->addWidget(makeHeader(tr("Metric")),    0, 0);
    grid->addWidget(makeHeader(tr("Document")),  0, 1, Qt::AlignRight);
    grid->addWidget(makeHeader(tr("Selection")), 0, 2, Qt::AlignRight);

    // Row labels and value cells.
    int row = 1;

    grid->addWidget(new QLabel(tr("Characters (with whitespace)"), this), row, 0);
    m_docCharsWs = makeCount();
    m_selCharsWs = makeCount();
    grid->addWidget(m_docCharsWs, row, 1);
    grid->addWidget(m_selCharsWs, row, 2);
    ++row;

    grid->addWidget(new QLabel(tr("Characters (no whitespace)"), this), row, 0);
    m_docCharsNoWs = makeCount();
    m_selCharsNoWs = makeCount();
    grid->addWidget(m_docCharsNoWs, row, 1);
    grid->addWidget(m_selCharsNoWs, row, 2);
    ++row;

    grid->addWidget(new QLabel(tr("Words"), this), row, 0);
    m_docWords = makeCount();
    m_selWords = makeCount();
    grid->addWidget(m_docWords, row, 1);
    grid->addWidget(m_selWords, row, 2);
    ++row;

    grid->addWidget(new QLabel(tr("Lines"), this), row, 0);
    m_docLines = makeCount();
    m_selLines = makeCount();
    grid->addWidget(m_docLines, row, 1);
    grid->addWidget(m_selLines, row, 2);
    ++row;

    grid->addWidget(new QLabel(tr("Paragraphs"), this), row, 0);
    m_docParas = makeCount();
    m_selParas = makeCount();
    grid->addWidget(m_docParas, row, 1);
    grid->addWidget(m_selParas, row, 2);
    ++row;

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 0);

    root->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_refreshBtn = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refreshBtn, &QPushButton::clicked, this, &WordCountDialog::onRefresh);

    root->addWidget(buttons);
}

void WordCountDialog::load(ScintillaEdit* editor, const QString& sourceTitle)
{
    m_editor = editor;
    m_title  = sourceTitle;

    if (sourceTitle.isEmpty())
        setWindowTitle(tr("Word Count"));
    else
        setWindowTitle(tr("Word Count \xE2\x80\x94 %1").arg(sourceTitle));

    QString docText;
    QString selText;

    if (editor) {
        QByteArray docBytes = editor->getText(editor->textLength() + 1);
        // ScintillaEdit::getText returns a NUL-terminated buffer; QByteArray::fromRawData/ctor
        // may include the trailing NUL — strip it for safety.
        if (!docBytes.isEmpty() && docBytes.endsWith('\0'))
            docBytes.chop(1);
        docText = QString::fromUtf8(docBytes);

        QByteArray selBytes = editor->getSelText();
        if (!selBytes.isEmpty() && selBytes.endsWith('\0'))
            selBytes.chop(1);
        selText = QString::fromUtf8(selBytes);
    }

    const Stats doc = computeStats(docText);
    const Stats sel = computeStats(selText);

    m_docCharsWs  ->setText(QString::number(doc.charsWithWs));
    m_docCharsNoWs->setText(QString::number(doc.charsNoWs));
    m_docWords    ->setText(QString::number(doc.words));
    m_docLines    ->setText(QString::number(doc.lines));
    m_docParas    ->setText(QString::number(doc.paragraphs));

    m_selCharsWs  ->setText(QString::number(sel.charsWithWs));
    m_selCharsNoWs->setText(QString::number(sel.charsNoWs));
    m_selWords    ->setText(QString::number(sel.words));
    m_selLines    ->setText(QString::number(sel.lines));
    m_selParas    ->setText(QString::number(sel.paragraphs));
}

WordCountDialog::Stats WordCountDialog::computeStats(const QString& text)
{
    Stats s;

    if (text.isEmpty())
        return s;

    // Characters with whitespace: total QString length (each QChar = 1, multibyte already
    // collapsed by UTF-8 decode; surrogate pairs still count as 2 — acceptable for the
    // simple counter here).
    s.charsWithWs = text.size();

    // Characters without whitespace.
    int noWs = 0;
    for (const QChar c : text) {
        if (!c.isSpace())
            ++noWs;
    }
    s.charsNoWs = noWs;

    // Words via Unicode word regex.
    static const QRegularExpression wordRe(QStringLiteral("[\\p{L}\\p{N}_]+"));
    int wc = 0;
    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++wc;
    }
    s.words = wc;

    // Lines: count of '\n' + 1 if non-empty.
    int newlines = text.count(QLatin1Char('\n'));
    s.lines = newlines + 1;

    // Paragraphs: split on /\n\s*\n/ and count non-empty trimmed segments.
    static const QRegularExpression paraRe(QStringLiteral("\\n\\s*\\n"));
    const QStringList parts = text.split(paraRe, Qt::KeepEmptyParts);
    int paraCount = 0;
    for (const QString& p : parts) {
        if (!p.trimmed().isEmpty())
            ++paraCount;
    }
    s.paragraphs = paraCount;

    return s;
}

void WordCountDialog::onRefresh()
{
    if (m_editor)
        load(m_editor, m_title);
}
