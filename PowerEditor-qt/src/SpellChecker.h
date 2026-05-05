#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>

#include "ScintillaEdit.h"

class Hunspell;
class QTimer;

class SpellChecker : public QObject {
    Q_OBJECT
public:
    explicit SpellChecker(QObject* parent = nullptr);
    ~SpellChecker() override;

    bool isEnabled() const;
    void setEnabled(bool b);

    QString currentLanguage() const;             // e.g. "pt_BR", "en_US"
    QStringList availableLanguages() const;       // discovered in /usr/share/hunspell/
    bool setLanguage(const QString& langCode);

    void setActiveEditor(ScintillaEdit* editor);  // hooks modified signal, runs initial check

    void recheckAll();                            // re-run spelling on entire buffer

    // Suggestions for the word at the given byte position. Empty if word is OK or no editor.
    QStringList suggestionsAt(int position) const;

    // Return the (start, end) byte range of the word at position, or {-1,-1} if no word.
    QPair<int,int> wordRangeAt(int position) const;

signals:
    void enabledChanged(bool b);
    void languageChanged(const QString& code);

private slots:
    void onModified(Scintilla::ModificationFlags type,
                    Scintilla::Position position,
                    Scintilla::Position length,
                    Scintilla::Position linesAdded,
                    const QByteArray& text,
                    Scintilla::Position line,
                    Scintilla::FoldLevel foldNow,
                    Scintilla::FoldLevel foldPrev);
    void runPendingRecheck();

private:
    static constexpr int kSpellIndicator = 8;
    static constexpr int kMaxBufferBytes = 1 * 1024 * 1024; // 1 MiB
    static constexpr int kMaxSuggestions = 10;

    void configureIndicator();
    void clearAllIndicators();
    void recheckRange(int start, int end);
    void checkWordsInText(const QByteArray& utf8, int baseOffset);

    Hunspell* m_hun = nullptr;
    QString   m_lang;
    bool      m_enabled = false;
    bool      m_indicatorConfigured = false;

    ScintillaEdit* m_editor = nullptr;
    QTimer*        m_debounce = nullptr;

    int m_pendingStart = -1;
    int m_pendingEnd   = -1;
    bool m_pendingFull = false;
};
