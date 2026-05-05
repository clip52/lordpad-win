// EditEnhancements.cpp
//
// Implementation notes:
//   * Auto-close hooks ScintillaEdit::charAdded(int). The Scintilla
//     notification fires *after* the character has been inserted, with the
//     caret already moved past it -- so currentPos() points at the slot just
//     after the freshly typed char. We insert the matching closer there and
//     restore the caret to the same position so the user can keep typing.
//   * Indent / unindent operates per touched line; we detect tab-vs-spaces
//     mode from the editor itself (useTabs()) so the caller only needs to
//     pass tabWidth.
//   * reloadFromDisk reads the file as UTF-8 and swaps the buffer in a single
//     undo action. Lexer / theme are intentionally untouched -- that's the
//     caller's responsibility.

#include "EditEnhancements.h"

#include "ScintillaEdit.h"

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QString>

namespace {

// Persistence key. Inlined to keep this component dependency-free.
constexpr const char* kKeyAutoClose = "edit/autoClose";

// Map an opener character to its matching closer. Returns 0 if `ch` is not an
// opener we handle.
char closerFor(int ch) {
    switch (ch) {
        case '(':  return ')';
        case '[':  return ']';
        case '{':  return '}';
        case '"':  return '"';
        case '\'': return '\'';
        default:   return 0;
    }
}

// Does it make sense to skip the auto-close because we appear to be typing
// inside an existing identifier? Heuristic: peek the character at the caret.
// If it's a letter, digit, or '_' we leave the user alone.
bool nextCharIsWord(ScintillaEdit* editor, sptr_t pos) {
    sptr_t docLen = editor->length();
    if (pos < 0 || pos >= docLen) return false;
    // SCI_GETCHARAT returns the byte (0..255); for ASCII this is sufficient,
    // and for multi-byte UTF-8 sequences the lead byte is >= 0x80 which we
    // treat conservatively as "word-ish" so we don't auto-close before a
    // letter outside ASCII either.
    const int byte = static_cast<int>(editor->charAt(pos)) & 0xFF;
    if (byte >= 0x80) return true;
    QChar c = QChar(static_cast<ushort>(byte));
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

// Was the *previously typed* character (i.e. the one before the just-inserted
// opener) a backslash? In that case the user is probably escaping a quote and
// we should not auto-close.
bool prevCharIsBackslash(ScintillaEdit* editor, sptr_t openerPos) {
    // openerPos is the position of the freshly typed opener byte.
    // The character before it lives at openerPos - 1.
    if (openerPos <= 0) return false;
    const int byte = static_cast<int>(editor->charAt(openerPos - 1)) & 0xFF;
    return byte == '\\';
}

// Insert `tabWidth` spaces or a literal '\t' at the start of every line in
// [firstLine, lastLine]. Editor mutation only -- caller wraps in undo action.
void insertIndentRange(ScintillaEdit* editor, sptr_t firstLine, sptr_t lastLine,
                       const QByteArray& indent) {
    // Walk bottom-up so each insertion doesn't shift the offsets of the lines
    // we still have to process.
    for (sptr_t line = lastLine; line >= firstLine; --line) {
        sptr_t pos = editor->positionFromLine(line);
        editor->insertText(pos, indent.constData());
    }
}

// Remove up to one indentation unit (a single '\t' or up to `tabWidth` leading
// spaces) from every line in [firstLine, lastLine].
void removeIndentRange(ScintillaEdit* editor, sptr_t firstLine, sptr_t lastLine,
                       int tabWidth) {
    for (sptr_t line = lastLine; line >= firstLine; --line) {
        sptr_t lineStart = editor->positionFromLine(line);
        sptr_t docLen = editor->length();
        if (lineStart >= docLen) continue;

        const int firstByte = static_cast<int>(editor->charAt(lineStart)) & 0xFF;
        if (firstByte == '\t') {
            editor->setTargetRange(lineStart, lineStart + 1);
            editor->replaceTarget(0, "");
            continue;
        }

        if (firstByte == ' ') {
            // Remove up to tabWidth consecutive leading spaces.
            int removable = 0;
            while (removable < tabWidth && (lineStart + removable) < docLen) {
                const int b = static_cast<int>(editor->charAt(lineStart + removable)) & 0xFF;
                if (b != ' ') break;
                ++removable;
            }
            if (removable > 0) {
                editor->setTargetRange(lineStart, lineStart + removable);
                editor->replaceTarget(0, "");
            }
        }
        // Otherwise: line has no leading indentation -- leave it alone.
    }
}

// Compute the inclusive line range covered by [start, end). When the range
// ends exactly at the start of a line (and is non-empty), that trailing line
// is excluded -- matches the rest of the codebase's selection-to-lines logic.
void selectionToLineRange(ScintillaEdit* editor, sptr_t start, sptr_t end,
                          sptr_t& firstLine, sptr_t& lastLine) {
    firstLine = editor->lineFromPosition(start);
    sptr_t endLine = editor->lineFromPosition(end);
    if (end > start && end == editor->positionFromLine(endLine) && endLine > firstLine) {
        endLine -= 1;
    }
    lastLine = endLine;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / persistence
// ---------------------------------------------------------------------------

EditEnhancements::EditEnhancements(QObject* parent)
    : QObject(parent) {
    QSettings s;
    m_autoClose = s.value(kKeyAutoClose, true).toBool();
}

bool EditEnhancements::autoCloseEnabled() const {
    return m_autoClose;
}

void EditEnhancements::setAutoCloseEnabled(bool b) {
    if (m_autoClose == b) return;
    m_autoClose = b;
    QSettings s;
    s.setValue(kKeyAutoClose, b);
    s.sync();
    emit autoCloseChanged(b);
}

// ---------------------------------------------------------------------------
// Active editor management
// ---------------------------------------------------------------------------

void EditEnhancements::setActiveEditor(ScintillaEdit* editor) {
    if (m_editor == editor) {
        return;
    }

    if (m_connCharAdded) {
        QObject::disconnect(m_connCharAdded);
        m_connCharAdded = QMetaObject::Connection();
    }

    m_editor = editor;
    if (!m_editor) {
        return;
    }

    m_connCharAdded = QObject::connect(
        m_editor, &ScintillaEditBase::charAdded,
        this, &EditEnhancements::onCharAdded);
}

// ---------------------------------------------------------------------------
// Auto-close hook
// ---------------------------------------------------------------------------

void EditEnhancements::onCharAdded(int ch) {
    if (!m_autoClose || !m_editor) return;

    const char closer = closerFor(ch);
    if (closer == 0) return;

    const sptr_t caret = m_editor->currentPos();

    // The opener byte sits at caret - 1 (Scintilla fires charAdded *after* the
    // insertion has shifted the caret forward).
    if (prevCharIsBackslash(m_editor, caret - 1)) {
        return;
    }

    if (nextCharIsWord(m_editor, caret)) {
        return;
    }

    const char closerStr[2] = { closer, '\0' };

    // Insert the closer AFTER the caret and rewind: insertText doesn't move
    // the caret if the insertion is at-or-after currentPos, but we set it
    // explicitly to be defensive across Scintilla versions.
    m_editor->insertText(caret, closerStr);
    m_editor->setCurrentPos(caret);
    m_editor->setAnchor(caret);
}

// ---------------------------------------------------------------------------
// Indent / unindent
// ---------------------------------------------------------------------------

void EditEnhancements::indentSelection(ScintillaEdit* editor, int tabWidth) {
    if (!editor) return;
    if (tabWidth <= 0) tabWidth = 4;

    sptr_t start;
    sptr_t end;
    bool hadSelection = !editor->selectionEmpty();
    if (hadSelection) {
        start = editor->selectionStart();
        end = editor->selectionEnd();
    } else {
        const sptr_t caret = editor->currentPos();
        const sptr_t line = editor->lineFromPosition(caret);
        start = editor->positionFromLine(line);
        end = start;
    }

    sptr_t firstLine;
    sptr_t lastLine;
    selectionToLineRange(editor, start, end, firstLine, lastLine);
    if (lastLine < firstLine) return;

    const bool useTabs = editor->useTabs();
    QByteArray indent;
    if (useTabs) {
        indent = QByteArray("\t");
    } else {
        indent = QByteArray(tabWidth, ' ');
    }

    editor->beginUndoAction();
    insertIndentRange(editor, firstLine, lastLine, indent);
    editor->endUndoAction();

    // Keep a useful selection: span the whole touched block so successive Tab
    // presses keep operating on the same lines.
    if (hadSelection) {
        const sptr_t newStart = editor->positionFromLine(firstLine);
        sptr_t newEnd;
        const sptr_t lineCount = editor->lineCount();
        if (lastLine + 1 < lineCount) {
            newEnd = editor->positionFromLine(lastLine + 1);
        } else {
            newEnd = editor->length();
        }
        editor->setSel(newStart, newEnd);
    }
}

void EditEnhancements::unindentSelection(ScintillaEdit* editor, int tabWidth) {
    if (!editor) return;
    if (tabWidth <= 0) tabWidth = 4;

    sptr_t start;
    sptr_t end;
    bool hadSelection = !editor->selectionEmpty();
    if (hadSelection) {
        start = editor->selectionStart();
        end = editor->selectionEnd();
    } else {
        const sptr_t caret = editor->currentPos();
        const sptr_t line = editor->lineFromPosition(caret);
        start = editor->positionFromLine(line);
        end = start;
    }

    sptr_t firstLine;
    sptr_t lastLine;
    selectionToLineRange(editor, start, end, firstLine, lastLine);
    if (lastLine < firstLine) return;

    editor->beginUndoAction();
    removeIndentRange(editor, firstLine, lastLine, tabWidth);
    editor->endUndoAction();

    if (hadSelection) {
        const sptr_t newStart = editor->positionFromLine(firstLine);
        sptr_t newEnd;
        const sptr_t lineCount = editor->lineCount();
        if (lastLine + 1 < lineCount) {
            newEnd = editor->positionFromLine(lastLine + 1);
        } else {
            newEnd = editor->length();
        }
        editor->setSel(newStart, newEnd);
    }
}

// ---------------------------------------------------------------------------
// Reload from disk
// ---------------------------------------------------------------------------

bool EditEnhancements::reloadFromDisk(ScintillaEdit* editor, const QString& path,
                                      QString* errorOut) {
    if (!editor) {
        if (errorOut) *errorOut = QStringLiteral("No active editor.");
        return false;
    }
    if (path.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Empty file path.");
        return false;
    }

    QFile f(path);
    if (!f.exists()) {
        if (errorOut) *errorOut = QStringLiteral("File does not exist: %1").arg(path);
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open '%1': %2").arg(path, f.errorString());
        }
        return false;
    }

    const QByteArray raw = f.readAll();
    if (f.error() != QFileDevice::NoError) {
        if (errorOut) {
            *errorOut = QStringLiteral("Read error on '%1': %2").arg(path, f.errorString());
        }
        f.close();
        return false;
    }
    f.close();

    // Decode as UTF-8 and re-encode for Scintilla so we strip any decoding
    // surprises (lone bytes etc.) deterministically. Caller can swap in the
    // full FileIO encoding pipeline if they need BOM / legacy handling.
    const QString decoded = QString::fromUtf8(raw);
    const QByteArray utf8 = decoded.toUtf8();

    editor->beginUndoAction();
    editor->setText(utf8.constData());
    editor->emptyUndoBuffer();
    editor->setSavePoint();
    editor->endUndoAction();

    return true;
}
