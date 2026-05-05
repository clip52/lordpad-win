// AutoCompleter.h
// Word-based autocompletion engine for ScintillaEdit instances.
//
// Self-contained component: scans the active document for words sharing
// the prefix currently being typed and pops up a Scintilla autocomplete
// list (SCI_AUTOCSHOW). State (auto-trigger flag, trigger length) is
// persisted via QSettings so the behaviour matches across sessions.

#pragma once

#include <QChar>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "ScintillaEdit.h"

class AutoCompleter : public QObject {
    Q_OBJECT
public:
    explicit AutoCompleter(QObject* parent = nullptr);

    // Attach to the editor whose buffer/caret should drive completions.
    // Passing nullptr detaches without attaching to a new editor.
    void setActiveEditor(ScintillaEdit* editor);

    bool isAutoTrigger() const;          // automatic popup as user types
    void setAutoTrigger(bool b);         // QSettings persisted
    int  triggerLength() const;          // min prefix chars (default 3, range 1..6)
    void setTriggerLength(int n);

public slots:
    // Trigger completion explicitly (e.g. on Ctrl+Space).
    void triggerNow();

private slots:
    void onCharAdded(int ch);
    void onModified(Scintilla::ModificationFlags type,
                    Scintilla::Position position,
                    Scintilla::Position length,
                    Scintilla::Position linesAdded,
                    const QByteArray& text,
                    Scintilla::Position line,
                    Scintilla::FoldLevel foldNow,
                    Scintilla::FoldLevel foldPrev);

private:
    // Configure Scintilla's autocomplete defaults on the attached editor.
    void configureEditor();

    // Build (or fetch from cache) the alphabetically-sorted list of unique
    // words currently present in the buffer.
    QStringList collectWords();

    // Run the actual show: takes prefix + minimum length to consider.
    // If the prefix is shorter than minLen the popup is suppressed.
    void showCompletions(int minLen);

    // Retrieve the word currently being typed (chars left of caret that
    // qualify as word characters). Returns empty string if none.
    QString currentPrefix(int& prefixByteLen) const;

    static bool isWordChar(QChar c);

    ScintillaEdit* m_editor = nullptr;
    QMetaObject::Connection m_connCharAdded;
    QMetaObject::Connection m_connModified;

    // Cached settings.
    bool m_autoTrigger = true;
    int  m_triggerLen  = 3;

    // Word-list memoisation for large buffers.
    QStringList m_cachedWords;
    qint64      m_cachedTextLen   = -1;
    quint64     m_cachedModCounter = 0;
    quint64     m_modCounter       = 0; // bumped on every modified() signal
    quint64     m_lastBuildModCounter = 0;
};
