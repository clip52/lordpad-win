#pragma once

#include <QDialog>

class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QDialogButtonBox;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

signals:
    void preferencesChanged();   // emitted on accept after Settings::save()

private slots:
    void onAccept();
    void onApply();

private:
    void loadSettings();
    void applySettings();

    // General tab
    QFontComboBox* m_fontFamily;
    QSpinBox*      m_fontSize;
    QSpinBox*      m_tabWidth;
    QCheckBox*     m_useSpaces;
    QCheckBox*     m_darkTheme;

    // Editor tab
    QCheckBox*     m_showLineNumbers;
    QCheckBox*     m_wordWrap;

    QDialogButtonBox* m_buttons;
};
