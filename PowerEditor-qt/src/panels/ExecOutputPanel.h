#pragma once
#include <QDockWidget>
#include <QString>

class QLineEdit;
class QToolButton;
class QLabel;
class ScintillaEdit;
class QProcess;

class ExecOutputPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit ExecOutputPanel(QWidget* parent = nullptr);
    ~ExecOutputPanel() override;

    // Set the working directory used for spawned processes. Defaults to the user's home dir.
    void setWorkingDirectory(const QString& path);
    QString workingDirectory() const;

public slots:
    // Convenience: programmatically run a command (used by callers besides the lineedit).
    void runCommand(const QString& cmdline);
    void stopRunning();

signals:
    void commandFinished(int exitCode);

private slots:
    void onRunClicked();
    void onStopClicked();
    void onClearClicked();
    void onCdClicked();
    void onReadyReadStdout();
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessError(int error);

private:
    void appendOutput(const QByteArray& bytes);
    void updateCwdLabel();
    void setStopEnabled(bool enabled);

    QLabel*        m_cwdLabel   = nullptr;
    QToolButton*   m_cdBtn      = nullptr;
    QLineEdit*     m_cmdEdit    = nullptr;
    QToolButton*   m_runBtn     = nullptr;
    QToolButton*   m_stopBtn    = nullptr;
    QToolButton*   m_clearBtn   = nullptr;
    ScintillaEdit* m_out        = nullptr;
    QProcess*      m_proc       = nullptr;
    QString        m_workingDir;
};
