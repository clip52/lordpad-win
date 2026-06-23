#include "CheatsheetPanel.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

CheatsheetPanel::CheatsheetPanel(QWidget* parent) : QDockWidget(tr("Cheatsheets"), parent)
{
    setObjectName(QStringLiteral("CheatsheetPanel"));
    setAllowedAreas(Qt::AllDockWidgetAreas);

    auto* root = new QWidget(this);
    m_dirEdit  = new QLineEdit(root);
    m_pickBtn  = new QPushButton(tr("…"), root);
    m_reloadBtn = new QPushButton(tr("Recarregar"), root);
    m_filter   = new QLineEdit(root);
    m_filter->setPlaceholderText(tr("filtro"));
    m_list = new QListWidget(root); m_list->setMaximumWidth(280);
    m_view = new QTextBrowser(root); m_view->setOpenExternalLinks(true);
    m_status = new QLabel(root);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel(tr("Pasta:"), root));
    row1->addWidget(m_dirEdit, 1);
    row1->addWidget(m_pickBtn);
    row1->addWidget(m_reloadBtn);

    auto* split = new QSplitter(Qt::Horizontal, root);
    auto* leftBox = new QWidget(root);
    auto* leftLay = new QVBoxLayout(leftBox);
    leftLay->setContentsMargins(0,0,0,0);
    leftLay->addWidget(m_filter);
    leftLay->addWidget(m_list, 1);
    split->addWidget(leftBox);
    split->addWidget(m_view);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 3);

    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(4,4,4,4);
    lay->addLayout(row1);
    lay->addWidget(split, 1);
    lay->addWidget(m_status);
    setWidget(root);

    QSettings s;
    QString dir = s.value(QStringLiteral("Cheats/dir")).toString();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        // Probe usual install/dev locations.
        const QStringList candidates = {
            // Windows / portátil: pasta ao lado do executável.
            QCoreApplication::applicationDirPath() + QStringLiteral("/cheatsheets"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../share/lordpad/cheatsheets"),
            QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("cheatsheets"),
                                    QStandardPaths::LocateDirectory),
            QStringLiteral("/usr/share/lordpad/cheatsheets"),
            // dev: build dir → repo/cheatsheets (sibling of build/)
            QCoreApplication::applicationDirPath() + QStringLiteral("/../PowerEditor-qt/cheatsheets"),
        };
        for (const QString& c : candidates) {
            if (!c.isEmpty() && QDir(c).exists()) { dir = c; break; }
        }
    }
    m_dirEdit->setText(dir);

    connect(m_pickBtn,   &QPushButton::clicked, this, &CheatsheetPanel::onPickDir);
    connect(m_reloadBtn, &QPushButton::clicked, this, &CheatsheetPanel::reload);
    connect(m_list, &QListWidget::itemActivated, this, &CheatsheetPanel::onItemActivated);
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* cur, QListWidgetItem*) { if (cur) onItemActivated(cur); });
    connect(m_filter, &QLineEdit::textChanged, this, &CheatsheetPanel::onFilterChanged);

    if (!m_dirEdit->text().isEmpty() && QDir(m_dirEdit->text()).exists()) reload();
}

void CheatsheetPanel::onPickDir()
{
    const QString p = QFileDialog::getExistingDirectory(this, tr("Pasta"), m_dirEdit->text());
    if (!p.isEmpty()) { m_dirEdit->setText(p); reload(); }
}

void CheatsheetPanel::reload()
{
    const QString dir = m_dirEdit->text().trimmed();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        m_list->clear(); m_status->setText(tr("Pasta inválida."));
        return;
    }
    QSettings().setValue(QStringLiteral("Cheats/dir"), dir);
    m_list->clear();
    QDirIterator it(dir, { QStringLiteral("*.md"), QStringLiteral("*.markdown"),
                            QStringLiteral("*.txt") },
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        auto* item = new QListWidgetItem(QDir(dir).relativeFilePath(p), m_list);
        item->setData(Qt::UserRole, p);
    }
    m_status->setText(tr("%1 cheatsheets").arg(m_list->count()));
}

void CheatsheetPanel::onFilterChanged(const QString& text)
{
    for (int i = 0; i < m_list->count(); ++i) {
        auto* it = m_list->item(i);
        it->setHidden(!it->text().contains(text, Qt::CaseInsensitive));
    }
}

QString CheatsheetPanel::markdownToHtml(const QString& md) const
{
    // QTextDocument supports markdown via setMarkdown — bridge via Qt API.
    QTextDocument doc;
    doc.setMarkdown(md);
    return doc.toHtml();
}

void CheatsheetPanel::onItemActivated(QListWidgetItem* it)
{
    if (!it) return;
    QFile f(it->data(Qt::UserRole).toString());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream ts(&f);
    const QString md = ts.readAll();
    if (it->text().endsWith(QStringLiteral(".md")) || it->text().endsWith(QStringLiteral(".markdown")))
        m_view->setHtml(markdownToHtml(md));
    else
        m_view->setPlainText(md);
}
