#include "WhitespaceView.h"

#include <QSettings>

namespace {
constexpr const char* kKeyWhitespace   = "view/whitespace";
constexpr const char* kKeyEol          = "view/eol";
constexpr const char* kKeyIndentGuides = "view/indentGuides";

// Scintilla constants (mirrored to avoid pulling Scintilla.h here).
constexpr int kScwsInvisible      = 0;
constexpr int kScwsVisibleAlways  = 1;
constexpr int kScIvNone           = 0;
constexpr int kScIvLookBoth       = 4;

// Subtle gray for whitespace marks (·, →).
constexpr int kWsForeColor = 0xA0A0A0; // 0x00BBGGRR — gray
constexpr int kWsDotSize   = 2;
} // namespace

WhitespaceView::WhitespaceView(QObject* parent)
    : QObject(parent)
{
    QSettings s;
    m_whitespaceVisible   = s.value(kKeyWhitespace,   false).toBool();
    m_eolVisible          = s.value(kKeyEol,          false).toBool();
    m_indentGuidesVisible = s.value(kKeyIndentGuides, false).toBool();
}

bool WhitespaceView::isWhitespaceVisible() const   { return m_whitespaceVisible; }
bool WhitespaceView::isEolVisible() const          { return m_eolVisible; }
bool WhitespaceView::areIndentGuidesVisible() const { return m_indentGuidesVisible; }

void WhitespaceView::setWhitespaceVisible(bool b)
{
    if (m_whitespaceVisible == b) return;
    m_whitespaceVisible = b;
    QSettings().setValue(kKeyWhitespace, b);
    emit changed();
}

void WhitespaceView::setEolVisible(bool b)
{
    if (m_eolVisible == b) return;
    m_eolVisible = b;
    QSettings().setValue(kKeyEol, b);
    emit changed();
}

void WhitespaceView::setIndentGuidesVisible(bool b)
{
    if (m_indentGuidesVisible == b) return;
    m_indentGuidesVisible = b;
    QSettings().setValue(kKeyIndentGuides, b);
    emit changed();
}

void WhitespaceView::applyTo(ScintillaEdit* editor)
{
    if (!editor) return;

    editor->setViewWS(m_whitespaceVisible ? kScwsVisibleAlways : kScwsInvisible);
    editor->setViewEOL(m_eolVisible);
    editor->setIndentationGuides(m_indentGuidesVisible ? kScIvLookBoth : kScIvNone);

    // Style the whitespace dot/arrow markers.
    editor->setWhitespaceFore(true, kWsForeColor);
    editor->setWhitespaceSize(kWsDotSize);
}
