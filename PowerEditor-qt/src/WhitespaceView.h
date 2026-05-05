#pragma once
#include <QObject>
#include <QSettings>
#include "ScintillaEdit.h"

class WhitespaceView : public QObject {
    Q_OBJECT
public:
    explicit WhitespaceView(QObject* parent = nullptr);

    bool isWhitespaceVisible() const;
    bool isEolVisible() const;
    bool areIndentGuidesVisible() const;

    void setWhitespaceVisible(bool b);   // persisted via QSettings
    void setEolVisible(bool b);          // persisted
    void setIndentGuidesVisible(bool b); // persisted

    // Apply current settings to the given editor. Called on tab change / file load.
    void applyTo(ScintillaEdit* editor);

signals:
    void changed();

private:
    bool m_whitespaceVisible = false;
    bool m_eolVisible = false;
    bool m_indentGuidesVisible = false;
};
