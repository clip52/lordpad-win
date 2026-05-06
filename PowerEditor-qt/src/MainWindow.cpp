#include "MainWindow.h"
#include "EditorTab.h"
#include "FileIO.h"
#include "Settings.h"
#include "Theme.h"
#include "LexerMap.h"
#include "MultiView.h"
#include "LanguagesMenu.h"
#include "AutoSavePolicy.h"
#include "SessionManager.h"
#include "AutoCompleter.h"
#include "BookmarkManager.h"
#include "EditOperations.h"
#include "EolMenu.h"
#include "JsonXmlFormatter.h"
#include "MacroRecorder.h"
#include "ColorPickerHelper.h"
#include "dialogs/BookmarkDialog.h"
#include "dialogs/MacroDialog.h"
#include "dialogs/WordCountDialog.h"
#include "BraceMatcher.h"
#include "EditEnhancements.h"
#include "ExternalFileWatcher.h"
#include "PrintHelper.h"
#include "RecentProjects.h"
#include "Snippets.h"
#include "SpellChecker.h"
#include "WhitespaceView.h"
#include "dialogs/HashDialog.h"
#include "dialogs/SnippetsDialog.h"
#include "panels/FunctionListPanel.h"
#include "panels/DocumentMapPanel.h"
#include "panels/FileBrowserPanel.h"
#include "panels/ExecOutputPanel.h"
#include "dialogs/FindReplaceDialog.h"
#include "dialogs/GoToLineDialog.h"
#include "dialogs/PreferencesDialog.h"
#include "dialogs/ComparePanel.h"
#include "dialogs/CssPreviewPane.h"
#include "dialogs/CsvTableView.h"
#include "dialogs/MarkdownPreviewPane.h"
#include "dialogs/HexViewer.h"
#include "dialogs/FindInFilesDialog.h"
#include "dialogs/CommandPalette.h"
#include "TabExtras.h"
#include "UrlHyperlink.h"
#include "CodeFormatter.h"
#include "GitStatusService.h"
#include "ThemePack.h"
#include "CrashRecovery.h"
#include "Workspace.h"
#include "OutlineRegex.h"

#include <ScintillaEdit.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSize>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>

namespace {
AppTheme themeFromSettings(const Settings& s) {
    if (s.darkTheme()) return AppTheme::Dark;
    return AppTheme::Light;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_multiView(nullptr),
      m_languagesMenu(nullptr),
      m_findDialog(nullptr),
      m_comparePanel(nullptr),
      m_cssPreviewPane(nullptr),
      m_csvTableView(nullptr),
      m_markdownPreviewPane(nullptr),
      m_hexViewer(nullptr),
      m_findInFilesDialog(nullptr),
      m_commandPalette(nullptr),
      m_functionListPanel(nullptr),
      m_documentMapPanel(nullptr),
      m_fileBrowserPanel(nullptr),
      m_execOutputPanel(nullptr),
      m_autoSave(nullptr),
      m_session(nullptr),
      m_autoCompleter(nullptr),
      m_bookmarks(nullptr),
      m_macros(nullptr),
      m_eolMenu(nullptr),
      m_wordCountDialog(nullptr),
      m_macroDialog(nullptr),
      m_bookmarkDialog(nullptr),
      m_spellChecker(nullptr),
      m_externalWatcher(nullptr),
      m_braceMatcher(nullptr),
      m_whitespaceView(nullptr),
      m_snippets(nullptr),
      m_recentProjects(nullptr),
      m_editEnhance(nullptr),
      m_hashDialog(nullptr),
      m_snippetsDialog(nullptr),
      m_tabExtras(nullptr),
      m_codeFormatter(nullptr),
      m_gitStatus(nullptr),
      m_crashRecovery(nullptr),
      m_workspace(nullptr),
      m_statusGit(nullptr),
      m_menuRecentWorkspaces(nullptr),
      m_statusPosition(nullptr),
      m_statusEncoding(nullptr),
      m_statusEol(nullptr) {
    setWindowTitle(tr("Notepad++ Qt"));
    {
        // Same fallback chain as main(): prefer themed icon, fall back to resource.
        QIcon icon = QIcon::fromTheme(QStringLiteral("notepadpp-qt"));
        if (icon.isNull()) icon = QIcon(QStringLiteral(":/icons/notepadpp-qt.svg"));
        setWindowIcon(icon);
    }

    createCentralWidget();
    createActions();
    createMenus();
    createToolBar();
    createStatusBar();

    m_findDialog = new FindReplaceDialog(nullptr, this);
    // m_languagesMenu was created lazily inside createMenus() — DO NOT instantiate
    // a second one here; doing so overwrites the pointer and orphans the QMenu in
    // the menu bar, leaving it permanently disabled.

    // Dock panels (created here, populated/wired below).
    m_functionListPanel = new FunctionListPanel(this);
    m_documentMapPanel  = new DocumentMapPanel(this);
    m_fileBrowserPanel  = new FileBrowserPanel(this);
    m_execOutputPanel   = new ExecOutputPanel(this);
    addDockWidget(Qt::LeftDockWidgetArea,  m_fileBrowserPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_functionListPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_documentMapPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_execOutputPanel);
    // Hide all docks by default; user toggles them via View menu.
    m_functionListPanel->hide();
    m_documentMapPanel->hide();
    m_fileBrowserPanel->hide();
    m_execOutputPanel->hide();

    connect(m_functionListPanel, &FunctionListPanel::gotoLineRequested,
            this, &MainWindow::onFunctionListGoto);
    connect(m_fileBrowserPanel,  &FileBrowserPanel::openFileRequested,
            this, &MainWindow::onFileBrowserOpenFile);

    // Auto-save: emit-only; we save dirty named tabs on each tick.
    m_autoSave = new AutoSavePolicy(this);
    connect(m_autoSave, &AutoSavePolicy::autoSaveTick, this, &MainWindow::onAutoSaveTick);

    // Session manager: load + save list of open files.
    m_session = new SessionManager(this);

    // M4 helpers (singletons attached to MainWindow's lifetime).
    m_autoCompleter = new AutoCompleter(this);
    m_bookmarks     = new BookmarkManager(this);
    m_macros        = new MacroRecorder(this);
    m_eolMenu       = new EolMenu(this);

    // M5 helpers
    m_spellChecker    = new SpellChecker(this);
    m_externalWatcher = new ExternalFileWatcher(this);
    m_braceMatcher    = new BraceMatcher(this);
    m_whitespaceView  = new WhitespaceView(this);
    m_snippets        = new Snippets(this);
    m_recentProjects  = new RecentProjects(this);
    m_editEnhance     = new EditEnhancements(this);
    connect(m_externalWatcher, &ExternalFileWatcher::fileChangedExternally,
            this, &MainWindow::onExternalFileChanged);
    connect(m_externalWatcher, &ExternalFileWatcher::fileRemovedExternally,
            this, &MainWindow::onExternalFileRemoved);

    // M6 helpers
    m_tabExtras     = new TabExtras(this);
    m_codeFormatter = new CodeFormatter(this);
    m_gitStatus     = new GitStatusService(this);
    m_crashRecovery = new CrashRecovery(this);
    m_workspace     = new Workspace(this);
    connect(m_gitStatus, &GitStatusService::statusReady,
            this, &MainWindow::onGitStatusReady);
    if (m_multiView) {
        if (auto* g = m_multiView->primaryGroup())   m_tabExtras->attachTabWidget(g);
        if (auto* g = m_multiView->secondaryGroup()) m_tabExtras->attachTabWidget(g);
    }
    m_crashRecovery->start();

    // Oferece restaurar buffers órfãos de uma execução anterior que travou.
    {
        const auto orphans = m_crashRecovery->findOrphanRecoveries();
        for (const auto& r : orphans) {
            const QString label = r.originalPath.isEmpty() ? tr("(sem título)") : r.originalPath;
            const auto btn = QMessageBox::question(this, tr("Recuperação"),
                tr("Recuperar conteúdo de \"%1\" da sessão anterior?").arg(label),
                QMessageBox::Yes | QMessageBox::No);
            if (btn == QMessageBox::Yes) {
                QFile f(r.recoveryFile);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray all = f.readAll();
                    int hdrEnd = all.indexOf("---END-META---\n");
                    QByteArray body = (hdrEnd >= 0) ? all.mid(hdrEnd + 15) : all;
                    auto* tab = new EditorTab(this);
                    applyEditorPreferences(tab);
                    applyThemeAndLexer(tab);
                    connectTabSignals(tab);
                    m_multiView->addTab(tab, tab->tabTitle());
                    tab->editor()->setText(body.constData());
                    tab->setModified(true);
                    setActiveTab(tab);
                }
            }
            m_crashRecovery->consume(r);
        }
    }

    const Settings& s = Settings::instance();
    if (!s.windowGeometry().isEmpty()) restoreGeometry(s.windowGeometry());
    else                               resize(1100, 750);
    if (!s.windowState().isEmpty())    restoreState(s.windowState());

    rebuildRecentFilesMenu();

    // Restore previous session if enabled and any files were saved.
    bool restoredAny = false;
    if (m_session && m_session->restoreOnStartup()) {
        int activeIndex = 0;
        const QStringList paths = m_session->loadSession(&activeIndex);
        for (const QString& p : paths) openFile(p);
        if (!paths.isEmpty()) {
            restoredAny = true;
            if (activeIndex >= 0 && activeIndex < m_multiView->tabCount())
                setActiveTab(tabAt(activeIndex));
        }
    }
    if (!restoredAny) onFileNew();
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Construction helpers
// ---------------------------------------------------------------------------
void MainWindow::createCentralWidget() {
    m_multiView = new MultiView(this);
    m_multiView->setTabsClosable(true);
    m_multiView->setTabsMovable(true);
    setCentralWidget(m_multiView);

    connect(m_multiView, &MultiView::currentTabChanged, this, &MainWindow::onMultiViewCurrentChanged);
    connect(m_multiView, &MultiView::tabCloseRequested, this, &MainWindow::onMultiViewTabCloseRequested);
}

void MainWindow::createActions() {
    auto mk = [this](const QString& text, const QKeySequence& shortcut, auto slot) {
        auto* a = new QAction(text, this);
        if (!shortcut.isEmpty()) a->setShortcut(shortcut);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    m_actNew      = mk(tr("&New"),         QKeySequence::New,    &MainWindow::onFileNew);
    m_actOpen     = mk(tr("&Open..."),     QKeySequence::Open,   &MainWindow::onFileOpen);
    m_actSave     = mk(tr("&Save"),        QKeySequence::Save,   &MainWindow::onFileSave);
    m_actSaveAs   = mk(tr("Save &As..."),  QKeySequence::SaveAs, &MainWindow::onFileSaveAs);
    m_actClose    = mk(tr("&Close"),       QKeySequence::Close,  &MainWindow::onFileClose);
    m_actExit     = mk(tr("E&xit"),        QKeySequence::Quit,   &MainWindow::onFileExit);

    m_actUndo      = mk(tr("&Undo"),       QKeySequence::Undo,      &MainWindow::onEditUndo);
    m_actRedo      = mk(tr("&Redo"),       QKeySequence::Redo,      &MainWindow::onEditRedo);
    m_actCut       = mk(tr("Cu&t"),        QKeySequence::Cut,       &MainWindow::onEditCut);
    m_actCopy      = mk(tr("&Copy"),       QKeySequence::Copy,      &MainWindow::onEditCopy);
    m_actPaste     = mk(tr("&Paste"),      QKeySequence::Paste,     &MainWindow::onEditPaste);
    m_actSelectAll = mk(tr("Select &All"), QKeySequence::SelectAll, &MainWindow::onEditSelectAll);

    m_actFind      = mk(tr("&Find..."),         QKeySequence::Find,    &MainWindow::onSearchFind);
    m_actReplace   = mk(tr("&Replace..."),      QKeySequence::Replace, &MainWindow::onSearchReplace);
    m_actGoToLine  = mk(tr("&Go To Line..."),   QKeySequence(Qt::CTRL | Qt::Key_G), &MainWindow::onSearchGoToLine);

    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    m_actThemeLight   = mk(tr("Theme: &Light"),   QKeySequence(), &MainWindow::onViewSetThemeLight);
    m_actThemeDark    = mk(tr("Theme: &Dark"),    QKeySequence(), &MainWindow::onViewSetThemeDark);
    m_actThemeDracula = mk(tr("Theme: D&racula"), QKeySequence(), &MainWindow::onViewSetThemeDracula);
    for (auto* a : { m_actThemeLight, m_actThemeDark, m_actThemeDracula }) {
        a->setCheckable(true);
        m_themeGroup->addAction(a);
    }
    const auto savedTheme = Settings::instance().darkTheme() ? AppTheme::Dark : AppTheme::Light;
    if (savedTheme == AppTheme::Dark) m_actThemeDark->setChecked(true);
    else                               m_actThemeLight->setChecked(true);

    m_actToggleLineNumbers = mk(tr("Show &Line Numbers"),  QKeySequence(), &MainWindow::onViewToggleLineNumbers);
    m_actToggleLineNumbers->setCheckable(true);
    m_actToggleLineNumbers->setChecked(Settings::instance().showLineNumbers());

    m_actToggleWordWrap    = mk(tr("&Word Wrap"),          QKeySequence(), &MainWindow::onViewToggleWordWrap);
    m_actToggleWordWrap->setCheckable(true);
    m_actToggleWordWrap->setChecked(Settings::instance().wordWrap());

    m_actSplitHorizontal      = mk(tr("Split &Horizontal"),     QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H), &MainWindow::onViewSplitHorizontal);
    m_actSplitVertical        = mk(tr("Split &Vertical"),       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), &MainWindow::onViewSplitVertical);
    m_actUnsplit              = mk(tr("&Unsplit"),              QKeySequence(),                                  &MainWindow::onViewUnsplit);
    m_actMoveTabToOtherGroup  = mk(tr("&Move Tab to Other Group"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M), &MainWindow::onViewMoveTabToOtherGroup);

    m_actPreferences = mk(tr("&Preferences..."), QKeySequence::Preferences, &MainWindow::onToolsPreferences);
    m_actCompare     = mk(tr("&Compare..."),     QKeySequence(),            &MainWindow::onToolsCompare);
    m_actCssPreview  = mk(tr("CSS &Preview..."), QKeySequence(),            &MainWindow::onToolsCssPreview);
    m_actCsvView     = mk(tr("&CSV Table View..."), QKeySequence(),         &MainWindow::onToolsCsvView);
    m_actMarkdownPreview = mk(tr("&Markdown Preview..."), QKeySequence(),   &MainWindow::onToolsMarkdownPreview);
    m_actHexViewer   = mk(tr("&Hex Viewer..."),  QKeySequence(),            &MainWindow::onToolsHexViewer);
    m_actFindInFiles = mk(tr("Find in &Files..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), &MainWindow::onToolsFindInFiles);
    m_actCommandPalette = mk(tr("Command &Palette..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), &MainWindow::onToolsCommandPalette);
    m_actRunCommand  = mk(tr("&Run Command..."), QKeySequence(Qt::CTRL | Qt::Key_R), &MainWindow::onToolsRunCommand);

    m_actToggleFunctionList = mk(tr("Function &List Panel"), QKeySequence(), &MainWindow::onViewToggleFunctionList);
    m_actToggleDocumentMap  = mk(tr("Document &Map Panel"),  QKeySequence(), &MainWindow::onViewToggleDocumentMap);
    m_actToggleFileBrowser  = mk(tr("File &Browser Panel"),  QKeySequence(), &MainWindow::onViewToggleFileBrowser);
    m_actToggleExecOutput   = mk(tr("&Exec Output Panel"),   QKeySequence(), &MainWindow::onViewToggleExecOutput);
    for (auto* a : { m_actToggleFunctionList, m_actToggleDocumentMap,
                     m_actToggleFileBrowser, m_actToggleExecOutput }) {
        a->setCheckable(true);
        a->setChecked(false);
    }

    m_actAbout = mk(tr("&About"), QKeySequence(), &MainWindow::onHelpAbout);

    // ---- M4 actions: edit ops, bookmarks, tools ----
    // (Defined here as anonymous to keep the constructor compact; menu wiring done in createMenus.)
}

// Helper: create + connect M4 actions. Called from createMenus() before the menus are built.
namespace { struct M4Tag {}; }

void MainWindow::createMenus() {
    auto* mb = menuBar();

    auto* mFile = mb->addMenu(tr("&File"));
    mFile->addAction(m_actNew);
    mFile->addAction(m_actOpen);
    auto mkF = [this](QMenu* m, const QString& t, const QKeySequence& sc, auto slot) {
        auto* a = m->addAction(t);
        if (!sc.isEmpty()) a->setShortcut(sc);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    mkF(mFile, tr("Open &Folder..."), QKeySequence(Qt::CTRL | Qt::Key_K), &MainWindow::onFileOpenFolder);
    m_menuRecent = mFile->addMenu(tr("Open &Recent"));
    mFile->addSeparator();
    mFile->addAction(m_actSave);
    mFile->addAction(m_actSaveAs);
    mkF(mFile, tr("&Reload from Disk"),  QKeySequence(),                       &MainWindow::onFileReloadFromDisk);
    mFile->addSeparator();
    mkF(mFile, tr("&Print..."),          QKeySequence::Print,                  &MainWindow::onFilePrint);
    mkF(mFile, tr("Print Pre&view..."),  QKeySequence(),                       &MainWindow::onFilePrintPreview);
    mFile->addSeparator();
    auto* mWorkspace = mFile->addMenu(tr("&Workspace"));
    mkF(mWorkspace, tr("Abrir Workspace..."), QKeySequence(),                                  &MainWindow::onWorkspaceOpen);
    mkF(mWorkspace, tr("Salvar Workspace"),    QKeySequence(),                                  &MainWindow::onWorkspaceSave);
    mkF(mWorkspace, tr("Salvar Workspace Como..."), QKeySequence(),                            &MainWindow::onWorkspaceSaveAs);
    m_menuRecentWorkspaces = mWorkspace->addMenu(tr("Workspaces Recentes"));
    rebuildRecentWorkspacesMenu();
    mFile->addSeparator();
    mFile->addAction(m_actClose);
    mFile->addSeparator();
    mFile->addAction(m_actExit);

    auto* mEdit = mb->addMenu(tr("&Edit"));
    mEdit->addAction(m_actUndo);
    mEdit->addAction(m_actRedo);
    mEdit->addSeparator();
    mEdit->addAction(m_actCut);
    mEdit->addAction(m_actCopy);
    mEdit->addAction(m_actPaste);
    mEdit->addSeparator();
    mEdit->addAction(m_actSelectAll);
    mEdit->addSeparator();

    auto mkE = [this](QMenu* m, const QString& text, const QKeySequence& sc, auto slot) {
        auto* a = m->addAction(text);
        if (!sc.isEmpty()) a->setShortcut(sc);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    auto* mCase = mEdit->addMenu(tr("Convert &Case"));
    mkE(mCase, tr("UPPER CASE"),    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U), &MainWindow::onEditUpperCase);
    mkE(mCase, tr("lower case"),    QKeySequence(Qt::CTRL | Qt::Key_U),             &MainWindow::onEditLowerCase);
    mkE(mCase, tr("Title Case"),    QKeySequence(),                                  &MainWindow::onEditTitleCase);

    auto* mSort = mEdit->addMenu(tr("&Sort Lines"));
    mkE(mSort, tr("Ascending"),     QKeySequence(),                                  &MainWindow::onEditSortAsc);
    mkE(mSort, tr("Descending"),    QKeySequence(),                                  &MainWindow::onEditSortDesc);
    mkE(mSort, tr("Unique"),        QKeySequence(),                                  &MainWindow::onEditSortUnique);

    mEdit->addSeparator();
    mkE(mEdit, tr("&Trim Trailing Whitespace"), QKeySequence(),                       &MainWindow::onEditTrimWhitespace);
    mkE(mEdit, tr("&Duplicate Line"),           QKeySequence(Qt::CTRL | Qt::Key_D),   &MainWindow::onEditDuplicateLine);
    mkE(mEdit, tr("Move Line &Up"),             QKeySequence(Qt::ALT | Qt::Key_Up),   &MainWindow::onEditMoveLineUp);
    mkE(mEdit, tr("Move Line &Down"),           QKeySequence(Qt::ALT | Qt::Key_Down), &MainWindow::onEditMoveLineDown);
    mEdit->addSeparator();
    mkE(mEdit, tr("Tabs to Spaces"),            QKeySequence(),                       &MainWindow::onEditTabsToSpaces);
    mkE(mEdit, tr("Spaces to Tabs"),            QKeySequence(),                       &MainWindow::onEditSpacesToTabs);
    mEdit->addSeparator();
    mkE(mEdit, tr("&Formatar Código"),          QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F), &MainWindow::onToolsCodeFormat);
    mEdit->addSeparator();
    if (m_eolMenu) mEdit->addMenu(m_eolMenu->createMenu(this));

    auto* mSearch = mb->addMenu(tr("&Search"));
    mSearch->addAction(m_actFind);
    mSearch->addAction(m_actReplace);
    mSearch->addSeparator();
    mSearch->addAction(m_actGoToLine);
    mSearch->addSeparator();
    auto mkS = [this](QMenu* m, const QString& text, const QKeySequence& sc, auto slot) {
        auto* a = m->addAction(text);
        if (!sc.isEmpty()) a->setShortcut(sc);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    auto* mBmk = mSearch->addMenu(tr("&Bookmarks"));
    mkS(mBmk, tr("Toggle Bookmark"),    QKeySequence(Qt::Key_F2),                       &MainWindow::onBookmarkToggle);
    mkS(mBmk, tr("Next Bookmark"),      QKeySequence(Qt::CTRL | Qt::Key_F2),            &MainWindow::onBookmarkNext);
    mkS(mBmk, tr("Previous Bookmark"),  QKeySequence(Qt::SHIFT | Qt::Key_F2),           &MainWindow::onBookmarkPrev);
    mBmk->addSeparator();
    mkS(mBmk, tr("Bookmark List..."),   QKeySequence(),                                  &MainWindow::onBookmarkList);
    mkS(mBmk, tr("Clear All Bookmarks"),QKeySequence(),                                  &MainWindow::onBookmarkClearAll);
    mSearch->addSeparator();
    mkS(mSearch, tr("Goto &Matching Brace"), QKeySequence(Qt::CTRL | Qt::Key_B),         &MainWindow::onSearchGotoMatchingBrace);

    auto* mView = mb->addMenu(tr("&View"));
    auto* mTheme = mView->addMenu(tr("&Theme"));
    mTheme->addAction(m_actThemeLight);
    mTheme->addAction(m_actThemeDark);
    mTheme->addAction(m_actThemeDracula);
    mTheme->addSeparator();
    auto* mPack = mTheme->addMenu(tr("Theme Pack"));
    auto* packGroup = new QActionGroup(this);
    packGroup->setExclusive(true);
    auto addPack = [&](ThemePackId id){
        QAction* a = mPack->addAction(ThemePack::displayName(id));
        a->setCheckable(true);
        a->setChecked(ThemePack::loaded() == id);
        a->setData(static_cast<int>(id));
        packGroup->addAction(a);
        connect(a, &QAction::triggered, this, &MainWindow::onThemePackSelected);
    };
    addPack(ThemePackId::None);
    addPack(ThemePackId::SolarizedLight);
    addPack(ThemePackId::SolarizedDark);
    addPack(ThemePackId::Monokai);
    addPack(ThemePackId::Nord);
    mView->addSeparator();
    mView->addAction(m_actToggleLineNumbers);
    mView->addAction(m_actToggleWordWrap);
    mView->addSeparator();
    mView->addAction(m_actSplitHorizontal);
    mView->addAction(m_actSplitVertical);
    mView->addAction(m_actUnsplit);
    mView->addAction(m_actMoveTabToOtherGroup);
    mView->addSeparator();
    auto mkV = [this](QMenu* m, const QString& t, const QKeySequence& sc, auto slot) {
        auto* a = m->addAction(t);
        if (!sc.isEmpty()) a->setShortcut(sc);
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    auto* aWs = mkV(mView, tr("Show Whitespace"),     QKeySequence(),  &MainWindow::onViewToggleWhitespace);
    auto* aEol = mkV(mView, tr("Show End of Line"),   QKeySequence(),  &MainWindow::onViewToggleEol);
    auto* aIg = mkV(mView, tr("Show Indent Guides"),  QKeySequence(),  &MainWindow::onViewToggleIndentGuides);
    if (m_whitespaceView) {
        aWs->setChecked(m_whitespaceView->isWhitespaceVisible());
        aEol->setChecked(m_whitespaceView->isEolVisible());
        aIg->setChecked(m_whitespaceView->areIndentGuidesVisible());
    }
    mView->addSeparator();
    if (m_tabExtras) {
        auto* mTab = mView->addMenu(tr("A&bas"));
        mTab->addAction(m_tabExtras->makePinAction(this));
        mTab->addAction(m_tabExtras->makeLockAction(this));
        mTab->addAction(m_tabExtras->makeColorAction(this));
        mTab->addSeparator();
        mTab->addAction(m_tabExtras->makeCloseOthersAction(this));
        mTab->addAction(m_tabExtras->makeCloseToRightAction(this));
        mTab->addAction(m_tabExtras->makeCloseToLeftAction(this));
    }
    mView->addSeparator();
    auto* mPanels = mView->addMenu(tr("&Panels"));
    mPanels->addAction(m_actToggleFunctionList);
    mPanels->addAction(m_actToggleDocumentMap);
    mPanels->addAction(m_actToggleFileBrowser);
    mPanels->addAction(m_actToggleExecOutput);

    // Languages menu — created lazily after constructor (LanguagesMenu created in ctor body before this).
    if (!m_languagesMenu) m_languagesMenu = new LanguagesMenu(this);
    auto* mLangs = m_languagesMenu->createMenu(this);
    mb->addMenu(mLangs);
    connect(m_languagesMenu, &LanguagesMenu::languageSelected, this, &MainWindow::onLanguageSelected);

    auto* mTools = mb->addMenu(tr("&Tools"));
    mTools->addAction(m_actCompare);
    mTools->addAction(m_actCssPreview);
    mTools->addAction(m_actCsvView);
    mTools->addAction(m_actMarkdownPreview);
    mTools->addAction(m_actHexViewer);
    mTools->addSeparator();
    auto mkT = [this](QMenu* m, const QString& text, const QKeySequence& sc, auto slot) {
        auto* a = m->addAction(text);
        if (!sc.isEmpty()) a->setShortcut(sc);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    mkT(mTools, tr("&Word Count..."),    QKeySequence(),                       &MainWindow::onToolsWordCount);
    auto* mFmt = mTools->addMenu(tr("&Format"));
    mkT(mFmt,   tr("JSON: Pretty"),      QKeySequence(),                       &MainWindow::onToolsJsonPretty);
    mkT(mFmt,   tr("JSON: Minify"),      QKeySequence(),                       &MainWindow::onToolsJsonMinify);
    mFmt->addSeparator();
    mkT(mFmt,   tr("XML: Pretty"),       QKeySequence(),                       &MainWindow::onToolsXmlPretty);
    mkT(mFmt,   tr("XML: Minify"),       QKeySequence(),                       &MainWindow::onToolsXmlMinify);
    mkT(mTools, tr("Pick &Color..."),    QKeySequence(),                       &MainWindow::onToolsPickColor);
    mkT(mTools, tr("&Macros..."),        QKeySequence(),                       &MainWindow::onToolsMacroDialog);
    mkT(mTools, tr("&Snippets..."),      QKeySequence(),                       &MainWindow::onToolsSnippets);
    mkT(mTools, tr("Check&sums..."),     QKeySequence(),                       &MainWindow::onToolsHash);
    auto* aSpell = mTools->addAction(tr("Spell &Check"));
    aSpell->setCheckable(true);
    if (m_spellChecker) aSpell->setChecked(m_spellChecker->isEnabled());
    connect(aSpell, &QAction::triggered, this, &MainWindow::onToolsToggleSpellCheck);
    mTools->addSeparator();
    mTools->addAction(m_actFindInFiles);
    mTools->addAction(m_actRunCommand);
    mTools->addSeparator();
    mTools->addAction(m_actCommandPalette);
    mTools->addSeparator();
    mTools->addAction(m_actPreferences);

    auto* mHelp = mb->addMenu(tr("&Help"));
    mHelp->addAction(m_actAbout);
}

void MainWindow::createToolBar() {
    // Helper: prefer freedesktop themed icon (Adwaita/Breeze/Papirus all expose
    // these names); fall back to QStyle's bundled standardIcon so the toolbar
    // is never blank even on minimal icon themes.
    auto themed = [this](const char* name, QStyle::StandardPixmap fallback) -> QIcon {
        QIcon i = QIcon::fromTheme(QString::fromLatin1(name));
        if (i.isNull()) i = style()->standardIcon(fallback);
        return i;
    };

    auto* tb = addToolBar(tr("Main Toolbar"));
    tb->setObjectName(QStringLiteral("MainToolBar"));   // needed by saveState/restoreState
    tb->setMovable(true);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);

    // Set icons on the actions we already created in createActions(). Qt picks them
    // up automatically wherever the action appears (menu + toolbar share state).
    m_actNew      ->setIcon(themed("document-new",          QStyle::SP_FileIcon));
    m_actOpen     ->setIcon(themed("document-open",         QStyle::SP_DirOpenIcon));
    m_actSave     ->setIcon(themed("document-save",         QStyle::SP_DialogSaveButton));
    m_actSaveAs   ->setIcon(themed("document-save-as",      QStyle::SP_DialogSaveButton));
    m_actCut      ->setIcon(themed("edit-cut",              QStyle::SP_FileLinkIcon));
    m_actCopy     ->setIcon(themed("edit-copy",             QStyle::SP_FileIcon));
    m_actPaste    ->setIcon(themed("edit-paste",            QStyle::SP_FileIcon));
    m_actUndo     ->setIcon(themed("edit-undo",             QStyle::SP_ArrowBack));
    m_actRedo     ->setIcon(themed("edit-redo",             QStyle::SP_ArrowForward));
    m_actFind     ->setIcon(themed("edit-find",             QStyle::SP_FileDialogContentsView));
    m_actReplace  ->setIcon(themed("edit-find-replace",     QStyle::SP_FileDialogContentsView));
    m_actFindInFiles->setIcon(themed("system-search",       QStyle::SP_FileDialogContentsView));
    m_actRunCommand ->setIcon(themed("system-run",          QStyle::SP_MediaPlay));
    m_actCommandPalette->setIcon(themed("edit-find",        QStyle::SP_TitleBarMenuButton));
    m_actClose    ->setIcon(themed("window-close",          QStyle::SP_DialogCloseButton));

    tb->addAction(m_actNew);
    tb->addAction(m_actOpen);
    tb->addAction(m_actSave);
    tb->addAction(m_actSaveAs);
    tb->addSeparator();
    tb->addAction(m_actCut);
    tb->addAction(m_actCopy);
    tb->addAction(m_actPaste);
    tb->addSeparator();
    tb->addAction(m_actUndo);
    tb->addAction(m_actRedo);
    tb->addSeparator();
    tb->addAction(m_actFind);
    tb->addAction(m_actReplace);
    tb->addAction(m_actFindInFiles);
    tb->addSeparator();
    tb->addAction(m_actRunCommand);
    tb->addAction(m_actCommandPalette);
}

void MainWindow::createStatusBar() {
    m_statusPosition = new QLabel(tr("Ln 1, Col 1"), this);
    m_statusEncoding = new QLabel(tr("UTF-8"), this);
    m_statusEol = new QLabel(tr("LF"), this);
    m_statusGit = new QLabel(QString(), this);
    m_statusGit->setToolTip(tr("Status do Git (branch / arquivo)"));

    statusBar()->addPermanentWidget(m_statusGit);
    statusBar()->addPermanentWidget(m_statusPosition);
    statusBar()->addPermanentWidget(m_statusEncoding);
    statusBar()->addPermanentWidget(m_statusEol);
}

// ---------------------------------------------------------------------------
// Tab helpers
// ---------------------------------------------------------------------------
EditorTab* MainWindow::currentTab() const   { return m_multiView->currentTab(); }
EditorTab* MainWindow::tabAt(int index) const { return m_multiView->tabAt(index); }

int MainWindow::findTabByPath(const QString& path) const {
    if (path.isEmpty()) return -1;
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        if (auto* t = tabAt(i); t && t->filePath() == path) return i;
    }
    return -1;
}

void MainWindow::setTabTitle(EditorTab* tab, const QString& title) {
    if (!tab) return;
    auto loc = m_multiView->locateTab(tab);
    if (loc.first && loc.second >= 0) loc.first->setTabText(loc.second, title);
}

void MainWindow::setTabTooltip(EditorTab* tab, const QString& tooltip) {
    if (!tab) return;
    auto loc = m_multiView->locateTab(tab);
    if (loc.first && loc.second >= 0) loc.first->setTabToolTip(loc.second, tooltip);
}

void MainWindow::setActiveTab(EditorTab* tab) {
    if (!tab) return;
    auto loc = m_multiView->locateTab(tab);
    if (loc.first && loc.second >= 0) loc.first->setCurrentIndex(loc.second);
}

void MainWindow::connectTabSignals(EditorTab* tab) {
    connect(tab, &EditorTab::modificationChanged, this, &MainWindow::onCurrentTabModified);
    connect(tab, &EditorTab::filePathChanged, this, &MainWindow::onCurrentTabFilePathChanged);
    connect(tab, &EditorTab::cursorPositionChanged, this, &MainWindow::onCursorPositionChanged);
    if (tab->editor()) UrlHyperlink::installFor(tab->editor());
    registerTabForRecovery(tab);
}

void MainWindow::registerTabForRecovery(EditorTab* tab) {
    if (!m_crashRecovery || !tab || !tab->editor()) return;
    const int bufferId = static_cast<int>(reinterpret_cast<qintptr>(tab) & 0x7FFFFFFF);
    QPointer<EditorTab> safe(tab);
    m_crashRecovery->registerBuffer(bufferId, tab->filePath(), QStringLiteral("UTF-8"),
        [safe]() -> QByteArray {
            if (!safe || !safe->editor()) return {};
            auto* sci = safe->editor();
            return sci->getText(sci->textLength() + 1);
        });
}

void MainWindow::applyEditorPreferences(EditorTab* tab) {
    // Pref-only — DOES NOT touch styles. styleClearAll() wipes lexer-applied styles,
    // so theme + lexer are paired separately in applyThemeAndLexer().
    if (!tab || !tab->editor()) return;
    auto* sci = tab->editor();
    const Settings& s = Settings::instance();
    sci->setTabWidth(s.tabWidth());
    sci->setUseTabs(!s.useSpaces());
    sci->setMarginWidthN(0, s.showLineNumbers() ? sci->textWidth(33 /*STYLE_LINENUMBER*/, "_9999") + 8 : 0);
    sci->setWrapMode(s.wordWrap() ? 1 /*SC_WRAP_WORD*/ : 0 /*SC_WRAP_NONE*/);
}

void MainWindow::applyThemeAndLexer(EditorTab* tab) {
    // Apply theme defaults (calls styleClearAll), THEN lexer (overrides per-style colors).
    // Order matters: theme first wipes everything, lexer last wins.
    if (!tab || !tab->editor()) return;
    auto* sci = tab->editor();
    const Settings& s = Settings::instance();
    ThemeManager::applyToScintilla(sci, ThemeManager::current(), s.fontFamily(), s.fontSize());
    LexerMap::applyLexerForPath(sci, tab->filePath());
}

void MainWindow::applyThemeToAllTabs() {
    const auto theme = ThemeManager::current();
    ThemeManager::apply(qApp, theme);
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        if (auto* t = tabAt(i)) applyThemeAndLexer(t);
    }
}

void MainWindow::updateWindowTitle() {
    auto* t = currentTab();
    if (!t) { setWindowTitle(tr("Notepad++ Qt")); return; }
    QString title = t->displayPath();
    if (t->isModified()) title += " *";
    setWindowTitle(QString("%1 — Notepad++ Qt").arg(title));
}

void MainWindow::updateStatusBar() {
    auto* t = currentTab();
    if (!t || !t->editor()) { m_statusPosition->setText(tr("Ln 1, Col 1")); return; }
    auto* sci = t->editor();
    const auto pos = sci->currentPos();
    const int line = static_cast<int>(sci->lineFromPosition(pos)) + 1;
    const int col = static_cast<int>(sci->column(pos)) + 1;
    m_statusPosition->setText(tr("Ln %1, Col %2").arg(line).arg(col));
}

void MainWindow::rebuildRecentFilesMenu() {
    m_menuRecent->clear();
    const QStringList files = Settings::instance().recentFiles();
    if (files.isEmpty()) {
        auto* a = m_menuRecent->addAction(tr("(empty)"));
        a->setEnabled(false);
        return;
    }
    for (const QString& path : files) {
        auto* a = m_menuRecent->addAction(QFileInfo(path).fileName() + "  " + path);
        a->setData(path);
        connect(a, &QAction::triggered, this, &MainWindow::onRecentFileTriggered);
    }
    m_menuRecent->addSeparator();
    auto* clear = m_menuRecent->addAction(tr("Clear list"));
    connect(clear, &QAction::triggered, this, [this]{
        Settings::instance().clearRecentFiles();
        Settings::instance().save();
        rebuildRecentFilesMenu();
    });
}

// ---------------------------------------------------------------------------
// File ops
// ---------------------------------------------------------------------------
void MainWindow::onFileNew() {
    auto* tab = new EditorTab(this);
    applyEditorPreferences(tab);
    applyThemeAndLexer(tab);
    connectTabSignals(tab);
    m_multiView->addTab(tab, tab->tabTitle());
    setActiveTab(tab);
    if (m_findDialog) m_findDialog->setActiveEditor(tab->editor());
    if (m_languagesMenu) {
        m_languagesMenu->setActiveEditor(tab->editor());
        m_languagesMenu->syncCheckedLanguage(QString());
    }
}

void MainWindow::onFileOpen() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
                                                      tr("All Files (*)"));
    if (path.isEmpty()) return;
    openFile(path);
}

void MainWindow::openFile(const QString& path) {
    if (path.isEmpty()) return;
    int existing = findTabByPath(path);
    if (existing >= 0) { setActiveTab(tabAt(existing)); return; }

    auto* tab = currentTab();
    const bool reuseEmpty = tab && tab->filePath().isEmpty() && !tab->isModified()
                            && tab->editor()->length() == 0;
    if (!reuseEmpty) {
        tab = new EditorTab(this);
        applyEditorPreferences(tab);
        applyThemeAndLexer(tab);
        connectTabSignals(tab);
        m_multiView->addTab(tab, tab->tabTitle());
        setActiveTab(tab);
    }

    loadFileIntoTab(tab, path);
}

void MainWindow::loadFileIntoTab(EditorTab* tab, const QString& path) {
    auto result = FileIO::readFile(path);
    if (!result.ok) { QMessageBox::warning(this, tr("Open failed"), result.error); return; }
    auto* sci = tab->editor();
    sci->setText(result.utf8.constData());
    sci->emptyUndoBuffer();
    tab->setFilePath(path);
    tab->setModified(false);

    applyEditorPreferences(tab);
    applyThemeAndLexer(tab);   // theme first then lexer — order matters (styleClearAll)

    Settings::instance().addRecentFile(path);
    Settings::instance().save();
    rebuildRecentFilesMenu();
    if (m_externalWatcher) m_externalWatcher->watch(path);

    setTabTitle(tab, tab->tabTitle());
    setTabTooltip(tab, path);
    m_statusEncoding->setText(result.detectedEncoding.isEmpty() ? "UTF-8" : result.detectedEncoding);
    if (m_languagesMenu) m_languagesMenu->syncCheckedLanguage(LexerMap::lexerNameForPath(path));
    updateWindowTitle();
}

bool MainWindow::saveTab(EditorTab* tab) {
    if (!tab) return false;
    if (tab->filePath().isEmpty()) return saveTabAs(tab);

    QByteArray bytes = tab->editor()->getText(tab->editor()->textLength() + 1);
    if (m_externalWatcher) m_externalWatcher->notifyOurWrite(tab->filePath());
    QString error;
    if (!FileIO::writeFile(tab->filePath(), bytes, &error)) {
        QMessageBox::warning(this, tr("Save failed"), error);
        return false;
    }
    tab->setModified(false);
    setTabTitle(tab, tab->tabTitle());
    updateWindowTitle();
    return true;
}

bool MainWindow::saveTabAs(EditorTab* tab) {
    if (!tab) return false;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save As"),
        tab->filePath().isEmpty() ? QString() : tab->filePath(), tr("All Files (*)"));
    if (path.isEmpty()) return false;
    tab->setFilePath(path);
    LexerMap::applyLexerForPath(tab->editor(), path);
    if (m_languagesMenu) m_languagesMenu->syncCheckedLanguage(LexerMap::lexerNameForPath(path));
    Settings::instance().addRecentFile(path);
    Settings::instance().save();
    rebuildRecentFilesMenu();
    return saveTab(tab);
}

bool MainWindow::maybeSaveTab(EditorTab* tab) {
    if (!tab || !tab->isModified()) return true;
    const auto ret = QMessageBox::warning(this, tr("Unsaved changes"),
        tr("'%1' has unsaved changes. Save before closing?").arg(tab->displayPath()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Cancel) return false;
    if (ret == QMessageBox::Save)  return saveTab(tab);
    return true;
}

void MainWindow::onFileSave()    { saveTab(currentTab()); }
void MainWindow::onFileSaveAs()  { saveTabAs(currentTab()); }
void MainWindow::onFileClose()   { onMultiViewTabCloseRequested(currentTab()); }
void MainWindow::onFileExit()    { close(); }

// ---------------------------------------------------------------------------
// Edit ops
// ---------------------------------------------------------------------------
#define WITH_SCI(call) do { if (auto* t = currentTab(); t && t->editor()) t->editor()->call; } while (0)

void MainWindow::onEditUndo()      { WITH_SCI(undo()); }
void MainWindow::onEditRedo()      { WITH_SCI(redo()); }
void MainWindow::onEditCut()       { WITH_SCI(cut()); }
void MainWindow::onEditCopy()      { WITH_SCI(copy()); }
void MainWindow::onEditPaste()     { WITH_SCI(paste()); }
void MainWindow::onEditSelectAll() { WITH_SCI(selectAll()); }

#undef WITH_SCI

// ---------------------------------------------------------------------------
// Search ops
// ---------------------------------------------------------------------------
void MainWindow::onSearchFind() {
    if (auto* t = currentTab()) {
        m_findDialog->setActiveEditor(t->editor());
        m_findDialog->showFind();
    }
}

void MainWindow::onSearchReplace() {
    if (auto* t = currentTab()) {
        m_findDialog->setActiveEditor(t->editor());
        m_findDialog->showReplace();
    }
}

void MainWindow::onSearchGoToLine() {
    auto* t = currentTab();
    if (!t) return;
    GoToLineDialog dlg(t->editor(), this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// View ops
// ---------------------------------------------------------------------------
void MainWindow::onViewSetThemeLight()   { ThemeManager::apply(qApp, AppTheme::Light);   Settings::instance().setDarkTheme(false); Settings::instance().save(); applyThemeToAllTabs(); }
void MainWindow::onViewSetThemeDark()    { ThemeManager::apply(qApp, AppTheme::Dark);    Settings::instance().setDarkTheme(true);  Settings::instance().save(); applyThemeToAllTabs(); }
void MainWindow::onViewSetThemeDracula() { ThemeManager::apply(qApp, AppTheme::Dracula); Settings::instance().setDarkTheme(true);  Settings::instance().save(); applyThemeToAllTabs(); }

void MainWindow::onViewToggleLineNumbers() {
    Settings::instance().setShowLineNumbers(m_actToggleLineNumbers->isChecked());
    Settings::instance().save();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) applyEditorPreferences(tabAt(i));   // pref only, lexer styles preserved
}

void MainWindow::onViewToggleWordWrap() {
    Settings::instance().setWordWrap(m_actToggleWordWrap->isChecked());
    Settings::instance().save();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) applyEditorPreferences(tabAt(i));   // pref only, lexer styles preserved
}

void MainWindow::onViewSplitHorizontal()      { m_multiView->splitView(MultiView::Orientation::Horizontal); }
void MainWindow::onViewSplitVertical()        { m_multiView->splitView(MultiView::Orientation::Vertical); }
void MainWindow::onViewUnsplit()              { m_multiView->unsplit(); }
void MainWindow::onViewMoveTabToOtherGroup()  { m_multiView->moveCurrentTabToOtherGroup(); }

// ---------------------------------------------------------------------------
// Languages
// ---------------------------------------------------------------------------
void MainWindow::onLanguageSelected(const QString& lexerName) {
    auto* t = currentTab();
    if (!t || !t->editor()) return;
    // Re-apply theme defaults first (clears stale lexer styling), then the new lexer.
    const Settings& s = Settings::instance();
    ThemeManager::applyToScintilla(t->editor(), ThemeManager::current(), s.fontFamily(), s.fontSize());
    LexerMap::applyLexerByName(t->editor(), lexerName);
}

// ---------------------------------------------------------------------------
// Tools / Help
// ---------------------------------------------------------------------------
void MainWindow::onToolsPreferences() {
    PreferencesDialog dlg(this);
    connect(&dlg, &PreferencesDialog::preferencesChanged, this, [this]{
        m_actToggleLineNumbers->setChecked(Settings::instance().showLineNumbers());
        m_actToggleWordWrap->setChecked(Settings::instance().wordWrap());
        if (Settings::instance().darkTheme()) m_actThemeDark->setChecked(true);
        else                                   m_actThemeLight->setChecked(true);
        const int n = m_multiView->tabCount();
        for (int i = 0; i < n; ++i) applyEditorPreferences(tabAt(i));
        applyThemeToAllTabs();   // already iterates tabs to re-apply theme+lexer
    });
    dlg.exec();
}

void MainWindow::onToolsCompare() {
    if (!m_comparePanel) m_comparePanel = new ComparePanel(this);
    auto* t = currentTab();
    if (t) {
        QByteArray a = t->editor()->getText(t->editor()->textLength() + 1);
        m_comparePanel->setLeft(t->displayPath(), QString::fromUtf8(a));
    }
    // For right-side: ask user to pick a file.
    const QString path = QFileDialog::getOpenFileName(this, tr("Compare with file..."));
    if (!path.isEmpty()) {
        auto r = FileIO::readFile(path);
        if (r.ok) m_comparePanel->setRight(path, QString::fromUtf8(r.utf8));
    }
    m_comparePanel->show();
    m_comparePanel->raise();
    m_comparePanel->activateWindow();
}

void MainWindow::onToolsCssPreview() {
    if (!m_cssPreviewPane) m_cssPreviewPane = new CssPreviewPane(this);
    if (auto* t = currentTab()) m_cssPreviewPane->bindToEditor(t->editor());
    m_cssPreviewPane->show();
    m_cssPreviewPane->raise();
    m_cssPreviewPane->activateWindow();
}

void MainWindow::onToolsCsvView() {
    if (!m_csvTableView) m_csvTableView = new CsvTableView(this);
    if (auto* t = currentTab()) {
        QByteArray a = t->editor()->getText(t->editor()->textLength() + 1);
        m_csvTableView->loadCsv(QString::fromUtf8(a), t->displayPath());
    }
    m_csvTableView->show();
    m_csvTableView->raise();
    m_csvTableView->activateWindow();
}

// ---------------------------------------------------------------------------
// New M3 slots
// ---------------------------------------------------------------------------
void MainWindow::onToolsMarkdownPreview() {
    if (!m_markdownPreviewPane) m_markdownPreviewPane = new MarkdownPreviewPane(this);
    if (auto* t = currentTab()) m_markdownPreviewPane->bindToEditor(t->editor());
    m_markdownPreviewPane->show();
    m_markdownPreviewPane->raise();
    m_markdownPreviewPane->activateWindow();
}

void MainWindow::onToolsHexViewer() {
    if (!m_hexViewer) m_hexViewer = new HexViewer(this);
    if (auto* t = currentTab()) {
        const QByteArray bytes = t->editor()->getText(t->editor()->textLength() + 1);
        m_hexViewer->load(bytes, t->displayPath());
    } else {
        m_hexViewer->load(QByteArray());
    }
    m_hexViewer->show();
    m_hexViewer->raise();
    m_hexViewer->activateWindow();
}

void MainWindow::onToolsFindInFiles() {
    if (!m_findInFilesDialog) {
        m_findInFilesDialog = new FindInFilesDialog(this);
        connect(m_findInFilesDialog, &FindInFilesDialog::openFileRequested,
                this, &MainWindow::onFindInFilesOpen);
    }
    m_findInFilesDialog->show();
    m_findInFilesDialog->raise();
    m_findInFilesDialog->activateWindow();
}

void MainWindow::onToolsCommandPalette() {
    if (!m_commandPalette) m_commandPalette = new CommandPalette(this);
    // Collect leaf actions (skip separators and submenu titles).
    QList<QAction*> actions;
    const auto menus = menuBar()->findChildren<QMenu*>();
    for (auto* m : menus) {
        for (auto* a : m->actions()) {
            if (a->isSeparator()) continue;
            if (a->menu()) continue;
            actions << a;
        }
    }
    m_commandPalette->setActions(actions);
    m_commandPalette->presentCentered();
}

void MainWindow::onToolsRunCommand() {
    if (m_execOutputPanel->isHidden()) {
        m_execOutputPanel->show();
        m_actToggleExecOutput->setChecked(true);
    }
    m_execOutputPanel->raise();
    // Focus the panel's command input — handled internally; we just bring it visible.
    m_execOutputPanel->setFocus();
}

void MainWindow::onViewToggleFunctionList() {
    const bool show = m_actToggleFunctionList->isChecked();
    m_functionListPanel->setVisible(show);
    if (show) {
        if (auto* t = currentTab()) {
            m_functionListPanel->setActiveEditor(t->editor(),
                LexerMap::lexerNameForPath(t->filePath()));
            m_functionListPanel->refresh();
        }
    }
}

void MainWindow::onViewToggleDocumentMap() {
    const bool show = m_actToggleDocumentMap->isChecked();
    m_documentMapPanel->setVisible(show);
    if (show) {
        if (auto* t = currentTab()) m_documentMapPanel->setActiveEditor(t->editor());
    }
}

void MainWindow::onViewToggleFileBrowser() {
    m_fileBrowserPanel->setVisible(m_actToggleFileBrowser->isChecked());
}

void MainWindow::onViewToggleExecOutput() {
    m_execOutputPanel->setVisible(m_actToggleExecOutput->isChecked());
}

void MainWindow::onFileBrowserOpenFile(const QString& path) { openFile(path); }

void MainWindow::onFindInFilesOpen(const QString& path, int line) {
    openFile(path);
    if (auto* t = currentTab()) {
        t->editor()->gotoLine(line - 1);
        t->editor()->scrollCaret();
    }
}

void MainWindow::onFunctionListGoto(int line) {
    if (auto* t = currentTab()) {
        t->editor()->gotoLine(line - 1);
        t->editor()->scrollCaret();
    }
}

void MainWindow::onAutoSaveTick() {
    const bool onlyNamed = m_autoSave && m_autoSave->saveOnlyNamedFiles();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        auto* t = tabAt(i);
        if (!t || !t->isModified()) continue;
        if (onlyNamed && t->filePath().isEmpty()) continue;
        if (!t->filePath().isEmpty()) saveTab(t);
    }
}

void MainWindow::onHelpAbout() {
    QMessageBox::about(this, tr("About Notepad++ Qt"),
        tr("<h3>Notepad++ Qt 0.1.0</h3>"
           "<p>Native Linux Qt6 port of Notepad++ — foundation milestone.</p>"
           "<p>Repository: <a href=\"https://github.com/clip52/notepad-fedora\">"
           "github.com/clip52/notepad-fedora</a></p>"
           "<p>Editor widget: Scintilla (Qt edition). Lexers: Lexilla.</p>"));
}

// ---------------------------------------------------------------------------
// MultiView event handlers
// ---------------------------------------------------------------------------
void MainWindow::onMultiViewCurrentChanged(EditorTab* tab) {
    auto* sci = tab ? tab->editor() : nullptr;
    if (m_findDialog) m_findDialog->setActiveEditor(sci);
    if (m_languagesMenu) {
        m_languagesMenu->setActiveEditor(sci);
        if (tab) m_languagesMenu->syncCheckedLanguage(LexerMap::lexerNameForPath(tab->filePath()));
    }
    if (m_functionListPanel && m_functionListPanel->isVisible()) {
        m_functionListPanel->setActiveEditor(sci,
            tab ? LexerMap::lexerNameForPath(tab->filePath()) : QString());
    }
    if (m_documentMapPanel && m_documentMapPanel->isVisible()) {
        m_documentMapPanel->setActiveEditor(sci);
    }
    if (m_autoCompleter) m_autoCompleter->setActiveEditor(sci);
    if (m_bookmarks)    m_bookmarks->setActiveEditor(sci);
    if (m_macros)       m_macros->setActiveEditor(sci);
    if (m_eolMenu) {
        m_eolMenu->setActiveEditor(sci);
        if (sci) m_eolMenu->syncCurrentMode();
    }
    if (m_braceMatcher)   m_braceMatcher->setActiveEditor(sci);
    if (m_spellChecker)   m_spellChecker->setActiveEditor(sci);
    if (m_editEnhance)    m_editEnhance->setActiveEditor(sci);
    if (m_whitespaceView && sci) m_whitespaceView->applyTo(sci);
    if (m_gitStatus && tab && !tab->filePath().isEmpty())
        m_gitStatus->queryStatus(tab->filePath());
    else if (m_statusGit) m_statusGit->setText(QString());
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::onMultiViewTabCloseRequested(EditorTab* tab) {
    if (!tab) return;
    if (!maybeSaveTab(tab)) return;
    auto loc = m_multiView->locateTab(tab);
    if (loc.first && loc.second >= 0) {
        loc.first->removeTab(loc.second);
    }
    delete tab;
    if (m_multiView->tabCount() == 0) onFileNew();
}

void MainWindow::onCurrentTabModified(bool /*modified*/) {
    auto* t = qobject_cast<EditorTab*>(sender());
    if (!t) return;
    setTabTitle(t, t->tabTitle());
    if (t == currentTab()) updateWindowTitle();
}

void MainWindow::onCurrentTabFilePathChanged(const QString& path) {
    auto* t = qobject_cast<EditorTab*>(sender());
    if (!t) return;
    setTabTitle(t, t->tabTitle());
    setTabTooltip(t, t->displayPath());
    if (t == currentTab()) updateWindowTitle();
    if (m_gitStatus && !path.isEmpty()) m_gitStatus->queryStatus(path);
}

void MainWindow::onCursorPositionChanged(int line, int column) {
    auto* t = qobject_cast<EditorTab*>(sender());
    if (t == currentTab()) m_statusPosition->setText(tr("Ln %1, Col %2").arg(line).arg(column));
}

void MainWindow::onRecentFileTriggered() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!a) return;
    openFile(a->data().toString());
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// M4 slot implementations
// ---------------------------------------------------------------------------
#define WITH_SCI_DO(call) do { if (auto* t = currentTab(); t && t->editor()) { call; } } while (0)
#define SCI_OR_NULL  (currentTab() ? currentTab()->editor() : nullptr)

void MainWindow::onEditTrimWhitespace()  { WITH_SCI_DO(EditOperations::trimTrailingWhitespace(t->editor())); }
void MainWindow::onEditUpperCase()       { WITH_SCI_DO(EditOperations::toUpperSelection(t->editor())); }
void MainWindow::onEditLowerCase()       { WITH_SCI_DO(EditOperations::toLowerSelection(t->editor())); }
void MainWindow::onEditTitleCase()       { WITH_SCI_DO(EditOperations::toTitleSelection(t->editor())); }
void MainWindow::onEditSortAsc()         { WITH_SCI_DO(EditOperations::sortLinesAscending(t->editor())); }
void MainWindow::onEditSortDesc()        { WITH_SCI_DO(EditOperations::sortLinesDescending(t->editor())); }
void MainWindow::onEditSortUnique()      { WITH_SCI_DO(EditOperations::sortLinesUnique(t->editor())); }
void MainWindow::onEditDuplicateLine()   { WITH_SCI_DO(EditOperations::duplicateLine(t->editor())); }
void MainWindow::onEditMoveLineUp()      { WITH_SCI_DO(EditOperations::moveLineUp(t->editor())); }
void MainWindow::onEditMoveLineDown()    { WITH_SCI_DO(EditOperations::moveLineDown(t->editor())); }
void MainWindow::onEditTabsToSpaces()    { WITH_SCI_DO(EditOperations::tabsToSpaces(t->editor(), Settings::instance().tabWidth())); }
void MainWindow::onEditSpacesToTabs()    { WITH_SCI_DO(EditOperations::spacesToTabs(t->editor(), Settings::instance().tabWidth())); }

void MainWindow::onBookmarkToggle()  { if (m_bookmarks) m_bookmarks->toggleAtCaret(); }
void MainWindow::onBookmarkNext()    { if (m_bookmarks) m_bookmarks->gotoNext(); }
void MainWindow::onBookmarkPrev()    { if (m_bookmarks) m_bookmarks->gotoPrevious(); }
void MainWindow::onBookmarkClearAll(){ if (m_bookmarks) m_bookmarks->clearAll(); }

void MainWindow::onBookmarkList() {
    auto* sci = SCI_OR_NULL;
    if (!sci || !m_bookmarks) return;
    BookmarkDialog dlg(m_bookmarks, sci, this);
    connect(&dlg, &BookmarkDialog::gotoLineRequested, this, [this](int line) {
        if (auto* s = SCI_OR_NULL) { s->gotoLine(line - 1); s->scrollCaret(); }
    });
    dlg.exec();
}

void MainWindow::onToolsWordCount() {
    if (!m_wordCountDialog) m_wordCountDialog = new WordCountDialog(this);
    if (auto* t = currentTab()) m_wordCountDialog->load(t->editor(), t->displayPath());
    m_wordCountDialog->show();
    m_wordCountDialog->raise();
    m_wordCountDialog->activateWindow();
}

void MainWindow::onToolsJsonPretty() {
    auto* sci = SCI_OR_NULL; if (!sci) return;
    QString err; auto r = JsonXmlFormatter::jsonPretty(sci, &err);
    if (r == JsonXmlFormatter::Result::ParseError)
        QMessageBox::warning(this, tr("JSON Pretty"), tr("Parse error: %1").arg(err));
}
void MainWindow::onToolsJsonMinify() {
    auto* sci = SCI_OR_NULL; if (!sci) return;
    QString err; auto r = JsonXmlFormatter::jsonMinify(sci, &err);
    if (r == JsonXmlFormatter::Result::ParseError)
        QMessageBox::warning(this, tr("JSON Minify"), tr("Parse error: %1").arg(err));
}
void MainWindow::onToolsXmlPretty() {
    auto* sci = SCI_OR_NULL; if (!sci) return;
    QString err; auto r = JsonXmlFormatter::xmlPretty(sci, &err);
    if (r == JsonXmlFormatter::Result::ParseError)
        QMessageBox::warning(this, tr("XML Pretty"), tr("Parse error: %1").arg(err));
}
void MainWindow::onToolsXmlMinify() {
    auto* sci = SCI_OR_NULL; if (!sci) return;
    QString err; auto r = JsonXmlFormatter::xmlMinify(sci, &err);
    if (r == JsonXmlFormatter::Result::ParseError)
        QMessageBox::warning(this, tr("XML Minify"), tr("Parse error: %1").arg(err));
}

void MainWindow::onToolsPickColor() {
    if (auto* sci = SCI_OR_NULL) ColorPickerHelper::pickAndReplace(sci, this);
}

void MainWindow::onToolsMacroDialog() {
    auto* sci = SCI_OR_NULL;
    if (!m_macroDialog) m_macroDialog = new MacroDialog(m_macros, sci, this);
    m_macroDialog->show();
    m_macroDialog->raise();
    m_macroDialog->activateWindow();
}

// ---------------------------------------------------------------------------
// M5 slot implementations
// ---------------------------------------------------------------------------
void MainWindow::onFilePrint() {
    if (auto* sci = SCI_OR_NULL) PrintHelper::printDocument(sci, this);
}
void MainWindow::onFilePrintPreview() {
    if (auto* sci = SCI_OR_NULL) PrintHelper::previewDocument(sci, this);
}
void MainWindow::onFileReloadFromDisk() {
    auto* t = currentTab();
    if (!t || t->filePath().isEmpty()) return;
    QString err;
    if (!EditEnhancements::reloadFromDisk(t->editor(), t->filePath(), &err)) {
        QMessageBox::warning(this, tr("Reload failed"), err);
        return;
    }
    t->setModified(false);
    applyEditorPreferences(t);
    applyThemeAndLexer(t);
}
void MainWindow::onFileOpenFolder() {
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    if (folder.isEmpty()) return;
    if (m_recentProjects) m_recentProjects->use(folder);
    if (m_fileBrowserPanel) {
        m_fileBrowserPanel->setRootPath(folder);
        m_fileBrowserPanel->show();
        if (m_actToggleFileBrowser) m_actToggleFileBrowser->setChecked(true);
    }
}

void MainWindow::onSearchGotoMatchingBrace() {
    if (m_braceMatcher) m_braceMatcher->gotoMatchingBrace();
}

void MainWindow::onViewToggleWhitespace() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!m_whitespaceView || !a) return;
    m_whitespaceView->setWhitespaceVisible(a->isChecked());
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) if (auto* t = tabAt(i)) m_whitespaceView->applyTo(t->editor());
}
void MainWindow::onViewToggleEol() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!m_whitespaceView || !a) return;
    m_whitespaceView->setEolVisible(a->isChecked());
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) if (auto* t = tabAt(i)) m_whitespaceView->applyTo(t->editor());
}
void MainWindow::onViewToggleIndentGuides() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!m_whitespaceView || !a) return;
    m_whitespaceView->setIndentGuidesVisible(a->isChecked());
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) if (auto* t = tabAt(i)) m_whitespaceView->applyTo(t->editor());
}

void MainWindow::onToolsHash() {
    if (!m_hashDialog) m_hashDialog = new HashDialog(this);
    if (auto* t = currentTab()) {
        const QByteArray sel = t->editor()->getSelText();
        if (!sel.isEmpty()) m_hashDialog->load(sel, tr("selection (%1 bytes)").arg(sel.size()));
        else                m_hashDialog->load(t->editor()->getText(t->editor()->textLength() + 1), t->displayPath());
    }
    m_hashDialog->show();
    m_hashDialog->raise();
    m_hashDialog->activateWindow();
}

void MainWindow::onToolsSnippets() {
    if (!m_snippetsDialog) m_snippetsDialog = new SnippetsDialog(m_snippets, this);
    m_snippetsDialog->show();
    m_snippetsDialog->raise();
    m_snippetsDialog->activateWindow();
}

void MainWindow::onToolsToggleSpellCheck() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!m_spellChecker || !a) return;
    m_spellChecker->setEnabled(a->isChecked());
    if (auto* sci = SCI_OR_NULL) m_spellChecker->setActiveEditor(sci);
}

void MainWindow::onExternalFileChanged(const QString& path) {
    int idx = findTabByPath(path);
    if (idx < 0) return;
    auto* t = tabAt(idx);
    if (!t) return;
    const auto ans = QMessageBox::question(this, tr("File changed externally"),
        tr("'%1' was modified outside the editor.\nReload from disk?").arg(path),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans == QMessageBox::Yes) {
        QString err;
        if (EditEnhancements::reloadFromDisk(t->editor(), path, &err)) {
            t->setModified(false);
            applyEditorPreferences(t);
            applyThemeAndLexer(t);
        } else {
            QMessageBox::warning(this, tr("Reload failed"), err);
        }
    }
}
void MainWindow::onExternalFileRemoved(const QString& path) {
    QMessageBox::warning(this, tr("File removed"),
        tr("'%1' no longer exists on disk.").arg(path));
}

#undef WITH_SCI_DO
#undef SCI_OR_NULL

void MainWindow::closeEvent(QCloseEvent* event) {
    // Save session BEFORE save-prompts (so even if user cancels we have current list).
    if (m_session) {
        QStringList paths;
        const int n = m_multiView->tabCount();
        for (int i = 0; i < n; ++i) {
            if (auto* t = tabAt(i)) {
                if (!t->filePath().isEmpty()) paths << t->filePath();
            }
        }
        // active index in the saved list (closest match)
        int active = 0;
        if (auto* cur = currentTab()) {
            const int idx = paths.indexOf(cur->filePath());
            if (idx >= 0) active = idx;
        }
        m_session->saveSession(paths, active);
    }

    for (int i = m_multiView->tabCount() - 1; i >= 0; --i) {
        if (!maybeSaveTab(tabAt(i))) { event->ignore(); return; }
    }
    Settings::instance().setWindowGeometry(saveGeometry());
    Settings::instance().setWindowState(saveState());
    Settings::instance().save();
    if (m_crashRecovery) m_crashRecovery->shutdownClean();
    event->accept();
}

// ---------------------------------------------------------------------------
// M6 slots
// ---------------------------------------------------------------------------
void MainWindow::onToolsCodeFormat() {
    if (!m_codeFormatter) return;
    auto* t = currentTab();
    if (!t || !t->editor()) return;
    m_codeFormatter->formatActiveEditor(t->editor());
}

void MainWindow::onThemePackSelected() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!a) return;
    const auto id = static_cast<ThemePackId>(a->data().toInt());
    ThemePack::applyToApp(qApp, id);
    const Settings& s = Settings::instance();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        if (auto* tab = tabAt(i); tab && tab->editor()) {
            ThemePack::applyToEditor(tab->editor(), id, s.fontFamily(), s.fontSize());
        }
    }
    ThemePack::save(id);
}

void MainWindow::onGitStatusReady(const QString& path, const GitStatus& status) {
    if (!m_statusGit) return;
    auto* t = currentTab();
    if (!t || t->filePath() != path) return;
    if (status.state == GitStatus::State::NotInRepo) {
        m_statusGit->setText(QString());
        return;
    }
    QString glyph = GitStatusService::stateGlyph(status.state);
    QString text = QString("git: %1 %2").arg(glyph, status.branch);
    if (status.ahead)  text += QString(" ↑%1").arg(status.ahead);
    if (status.behind) text += QString(" ↓%1").arg(status.behind);
    m_statusGit->setText(text);
}

void MainWindow::onWorkspaceOpen() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir Workspace"), QString(),
                                                      tr("Workspaces (*.nppproj.json)"));
    if (path.isEmpty()) return;
    if (!m_workspace->load(path)) {
        QMessageBox::warning(this, tr("Workspace"), m_workspace->lastError());
        return;
    }
    applyWorkspaceData();
    rebuildRecentWorkspacesMenu();
}

void MainWindow::onWorkspaceSave() {
    if (!m_workspace) return;
    QString path = m_workspace->currentPath();
    if (path.isEmpty()) { onWorkspaceSaveAs(); return; }
    auto& d = m_workspace->mutableData();
    d.openFiles.clear();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        if (auto* t = tabAt(i); t && !t->filePath().isEmpty()) {
            WorkspaceFile wf;
            wf.path = t->filePath();
            wf.active = (t == currentTab());
            d.openFiles.append(wf);
        }
    }
    if (!m_workspace->save(path)) {
        QMessageBox::warning(this, tr("Workspace"), m_workspace->lastError());
    }
    rebuildRecentWorkspacesMenu();
}

void MainWindow::onWorkspaceSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Salvar Workspace"),
                                                QStringLiteral("workspace.nppproj.json"),
                                                tr("Workspaces (*.nppproj.json)"));
    if (path.isEmpty()) return;
    auto& d = m_workspace->mutableData();
    d.openFiles.clear();
    const int n = m_multiView->tabCount();
    for (int i = 0; i < n; ++i) {
        if (auto* t = tabAt(i); t && !t->filePath().isEmpty()) {
            WorkspaceFile wf;
            wf.path = t->filePath();
            wf.active = (t == currentTab());
            d.openFiles.append(wf);
        }
    }
    if (!m_workspace->save(path)) {
        QMessageBox::warning(this, tr("Workspace"), m_workspace->lastError());
    }
    rebuildRecentWorkspacesMenu();
}

void MainWindow::onRecentWorkspaceTriggered() {
    auto* a = qobject_cast<QAction*>(sender());
    if (!a) return;
    const QString path = a->data().toString();
    if (path.isEmpty()) return;
    if (!m_workspace->load(path)) {
        QMessageBox::warning(this, tr("Workspace"), m_workspace->lastError());
        return;
    }
    applyWorkspaceData();
    rebuildRecentWorkspacesMenu();
}

void MainWindow::rebuildRecentWorkspacesMenu() {
    if (!m_menuRecentWorkspaces) return;
    m_menuRecentWorkspaces->clear();
    const QStringList recent = Workspace::recentWorkspaces();
    if (recent.isEmpty()) {
        auto* a = m_menuRecentWorkspaces->addAction(tr("(vazio)"));
        a->setEnabled(false);
        return;
    }
    for (const QString& p : recent) {
        auto* a = m_menuRecentWorkspaces->addAction(QFileInfo(p).fileName() + "  " + p);
        a->setData(p);
        connect(a, &QAction::triggered, this, &MainWindow::onRecentWorkspaceTriggered);
    }
    m_menuRecentWorkspaces->addSeparator();
    auto* clr = m_menuRecentWorkspaces->addAction(tr("Limpar lista"));
    connect(clr, &QAction::triggered, this, [this]{
        Workspace::clearRecent();
        rebuildRecentWorkspacesMenu();
    });
}

void MainWindow::applyWorkspaceData() {
    if (!m_workspace) return;
    const auto& d = m_workspace->data();
    EditorTab* lastActive = nullptr;
    for (const WorkspaceFile& wf : d.openFiles) {
        if (wf.path.isEmpty()) continue;
        openFile(wf.path);
        if (wf.active) {
            const int idx = findTabByPath(wf.path);
            if (idx >= 0) lastActive = tabAt(idx);
        }
    }
    if (lastActive) setActiveTab(lastActive);
}
