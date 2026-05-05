#include "CsvTableView.h"

#include <QDialog>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QAbstractTableModel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QStringList>
#include <QVector>

namespace {

// ---------------------------------------------------------------------------
// CsvModel: read-only QAbstractTableModel backed by QVector<QStringList>.
// ---------------------------------------------------------------------------
class CsvModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit CsvModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setData(const QStringList& headers, const QVector<QStringList>& rows) {
        beginResetModel();
        m_headers = headers;
        m_rows    = rows;
        m_columnCount = m_headers.size();
        for (const auto& r : m_rows)
            if (r.size() > m_columnCount) m_columnCount = r.size();
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return m_rows.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return m_columnCount;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid()) return {};
        if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
        const int r = index.row();
        const int c = index.column();
        if (r < 0 || r >= m_rows.size()) return {};
        const QStringList& row = m_rows.at(r);
        if (c < 0 || c >= row.size()) return {};
        return row.at(c);
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) return QAbstractTableModel::headerData(section, orientation, role);
        if (orientation == Qt::Horizontal) {
            if (section >= 0 && section < m_headers.size()) return m_headers.at(section);
            return QStringLiteral("Column %1").arg(section + 1);
        }
        return section + 1;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) return Qt::NoItemFlags;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

private:
    QStringList            m_headers;
    QVector<QStringList>   m_rows;
    int                    m_columnCount = 0;
};

// ---------------------------------------------------------------------------
// CSV parser (RFC 4180-ish). Returns rows; honours quoted fields with embedded
// delimiters, embedded newlines, and "" as a literal quote. Handles CRLF & LF.
// ---------------------------------------------------------------------------
QVector<QStringList> parseCsv(const QString& text, QChar delim)
{
    QVector<QStringList> rows;
    QStringList current;
    QString     field;
    bool        inQuotes  = false;
    bool        rowHasAny = false;

    auto endField = [&]() {
        current.append(field);
        field.clear();
        rowHasAny = true;
    };

    auto endRow = [&]() {
        if (rowHasAny || !current.isEmpty()) {
            rows.append(current);
        }
        current.clear();
        rowHasAny = false;
    };

    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);

        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                // Escaped quote ""?
                if (i + 1 < n && text.at(i + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.append(ch);
            }
            continue;
        }

        // Not in quotes.
        if (ch == QLatin1Char('"') && field.isEmpty()) {
            inQuotes = true;
            rowHasAny = true;
            continue;
        }
        if (ch == delim) {
            endField();
            continue;
        }
        if (ch == QLatin1Char('\r')) {
            // Treat CR or CRLF as a row terminator.
            endField();
            endRow();
            if (i + 1 < n && text.at(i + 1) == QLatin1Char('\n')) ++i;
            continue;
        }
        if (ch == QLatin1Char('\n')) {
            endField();
            endRow();
            continue;
        }
        field.append(ch);
        rowHasAny = true;
    }

    // Flush trailing field/row (no terminating newline).
    if (!field.isEmpty() || !current.isEmpty() || rowHasAny) {
        endField();
        endRow();
    }

    // Strip purely empty trailing lines.
    while (!rows.isEmpty()) {
        const QStringList& last = rows.last();
        bool empty = true;
        for (const auto& f : last) {
            if (!f.isEmpty()) { empty = false; break; }
        }
        if (empty) rows.removeLast();
        else break;
    }

    return rows;
}

char autoDetectDelimiter(const QString& text)
{
    // Find first non-empty line.
    int start = 0;
    const int n = text.size();
    while (start < n) {
        int eol = start;
        while (eol < n && text.at(eol) != QLatin1Char('\n') && text.at(eol) != QLatin1Char('\r'))
            ++eol;
        const QStringView line = QStringView(text).mid(start, eol - start);
        bool nonEmpty = false;
        for (QChar c : line) if (!c.isSpace()) { nonEmpty = true; break; }
        if (nonEmpty) {
            int comma = 0, semi = 0, tab = 0, pipe = 0;
            for (QChar c : line) {
                if      (c == QLatin1Char(','))  ++comma;
                else if (c == QLatin1Char(';'))  ++semi;
                else if (c == QLatin1Char('\t')) ++tab;
                else if (c == QLatin1Char('|'))  ++pipe;
            }
            int best = comma; char ch = ',';
            if (semi > best) { best = semi; ch = ';'; }
            if (tab  > best) { best = tab;  ch = '\t'; }
            if (pipe > best) { best = pipe; ch = '|'; }
            if (best == 0)   return ',';
            return ch;
        }
        // Skip the EOL char(s).
        if (eol < n && text.at(eol) == QLatin1Char('\r')) {
            ++eol;
            if (eol < n && text.at(eol) == QLatin1Char('\n')) ++eol;
        } else if (eol < n && text.at(eol) == QLatin1Char('\n')) {
            ++eol;
        }
        start = eol;
    }
    return ',';
}

} // namespace

// ---------------------------------------------------------------------------
// CsvTableView
// ---------------------------------------------------------------------------
CsvTableView::CsvTableView(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("CSV Table View"));
    resize(1000, 700);
    setSizeGripEnabled(true);

    auto* outer = new QVBoxLayout(this);

    // Top toolbar.
    auto* toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    auto* delimLabel = new QLabel(tr("Delimiter:"), toolbar);
    toolbar->addWidget(delimLabel);

    m_delimCombo = new QComboBox(toolbar);
    m_delimCombo->addItem(tr("Auto"),         QVariant(QChar(0)));
    m_delimCombo->addItem(tr("Comma (,)"),    QVariant(QChar(',')));
    m_delimCombo->addItem(tr("Semicolon (;)"),QVariant(QChar(';')));
    m_delimCombo->addItem(tr("Tab (\\t)"),    QVariant(QChar('\t')));
    m_delimCombo->addItem(tr("Pipe (|)"),     QVariant(QChar('|')));
    m_delimCombo->setCurrentIndex(0);
    toolbar->addWidget(m_delimCombo);

    toolbar->addSeparator();

    m_headerCheck = new QCheckBox(tr("First row is header"), toolbar);
    m_headerCheck->setChecked(true);
    toolbar->addWidget(m_headerCheck);

    toolbar->addSeparator();

    auto* reloadBtn = new QPushButton(tr("Reload"), toolbar);
    toolbar->addWidget(reloadBtn);

    outer->addWidget(toolbar);

    // Central table view.
    m_table = new QTableView(this);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSortingEnabled(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* model = new CsvModel(this);
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    m_table->setModel(proxy);

    outer->addWidget(m_table, 1);

    // Bottom: status + button box.
    auto* bottom = new QHBoxLayout();
    m_status = new QLabel(tr("0 rows \xC3\x97 0 columns"), this);
    m_status->setStyleSheet(QStringLiteral("color: #666;"));
    bottom->addWidget(m_status, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottom->addWidget(buttons);
    outer->addLayout(bottom);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(reloadBtn, &QPushButton::clicked, this, &CsvTableView::onReload);
}

char CsvTableView::resolveDelimiter(const QString& text) const
{
    const QChar selected = m_delimCombo->currentData().toChar();
    if (selected.unicode() == 0) return autoDetectDelimiter(text);
    return static_cast<char>(selected.toLatin1());
}

void CsvTableView::loadCsv(const QString& csvText, const QString& sourceTitle)
{
    m_rawCsv      = csvText;
    m_sourceTitle = sourceTitle;

    if (sourceTitle.isEmpty())
        setWindowTitle(tr("CSV Table View"));
    else
        setWindowTitle(tr("CSV Table View \xE2\x80\x94 %1").arg(sourceTitle));

    rebuildFromCache();
}

void CsvTableView::onReload()
{
    rebuildFromCache();
}

void CsvTableView::rebuildFromCache()
{
    const char delim = resolveDelimiter(m_rawCsv);
    QVector<QStringList> rows = parseCsv(m_rawCsv, QChar::fromLatin1(delim));

    QStringList headers;
    int columnCount = 0;
    for (const auto& r : rows) if (r.size() > columnCount) columnCount = r.size();

    if (m_headerCheck->isChecked() && !rows.isEmpty()) {
        headers = rows.first();
        rows.removeFirst();
        // Pad header if narrower than widest data row.
        while (headers.size() < columnCount) headers.append(QStringLiteral("Column %1").arg(headers.size() + 1));
    } else {
        for (int i = 0; i < columnCount; ++i)
            headers.append(QStringLiteral("Column %1").arg(i + 1));
    }

    auto* proxy = qobject_cast<QSortFilterProxyModel*>(m_table->model());
    auto* model = proxy ? qobject_cast<CsvModel*>(proxy->sourceModel()) : nullptr;
    if (model) model->setData(headers, rows);

    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);

    const int rCount = rows.size();
    const int cCount = headers.size();
    m_status->setText(tr("%1 rows \xC3\x97 %2 columns").arg(rCount).arg(cCount));
}

#include "CsvTableView.moc"
