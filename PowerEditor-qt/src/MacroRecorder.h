#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>

class ScintillaEdit;

struct MacroStep {
    int  message;     // SCI_* message id (Scintilla::Message as int)
    long wParam;
    QByteArray lParamText;   // For text-bearing messages, store the text. Else empty.
};

class MacroRecorder : public QObject {
    Q_OBJECT
public:
    explicit MacroRecorder(QObject* parent = nullptr);

    void setActiveEditor(ScintillaEdit* editor);

    bool isRecording() const;
    void startRecording();
    void stopRecording();

    // Replay the currently-stored macro on the active editor (no-op if empty).
    void play();

    // Persistence: save/load named macros to/from QSettings under "macros/<name>".
    QStringList savedMacroNames() const;
    void saveCurrentAs(const QString& name);
    void loadByName(const QString& name);     // overwrites current
    void deleteByName(const QString& name);

    // Direct access (for the dialog to inspect/edit).
    QVector<MacroStep> currentMacro() const;
    void setCurrentMacro(const QVector<MacroStep>& steps);

signals:
    void recordingChanged(bool recording);
    void macroChanged();   // currentMacro replaced or recording finished

private slots:
    void onMacroRecord(int msg, long wParam, long lParam);

private:
    static bool messageCarriesText(int message);

    ScintillaEdit*       m_editor    = nullptr;
    QMetaObject::Connection m_recordConn;
    bool                 m_recording = false;
    QVector<MacroStep>   m_steps;
};
