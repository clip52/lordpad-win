#include "BraceMatcher.h"

#include <ScintillaTypes.h>

namespace {
// Scintilla style numbers for brace highlighting (from Scintilla.h).
constexpr int kStyleBraceLight = 34; // STYLE_BRACELIGHT
constexpr int kStyleBraceBad   = 35; // STYLE_BRACEBAD

// Color helpers — Scintilla's styleSetFore/Back take an integer of the
// form 0x00BBGGRR (note: NOT the usual 0xRRGGBB).
constexpr sptr_t scintillaColor(int r, int g, int b) {
    return (static_cast<sptr_t>(b) << 16)
         | (static_cast<sptr_t>(g) << 8)
         |  static_cast<sptr_t>(r);
}
} // namespace

BraceMatcher::BraceMatcher(QObject* parent)
    : QObject(parent) {
}

void BraceMatcher::setActiveEditor(ScintillaEdit* editor) {
    if (m_editor == editor) {
        return;
    }

    if (m_editor) {
        disconnect(m_editor, &ScintillaEditBase::updateUi,
                   this, &BraceMatcher::onUpdateUi);
        // Clear any leftover highlight on the old editor.
        m_editor->braceHighlight(-1, -1);
    }

    m_editor = editor;

    if (m_editor) {
        configureStyles(m_editor);
        connect(m_editor, &ScintillaEditBase::updateUi,
                this, &BraceMatcher::onUpdateUi);
    }
}

void BraceMatcher::configureStyles(ScintillaEdit* editor) {
    if (!editor) {
        return;
    }

    // STYLE_BRACELIGHT — black on light cyan, bold.
    editor->styleSetFore(kStyleBraceLight, scintillaColor(0x00, 0x00, 0x00));
    editor->styleSetBack(kStyleBraceLight, scintillaColor(0x80, 0xFF, 0xFF));
    editor->styleSetBold(kStyleBraceLight, true);

    // STYLE_BRACEBAD — red foreground, bold.
    editor->styleSetFore(kStyleBraceBad, scintillaColor(0xFF, 0x00, 0x00));
    editor->styleSetBold(kStyleBraceBad, true);
}

bool BraceMatcher::isBraceChar(int ch) {
    switch (ch) {
    case '(': case ')':
    case '[': case ']':
    case '{': case '}':
    case '<': case '>':
        return true;
    default:
        return false;
    }
}

bool BraceMatcher::locateBrace(ScintillaEdit* editor,
                               sptr_t& bracePos,
                               sptr_t& matchPos) const {
    if (!editor) {
        return false;
    }

    const sptr_t caret = editor->currentPos();

    // Prefer the character at the caret; fall back to the one before it.
    sptr_t candidates[2] = { caret, caret - 1 };
    for (sptr_t pos : candidates) {
        if (pos < 0) {
            continue;
        }
        const int ch = static_cast<int>(editor->charAt(pos));
        if (isBraceChar(ch)) {
            bracePos = pos;
            // maxReStyle == 0: do not re-style during the search.
            matchPos = editor->braceMatch(pos, 0);
            return true;
        }
    }

    return false;
}

void BraceMatcher::onUpdateUi(Scintilla::Update /*updated*/) {
    if (!m_editor) {
        return;
    }

    sptr_t bracePos = -1;
    sptr_t matchPos = -1;

    if (locateBrace(m_editor, bracePos, matchPos)) {
        if (matchPos == -1) {
            m_editor->braceBadLight(bracePos);
        } else {
            m_editor->braceHighlight(bracePos, matchPos);
        }
    } else {
        // No brace adjacent to the caret — clear any prior highlight.
        m_editor->braceHighlight(-1, -1);
    }
}

void BraceMatcher::gotoMatchingBrace() {
    if (!m_editor) {
        return;
    }

    sptr_t bracePos = -1;
    sptr_t matchPos = -1;

    if (!locateBrace(m_editor, bracePos, matchPos)) {
        return;
    }
    if (matchPos == -1) {
        return;
    }

    m_editor->gotoPos(matchPos);
    m_editor->scrollCaret();
}
