cmake_minimum_required(VERSION 3.20)
project(notepadpp-qt VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Core5Compat LinguistTools Concurrent Xml)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UCHARDET REQUIRED IMPORTED_TARGET uchardet)

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
# notepadpp-qt: the application
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
    src/ColorPickerHelper.cpp
    src/EditOperations.cpp
    src/EolMenu.cpp
    src/JsonXmlFormatter.cpp
    src/MacroRecorder.cpp
    src/panels/FunctionListPanel.cpp
    src/panels/DocumentMapPanel.cpp
    src/panels/FileBrowserPanel.cpp
    src/panels/ExecOutputPanel.cpp
    src/dialogs/BookmarkDialog.cpp
    src/dialogs/MacroDialog.cpp
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
    src/ColorPickerHelper.h
    src/EditOperations.h
    src/EolMenu.h
    src/JsonXmlFormatter.h
    src/MacroRecorder.h
    src/panels/FunctionListPanel.h
    src/panels/DocumentMapPanel.h
    src/panels/FileBrowserPanel.h
    src/panels/ExecOutputPanel.h
    src/dialogs/BookmarkDialog.h
    src/dialogs/MacroDialog.h
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
set(TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/notepadpp-qt_pt_BR.ts")

set(APP_RESOURCES resources/notepadpp-qt.qrc)

qt_add_executable(notepadpp-qt ${APP_SOURCES} ${APP_HEADERS} ${APP_RESOURCES})
qt_add_translations(notepadpp-qt
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

target_include_directories(notepadpp-qt PRIVATE src src/dialogs)
target_link_libraries(notepadpp-qt PRIVATE
    scintilla-qt
    lexilla-static
    PkgConfig::UCHARDET
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Xml
)

# Install
include(GNUInstallDirs)
install(TARGETS notepadpp-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ../packaging/notepadpp-qt.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../packaging/notepadpp-qt.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ../packaging/icons/notepadpp-qt.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
