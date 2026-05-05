#include "GoToLineDialog.h"

#include "ScintillaEdit.h"

#include <QDialog>
#include <QSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>

GoToLineDialog::GoToLineDialog(ScintillaEdit* editor, QWidget* parent)
    : QDialog(parent)
    , m_editor(editor)
    , m_spinBox(new QSpinBox(this))
    , m_infoLabel(new QLabel(this))
{
    setWindowTitle(tr("Go To Line"));
    setWindowFlags(Qt::Dialog);
    setModal(true);

    int lineCount = 1;
    int currentLine = 0;
    if (m_editor) {
        lineCount = static_cast<int>(m_editor->send(SCI_GETLINECOUNT));
        if (lineCount < 1) {
            lineCount = 1;
        }
        const long long pos = m_editor->send(SCI_GETCURRENTPOS);
        currentLine = static_cast<int>(m_editor->send(SCI_LINEFROMPOSITION, pos));
    }

    m_spinBox->setRange(1, lineCount);
    m_spinBox->setValue(currentLine + 1);
    m_spinBox->setSuffix(QString(" of %1").arg(lineCount));

    m_infoLabel->setText(tr("(file has %1 lines)").arg(lineCount));

    auto* formLayout = new QFormLayout();
    formLayout->addRow(new QLabel(tr("Line number:"), this), m_spinBox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_infoLabel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &GoToLineDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void GoToLineDialog::onAccept()
{
    if (!m_editor) {
        return;
    }
    const int target = m_spinBox->value() - 1;
    m_editor->send(SCI_GOTOLINE, target);
    m_editor->send(SCI_SCROLLCARET);
    accept();
}
