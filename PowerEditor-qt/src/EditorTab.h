#pragma once
#include <QWidget>
#include <QString>
#include <ScintillaTypes.h>

class ScintillaEdit;

class EditorTab : public QWidget {
    Q_OBJECT
public:
    explicit EditorTab(QWidget* parent = nullptr);
    ~EditorTab() override;

    ScintillaEdit* editor() const;
    QString filePath() const;
    void setFilePath(const QString& path);   // empty string = "untitled"
    bool isModified() const;
    void setModified(bool);
    QString tabTitle() const;                 // basename of filePath (or "untitled"), with " *" suffix if modified
    QString displayPath() const;              // full path or "untitled"

signals:
    void modificationChanged(bool modified);
    void filePathChanged(const QString& newPath);
    void cursorPositionChanged(int line, int column);   // 1-based

private slots:
    void onScintillaModified(Scintilla::ModificationFlags type,
                             Scintilla::Position position,
                             Scintilla::Position length,
                             Scintilla::Position linesAdded,
                             const QByteArray& text,
                             Scintilla::Position line,
                             Scintilla::FoldLevel foldNow,
                             Scintilla::FoldLevel foldPrev);
    void onScintillaUpdateUi(Scintilla::Update updated);

private:
    ScintillaEdit* m_editor = nullptr;
    QString m_path;
    bool m_modified = false;
};
