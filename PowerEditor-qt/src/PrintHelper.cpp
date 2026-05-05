// PrintHelper.cpp
//
// Build requirement: link against Qt6::PrintSupport (e.g. in CMakeLists.txt:
//   find_package(Qt6 REQUIRED COMPONENTS PrintSupport)
//   target_link_libraries(<target> PRIVATE Qt6::PrintSupport)
// ).

#include "PrintHelper.h"

#include <QPainter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QRect>
#include <QString>
#include <QWidget>

#include "ScintillaEdit.h"

namespace {

// Paint the editor contents onto the supplied QPrinter using
// Scintilla's SCI_FORMATRANGEFULL (exposed as ScintillaEdit::formatRange).
// Honours the printer's print range (AllPages / PageRange / Selection /
// CurrentPage) as set by the QPrintDialog.
bool paintEditorOnPrinter(ScintillaEdit* editor, QPrinter* printer)
{
    if (!editor || !printer) {
        return false;
    }

    // Determine the byte range to print. Scintilla works in byte offsets.
    long rangeStart = 0;
    long rangeEnd   = static_cast<long>(editor->textLength());

    if (printer->printRange() == QPrinter::Selection) {
        const long selStart = static_cast<long>(editor->selectionStart());
        const long selEnd   = static_cast<long>(editor->selectionEnd());
        if (selEnd > selStart) {
            rangeStart = selStart;
            rangeEnd   = selEnd;
        }
    }

    if (rangeEnd <= rangeStart) {
        return false;
    }

    QPainter painter;
    if (!painter.begin(printer)) {
        return false;
    }

    // Page geometry in device pixels. paperRect() is the full physical sheet,
    // pageRect() is the printable area inside the device margins.
    const QRect paperRect = printer->paperRect(QPrinter::DevicePixel).toRect();
    const QRect pageRect  = printer->pageRect(QPrinter::DevicePixel).toRect();

    // For QPrinter::PageRange we honour fromPage()/toPage() by counting pages
    // as Scintilla emits them. Page numbering is 1-based for QPrinter.
    const bool useRange = (printer->printRange() == QPrinter::PageRange);
    const int  fromPage = useRange ? printer->fromPage() : 0;
    const int  toPage   = useRange ? printer->toPage()   : 0;

    const int copies = printer->copyCount();

    for (int copy = 0; copy < copies; ++copy) {
        long pos = rangeStart;
        int  pageNumber = 1;
        bool firstEmittedPage = true;

        while (pos < rangeEnd) {
            const bool drawThisPage = !useRange ||
                (pageNumber >= fromPage && (toPage == 0 || pageNumber <= toPage));

            // Measurement-only pass when the page is outside the requested range.
            const long nextPos = editor->formatRange(
                drawThisPage,
                printer,                                  // target paint device (QPrinter is a QPaintDevice)
                printer,                                  // measure paint device
                drawThisPage ? pageRect : pageRect,       // print rect
                drawThisPage ? paperRect : paperRect,     // page rect
                pos,
                rangeEnd);

            if (nextPos <= pos) {
                // Defensive: avoid infinite loops if Scintilla cannot advance.
                break;
            }

            if (drawThisPage) {
                if (!firstEmittedPage) {
                    printer->newPage();
                }
                firstEmittedPage = false;
            }

            pos = nextPos;
            ++pageNumber;

            if (useRange && toPage != 0 && pageNumber > toPage) {
                break;
            }
        }

        if (copy + 1 < copies) {
            printer->newPage();
        }
    }

    painter.end();
    return true;
}

} // namespace

namespace PrintHelper {

bool printDocument(ScintillaEdit* editor, QWidget* dialogParent)
{
    if (!editor) {
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);

    QPrintDialog dialog(&printer, dialogParent);
    dialog.setWindowTitle(QObject::tr("Print"));
    dialog.setOption(QAbstractPrintDialog::PrintToFile, true);
    dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
    dialog.setOption(QAbstractPrintDialog::PrintCollateCopies, true);

    // Enable the "Selection" radio button if the editor has a non-empty
    // selection.
    if (editor->selectionEnd() > editor->selectionStart()) {
        dialog.setOption(QAbstractPrintDialog::PrintSelection, true);
    }

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    return paintEditorOnPrinter(editor, &printer);
}

bool previewDocument(ScintillaEdit* editor, QWidget* dialogParent)
{
    if (!editor) {
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);

    QPrintPreviewDialog preview(&printer, dialogParent);
    preview.setWindowTitle(QObject::tr("Print Preview"));
    preview.resize(900, 700);

    bool printed = false;

    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested,
                     [editor, &printed](QPrinter* p) {
                         printed = paintEditorOnPrinter(editor, p) || printed;
                     });

    // exec() returns Accepted if the user pressed the "Print" toolbar button,
    // which causes QPrintPreviewDialog to print using the current QPrinter.
    const int result = preview.exec();
    return printed && result == QDialog::Accepted;
}

} // namespace PrintHelper
