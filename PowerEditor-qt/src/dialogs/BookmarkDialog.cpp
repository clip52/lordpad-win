#include "BookmarkDialog.h"

#include "../BookmarkManager.h"
#include "ScintillaEdit.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

BookmarkDialog::BookmarkDialog(BookmarkManager* manager, ScintillaEdit* editor, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_editor(editor)
{
    setWindowTitle(tr("Bookmarks"));
    resize(600, 400);

    auto* layout = new QVBoxLayout(this);

    const QList<int> lines = m_manager ? m_manager->bookmarkedLines() : QList<int>{};

    if (lines.isEmpty()) {
        auto* placeholder = new QLabel(tr("(no bookmarks)"), this);
        placeholder->setAlignment(Qt::AlignCenter);
        layout->addWidget(placeholder, 1);
    } else {
        m_model = new QStandardItemModel(this);
        for (int line : lines) {
            QString text;
            if (m_editor) {
                // Pull the line text from the editor; getLine returns the raw
                // line including any trailing newline, so trim it.
                text = m_editor->getLine(line);
            }
            text = text.trimmed();
            if (text.size() > 100)
                text = text.left(100);

            const QString display = tr("Line %1: %2")
                                        .arg(line + 1)
                                        .arg(text);
            auto* item = new QStandardItem(display);
            item->setEditable(false);
            // Stash the 0-based line for activation lookup.
            item->setData(line, Qt::UserRole + 1);
            m_model->appendRow(item);
        }

        m_view = new QListView(this);
        m_view->setModel(m_model);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(m_view, 1);

        connect(m_view, &QAbstractItemView::doubleClicked, this,
                [this](const QModelIndex& idx) {
                    if (!idx.isValid())
                        return;
                    const int line = idx.data(Qt::UserRole + 1).toInt();
                    emit gotoLineRequested(line + 1);
                    accept();
                });
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* clearAllBtn = buttons->addButton(tr("Clear All"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        if (m_manager)
            m_manager->clearAll();
        accept();
    });
}
