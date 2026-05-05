#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <atomic>
#include <memory>

class QLineEdit;
class QPushButton;
class QToolButton;
class QCheckBox;
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QLabel;
template <typename T> class QFutureWatcher;

class FindInFilesDialog : public QDialog {
    Q_OBJECT
public:
    explicit FindInFilesDialog(QWidget* parent = nullptr);
    ~FindInFilesDialog() override;

    void setSearchFolder(const QString& path);
    void setSearchPattern(const QString& pattern);

signals:
    void openFileRequested(const QString& path, int line);

private slots:
    void onBrowse();
    void onSearch();
    void onStop();
    void onClearResults();
    void onResultActivated(const QModelIndex& index);
    void onSearchFinished();
    void appendBatch();

public:
    struct Hit {
        QString filePath;
        int     lineNumber;   // 1-based
        QString lineText;
    };

private:
    // GUI widgets
    QLineEdit*   m_pattern    = nullptr;
    QLineEdit*   m_folder     = nullptr;
    QToolButton* m_browseBtn  = nullptr;
    QLineEdit*   m_filter     = nullptr;

    QCheckBox*   m_caseChk    = nullptr;
    QCheckBox*   m_wordChk    = nullptr;
    QCheckBox*   m_regexChk   = nullptr;
    QCheckBox*   m_hiddenChk  = nullptr;
    QCheckBox*   m_symlinkChk = nullptr;

    QPushButton* m_searchBtn  = nullptr;
    QPushButton* m_stopBtn    = nullptr;
    QPushButton* m_clearBtn   = nullptr;
    QPushButton* m_closeBtn   = nullptr;

    QTreeView*           m_tree     = nullptr;
    QStandardItemModel*  m_model    = nullptr;
    QLabel*              m_status   = nullptr;

    // worker state
    QFutureWatcher<void>* m_watcher = nullptr;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    std::shared_ptr<std::atomic<int>>  m_filesScanned;
    std::shared_ptr<std::atomic<int>>  m_matchesFound;

    // GUI-thread accumulator filled via QueuedConnection from worker
    QList<Hit> m_pendingHits;
    qint64     m_elapsedMs = 0;
    bool       m_stopped   = false;
    bool       m_running   = false;
    QString    m_rootFolder;

    // file -> top-level item, to group hits by file
    QHash<QString, QStandardItem*> m_fileNodes;

    void setupUi();
    void setSearchingUi(bool searching);
    void updateStatus(const QString& extra = QString());

    // worker thread entry (executed via QtConcurrent::run)
    void runWorker(const QString& folder,
                   const QString& pattern,
                   const QString& filter,
                   bool matchCase,
                   bool wholeWord,
                   bool regex,
                   bool includeHidden,
                   bool followSymlinks);

    // called on GUI thread (via QueuedConnection) by the worker
    Q_INVOKABLE void receiveHits(const QList<FindInFilesDialog::Hit>& hits);
    Q_INVOKABLE void receiveError(const QString& message);
};

Q_DECLARE_METATYPE(FindInFilesDialog::Hit)
