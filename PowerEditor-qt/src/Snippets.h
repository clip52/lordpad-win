#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>

class ScintillaEdit;

struct Snippet {
    QString trigger;     // word the user types before pressing Tab (e.g. "for")
    QString body;        // expansion text; ${1}, ${2:default} placeholders
    QString description; // shown in the manager dialog
};

class Snippets : public QObject {
    Q_OBJECT
public:
    explicit Snippets(QObject* parent = nullptr);

    // Look up snippets for a specific Lexilla lexer name (e.g. "cpp", "python").
    // "" = global snippets that apply to any language.
    QList<Snippet> forLanguage(const QString& lexerName) const;

    // Add or update a snippet under the given language.
    void upsert(const QString& lexerName, const Snippet& s);
    void remove(const QString& lexerName, const QString& trigger);

    // All language buckets (sorted).
    QStringList allLanguages() const;

    // Try to expand the trigger word at the caret.
    // Returns true if expansion happened (caller should consume the Tab key event).
    // Logic:
    //   - get the word immediately before caret (word chars only)
    //   - look up snippet for current lexer name; if none, fall back to "" global bucket
    //   - if found: replace trigger word with body; expand ${1}..${N} placeholders
    //     (for now: just remove placeholder syntax and place caret at the FIRST ${1}).
    bool tryExpand(ScintillaEdit* editor, const QString& currentLexerName);

signals:
    void changed();

private:
    void load();
    void save() const;
    void seedDefaults();

    // m_buckets[lexerName][trigger] -> Snippet.
    QHash<QString, QHash<QString, Snippet>> m_buckets;
};
