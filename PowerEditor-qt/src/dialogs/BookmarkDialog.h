#pragma once
#include <QDialog>

class ScintillaEdit;
class BookmarkManager;
class QListView;
class QStandardItemModel;

class BookmarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit BookmarkDialog(BookmarkManager* manager, ScintillaEdit* editor, QWidget* parent = nullptr);

signals:
    void gotoLineRequested(int line);   // 1-based

private:
    BookmarkManager* m_manager = nullptr;
    ScintillaEdit* m_editor = nullptr;
    QListView* m_view = nullptr;
    QStandardItemModel* m_model = nullptr;
};
