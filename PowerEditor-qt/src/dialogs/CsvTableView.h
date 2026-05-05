#pragma once
#include <QDialog>
#include <QString>
class QTableView;
class QComboBox;
class QCheckBox;
class QLabel;

class CsvTableView : public QDialog {
    Q_OBJECT
public:
    explicit CsvTableView(QWidget* parent = nullptr);

    // Load CSV from a UTF-8 byte buffer (e.g., active editor's text).
    void loadCsv(const QString& csvText, const QString& sourceTitle = QString());

private slots:
    void onReload();

private:
    void rebuildFromCache();
    char resolveDelimiter(const QString& text) const;

    QTableView*   m_table       = nullptr;
    QComboBox*    m_delimCombo  = nullptr;
    QCheckBox*    m_headerCheck = nullptr;
    QLabel*       m_status      = nullptr;

    QString m_rawCsv;
    QString m_sourceTitle;
};
