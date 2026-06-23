cmake_minimum_required(VERSION 3.20)
project(lordpad VERSION 0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Core5Compat LinguistTools Concurrent Xml PrintSupport Network Sql)
find_package(PkgConfig)
# uchardet e hunspell são opcionais. No build Linux nativo eles existem e são
# usados; no cross-build Windows (MinGW) não há pacote, então caem em fallback
# (Qt p/ detecção de encoding; stub no-op p/ corretor — compat/HunspellStub.hxx).
if(PkgConfig_FOUND)
    pkg_check_modules(UCHARDET IMPORTED_TARGET uchardet)
    pkg_check_modules(HUNSPELL IMPORTED_TARGET hunspell)
endif()

# M9: Python plugin host (optional). When the embed dev headers are absent we
# build the rest of the editor without the plugin subsystem; the host class
# becomes a no-op and the menu entry stays disabled.
#
# Para distribuir pacotes (.deb/.rpm) entre distros, desligue com
# `-DLORDPAD_PLUGIN_HOST=OFF`: o binário deixa de exigir libpython3.X.so do
# build host, ficando portável.
option(LORDPAD_PLUGIN_HOST "Build with Python plugin host (libpython embed)" ON)
if(LORDPAD_PLUGIN_HOST)
    pkg_check_modules(PYTHON_EMBED IMPORTED_TARGET python3-embed)
    if(PYTHON_EMBED_FOUND)
        message(STATUS "Python plugin host: enabled (python3-embed ${PYTHON_EMBED_VERSION})")
    else()
        message(STATUS "Python plugin host: disabled (python3-embed pkg-config not found)")
    endif()
else()
    message(STATUS "Python plugin host: disabled (LORDPAD_PLUGIN_HOST=OFF)")
endif()

set(SCINTILLA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../scintilla")
set(LEXILLA_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../lexilla")

# ---------------------------------------------------------------------------
# scintilla-qt: Scintilla edit widget for Qt (ScintillaEditBase + ScintillaEdit)
# ---------------------------------------------------------------------------
set(SCI_SRC ${SCINTILLA_DIR}/src)
set(SCI_QT  ${SCINTILLA_DIR}/qt)

set(SCI_CORE_SOURCES
    ${SCI_SRC}/AutoComplete.cxx
    ${SCI_SRC}/CallTip.cxx
    ${SCI_SRC}/CaseConvert.cxx
    ${SCI_SRC}/CaseFolder.cxx
    ${SCI_SRC}/CellBuffer.cxx
    ${SCI_SRC}/ChangeHistory.cxx
    ${SCI_SRC}/CharacterCategoryMap.cxx
    ${SCI_SRC}/CharacterType.cxx
    ${SCI_SRC}/CharClassify.cxx
    ${SCI_SRC}/ContractionState.cxx
    ${SCI_SRC}/DBCS.cxx
    ${SCI_SRC}/Decoration.cxx
    ${SCI_SRC}/Document.cxx
    ${SCI_SRC}/EditModel.cxx
    ${SCI_SRC}/Editor.cxx
    ${SCI_SRC}/EditView.cxx
    ${SCI_SRC}/Geometry.cxx
    ${SCI_SRC}/Indicator.cxx
    ${SCI_SRC}/KeyMap.cxx
    ${SCI_SRC}/LineMarker.cxx
    ${SCI_SRC}/MarginView.cxx
    ${SCI_SRC}/PerLine.cxx
    ${SCI_SRC}/PositionCache.cxx
    ${SCI_SRC}/RESearch.cxx
    ${SCI_SRC}/RunStyles.cxx
    ${SCI_SRC}/ScintillaBase.cxx
    ${SCI_SRC}/Selection.cxx
    ${SCI_SRC}/Style.cxx
    ${SCI_SRC}/UndoHistory.cxx
    ${SCI_SRC}/UniConversion.cxx
    ${SCI_SRC}/UniqueString.cxx
    ${SCI_SRC}/ViewStyle.cxx
    ${SCI_SRC}/XPM.cxx
)

set(SCI_QT_SOURCES
    ${SCI_QT}/ScintillaEditBase/PlatQt.cpp
    ${SCI_QT}/ScintillaEditBase/ScintillaQt.cpp
    ${SCI_QT}/ScintillaEditBase/ScintillaEditBase.cpp
    ${SCI_QT}/ScintillaEdit/ScintillaDocument.cpp
    ${SCI_QT}/ScintillaEdit/ScintillaEdit.cpp
)

# Headers that need MOC processing (have Q_OBJECT)
set(SCI_QT_MOC_HEADERS
    ${SCI_QT}/ScintillaEditBase/ScintillaEditBase.h
    ${SCI_QT}/ScintillaEditBase/ScintillaQt.h
    ${SCI_QT}/ScintillaEdit/ScintillaEdit.h
    ${SCI_QT}/ScintillaEdit/ScintillaDocument.h
)

add_library(scintilla-qt STATIC ${SCI_CORE_SOURCES} ${SCI_QT_SOURCES} ${SCI_QT_MOC_HEADERS})
target_include_directories(scintilla-qt PUBLIC
    ${SCINTILLA_DIR}/include
    ${SCI_SRC}
    ${SCI_QT}/ScintillaEditBase
    ${SCI_QT}/ScintillaEdit
)
target_compile_definitions(scintilla-qt PUBLIC SCINTILLA_QT=1 MAKING_LIBRARY=1)
target_link_libraries(scintilla-qt PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat)
set_target_properties(scintilla-qt PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_options(scintilla-qt PRIVATE -Wno-conversion -Wno-deprecated-declarations)

# ---------------------------------------------------------------------------
# lexilla-static: language lexers (platform-neutral)
# ---------------------------------------------------------------------------
file(GLOB LEXILLA_LEXER_SOURCES "${LEXILLA_DIR}/lexers/*.cxx")
file(GLOB LEXILLA_LEXLIB_SOURCES "${LEXILLA_DIR}/lexlib/*.cxx")
# LexUser.cxx is a Notepad++ Windows-only User-Defined-Language lexer; exclude on Linux.
list(FILTER LEXILLA_LEXER_SOURCES EXCLUDE REGEX "LexUser\\.cxx$")
set(LEXILLA_SOURCES
    ${LEXILLA_DIR}/src/Lexilla.cxx
    ${LEXILLA_LEXLIB_SOURCES}
    ${LEXILLA_LEXER_SOURCES}
)

add_library(lexilla-static STATIC ${LEXILLA_SOURCES})
target_include_directories(lexilla-static PUBLIC
    ${LEXILLA_DIR}/include
    ${LEXILLA_DIR}/lexlib
    ${SCINTILLA_DIR}/include
)
target_compile_definitions(lexilla-static PUBLIC LEXILLA_STATIC)
set_target_properties(lexilla-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(lexilla-static PRIVATE -Wno-conversion -Wno-deprecated-declarations -include cstdint)
target_compile_options(scintilla-qt   PRIVATE -include cstdint)

# ---------------------------------------------------------------------------
# lordpad: the application
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/EditorTab.cpp
    src/FileIO.cpp
    src/Theme.cpp
    src/Settings.cpp
    src/LexerMap.cpp
    src/I18n.cpp
    src/LanguagesMenu.cpp
    src/MultiView.cpp
    src/AutoSavePolicy.cpp
    src/SessionManager.cpp
    src/AutoCompleter.cpp
    src/BookmarkManager.cpp
    src/BraceMatcher.cpp
    src/ColorPickerHelper.cpp
    src/CrossGroupTabBar.cpp
    src/EditEnhancements.cpp
    src/EditOperations.cpp
    src/EolMenu.cpp
    src/ExternalFileWatcher.cpp
    src/JsonXmlFormatter.cpp
    src/MacroRecorder.cpp
    src/PrintHelper.cpp
    src/RecentProjects.cpp
    src/Snippets.cpp
    src/SpellChecker.cpp
    src/WhitespaceView.cpp
    src/TabExtras.cpp
    src/UrlHyperlink.cpp
    src/CodeFormatter.cpp
    src/GitStatusService.cpp
    src/ThemePack.cpp
    src/CrashRecovery.cpp
    src/Workspace.cpp
    src/OutlineRegex.cpp
    src/MultiCursor.cpp
    src/SmartHighlight.cpp
    src/ColorMarkers.cpp
    src/FoldingMenu.cpp
    src/LspClient.cpp
    src/LspFeatures.cpp
    src/GitDiffGutter.cpp
    src/ViewModes.cpp
    src/EncodingConvert.cpp
    src/plugins/PythonPluginHost.cpp
    src/dialogs/PluginManagerDialog.cpp
    src/dialogs/QuickPickDialog.cpp
    src/panels/FunctionListPanel.cpp
    src/panels/LspDiagnosticsPanel.cpp
    src/panels/TerminalPanel.cpp
    src/panels/FtpPanel.cpp
    src/panels/SftpPanel.cpp
    src/network/FtpClient.cpp
    src/network/SftpClient.cpp
    src/dialogs/SshConnectDialog.cpp
    src/dialogs/KeybindingsDialog.cpp
    src/dialogs/GitCommitDialog.cpp
    src/panels/GitLogPanel.cpp
    src/KeybindingsManager.cpp
    src/GitOps.cpp
    src/StickyScroll.cpp
    src/BracketColors.cpp
    src/SearchEscapes.cpp
    src/TasksRunner.cpp
    src/panels/CalendarPanel.cpp
    src/panels/CalculatorPanel.cpp
    src/panels/TodoPanel.cpp
    src/panels/QrPanel.cpp
    src/panels/MermaidPanel.cpp
    src/dialogs/GitBranchDialog.cpp
    src/dialogs/GitStashDialog.cpp
    src/panels/MergeResolverPanel.cpp
    src/panels/TestRunnerPanel.cpp
    src/panels/NotebookPanel.cpp
    src/panels/NotesPanel.cpp
    src/FormatOnSave.cpp
    src/TypewriterMode.cpp
    src/Refactor.cpp
    src/VimMode.cpp
    src/dialogs/ThemeEditorDialog.cpp
    src/panels/RestClientPanel.cpp
    src/panels/SqlitePanel.cpp
    src/panels/AiPanel.cpp
    src/panels/PoEditorPanel.cpp
    src/CoverageGutter.cpp
    src/JsonSchemaValidator.cpp
    src/EditorConfig.cpp
    src/AutoPair.cpp
    src/SmartIndent.cpp
    src/CodeMetrics.cpp
    src/panels/ImageViewerPanel.cpp
    src/panels/RegexTesterPanel.cpp
    src/panels/UnitConverterPanel.cpp
    src/panels/JsonPathPanel.cpp
    src/panels/ColorPalettePanel.cpp
    src/panels/SysInfoPanel.cpp
    src/dialogs/GitignoreDialog.cpp
    src/dialogs/PreCommitDialog.cpp
    src/AiGhostCompletion.cpp
    src/LayoutPresets.cpp
    src/panels/DevToolsPanel.cpp
    src/panels/DbShellPanel.cpp
    src/panels/JsonLinesPanel.cpp
    src/panels/DocPreviewPanel.cpp
    src/panels/ClipboardHistoryPanel.cpp
    src/panels/ArchiveBrowserPanel.cpp
    src/panels/TimerPanel.cpp
    src/AutoCorrect.cpp
    src/SmartSelection.cpp
    src/panels/GraphQlPanel.cpp
    src/panels/CodeReviewPanel.cpp
    src/panels/ProfileRunnerPanel.cpp
    src/panels/ImageAnnotPanel.cpp
    src/panels/CsvChartPanel.cpp
    src/panels/MdTablePanel.cpp
    src/panels/CronEditorPanel.cpp
    src/panels/FileWatcherPanel.cpp
    src/panels/DocumentMapPanel.cpp
    src/panels/FileBrowserPanel.cpp
    src/panels/ExecOutputPanel.cpp
    src/panels/TextStatsPanel.cpp
    src/panels/MindMapPanel.cpp
    src/panels/PastebinPanel.cpp
    src/panels/TimeTrackerPanel.cpp
    src/panels/CodeClonesPanel.cpp
    src/panels/CallGraphPanel.cpp
    src/panels/SecretScannerPanel.cpp
    src/panels/ScreenshotPanel.cpp
    src/panels/RssReaderPanel.cpp
    src/panels/CliShellPanel.cpp
    src/panels/HexViewerPanel.cpp
    src/panels/SqlSchemaPanel.cpp
    src/panels/BuildWatchPanel.cpp
    src/panels/TodoAggregatorPanel.cpp
    src/panels/GrepPanel.cpp
    src/panels/YamlValidatorPanel.cpp
    src/panels/DockerPanel.cpp
    src/panels/JqRunnerPanel.cpp
    src/panels/HtmlPreviewPanel.cpp
    src/panels/GistPanel.cpp
    src/panels/EnvManagerPanel.cpp
    src/panels/SshExecPanel.cpp
    src/panels/CheatsheetPanel.cpp
    src/panels/VaultPanel.cpp
    src/panels/KubectlPanel.cpp
    src/panels/SystemMonitorPanel.cpp
    src/panels/GpgPanel.cpp
    src/panels/SslCertViewerPanel.cpp
    src/panels/AsciiDocPreviewPanel.cpp
    src/panels/PortScanPanel.cpp
    src/panels/SystemdServicesPanel.cpp
    src/panels/TunnelManagerPanel.cpp
    src/panels/ApiBrowserPanel.cpp
    src/panels/LogTailPanel.cpp
    src/panels/SshfsPanel.cpp
    src/dialogs/BookmarkDialog.cpp
    src/dialogs/HashDialog.cpp
    src/dialogs/MacroDialog.cpp
    src/dialogs/SnippetsDialog.cpp
    src/dialogs/WordCountDialog.cpp
    src/dialogs/FindReplaceDialog.cpp
    src/dialogs/GoToLineDialog.cpp
    src/dialogs/PreferencesDialog.cpp
    src/dialogs/ComparePanel.cpp
    src/dialogs/CssPreviewPane.cpp
    src/dialogs/CsvTableView.cpp
    src/dialogs/MarkdownPreviewPane.cpp
    src/dialogs/HexViewer.cpp
    src/dialogs/FindInFilesDialog.cpp
    src/dialogs/CommandPalette.cpp
)

set(APP_HEADERS
    src/MainWindow.h
    src/EditorTab.h
    src/FileIO.h
    src/Theme.h
    src/Settings.h
    src/LexerMap.h
    src/I18n.h
    src/LanguagesMenu.h
    src/MultiView.h
    src/AutoSavePolicy.h
    src/SessionManager.h
    src/AutoCompleter.h
    src/BookmarkManager.h
    src/BraceMatcher.h
    src/ColorPickerHelper.h
    src/CrossGroupTabBar.h
    src/EditEnhancements.h
    src/EditOperations.h
    src/EolMenu.h
    src/ExternalFileWatcher.h
    src/JsonXmlFormatter.h
    src/MacroRecorder.h
    src/PrintHelper.h
    src/RecentProjects.h
    src/Snippets.h
    src/SpellChecker.h
    src/WhitespaceView.h
    src/TabExtras.h
    src/UrlHyperlink.h
    src/CodeFormatter.h
    src/GitStatusService.h
    src/ThemePack.h
    src/CrashRecovery.h
    src/Workspace.h
    src/OutlineRegex.h
    src/MultiCursor.h
    src/SmartHighlight.h
    src/ColorMarkers.h
    src/FoldingMenu.h
    src/LspClient.h
    src/LspFeatures.h
    src/GitDiffGutter.h
    src/ViewModes.h
    src/EncodingConvert.h
    src/plugins/PythonPluginHost.h
    src/dialogs/PluginManagerDialog.h
    src/dialogs/QuickPickDialog.h
    src/panels/FunctionListPanel.h
    src/panels/LspDiagnosticsPanel.h
    src/panels/TerminalPanel.h
    src/panels/FtpPanel.h
    src/panels/SftpPanel.h
    src/network/FtpClient.h
    src/network/SftpClient.h
    src/dialogs/SshConnectDialog.h
    src/dialogs/KeybindingsDialog.h
    src/dialogs/GitCommitDialog.h
    src/panels/GitLogPanel.h
    src/KeybindingsManager.h
    src/GitOps.h
    src/StickyScroll.h
    src/BracketColors.h
    src/SearchEscapes.h
    src/TasksRunner.h
    src/panels/CalendarPanel.h
    src/panels/CalculatorPanel.h
    src/panels/TodoPanel.h
    src/panels/QrPanel.h
    src/panels/MermaidPanel.h
    src/dialogs/GitBranchDialog.h
    src/dialogs/GitStashDialog.h
    src/panels/MergeResolverPanel.h
    src/panels/TestRunnerPanel.h
    src/panels/NotebookPanel.h
    src/panels/NotesPanel.h
    src/FormatOnSave.h
    src/TypewriterMode.h
    src/Refactor.h
    src/VimMode.h
    src/dialogs/ThemeEditorDialog.h
    src/panels/RestClientPanel.h
    src/panels/SqlitePanel.h
    src/panels/AiPanel.h
    src/panels/PoEditorPanel.h
    src/CoverageGutter.h
    src/JsonSchemaValidator.h
    src/EditorConfig.h
    src/AutoPair.h
    src/SmartIndent.h
    src/CodeMetrics.h
    src/panels/ImageViewerPanel.h
    src/panels/RegexTesterPanel.h
    src/panels/UnitConverterPanel.h
    src/panels/JsonPathPanel.h
    src/panels/ColorPalettePanel.h
    src/panels/SysInfoPanel.h
    src/dialogs/GitignoreDialog.h
    src/dialogs/PreCommitDialog.h
    src/AiGhostCompletion.h
    src/LayoutPresets.h
    src/panels/DevToolsPanel.h
    src/panels/DbShellPanel.h
    src/panels/JsonLinesPanel.h
    src/panels/DocPreviewPanel.h
    src/panels/ClipboardHistoryPanel.h
    src/panels/ArchiveBrowserPanel.h
    src/panels/TimerPanel.h
    src/AutoCorrect.h
    src/SmartSelection.h
    src/panels/GraphQlPanel.h
    src/panels/CodeReviewPanel.h
    src/panels/ProfileRunnerPanel.h
    src/panels/ImageAnnotPanel.h
    src/panels/CsvChartPanel.h
    src/panels/MdTablePanel.h
    src/panels/CronEditorPanel.h
    src/panels/FileWatcherPanel.h
    src/panels/DocumentMapPanel.h
    src/panels/FileBrowserPanel.h
    src/panels/ExecOutputPanel.h
    src/panels/TextStatsPanel.h
    src/panels/MindMapPanel.h
    src/panels/PastebinPanel.h
    src/panels/TimeTrackerPanel.h
    src/panels/CodeClonesPanel.h
    src/panels/CallGraphPanel.h
    src/panels/SecretScannerPanel.h
    src/panels/ScreenshotPanel.h
    src/panels/RssReaderPanel.h
    src/panels/CliShellPanel.h
    src/panels/HexViewerPanel.h
    src/panels/SqlSchemaPanel.h
    src/panels/BuildWatchPanel.h
    src/panels/TodoAggregatorPanel.h
    src/panels/GrepPanel.h
    src/panels/YamlValidatorPanel.h
    src/panels/DockerPanel.h
    src/panels/JqRunnerPanel.h
    src/panels/HtmlPreviewPanel.h
    src/panels/GistPanel.h
    src/panels/EnvManagerPanel.h
    src/panels/SshExecPanel.h
    src/panels/CheatsheetPanel.h
    src/panels/VaultPanel.h
    src/panels/KubectlPanel.h
    src/panels/SystemMonitorPanel.h
    src/panels/GpgPanel.h
    src/panels/SslCertViewerPanel.h
    src/panels/AsciiDocPreviewPanel.h
    src/panels/PortScanPanel.h
    src/panels/SystemdServicesPanel.h
    src/panels/TunnelManagerPanel.h
    src/panels/ApiBrowserPanel.h
    src/panels/LogTailPanel.h
    src/panels/SshfsPanel.h
    src/dialogs/BookmarkDialog.h
    src/dialogs/HashDialog.h
    src/dialogs/MacroDialog.h
    src/dialogs/SnippetsDialog.h
    src/dialogs/WordCountDialog.h
    src/dialogs/FindReplaceDialog.h
    src/dialogs/GoToLineDialog.h
    src/dialogs/PreferencesDialog.h
    src/dialogs/ComparePanel.h
    src/dialogs/CssPreviewPane.h
    src/dialogs/CsvTableView.h
    src/dialogs/MarkdownPreviewPane.h
    src/dialogs/HexViewer.h
    src/dialogs/FindInFilesDialog.h
    src/dialogs/CommandPalette.h
)

# Translations declared here; resource-bundling deferred until target exists.
set(TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/lordpad_pt_BR.ts")

set(APP_RESOURCES resources/lordpad.qrc)

# Windows: recurso .rc (ícone do .exe + version info), compilado por windres.
set(APP_PLATFORM_SOURCES)
if(WIN32)
    enable_language(RC)
    list(APPEND APP_PLATFORM_SOURCES resources/lordpad.rc)
endif()

qt_add_executable(lordpad ${APP_SOURCES} ${APP_HEADERS} ${APP_RESOURCES} ${APP_PLATFORM_SOURCES})
qt_add_translations(lordpad
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(lordpad PRIVATE src src/dialogs)
target_link_libraries(lordpad PRIVATE
    scintilla-qt
    lexilla-static
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Sql
)

# Liga uchardet/hunspell quando presentes; senão define flags que ativam os
# fallbacks no código (FileIO.cpp e SpellChecker.cpp).
if(UCHARDET_FOUND)
    target_link_libraries(lordpad PRIVATE PkgConfig::UCHARDET)
else()
    target_compile_definitions(lordpad PRIVATE LORDPAD_NO_UCHARDET=1)
endif()
if(HUNSPELL_FOUND)
    target_link_libraries(lordpad PRIVATE PkgConfig::HUNSPELL)
else()
    target_compile_definitions(lordpad PRIVATE LORDPAD_NO_HUNSPELL=1)
endif()

# M9: link Python embed when available, define a compile flag so the host
# code can stub out without it.
if(PYTHON_EMBED_FOUND)
    target_link_libraries(lordpad PRIVATE PkgConfig::PYTHON_EMBED)
    target_compile_definitions(lordpad PRIVATE NPP_HAVE_PYTHON=1)
endif()

# M9: TerminalPanel uses forkpty(3); on Linux this lives in libutil.
if(UNIX AND NOT APPLE)
    target_link_libraries(lordpad PRIVATE util)
endif()

# Windows: subsistema GUI (sem janela de console acompanhando o app).
if(WIN32)
    set_target_properties(lordpad PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS lordpad RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ../packaging/lordpad.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../packaging/lordpad.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ../packaging/icons/lordpad.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
# Bundled cheatsheets (used by CheatsheetPanel as default).
install(DIRECTORY cheatsheets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/lordpad/cheatsheets
        FILES_MATCHING PATTERN "*.md" PATTERN "*.txt")
# Plugin examples — referência para devs.
install(DIRECTORY plugins/examples/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/lordpad/plugins-examples
        FILES_MATCHING PATTERN "*.py" PATTERN "*.md")

# ─── CPack: produz lordpad.deb (Debian/Ubuntu) e lordpad.rpm (Fedora/RHEL) ───
set(CPACK_PACKAGE_NAME             "lordpad")
set(CPACK_PACKAGE_VERSION          "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR           "clip52")
set(CPACK_PACKAGE_CONTACT          "Lord Clip <provisionamento@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL     "https://github.com/clip52/notepad-fedora")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Editor de código nativo Linux Qt6 (Notepad++ inspired)")
set(CPACK_PACKAGE_DESCRIPTION
    "LordPad é um editor de código nativo Linux baseado em Qt6, com Scintilla e Lexilla \
para edição e realce de sintaxe, suporte a LSP, integração Git, mais de 60 painéis \
(REST, GraphQL, SQLite, DB shells, Docker, kubectl, systemd, túneis SSH, SSHFS, cofre \
de senhas, CSV/MD/SQL viewers, etc.) e plugin host em Python.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES TRUE)
# Generator-specific filenames (DEB-DEFAULT = pkg_ver_arch.deb com arch=amd64;
# RPM-DEFAULT = pkg-ver-rel.arch.rpm com arch=x86_64).
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_RPM_FILE_NAME    "RPM-DEFAULT")

# Geradores: usuário pode escolher via -G DEB | -G RPM | -G "DEB;RPM"
set(CPACK_GENERATOR "DEB;RPM")

# ── DEB ─────────────────────────────────────────────────────────────────────
set(CPACK_DEBIAN_PACKAGE_NAME       "lordpad")
set(CPACK_DEBIAN_PACKAGE_SECTION    "editors")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE   "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Auto-resolução por shlibdeps quando o build host tiver dpkg-shlibdeps
# (pacote dpkg-dev). Em Fedora ele não existe, então mantemos uma lista
# estática completa abaixo como fallback — apt aceita ambas.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  ON)
# Lista explícita das libs runtime. Alternativas `t64` cobrem Debian 13/Kali
# rolling (transição time_t); base names cobrem Debian 12/Ubuntu 22.04.
# Apt escolhe a primeira disponível.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6t64 | libqt6core6, \
libqt6gui6t64 | libqt6gui6, \
libqt6widgets6t64 | libqt6widgets6, \
libqt6concurrent6t64 | libqt6concurrent6, \
libqt6network6t64 | libqt6network6, \
libqt6sql6t64 | libqt6sql6, \
libqt6xml6t64 | libqt6xml6, \
libqt6printsupport6t64 | libqt6printsupport6, \
libqt6core5compat6, \
libuchardet0, \
libhunspell-1.7-0, \
openssl")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "git, python3, ripgrep, jq")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS
    "docker.io, kubectl, sshfs, gpg, asciidoctor, gh, openssh-client")

# Roda update-desktop-database e gtk-update-icon-cache pós-install/remove.
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/../packaging/debian/postrm")

# ── RPM ─────────────────────────────────────────────────────────────────────
set(CPACK_RPM_PACKAGE_NAME          "lordpad")
set(CPACK_RPM_PACKAGE_LICENSE       "GPLv3+")
set(CPACK_RPM_PACKAGE_GROUP         "Development/Tools")
set(CPACK_RPM_PACKAGE_URL           "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_AUTOREQ       ON)
set(CPACK_RPM_PACKAGE_REQUIRES
    "qt6-qtbase, qt6-qtbase-gui, qt6-qt5compat, uchardet, hunspell, openssl-libs")
set(CPACK_RPM_PACKAGE_SUGGESTS
    "git, python3, ripgrep, jq, docker, kubectl, fuse-sshfs, gnupg2, rubygem-asciidoctor, gh, openssh-clients")
# Não pisa em diretórios já-de-sistema (evita conflitos com filesystem rpm).
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "/usr/share/applications;/usr/share/icons;/usr/share/icons/hicolor;/usr/share/icons/hicolor/scalable;/usr/share/icons/hicolor/scalable/apps;/usr/share/metainfo")

set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/rpm/postinst.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/rpm/postrm.sh")

include(CPack)
