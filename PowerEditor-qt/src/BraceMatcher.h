#pragma once

#include <QObject>
#include <QPointer>

#include <ScintillaTypes.h>

#include "ScintillaEdit.h"

class BraceMatcher : public QObject {
    Q_OBJECT
public:
    explicit BraceMatcher(QObject* parent = nullptr);

    void setActiveEditor(ScintillaEdit* editor);

public slots:
    // Jump caret to the matching brace, if any.
    void gotoMatchingBrace();

private slots:
    void onUpdateUi(Scintilla::Update updated);

private:
    // Returns true if ch is one of the brace characters considered.
    static bool isBraceChar(int ch);

    // Locates a brace at caret or just before it.
    // Returns true and fills out bracePos / matchPos if a brace was found.
    // matchPos == -1 means no match (bad brace).
    bool locateBrace(ScintillaEdit* editor, sptr_t& bracePos, sptr_t& matchPos) const;

    void configureStyles(ScintillaEdit* editor);

    QPointer<ScintillaEdit> m_editor;
};
