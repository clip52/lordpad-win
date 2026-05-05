#include "PreferencesDialog.h"

#include <QDialog>
#include <QTabWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFont>

#include "Settings.h"

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontFamily(new QFontComboBox(this))
    , m_fontSize(new QSpinBox(this))
    , m_tabWidth(new QSpinBox(this))
    , m_useSpaces(new QCheckBox(tr("Use spaces instead of tabs"), this))
    , m_darkTheme(new QCheckBox(tr("Dark theme"), this))
    , m_showLineNumbers(new QCheckBox(tr("Show line numbers"), this))
    , m_wordWrap(new QCheckBox(tr("Word wrap"), this))
    , m_buttons(nullptr)
{
    setWindowTitle(tr("Preferences"));

    m_fontSize->setRange(6, 32);
    m_tabWidth->setRange(1, 16);

    auto* tabs = new QTabWidget(this);

    // ----- General tab -----
    auto* generalTab = new QWidget(tabs);
    auto* generalLayout = new QFormLayout(generalTab);
    generalLayout->addRow(tr("Font family:"), m_fontFamily);
    generalLayout->addRow(tr("Font size:"), m_fontSize);
    generalLayout->addRow(tr("Tab width:"), m_tabWidth);
    generalLayout->addRow(m_useSpaces);
    generalLayout->addRow(m_darkTheme);
    tabs->addTab(generalTab, tr("General"));

    // ----- Editor tab -----
    auto* editorTab = new QWidget(tabs);
    auto* editorLayout = new QVBoxLayout(editorTab);
    editorLayout->addWidget(m_showLineNumbers);
    editorLayout->addWidget(m_wordWrap);
    editorLayout->addStretch(1);
    tabs->addTab(editorTab, tr("Editor"));

    // ----- About tab -----
    auto* aboutTab = new QWidget(tabs);
    auto* aboutLayout = new QVBoxLayout(aboutTab);

    auto* nameLabel = new QLabel(tr("notepadpp-qt"), aboutTab);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameLabel->setFont(nameFont);

    auto* versionLabel = new QLabel(tr("Version 0.1.0"), aboutTab);

    auto* blurbLabel = new QLabel(
        tr("notepadpp-qt \xe2\x80\x94 native Linux Qt6 port of Notepad++. "
           "Foundation milestone."),
        aboutTab);
    blurbLabel->setWordWrap(true);

    aboutLayout->addWidget(nameLabel);
    aboutLayout->addWidget(versionLabel);
    aboutLayout->addSpacing(8);
    aboutLayout->addWidget(blurbLabel);
    aboutLayout->addStretch(1);
    tabs->addTab(aboutTab, tr("About"));

    // ----- Buttons -----
    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        Qt::Horizontal,
        this);

    connect(m_buttons, &QDialogButtonBox::accepted,
            this, &PreferencesDialog::onAccept);
    connect(m_buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::onApply);

    // ----- Top-level layout -----
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs);
    mainLayout->addWidget(m_buttons);

    loadSettings();
}

void PreferencesDialog::loadSettings()
{
    Settings& s = Settings::instance();
    m_fontFamily->setCurrentFont(QFont(s.fontFamily()));
    m_fontSize->setValue(s.fontSize());
    m_tabWidth->setValue(s.tabWidth());
    m_useSpaces->setChecked(s.useSpaces());
    m_darkTheme->setChecked(s.darkTheme());
    m_showLineNumbers->setChecked(s.showLineNumbers());
    m_wordWrap->setChecked(s.wordWrap());
}

void PreferencesDialog::applySettings()
{
    Settings& s = Settings::instance();
    s.setFontFamily(m_fontFamily->currentFont().family());
    s.setFontSize(m_fontSize->value());
    s.setTabWidth(m_tabWidth->value());
    s.setUseSpaces(m_useSpaces->isChecked());
    s.setDarkTheme(m_darkTheme->isChecked());
    s.setShowLineNumbers(m_showLineNumbers->isChecked());
    s.setWordWrap(m_wordWrap->isChecked());
    s.save();
    emit preferencesChanged();
}

void PreferencesDialog::onAccept()
{
    applySettings();
    accept();
}

void PreferencesDialog::onApply()
{
    applySettings();
}
