#include "SnippetsDialog.h"

#include "../Snippets.h"

#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>

namespace {
constexpr const char* kGlobalDisplay = "(global)";
}

SnippetsDialog::SnippetsDialog(Snippets* snippets, QWidget* parent)
    : QDialog(parent),
      m_snippets(snippets) {
    setWindowTitle(tr("Snippets"));
    resize(800, 500);

    // ---- Left side: language picker + trigger list ----
    m_langCombo  = new QComboBox(this);
    m_addLangBtn = new QPushButton(tr("+"), this);
    m_addLangBtn->setToolTip(tr("Add a new language bucket"));
    m_addLangBtn->setFixedWidth(28);

    auto* langRow = new QHBoxLayout;
    langRow->setContentsMargins(0, 0, 0, 0);
    langRow->addWidget(new QLabel(tr("Language:"), this));
    langRow->addWidget(m_langCombo, 1);
    langRow->addWidget(m_addLangBtn);

    m_triggerList = new QListWidget(this);

    auto* leftCol = new QVBoxLayout;
    leftCol->addLayout(langRow);
    leftCol->addWidget(m_triggerList, 1);

    // ---- Right side: form ----
    m_triggerEdit = new QLineEdit(this);
    m_bodyEdit    = new QPlainTextEdit(this);
    m_bodyEdit->setTabChangesFocus(false);
    m_descEdit    = new QLineEdit(this);

    m_saveBtn   = new QPushButton(tr("Save"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);

    auto* form = new QFormLayout;
    form->addRow(tr("Trigger:"),     m_triggerEdit);
    form->addRow(tr("Body:"),        m_bodyEdit);
    form->addRow(tr("Description:"), m_descEdit);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_deleteBtn);

    auto* rightCol = new QVBoxLayout;
    rightCol->addLayout(form, 1);
    rightCol->addLayout(btnRow);

    // ---- Top split layout ----
    auto* split = new QHBoxLayout;
    split->addLayout(leftCol, 1);
    split->addLayout(rightCol, 2);

    // ---- Bottom: Close button ----
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->addLayout(split, 1);
    mainLay->addWidget(buttons);

    // ---- Wiring ----
    connect(m_langCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int){ refreshTriggers(); });
    connect(m_triggerList, &QListWidget::itemSelectionChanged,
            this, &SnippetsDialog::onTriggerSelected);
    connect(m_saveBtn,    &QPushButton::clicked, this, &SnippetsDialog::onSave);
    connect(m_deleteBtn,  &QPushButton::clicked, this, &SnippetsDialog::onDelete);
    connect(m_addLangBtn, &QPushButton::clicked, this, &SnippetsDialog::onAddLanguage);

    if (m_snippets) {
        connect(m_snippets, &Snippets::changed, this, [this]() {
            const QString keepLang = currentLanguage();
            const QString keepTrig = m_triggerEdit ? m_triggerEdit->text() : QString();
            refreshLanguages();
            // Try to keep the previously selected language.
            const int idx = m_langCombo->findData(keepLang);
            if (idx >= 0)
                m_langCombo->setCurrentIndex(idx);
            refreshTriggers();
            // Try to reselect previous trigger.
            if (!keepTrig.isEmpty()) {
                for (int i = 0; i < m_triggerList->count(); ++i) {
                    if (m_triggerList->item(i)->text() == keepTrig) {
                        m_triggerList->setCurrentRow(i);
                        break;
                    }
                }
            }
        });
    }

    refreshLanguages();
    refreshTriggers();
}

QString SnippetsDialog::currentLanguage() const {
    if (!m_langCombo || m_langCombo->currentIndex() < 0)
        return QString();
    return m_langCombo->currentData().toString();
}

void SnippetsDialog::clearForm() {
    if (m_triggerEdit) m_triggerEdit->clear();
    if (m_bodyEdit)    m_bodyEdit->clear();
    if (m_descEdit)    m_descEdit->clear();
}

void SnippetsDialog::refreshLanguages() {
    if (!m_snippets || !m_langCombo) return;

    QSignalBlocker block(m_langCombo);
    const QString keep = currentLanguage();
    m_langCombo->clear();

    QStringList langs = m_snippets->allLanguages();
    // Always include the global bucket as the first entry, even if empty.
    if (!langs.contains(QString()))
        langs.prepend(QString());

    for (const QString& lang : langs) {
        const QString display = lang.isEmpty()
                                    ? tr(kGlobalDisplay)
                                    : lang;
        m_langCombo->addItem(display, lang);
    }

    if (!keep.isNull()) {
        const int idx = m_langCombo->findData(keep);
        if (idx >= 0)
            m_langCombo->setCurrentIndex(idx);
    }
}

void SnippetsDialog::refreshTriggers() {
    if (!m_snippets || !m_triggerList) return;

    QSignalBlocker block(m_triggerList);
    m_triggerList->clear();

    const QString lang = currentLanguage();
    const QList<Snippet> list = m_snippets->forLanguage(lang);
    for (const Snippet& s : list) {
        auto* item = new QListWidgetItem(s.trigger, m_triggerList);
        item->setToolTip(s.description);
    }

    clearForm();
}

void SnippetsDialog::onTriggerSelected() {
    if (!m_snippets || !m_triggerList) return;
    QListWidgetItem* item = m_triggerList->currentItem();
    if (!item) {
        clearForm();
        return;
    }
    const QString lang = currentLanguage();
    const QString trigger = item->text();
    const QList<Snippet> list = m_snippets->forLanguage(lang);
    for (const Snippet& s : list) {
        if (s.trigger == trigger) {
            m_triggerEdit->setText(s.trigger);
            m_bodyEdit->setPlainText(s.body);
            m_descEdit->setText(s.description);
            return;
        }
    }
}

void SnippetsDialog::onSave() {
    if (!m_snippets) return;

    const QString lang = currentLanguage();
    Snippet s;
    s.trigger     = m_triggerEdit->text().trimmed();
    s.body        = m_bodyEdit->toPlainText();
    s.description = m_descEdit->text();

    if (s.trigger.isEmpty()) {
        QMessageBox::warning(this, tr("Snippets"),
                             tr("Trigger cannot be empty."));
        return;
    }

    m_snippets->upsert(lang, s);
}

void SnippetsDialog::onDelete() {
    if (!m_snippets || !m_triggerList) return;
    QListWidgetItem* item = m_triggerList->currentItem();
    QString trigger;
    if (item)
        trigger = item->text();
    else
        trigger = m_triggerEdit->text().trimmed();
    if (trigger.isEmpty())
        return;

    const QString lang = currentLanguage();
    const auto answer = QMessageBox::question(
        this, tr("Snippets"),
        tr("Delete snippet \"%1\"?").arg(trigger),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_snippets->remove(lang, trigger);
}

void SnippetsDialog::onAddLanguage() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Add Language"),
        tr("Lexer name (e.g. cpp, python, javascript). Leave empty for global:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    const QString lang = name.trimmed();
    // Add a placeholder entry so the bucket appears, but only once user saves
    // an actual snippet does it persist. We just select/add the entry to the
    // combo here.
    QSignalBlocker block(m_langCombo);
    const int existing = m_langCombo->findData(lang);
    if (existing < 0) {
        const QString display = lang.isEmpty() ? tr(kGlobalDisplay) : lang;
        m_langCombo->addItem(display, lang);
        m_langCombo->setCurrentIndex(m_langCombo->count() - 1);
    } else {
        m_langCombo->setCurrentIndex(existing);
    }
    refreshTriggers();
    if (m_triggerEdit)
        m_triggerEdit->setFocus();
}
