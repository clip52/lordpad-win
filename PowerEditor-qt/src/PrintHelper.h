#pragma once

#include <QString>

class ScintillaEdit;
class QWidget;

namespace PrintHelper {

// Show QPrintDialog and print the active editor's content. Returns true if printed.
bool printDocument(ScintillaEdit* editor, QWidget* dialogParent);

// Show QPrintPreviewDialog before printing.
bool previewDocument(ScintillaEdit* editor, QWidget* dialogParent);

} // namespace PrintHelper
