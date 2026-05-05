#pragma once
class ScintillaEdit;
class QWidget;

namespace ColorPickerHelper {

// Open a QColorDialog. Initial color = parsed from current selection if it looks like
// "#RRGGBB", "#RRGGBBAA", "rgb(r,g,b)", or "rgba(r,g,b,a)". On accept, replace selection
// with the picked color in the same format detected (or "#RRGGBB" if no selection).
// Returns true if a color was picked AND inserted; false if dialog cancelled.
bool pickAndReplace(ScintillaEdit* editor, QWidget* dialogParent);

} // namespace ColorPickerHelper
