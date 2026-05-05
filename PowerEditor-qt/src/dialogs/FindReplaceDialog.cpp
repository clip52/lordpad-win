#include "FindReplaceDialog.h"

#include "ScintillaEdit.h"

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QShortcut>
#include <QKeySequence>
#include <QWidget>
#include <QByteArray>
#include <QString>

// Scintilla constants. Defined locally to avoid pulling extra headers from the
// project; values are part of the stable Scintilla public ABI.
#ifndef SCFIND_MATCHCASE
#define SCFIND_MATCHCASE  0x4
#endif
#ifndef SCFIND_WHOLEWORD
#define SCFIND_WHOLEWORD  0x2
#endif
#ifndef SCFIND_REGEXP
#define SCFIND_REGEXP     0x00200000
#endif
#ifndef SCFIND_CXX11REGEX
#define SCFIND_CXX11REGEX 0x00800000
#endif

FindReplaceDialog::FindReplaceDialog(ScintillaEdit* editor, QWidget* parent)
    : QDialog(parent)
    , m_editor(editor)
{
    setWindowTitle(tr("Find / Replace"));
    setWindowModality(Qt::NonModal);
    setWindowFlags(windowFlags() | Qt::Tool);

    // ---------- Tab widget ----------
    m_tabs = new QTabWidget(this);

    // ----- Find tab -----
    QWidget*     findTab    = new QWidget(m_tabs);
    QVBoxLayout* findLayout = new QVBoxLayout(findTab);

    QLabel* findLabel = new QLabel(tr("Find what:"), findTab);
    m_findEdit = new QLineEdit(findTab);

    QHBoxLayout* findBtnRow = new QHBoxLayout();
    m_btnFindNext = new QPushButton(tr("Find Next"), findTab);
    m_btnFindPrev = new QPushButton(tr("Find Previous"), findTab);
    findBtnRow->addWidget(m_btnFindNext);
    findBtnRow->addWidget(m_btnFindPrev);
    findBtnRow->addStretch(1);

    m_findStatus = new QLabel(QString(), findTab);

    findLayout->addWidget(findLabel);
    findLayout->addWidget(m_findEdit);
    findLayout->addLayout(findBtnRow);
    findLayout->addStretch(1);
    findLayout->addWidget(m_findStatus);

    // ----- Replace tab -----
    QWidget*     replaceTab    = new QWidget(m_tabs);
    QVBoxLayout* replaceLayout = new QVBoxLayout(replaceTab);

    QLabel* findLabelR = new QLabel(tr("Find what:"), replaceTab);
    m_findEditR = new QLineEdit(replaceTab);

    QLabel* replaceLabel = new QLabel(tr("Replace with:"), replaceTab);
    m_replaceEdit = new QLineEdit(replaceTab);

    QHBoxLayout* replaceBtnRow = new QHBoxLayout();
    m_btnFindNextR  = new QPushButton(tr("Find Next"), replaceTab);
    m_btnFindPrevR  = new QPushButton(tr("Find Previous"), replaceTab);
    m_btnReplace    = new QPushButton(tr("Replace"), replaceTab);
    m_btnReplaceAll = new QPushButton(tr("Replace All"), replaceTab);
    replaceBtnRow->addWidget(m_btnFindNextR);
    replaceBtnRow->addWidget(m_btnFindPrevR);
    replaceBtnRow->addWidget(m_btnReplace);
    replaceBtnRow->addWidget(m_btnReplaceAll);
    replaceBtnRow->addStretch(1);

    m_replaceStatus = new QLabel(QString(), replaceTab);

    replaceLayout->addWidget(findLabelR);
    replaceLayout->addWidget(m_findEditR);
    replaceLayout->addWidget(replaceLabel);
    replaceLayout->addWidget(m_replaceEdit);
    replaceLayout->addLayout(replaceBtnRow);
    replaceLayout->addStretch(1);
    replaceLayout->addWidget(m_replaceStatus);

    m_tabs->addTab(findTab,    tr("Find"));
    m_tabs->addTab(replaceTab, tr("Replace"));

    // ---------- Shared options group ----------
    QGroupBox*   optionsBox    = new QGroupBox(tr("Options"), this);
    QHBoxLayout* optionsLayout = new QHBoxLayout(optionsBox);

    m_chkMatchCase = new QCheckBox(tr("Match case"),         optionsBox);
    m_chkWholeWord = new QCheckBox(tr("Whole word"),         optionsBox);
    m_chkRegex     = new QCheckBox(tr("Regular expression"), optionsBox);
    m_chkWrap      = new QCheckBox(tr("Wrap around"),        optionsBox);
    m_chkWrap->setChecked(true);

    optionsLayout->addWidget(m_chkMatchCase);
    optionsLayout->addWidget(m_chkWholeWord);
    optionsLayout->addWidget(m_chkRegex);
    optionsLayout->addWidget(m_chkWrap);
    optionsLayout->addStretch(1);

    // ---------- Top-level layout ----------
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tabs);
    mainLayout->addWidget(optionsBox);

    // ---------- Connections ----------
    // Buttons
    connect(m_btnFindNext,   &QPushButton::clicked, this, &FindReplaceDialog::onFindNext);
    connect(m_btnFindPrev,   &QPushButton::clicked, this, &FindReplaceDialog::onFindPrevious);
    connect(m_btnFindNextR,  &QPushButton::clicked, this, &FindReplaceDialog::onFindNext);
    connect(m_btnFindPrevR,  &QPushButton::clicked, this, &FindReplaceDialog::onFindPrevious);
    connect(m_btnReplace,    &QPushButton::clicked, this, &FindReplaceDialog::onReplace);
    connect(m_btnReplaceAll, &QPushButton::clicked, this, &FindReplaceDialog::onReplaceAll);

    // Enter in find fields triggers Find Next
    connect(m_findEdit,  &QLineEdit::returnPressed, this, &FindReplaceDialog::onFindNext);
    connect(m_findEditR, &QLineEdit::returnPressed, this, &FindReplaceDialog::onFindNext);
    // Enter in the replace field performs a replace
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceDialog::onReplace);

    // Keep the two "find what" fields in sync across tabs
    connect(m_findEdit,  &QLineEdit::textChanged, this, [this](const QString& t){
        if (m_findEditR->text() != t) m_findEditR->setText(t);
    });
    connect(m_findEditR, &QLineEdit::textChanged, this, [this](const QString& t){
        if (m_findEdit->text() != t) m_findEdit->setText(t);
    });

    // Esc closes the dialog
    QShortcut* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, &FindReplaceDialog::close);
}

void FindReplaceDialog::setActiveEditor(ScintillaEdit* editor)
{
    m_editor = editor;
    m_lastMatchStart = -1;
    m_lastMatchEnd   = -1;
}

void FindReplaceDialog::showFind()
{
    m_tabs->setCurrentIndex(0);
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceDialog::showReplace()
{
    m_tabs->setCurrentIndex(1);
    show();
    raise();
    activateWindow();
    m_findEditR->setFocus();
    m_findEditR->selectAll();
}

void FindReplaceDialog::setStatus(const QString& text)
{
    if (m_findStatus)    m_findStatus->setText(text);
    if (m_replaceStatus) m_replaceStatus->setText(text);
}

int FindReplaceDialog::buildSearchFlags() const
{
    int flags = 0;
    if (m_chkMatchCase && m_chkMatchCase->isChecked()) flags |= SCFIND_MATCHCASE;
    if (m_chkWholeWord && m_chkWholeWord->isChecked()) flags |= SCFIND_WHOLEWORD;
    if (m_chkRegex     && m_chkRegex->isChecked())     flags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
    return flags;
}

long long FindReplaceDialog::findOnce(long long startPos, long long endPos,
                                      const QByteArray& needle,
                                      long long* matchStart, long long* matchEnd)
{
    if (!m_editor) return -1;
    m_editor->setSearchFlags(buildSearchFlags());
    m_editor->setTargetStart(startPos);
    m_editor->setTargetEnd(endPos);

    long long pos = m_editor->searchInTarget(needle.size(), needle.constData());
    if (pos >= 0) {
        if (matchStart) *matchStart = m_editor->targetStart();
        if (matchEnd)   *matchEnd   = m_editor->targetEnd();
    }
    return pos;
}

bool FindReplaceDialog::findDirectional(bool forward)
{
    if (!hasEditor()) {
        setStatus(tr("No editor"));
        return false;
    }

    const QString needleStr = m_findEdit->text();
    if (needleStr.isEmpty()) {
        setStatus(tr("Nothing to search for"));
        return false;
    }
    const QByteArray needle = needleStr.toUtf8();

    const long long docLen = m_editor->length();
    const long long selStart = m_editor->selectionStart();
    const long long selEnd   = m_editor->selectionEnd();

    long long startPos, endPos;
    if (forward) {
        startPos = selEnd;
        endPos   = docLen;
    } else {
        // Scintilla supports start > end for backwards search.
        startPos = selStart;
        endPos   = 0;
    }

    long long mStart = -1, mEnd = -1;
    long long pos = findOnce(startPos, endPos, needle, &mStart, &mEnd);

    if (pos < 0 && m_chkWrap && m_chkWrap->isChecked()) {
        if (forward) {
            startPos = 0;
            endPos   = docLen;
        } else {
            startPos = docLen;
            endPos   = 0;
        }
        pos = findOnce(startPos, endPos, needle, &mStart, &mEnd);
    }

    if (pos < 0) {
        m_lastMatchStart = -1;
        m_lastMatchEnd   = -1;
        setStatus(tr("Not found"));
        return false;
    }

    m_lastMatchStart = mStart;
    m_lastMatchEnd   = mEnd;
    m_editor->setSel(mStart, mEnd);
    m_editor->scrollCaret();
    setStatus(tr("Found"));
    return true;
}

void FindReplaceDialog::onFindNext()
{
    findDirectional(true);
}

void FindReplaceDialog::onFindPrevious()
{
    findDirectional(false);
}

void FindReplaceDialog::onReplace()
{
    if (!hasEditor()) {
        setStatus(tr("No editor"));
        return;
    }

    const QString needleStr = m_findEdit->text();
    if (needleStr.isEmpty()) {
        setStatus(tr("Nothing to search for"));
        return;
    }

    const long long selStart = m_editor->selectionStart();
    const long long selEnd   = m_editor->selectionEnd();
    const QByteArray replacement = m_replaceEdit->text().toUtf8();
    const bool useRegex = (m_chkRegex && m_chkRegex->isChecked());

    // Replace only if the current selection is the last match we found.
    if (m_lastMatchStart >= 0
        && selStart == m_lastMatchStart
        && selEnd   == m_lastMatchEnd
        && selStart != selEnd) {

        m_editor->setTargetStart(m_lastMatchStart);
        m_editor->setTargetEnd(m_lastMatchEnd);

        long long newLen = 0;
        if (useRegex) {
            newLen = m_editor->replaceTargetRE(replacement.size(), replacement.constData());
        } else {
            newLen = m_editor->replaceTarget(replacement.size(), replacement.constData());
        }

        // After replacement, move the caret past the replaced text so the
        // subsequent Find Next starts after it.
        const long long after = m_lastMatchStart + newLen;
        m_editor->setSel(after, after);
        m_lastMatchStart = -1;
        m_lastMatchEnd   = -1;
        setStatus(tr("Replaced"));
    }

    // Always advance to the next match.
    findDirectional(true);
}

void FindReplaceDialog::onReplaceAll()
{
    if (!hasEditor()) {
        setStatus(tr("No editor"));
        return;
    }

    const QString needleStr = m_findEdit->text();
    if (needleStr.isEmpty()) {
        setStatus(tr("Nothing to search for"));
        return;
    }
    const QByteArray needle = needleStr.toUtf8();
    const QByteArray replacement = m_replaceEdit->text().toUtf8();
    const bool useRegex = (m_chkRegex && m_chkRegex->isChecked());

    m_editor->setSearchFlags(buildSearchFlags());
    m_editor->beginUndoAction();

    long long count = 0;
    long long pos = 0;
    long long docLen = m_editor->length();

    while (pos <= docLen) {
        m_editor->setTargetStart(pos);
        m_editor->setTargetEnd(docLen);

        const long long found = m_editor->searchInTarget(needle.size(), needle.constData());
        if (found < 0) break;

        const long long mStart = m_editor->targetStart();
        const long long mEnd   = m_editor->targetEnd();

        long long newLen = 0;
        if (useRegex) {
            newLen = m_editor->replaceTargetRE(replacement.size(), replacement.constData());
        } else {
            newLen = m_editor->replaceTarget(replacement.size(), replacement.constData());
        }

        ++count;

        // Advance past the replacement. Guard against zero-length matches
        // (e.g. regex that matches empty string) to avoid an infinite loop.
        long long advance = mStart + newLen;
        if (advance <= mStart) {
            advance = mStart + 1;
        }
        pos = advance;

        // Document length may have changed.
        docLen = m_editor->length();
    }

    m_editor->endUndoAction();

    m_lastMatchStart = -1;
    m_lastMatchEnd   = -1;
    setStatus(tr("Replaced %1 occurrence(s)").arg(count));
}
