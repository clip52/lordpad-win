#include "MacroRecorder.h"

#include <QSettings>
#include <QDataStream>
#include <QByteArray>
#include <QIODevice>

#include "ScintillaEdit.h"

namespace {
    // Scintilla message ids we know carry a NUL-terminated text payload via lParam.
    // We deliberately keep this whitelist small to avoid mis-copying arbitrary
    // pointers as text.
    constexpr int kSCI_REPLACESEL          = 2170;
    constexpr int kSCI_INSERTTEXT          = 2003;
    constexpr int kSCI_ADDTEXT             = 2001;
    constexpr int kSCI_APPENDTEXT          = 2282;
    constexpr int kSCI_SEARCHNEXT          = 2367;
    constexpr int kSCI_SEARCHPREV          = 2368;
    constexpr int kSCI_REPLACETARGET       = 2194;
    constexpr int kSCI_REPLACETARGETRE     = 2195;

    constexpr int kSCI_STARTRECORD         = 3001;
    constexpr int kSCI_STOPRECORD          = 3002;

    QDataStream& operator<<(QDataStream& s, const MacroStep& step) {
        s << qint32(step.message)
          << qint64(step.wParam)
          << step.lParamText;
        return s;
    }
    QDataStream& operator>>(QDataStream& s, MacroStep& step) {
        qint32 msg;
        qint64 wp;
        s >> msg >> wp >> step.lParamText;
        step.message = int(msg);
        step.wParam  = long(wp);
        return s;
    }
}

bool MacroRecorder::messageCarriesText(int message) {
    switch (message) {
        case kSCI_REPLACESEL:
        case kSCI_INSERTTEXT:
        case kSCI_ADDTEXT:
        case kSCI_APPENDTEXT:
        case kSCI_SEARCHNEXT:
        case kSCI_SEARCHPREV:
        case kSCI_REPLACETARGET:
        case kSCI_REPLACETARGETRE:
            return true;
        default:
            return false;
    }
}

MacroRecorder::MacroRecorder(QObject* parent)
    : QObject(parent)
{
}

void MacroRecorder::setActiveEditor(ScintillaEdit* editor) {
    if (m_editor == editor) {
        return;
    }
    if (m_recordConn) {
        QObject::disconnect(m_recordConn);
        m_recordConn = QMetaObject::Connection();
    }
    m_editor = editor;
    if (m_editor) {
        // ScintillaEdit emits macroRecord(Scintilla::Message msg, sptr_t wParam, sptr_t lParam).
        // We use a generic lambda so we don't need to name Scintilla::Message in the header
        // and we coerce to our slot's int/long/long signature.
        m_recordConn = QObject::connect(
            m_editor, &ScintillaEdit::macroRecord,
            this, [this](Scintilla::Message msg, sptr_t wParam, sptr_t lParam) {
                this->onMacroRecord(static_cast<int>(msg),
                                    static_cast<long>(wParam),
                                    static_cast<long>(lParam));
            });
    }
}

bool MacroRecorder::isRecording() const {
    return m_recording;
}

void MacroRecorder::startRecording() {
    if (!m_editor || m_recording) {
        return;
    }
    m_steps.clear();
    m_editor->send(kSCI_STARTRECORD, 0, 0);
    m_recording = true;
    emit recordingChanged(true);
    emit macroChanged();
}

void MacroRecorder::stopRecording() {
    if (!m_recording) {
        return;
    }
    if (m_editor) {
        m_editor->send(kSCI_STOPRECORD, 0, 0);
    }
    m_recording = false;
    emit recordingChanged(false);
    emit macroChanged();
}

void MacroRecorder::onMacroRecord(int msg, long wParam, long lParam) {
    if (!m_recording) {
        return;
    }
    MacroStep step;
    step.message = msg;
    step.wParam  = wParam;
    if (messageCarriesText(msg) && lParam != 0) {
        const char* text = reinterpret_cast<const char*>(static_cast<intptr_t>(lParam));
        step.lParamText = QByteArray(text);
    }
    m_steps.append(step);
}

void MacroRecorder::play() {
    if (!m_editor || m_steps.isEmpty()) {
        return;
    }
    for (const MacroStep& step : m_steps) {
        sptr_t lParamArg = 0;
        if (!step.lParamText.isEmpty()) {
            lParamArg = reinterpret_cast<sptr_t>(step.lParamText.constData());
        }
        m_editor->send(step.message,
                       static_cast<uptr_t>(step.wParam),
                       lParamArg);
    }
}

QStringList MacroRecorder::savedMacroNames() const {
    QSettings settings;
    settings.beginGroup(QStringLiteral("macros"));
    QStringList names = settings.childKeys();
    settings.endGroup();
    return names;
}

void MacroRecorder::saveCurrentAs(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    QByteArray buffer;
    {
        QDataStream out(&buffer, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << qint32(m_steps.size());
        for (const MacroStep& step : m_steps) {
            out << step;
        }
    }
    QSettings settings;
    settings.setValue(QStringLiteral("macros/") + name, buffer);
}

void MacroRecorder::loadByName(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    QSettings settings;
    QByteArray buffer = settings.value(QStringLiteral("macros/") + name).toByteArray();
    if (buffer.isEmpty()) {
        return;
    }
    QVector<MacroStep> loaded;
    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    qint32 count = 0;
    in >> count;
    loaded.reserve(count);
    for (qint32 i = 0; i < count; ++i) {
        MacroStep step;
        in >> step;
        if (in.status() != QDataStream::Ok) {
            return;
        }
        loaded.append(step);
    }
    m_steps = loaded;
    emit macroChanged();
}

void MacroRecorder::deleteByName(const QString& name) {
    if (name.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.remove(QStringLiteral("macros/") + name);
}

QVector<MacroStep> MacroRecorder::currentMacro() const {
    return m_steps;
}

void MacroRecorder::setCurrentMacro(const QVector<MacroStep>& steps) {
    m_steps = steps;
    emit macroChanged();
}
