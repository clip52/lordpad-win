#include "FunctionListPanel.h"

#include <QDockWidget>
#include <QListView>
#include <QAbstractItemView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QTimer>
#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>

#include "ScintillaEdit.h"
#include "../OutlineRegex.h"

namespace {

QList<QRegularExpression> patternsFor(const QString& lexerName)
{
    QList<QRegularExpression> result;
    const auto NoOpt = QRegularExpression::NoPatternOption;
    const auto CI    = QRegularExpression::CaseInsensitiveOption;

    if (lexerName == QLatin1String("cpp") || lexerName == QLatin1String("javascript")) {
        result << QRegularExpression(
                      QStringLiteral("^\\s*(?:[\\w:<>,\\s\\*&]+\\s+)?(\\w+)\\s*\\([^;]*\\)\\s*"
                                     "(?:const\\s*)?(?:noexcept\\s*)?(?:override\\s*)?\\{"),
                      NoOpt)
               << QRegularExpression(
                      QStringLiteral("^\\s*(?:export\\s+)?(?:async\\s+)?function\\s+(\\w+)\\s*\\("),
                      NoOpt)
               << QRegularExpression(
                      QStringLiteral("^\\s*(?:public|private|protected|static|export|async)?\\s*"
                                     "(\\w+)\\s*\\([^)]*\\)\\s*\\{"),
                      NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*class\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*struct\\s+(\\w+)"), NoOpt);
    } else if (lexerName == QLatin1String("python")) {
        result << QRegularExpression(QStringLiteral("^\\s*def\\s+(\\w+)\\s*\\("), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*async\\s+def\\s+(\\w+)\\s*\\("), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*class\\s+(\\w+)\\s*[\\(:]"), NoOpt);
    } else if (lexerName == QLatin1String("phpscript")) {
        result << QRegularExpression(
                      QStringLiteral("^\\s*(?:public|private|protected|static)?\\s*function\\s+(\\w+)\\s*\\("),
                      NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*class\\s+(\\w+)"), NoOpt);
    } else if (lexerName == QLatin1String("sql")) {
        result << QRegularExpression(
            QStringLiteral("^\\s*(?:CREATE|ALTER)\\s+(?:OR\\s+REPLACE\\s+)?"
                           "(?:PROCEDURE|FUNCTION|TRIGGER|TABLE|VIEW)\\s+(\\w+)"),
            CI);
    } else if (lexerName == QLatin1String("ruby")) {
        result << QRegularExpression(QStringLiteral("^\\s*def\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*class\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*module\\s+(\\w+)"), NoOpt);
    } else {
        // generic fallback
        result << QRegularExpression(QStringLiteral("^\\s*function\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*def\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*class\\s+(\\w+)"), NoOpt)
               << QRegularExpression(QStringLiteral("^\\s*public\\s+\\S+\\s+(\\w+)\\s*\\("), NoOpt);
    }

    return result;
}

} // namespace

FunctionListPanel::FunctionListPanel(QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("Function List"));
    setObjectName(QStringLiteral("FunctionListPanel"));

    auto* container = new QWidget(this);
    auto* root      = new QVBoxLayout(container);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);

    m_filterEdit = new QLineEdit(container);
    m_filterEdit->setPlaceholderText(tr("Filter..."));
    topRow->addWidget(m_filterEdit, /*stretch*/ 1);

    auto* refreshBtn = new QToolButton(container);
    refreshBtn->setText(tr("Refresh"));
    refreshBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    topRow->addWidget(refreshBtn);

    root->addLayout(topRow);

    m_listView = new QListView(container);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->setUniformItemSizes(true);
    root->addWidget(m_listView, /*stretch*/ 1);

    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(1);
    m_listView->setModel(m_model);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &FunctionListPanel::refresh);

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &FunctionListPanel::onFilterTextChanged);
    connect(m_listView, &QAbstractItemView::activated,
            this, &FunctionListPanel::onItemActivated);
    connect(refreshBtn, &QToolButton::clicked,
            this, &FunctionListPanel::onRefreshClicked);

    setWidget(container);

    // Initial empty state
    refresh();
}

void FunctionListPanel::setActiveEditor(ScintillaEdit* editor, const QString& lexerName)
{
    if (m_modifiedConn) {
        QObject::disconnect(m_modifiedConn);
        m_modifiedConn = QMetaObject::Connection();
    }

    m_editor    = editor;
    m_lexerName = lexerName;

    if (m_editor) {
        // Hook the editor's modified signal; on any change schedule a debounced refresh.
        m_modifiedConn = connect(m_editor, &ScintillaEdit::modified,
                                 this, &FunctionListPanel::onEditorModified);
    }

    refresh();
}

void FunctionListPanel::onEditorModified()
{
    if (m_debounce) {
        m_debounce->start();
    }
}

void FunctionListPanel::onRefreshClicked()
{
    refresh();
}

void FunctionListPanel::refresh()
{
    if (!m_model) return;

    m_model->removeRows(0, m_model->rowCount());

    if (!m_editor) {
        auto* item = new QStandardItem(tr("(no editor)"));
        item->setFlags(Qt::ItemIsEnabled); // not selectable
        m_model->appendRow(item);
        return;
    }

    const int textLen = m_editor->textLength();
    if (textLen <= 0) {
        auto* item = new QStandardItem(tr("(no editor)"));
        item->setFlags(Qt::ItemIsEnabled);
        m_model->appendRow(item);
        return;
    }

    QByteArray utf8 = m_editor->getText(textLen + 1);
    if (utf8.isEmpty()) {
        auto* item = new QStandardItem(tr("(no editor)"));
        item->setFlags(Qt::ItemIsEnabled);
        m_model->appendRow(item);
        return;
    }

    const QString text = QString::fromUtf8(utf8);
    QList<OutlineSymbol> symbols = OutlineRegex::parse(text, m_lexerName);

    if (symbols.isEmpty()) {
        // Fallback to legacy regex parser if OutlineRegex doesn't support this lexer.
        const QStringList lines = text.split(QLatin1Char('\n'));
        const QList<QRegularExpression> patterns = patternsFor(m_lexerName);
        int lineNo = 0;
        for (const QString& rawLine : lines) {
            ++lineNo;
            if (rawLine.size() > 500) continue;
            QString line = rawLine;
            if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
            for (const QRegularExpression& re : patterns) {
                QRegularExpressionMatch m = re.match(line);
                if (m.hasMatch()) {
                    const QString name = m.captured(1);
                    if (name.isEmpty()) continue;
                    OutlineSymbol s;
                    s.name = name;
                    s.kind = QStringLiteral("function");
                    s.line = lineNo;
                    symbols.append(s);
                    break;
                }
            }
        }
    }

    for (const OutlineSymbol& sym : symbols) {
        const QString display =
            QStringLiteral("%1  [%2]  (linha %3)").arg(sym.name, sym.kind).arg(sym.line);
        auto* item = new QStandardItem(display);
        item->setData(sym.line, Qt::UserRole);
        item->setEditable(false);
        m_model->appendRow(item);
    }

    if (m_model->rowCount() == 0) {
        auto* item = new QStandardItem(tr("(no symbols found)"));
        item->setFlags(Qt::ItemIsEnabled);
        m_model->appendRow(item);
    }

    applyFilter();
}

void FunctionListPanel::onFilterTextChanged(const QString& /*text*/)
{
    applyFilter();
}

void FunctionListPanel::applyFilter()
{
    if (!m_model || !m_listView || !m_filterEdit) return;

    const QString needle = m_filterEdit->text().trimmed();
    const bool hasFilter = !needle.isEmpty();

    const int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        bool hide = false;
        if (hasFilter) {
            QStandardItem* it = m_model->item(row, 0);
            if (!it) continue;
            // Only filter rows that carry a line number (real entries).
            const QVariant v = it->data(Qt::UserRole);
            if (v.isValid()) {
                const QString display = it->text();
                hide = !display.contains(needle, Qt::CaseInsensitive);
            }
        }
        m_listView->setRowHidden(row, hide);
    }
}

void FunctionListPanel::onItemActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;
    const QVariant v = index.data(Qt::UserRole);
    if (!v.isValid()) return;
    bool ok = false;
    const int line = v.toInt(&ok);
    if (!ok || line <= 0) return;
    emit gotoLineRequested(line);
}
