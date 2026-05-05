#include "FileBrowserPanel.h"

#include <QDockWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QToolBar>
#include <QToolButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QHeaderView>
#include <QFileInfo>
#include <QWidget>

namespace {
constexpr const char* kSettingsKeyRootPath = "fileBrowser/rootPath";
}

FileBrowserPanel::FileBrowserPanel(QWidget* parent)
    : QDockWidget(tr("File Browser"), parent)
{
    setObjectName("FileBrowserPanel");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* container = new QWidget(this);
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ----- Toolbar -----
    m_toolbar = new QToolBar(container);
    m_toolbar->setIconSize(QSize(16, 16));

    auto* btnOpenFolder = new QToolButton(m_toolbar);
    btnOpenFolder->setText(tr("Open Folder..."));
    btnOpenFolder->setToolTip(tr("Open Folder..."));
    btnOpenFolder->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(btnOpenFolder, &QToolButton::clicked,
            this, &FileBrowserPanel::onOpenFolderClicked);
    m_toolbar->addWidget(btnOpenFolder);

    auto* btnUp = new QToolButton(m_toolbar);
    btnUp->setText(tr("Up"));
    btnUp->setToolTip(tr("Up"));
    btnUp->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(btnUp, &QToolButton::clicked,
            this, &FileBrowserPanel::onUpClicked);
    m_toolbar->addWidget(btnUp);

    auto* btnHome = new QToolButton(m_toolbar);
    btnHome->setText(tr("Home"));
    btnHome->setToolTip(tr("Home"));
    btnHome->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(btnHome, &QToolButton::clicked,
            this, &FileBrowserPanel::onHomeClicked);
    m_toolbar->addWidget(btnHome);

    auto* btnRefresh = new QToolButton(m_toolbar);
    btnRefresh->setText(tr("Refresh"));
    btnRefresh->setToolTip(tr("Refresh"));
    btnRefresh->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(btnRefresh, &QToolButton::clicked,
            this, &FileBrowserPanel::onRefreshClicked);
    m_toolbar->addWidget(btnRefresh);

    layout->addWidget(m_toolbar);

    // ----- Filter -----
    m_filterEdit = new QLineEdit(container);
    m_filterEdit->setPlaceholderText(tr("Filter (e.g. *.cpp)"));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &FileBrowserPanel::onFilterChanged);
    layout->addWidget(m_filterEdit);

    // ----- Model -----
    m_model = new QFileSystemModel(this);
    m_model->setReadOnly(true);
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    m_model->setNameFilterDisables(false);

    // ----- Tree -----
    m_tree = new QTreeView(container);
    m_tree->setModel(m_model);
    m_tree->setIndentation(12);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);
    m_tree->setSortingEnabled(false);
    m_tree->setHeaderHidden(false);
    // Hide all but the Name column.
    m_tree->setColumnHidden(1, true);
    m_tree->setColumnHidden(2, true);
    m_tree->setColumnHidden(3, true);
    if (auto* hdr = m_tree->header()) {
        hdr->setStretchLastSection(true);
        hdr->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    connect(m_tree, &QTreeView::doubleClicked,
            this, &FileBrowserPanel::onDoubleClicked);

    layout->addWidget(m_tree, 1);

    container->setLayout(layout);
    setWidget(container);

    // ----- Initial root: persisted value if valid, else home -----
    QSettings settings;
    QString initial = settings.value(kSettingsKeyRootPath).toString();
    if (initial.isEmpty() || !QFileInfo(initial).isDir()) {
        initial = QDir::homePath();
    }
    setRootPath(initial);
}

void FileBrowserPanel::setRootPath(const QString& folder)
{
    QString target = folder;
    if (target.isEmpty()) {
        target = QDir::homePath();
    }

    const QFileInfo fi(target);
    if (!fi.exists() || !fi.isDir()) {
        target = QDir::homePath();
    }

    m_rootPath = QDir(target).absolutePath();

    if (m_model) {
        m_model->setRootPath(m_rootPath);
    }
    if (m_tree && m_model) {
        m_tree->setRootIndex(m_model->index(m_rootPath));
    }

    QSettings settings;
    settings.setValue(kSettingsKeyRootPath, m_rootPath);
}

QString FileBrowserPanel::rootPath() const
{
    return m_rootPath;
}

void FileBrowserPanel::onOpenFolderClicked()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this,
        tr("Open Folder..."),
        m_rootPath.isEmpty() ? QDir::homePath() : m_rootPath);
    if (!chosen.isEmpty()) {
        setRootPath(chosen);
    }
}

void FileBrowserPanel::onUpClicked()
{
    if (m_rootPath.isEmpty()) {
        return;
    }
    QDir d(m_rootPath);
    if (d.cdUp()) {
        setRootPath(d.absolutePath());
    }
}

void FileBrowserPanel::onHomeClicked()
{
    setRootPath(QDir::homePath());
}

void FileBrowserPanel::onRefreshClicked()
{
    if (m_model && !m_rootPath.isEmpty()) {
        // Re-scan: setRootPath() on QFileSystemModel re-resolves the directory.
        m_model->setRootPath(m_rootPath);
        if (m_tree) {
            m_tree->setRootIndex(m_model->index(m_rootPath));
        }
    }
}

void FileBrowserPanel::onFilterChanged(const QString& text)
{
    if (!m_model) {
        return;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_model->setNameFilters(QStringList());
    } else {
        m_model->setNameFilters(QStringList{trimmed});
    }
}

void FileBrowserPanel::onDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid() || !m_model) {
        return;
    }
    if (m_model->isDir(index)) {
        // QTreeView toggles expansion automatically on double-click.
        return;
    }
    const QString path = m_model->filePath(index);
    if (!path.isEmpty()) {
        emit openFileRequested(path);
    }
}
