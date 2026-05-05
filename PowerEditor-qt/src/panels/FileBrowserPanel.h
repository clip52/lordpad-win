#pragma once
#include <QDockWidget>
#include <QString>
class QTreeView;
class QFileSystemModel;
class QToolBar;
class QLineEdit;

class FileBrowserPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit FileBrowserPanel(QWidget* parent = nullptr);

    // Set the root folder shown by the tree. Empty string = home dir.
    void setRootPath(const QString& folder);
    QString rootPath() const;

signals:
    void openFileRequested(const QString& path);

private slots:
    void onOpenFolderClicked();
    void onUpClicked();
    void onHomeClicked();
    void onRefreshClicked();
    void onFilterChanged(const QString& text);
    void onDoubleClicked(const QModelIndex& index);

private:
    QToolBar*          m_toolbar = nullptr;
    QLineEdit*         m_filterEdit = nullptr;
    QTreeView*         m_tree = nullptr;
    QFileSystemModel*  m_model = nullptr;
    QString            m_rootPath;
};
