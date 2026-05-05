#include "MacroDialog.h"

#include <QObject>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDataStream>
#include <QByteArray>

#include "../MacroRecorder.h"
#include "ScintillaEdit.h"

MacroDialog::MacroDialog(MacroRecorder* recorder, ScintillaEdit* editor, QWidget* parent)
    : QDialog(parent)
    , m_recorder(recorder)
    , m_editor(editor)
{
    setWindowTitle(tr("Macros"));

    m_list      = new QListWidget(this);
    m_btnRecord = new QPushButton(tr("Record"),    this);
    m_btnStop   = new QPushButton(tr("Stop"),      this);
    m_btnPlay   = new QPushButton(tr("Play"),      this);
    m_btnSaveAs = new QPushButton(tr("Save As..."), this);
    m_btnLoad   = new QPushButton(tr("Load"),      this);
    m_btnDelete = new QPushButton(tr("Delete"),    this);
    m_status    = new QLabel(tr("(idle)"), this);

    auto* buttonsCol = new QVBoxLayout();
    buttonsCol->addWidget(m_btnRecord);
    buttonsCol->addWidget(m_btnStop);
    buttonsCol->addWidget(m_btnPlay);
    buttonsCol->addWidget(m_btnSaveAs);
    buttonsCol->addWidget(m_btnLoad);
    buttonsCol->addWidget(m_btnDelete);
    buttonsCol->addStretch(1);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(m_list, 1);
    topRow->addLayout(buttonsCol);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topRow, 1);
    mainLayout->addWidget(m_status);

    if (m_recorder) {
        connect(m_btnRecord, &QPushButton::clicked, this, &MacroDialog::onRecord);
        connect(m_btnStop,   &QPushButton::clicked, this, &MacroDialog::onStop);
        connect(m_btnPlay,   &QPushButton::clicked, this, &MacroDialog::onPlay);
        connect(m_btnSaveAs, &QPushButton::clicked, this, &MacroDialog::onSaveAs);
        connect(m_btnLoad,   &QPushButton::clicked, this, &MacroDialog::onLoad);
        connect(m_btnDelete, &QPushButton::clicked, this, &MacroDialog::onDelete);

        connect(m_recorder, &MacroRecorder::recordingChanged,
                this, [this](bool){ updateStatus(); });
        connect(m_recorder, &MacroRecorder::macroChanged,
                this, &MacroDialog::updateStatus);
    }

    refreshList();
    updateStatus();
}

void MacroDialog::refreshList() {
    m_list->clear();
    if (!m_recorder) {
        return;
    }
    m_list->addItems(m_recorder->savedMacroNames());
}

void MacroDialog::updateStatus() {
    if (!m_recorder) {
        m_status->setText(tr("(idle)"));
        return;
    }
    if (m_recorder->isRecording()) {
        const int n = m_recorder->currentMacro().size();
        m_status->setText(tr("Recording: %1 steps").arg(n));
    } else {
        const int n = m_recorder->currentMacro().size();
        if (n > 0) {
            m_status->setText(tr("Recording: %1 steps").arg(n));
        } else {
            m_status->setText(tr("(idle)"));
        }
    }
}

void MacroDialog::onRecord() {
    if (!m_recorder) return;
    if (m_editor) {
        m_recorder->setActiveEditor(m_editor);
    }
    m_recorder->startRecording();
    updateStatus();
}

void MacroDialog::onStop() {
    if (!m_recorder) return;
    m_recorder->stopRecording();
    updateStatus();
}

void MacroDialog::onPlay() {
    if (!m_recorder) return;
    if (m_editor) {
        m_recorder->setActiveEditor(m_editor);
    }
    m_recorder->play();
}

void MacroDialog::onSaveAs() {
    if (!m_recorder) return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        tr("Save Macro"),
        tr("Macro name:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_recorder->saveCurrentAs(name.trimmed());
    refreshList();
}

void MacroDialog::onLoad() {
    if (!m_recorder) return;
    auto* item = m_list->currentItem();
    if (!item) {
        return;
    }
    m_recorder->loadByName(item->text());
    updateStatus();
}

void MacroDialog::onDelete() {
    if (!m_recorder) return;
    auto* item = m_list->currentItem();
    if (!item) {
        return;
    }
    const QString name = item->text();
    const auto answer = QMessageBox::question(
        this,
        tr("Delete Macro"),
        tr("Delete macro \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_recorder->deleteByName(name);
    refreshList();
}
