#include "EditorTab.h"

#include <QVBoxLayout>
#include <QFileInfo>
#include <QFont>

#include "ScintillaEdit.h"

EditorTab::EditorTab(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_editor = new ScintillaEdit(this);
    layout->addWidget(m_editor);

    // UTF-8 code page
    m_editor->setCodePage(SC_CP_UTF8);

    // Default font: monospace, size 11
    QFont mono(QStringLiteral("Monospace"), 11);
    mono.setStyleHint(QFont::Monospace);
    m_editor->styleSetFont(STYLE_DEFAULT, mono.family().toUtf8().constData());
    m_editor->styleSetSize(STYLE_DEFAULT, 11);
    m_editor->styleClearAll();

    // Margin 0: line numbers, width auto-sized to 4 digits
    m_editor->setMarginTypeN(0, SC_MARGIN_NUMBER);
    const int lineNumWidth = static_cast<int>(
        m_editor->textWidth(STYLE_LINENUMBER, "_9999"));
    m_editor->setMarginWidthN(0, lineNumWidth);

    // Margin 1: unused — keep at 0
    m_editor->setMarginWidthN(1, 0);

    // Margin 2: folding margin
    m_editor->setMarginTypeN(2, SC_MARGIN_SYMBOL);
    m_editor->setMarginMaskN(2, SC_MASK_FOLDERS);
    m_editor->setMarginWidthN(2, 16);
    m_editor->setMarginSensitiveN(2, true);

    // Tabs: width 4, use spaces
    m_editor->setTabWidth(4);
    m_editor->setUseTabs(false);

    // Wire signals
    connect(m_editor, &ScintillaEdit::modified,
            this, &EditorTab::onScintillaModified);
    connect(m_editor, &ScintillaEdit::updateUi,
            this, &EditorTab::onScintillaUpdateUi);
}

EditorTab::~EditorTab() = default;

ScintillaEdit* EditorTab::editor() const
{
    return m_editor;
}

QString EditorTab::filePath() const
{
    return m_path;
}

void EditorTab::setFilePath(const QString& path)
{
    if (m_path == path)
        return;
    m_path = path;
    emit filePathChanged(m_path);
}

bool EditorTab::isModified() const
{
    return m_modified;
}

void EditorTab::setModified(bool modified)
{
    if (!modified && m_editor) {
        // Clear Scintilla's dirty flag
        m_editor->setSavePoint();
    }
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modificationChanged(m_modified);
}

QString EditorTab::tabTitle() const
{
    QString base;
    if (m_path.isEmpty()) {
        base = QStringLiteral("untitled");
    } else {
        base = QFileInfo(m_path).fileName();
        if (base.isEmpty())
            base = QStringLiteral("untitled");
    }
    if (m_modified)
        base += QStringLiteral(" *");
    return base;
}

QString EditorTab::displayPath() const
{
    if (m_path.isEmpty())
        return QStringLiteral("untitled");
    return m_path;
}

void EditorTab::onScintillaModified(Scintilla::ModificationFlags type,
                                    Scintilla::Position /*position*/,
                                    Scintilla::Position /*length*/,
                                    Scintilla::Position /*linesAdded*/,
                                    const QByteArray& /*text*/,
                                    Scintilla::Position /*line*/,
                                    Scintilla::FoldLevel /*foldNow*/,
                                    Scintilla::FoldLevel /*foldPrev*/)
{
    using F = Scintilla::ModificationFlags;
    const auto mask = F::InsertText | F::DeleteText;
    if ((type & mask) != F::None) {
        if (!m_modified) {
            m_modified = true;
            emit modificationChanged(true);
        }
    }
}

void EditorTab::onScintillaUpdateUi(Scintilla::Update /*updated*/)
{
    if (!m_editor)
        return;
    const sptr_t pos  = m_editor->currentPos();
    const sptr_t line = m_editor->lineFromPosition(pos);
    const sptr_t col  = m_editor->column(pos);
    emit cursorPositionChanged(static_cast<int>(line) + 1,
                               static_cast<int>(col) + 1);
}
