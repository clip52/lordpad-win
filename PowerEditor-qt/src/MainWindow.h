#pragma once
#include <QMainWindow>
#include <QString>

class QLabel;
class QAction;
class QActionGroup;
class EditorTab;
class FindReplaceDialog;
class ComparePanel;
class CssPreviewPane;
class CsvTableView;
class MultiView;
class LanguagesMenu;
class FunctionListPanel;
class DocumentMapPanel;
class FileBrowserPanel;
class ExecOutputPanel;
class MarkdownPreviewPane;
class HexViewer;
class FindInFilesDialog;
class CommandPalette;
class AutoSavePolicy;
class SessionManager;
class AutoCompleter;
class BookmarkManager;
class BookmarkDialog;
class WordCountDialog;
class MacroRecorder;
class MacroDialog;
class EolMenu;
class SpellChecker;
class ExternalFileWatcher;
class BraceMatcher;
class WhitespaceView;
class Snippets;
class SnippetsDialog;
class RecentProjects;
class EditEnhancements;
class HashDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onFileNew();
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();
    void onFileClose();
    void onFileExit();

    void onEditUndo();
    void onEditRedo();
    void onEditCut();
    void onEditCopy();
    void onEditPaste();
    void onEditSelectAll();

    void onSearchFind();
    void onSearchReplace();
    void onSearchGoToLine();

    void onViewSetThemeLight();
    void onViewSetThemeDark();
    void onViewSetThemeDracula();
    void onViewToggleLineNumbers();
    void onViewToggleWordWrap();
    void onViewSplitHorizontal();
    void onViewSplitVertical();
    void onViewUnsplit();
    void onViewMoveTabToOtherGroup();

    void onLanguageSelected(const QString& lexerName);

    void onToolsPreferences();
    void onToolsCompare();
    void onToolsCssPreview();
    void onToolsCsvView();
    void onToolsMarkdownPreview();
    void onToolsHexViewer();
    void onToolsFindInFiles();
    void onToolsCommandPalette();
    void onToolsRunCommand();          // focuses the Exec panel cmdline

    void onViewToggleFunctionList();
    void onViewToggleDocumentMap();
    void onViewToggleFileBrowser();
    void onViewToggleExecOutput();

    void onFileBrowserOpenFile(const QString& path);
    void onFindInFilesOpen(const QString& path, int line);
    void onFunctionListGoto(int line);
    void onAutoSaveTick();

    // Edit operations
    void onEditTrimWhitespace();
    void onEditUpperCase();
    void onEditLowerCase();
    void onEditTitleCase();
    void onEditSortAsc();
    void onEditSortDesc();
    void onEditSortUnique();
    void onEditDuplicateLine();
    void onEditMoveLineUp();
    void onEditMoveLineDown();
    void onEditTabsToSpaces();
    void onEditSpacesToTabs();

    // Bookmarks
    void onBookmarkToggle();
    void onBookmarkNext();
    void onBookmarkPrev();
    void onBookmarkClearAll();
    void onBookmarkList();

    // Tools (M4)
    void onToolsWordCount();
    void onToolsJsonPretty();
    void onToolsJsonMinify();
    void onToolsXmlPretty();
    void onToolsXmlMinify();
    void onToolsPickColor();
    void onToolsMacroDialog();

    // M5
    void onFilePrint();
    void onFilePrintPreview();
    void onFileReloadFromDisk();
    void onFileOpenFolder();
    void onSearchGotoMatchingBrace();
    void onViewToggleWhitespace();
    void onViewToggleEol();
    void onViewToggleIndentGuides();
    void onToolsHash();
    void onToolsSnippets();
    void onToolsToggleSpellCheck();
    void onExternalFileChanged(const QString& path);
    void onExternalFileRemoved(const QString& path);

    void onHelpAbout();

    void onMultiViewCurrentChanged(EditorTab* tab);
    void onMultiViewTabCloseRequested(EditorTab* tab);
    void onCurrentTabModified(bool modified);
    void onCurrentTabFilePathChanged(const QString& path);
    void onCursorPositionChanged(int line, int column);

    void onRecentFileTriggered();

private:
    EditorTab* currentTab() const;
    EditorTab* tabAt(int index) const;
    int findTabByPath(const QString& path) const;
    void setTabTitle(EditorTab* tab, const QString& title);
    void setTabTooltip(EditorTab* tab, const QString& tooltip);
    void setActiveTab(EditorTab* tab);

    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createCentralWidget();

    bool maybeSaveTab(EditorTab* tab);
    bool saveTab(EditorTab* tab);
    bool saveTabAs(EditorTab* tab);
    void loadFileIntoTab(EditorTab* tab, const QString& path);
    void applyEditorPreferences(EditorTab* tab);
    void applyThemeAndLexer(EditorTab* tab);
    void applyThemeToAllTabs();
    void updateWindowTitle();
    void updateStatusBar();
    void rebuildRecentFilesMenu();
    void connectTabSignals(EditorTab* tab);

    MultiView* m_multiView;
    LanguagesMenu* m_languagesMenu;
    FindReplaceDialog* m_findDialog;
    ComparePanel* m_comparePanel;
    CssPreviewPane* m_cssPreviewPane;
    CsvTableView* m_csvTableView;
    MarkdownPreviewPane* m_markdownPreviewPane;
    HexViewer* m_hexViewer;
    FindInFilesDialog* m_findInFilesDialog;
    CommandPalette* m_commandPalette;

    // Dock panels
    FunctionListPanel* m_functionListPanel;
    DocumentMapPanel*  m_documentMapPanel;
    FileBrowserPanel*  m_fileBrowserPanel;
    ExecOutputPanel*   m_execOutputPanel;

    // Lifecycle helpers
    AutoSavePolicy*   m_autoSave;
    SessionManager*   m_session;
    AutoCompleter*    m_autoCompleter;
    BookmarkManager*  m_bookmarks;
    MacroRecorder*    m_macros;
    EolMenu*          m_eolMenu;
    WordCountDialog*  m_wordCountDialog;
    MacroDialog*      m_macroDialog;
    BookmarkDialog*   m_bookmarkDialog;

    // M5 helpers
    SpellChecker*        m_spellChecker;
    ExternalFileWatcher* m_externalWatcher;
    BraceMatcher*        m_braceMatcher;
    WhitespaceView*      m_whitespaceView;
    Snippets*            m_snippets;
    RecentProjects*      m_recentProjects;
    EditEnhancements*    m_editEnhance;
    HashDialog*          m_hashDialog;
    SnippetsDialog*      m_snippetsDialog;

    QLabel* m_statusPosition;
    QLabel* m_statusEncoding;
    QLabel* m_statusEol;

    // File menu
    QAction* m_actNew;
    QAction* m_actOpen;
    QAction* m_actSave;
    QAction* m_actSaveAs;
    QAction* m_actClose;
    QAction* m_actExit;
    QMenu*   m_menuRecent;

    // Edit menu
    QAction* m_actUndo;
    QAction* m_actRedo;
    QAction* m_actCut;
    QAction* m_actCopy;
    QAction* m_actPaste;
    QAction* m_actSelectAll;

    // Search menu
    QAction* m_actFind;
    QAction* m_actReplace;
    QAction* m_actGoToLine;

    // View menu
    QAction* m_actThemeLight;
    QAction* m_actThemeDark;
    QAction* m_actThemeDracula;
    QActionGroup* m_themeGroup;
    QAction* m_actToggleLineNumbers;
    QAction* m_actToggleWordWrap;
    QAction* m_actSplitHorizontal;
    QAction* m_actSplitVertical;
    QAction* m_actUnsplit;
    QAction* m_actMoveTabToOtherGroup;

    // Tools menu
    QAction* m_actPreferences;
    QAction* m_actCompare;
    QAction* m_actCssPreview;
    QAction* m_actCsvView;
    QAction* m_actMarkdownPreview;
    QAction* m_actHexViewer;
    QAction* m_actFindInFiles;
    QAction* m_actCommandPalette;
    QAction* m_actRunCommand;
    QAction* m_actToggleFunctionList;
    QAction* m_actToggleDocumentMap;
    QAction* m_actToggleFileBrowser;
    QAction* m_actToggleExecOutput;

    // Help menu
    QAction* m_actAbout;
};
