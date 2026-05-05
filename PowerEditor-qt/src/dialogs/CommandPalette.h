#pragma once
#include <QDialog>
#include <QString>
#include <QList>

class QLineEdit;
class QListView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QAction;
class QObject;
class QEvent;

class CommandPaletteFilterProxy;

class CommandPalette : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);

    // Replace the action list. Typically the caller scans QMenuBar for all child actions
    // (excluding separators and submenu titles) and passes them here.
    void setActions(const QList<QAction*>& actions);

    // Show, focus the search field, clear filter.
    void presentCentered();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated();

private:
    void triggerSelected();
    void moveSelection(int delta);
    QString buildDisplayText(QAction* action) const;

    QLineEdit*                  m_search    = nullptr;
    QListView*                  m_list      = nullptr;
    QStandardItemModel*         m_model     = nullptr;
    CommandPaletteFilterProxy*  m_proxy     = nullptr;
};
