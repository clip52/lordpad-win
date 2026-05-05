#pragma once
#include <QDialog>

class Snippets;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class SnippetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SnippetsDialog(Snippets* snippets, QWidget* parent = nullptr);

private slots:
    void refreshLanguages();
    void refreshTriggers();
    void onTriggerSelected();
    void onSave();
    void onDelete();
    void onAddLanguage();

private:
    QString currentLanguage() const;
    void clearForm();

    Snippets* m_snippets = nullptr;

    QComboBox*       m_langCombo   = nullptr;
    QPushButton*     m_addLangBtn  = nullptr;
    QListWidget*     m_triggerList = nullptr;

    QLineEdit*       m_triggerEdit = nullptr;
    QPlainTextEdit*  m_bodyEdit    = nullptr;
    QLineEdit*       m_descEdit    = nullptr;
    QPushButton*     m_saveBtn     = nullptr;
    QPushButton*     m_deleteBtn   = nullptr;
};
