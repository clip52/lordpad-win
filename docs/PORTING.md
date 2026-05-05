# Porting Notepad++ to Native Linux (Qt6)

This document describes the rationale and strategy behind `notepadpp-qt`, a
native Linux Qt6 port of Notepad++ targeted at Fedora. It is aimed at
contributors who want to understand why this fork exists, how it relates to
the upstream Win32 codebase, and what is realistic to expect from each
milestone.

## 1. Why this fork exists

Notepad++ is one of the most widely used free text editors on Windows, but
its codebase is deeply Win32-bound. The UI is built directly on the Win32
API: dialogs come from `.rc` resource scripts, drawing goes through
Direct2D/GDI, drag-and-drop uses OLE, file associations live in the
registry, plugins talk to the host through `HWND`/`HMENU`/`WM_*` messages,
and most strings are wide-char (`TCHAR`/`wchar_t`) flowing through Win32
APIs. None of this maps cleanly to any Linux toolkit.

The practical consequence is that Notepad++ does not run natively on Linux.
The options users have today are:

- **Wine.** Works, but ships an entire Win32 emulation layer, integrates
  poorly with the Linux desktop (HiDPI, theming, IME, file dialogs), and is
  not what Fedora users typically want.
- **NotepadNext.** A separate Qt-based reimplementation with its own goals
  and scope. It is not a port of Notepad++ — it is its own project.
- **Notepadqq.** Another independent Qt project, no longer actively
  developed in the same way, and again a separate codebase.

`notepadpp-qt` takes a different position: keep the upstream Notepad++
sources accessible as a *reference*, and build a parallel native
implementation that reuses the parts of Notepad++ that are already
portable (Scintilla, Lexilla, the document model, file format support)
while rewriting everything Win32-specific against Qt6.

The goal is not to be "Notepad++ on Linux pixel-for-pixel". The goal is to
deliver a Linux-native editor whose UX, file handling, and lexer support
feel like Notepad++ to long-time users, packaged the way a Fedora user
expects (RPM, desktop file, MIME associations, system theming).

## 2. Strategy

The repository is organised so the original codebase and the port live
side-by-side without interfering:

- `PowerEditor/src/` — original Notepad++ Win32 sources, kept **read-only**
  as a reference. Useful when reimplementing a dialog or behaviour: read
  what the Win32 version does, then write the Qt equivalent.
- `PowerEditor-qt/` — the new native port. CMake-based, Qt6, no Win32
  headers, no `windows.h`, no MFC.
- `scintilla/qt/` — Scintilla's official Qt edition (`ScintillaEditBase`,
  `ScintillaEdit`). Upstream Scintilla maintains this; we reuse it as-is.
- `lexilla/` — Lexilla lexers, platform-neutral, used by both the original
  and the Qt port without modification.
- `packaging/notepadpp-qt.spec` — Fedora RPM spec. Produces a
  `notepadpp-qt` package with desktop file and MIME entries.

The split lets us make progress on the Qt side without touching the Win32
tree (so the upstream history remains useful as reference), and it lets
contributors who want to compare implementations do so directly.

The toolkit baseline is **Qt 6.5+** (developed against Qt 6.10 on Fedora
43), CMake 3.20+, gcc-c++ 15+, and `uchardet` for encoding detection. The
binary ships as `notepadpp-qt` so it does not collide with any other
`notepad`-named tool a user may have installed.

## 3. What changes between Win32 and Qt

A rough mapping for contributors used to the Notepad++ codebase:

| Win32 / Notepad++                       | Qt6 / notepadpp-qt                          |
|-----------------------------------------|---------------------------------------------|
| `HWND`                                  | `QWidget` (and subclasses)                  |
| `HMENU`                                 | `QMenuBar` / `QMenu`                        |
| `SendMessage(hwnd, WM_*, ...)`          | Qt signals/slots, direct method calls       |
| `.rc` dialog resources                  | `.ui` Qt Designer files (or programmatic `QDialog`) |
| Direct2D / GDI drawing                  | `QPainter` (Scintilla's Qt edition handles its own) |
| OLE drag-and-drop                       | `QMimeData` + `QDrag`                       |
| Registry (`HKCU\Software\Notepad++`)    | `QSettings` (INI under `~/.config/`)        |
| `WideCharToMultiByte` / `TCHAR`         | `QString` (UTF-16 internally) + `QTextCodec` / `QStringConverter` |
| `HICON`, `.ico` resources               | SVG via `QResource` / `:/icons/`            |
| `CreateFile` / `ReadFile`               | `QFile` / `QSaveFile`                       |
| `PrintDlg`, `SHBrowseForFolder`         | `QPrintDialog`, `QFileDialog`               |
| COM `IFileDialog`                       | `QFileDialog`                               |
| Win32 timers (`SetTimer`)               | `QTimer`                                    |
| `MessageBox`                            | `QMessageBox`                               |

Most of the rewrite work is in the *shell* around Scintilla: tabs, menus,
toolbars, dialogs, settings, file I/O. Scintilla itself is largely a
drop-in via `ScintillaEdit`.

## 4. What is preserved

- **Scintilla document model.** Same buffer semantics, same undo stack
  semantics, same lexer-driven styling. Files edited in Notepad++ on
  Windows and `notepadpp-qt` on Linux remain interchangeable.
- **Lexilla lexers.** Every language Notepad++ supports through Lexilla is
  available unchanged. Adding a new lexer is the same workflow on both
  sides.
- **File format compatibility.** UTF-8, UTF-8 with BOM, UTF-16 LE/BE with
  BOM, and common legacy encodings via `uchardet`. Line endings (LF,
  CRLF, CR) are detected and preserved on save.
- **General UX cues.** Tab bar layout, menu structure (File / Edit /
  Search / View / Encoding / Language / Settings), and the find/replace
  dialog layout are kept close enough that long-time Notepad++ users feel
  at home.

## 5. What is dropped (for now)

- **Plugin DLL ABI.** `PluginInterface.h` is fundamentally Win32: it hands
  plugins `HWND` for the editor, `HMENU` for the menu bar, and a function
  table that talks `WM_*` messages. None of that has a Qt counterpart, so
  this port does **not** load existing Notepad++ plugins. A future
  milestone designs a Qt-friendly plugin API; until then, see
  `PLUGINS-FEDORA.md` for Linux equivalents to popular plugins.
- **Win32-specific dialogs.** `PrintDlg`, `SHBrowseForFolder`, COM file
  pickers — replaced by their Qt counterparts.
- **Registry-based file associations.** On Linux, file associations come
  from the `.desktop` file and MIME database (`update-mime-database`,
  `xdg-mime`). The RPM ships these.
- **Windows-specific shell integrations** (`Open with Notepad++` context
  menu entries, `LaunchUI` behaviour, `nppShell.dll`).

## 6. Roadmap

Milestone 1 (this release) is intentionally narrow. It exists to prove
that the architecture works end-to-end and to give Fedora users a
packaged, daily-driver editor. Anything not listed below is deliberately
out of scope for M1.

**M1 — Foundational editor (current).**

- Multi-document tabbed editor on top of `ScintillaEdit`.
- Open / Save / Save As with `uchardet`-based encoding detection.
- Find / Replace dialog (current document, regex, case sensitivity).
- Go-to-line dialog.
- Preferences dialog (font, tabs, theme).
- Light / dark themes.
- Settings persistence via `QSettings`.
- RPM packaging (`notepadpp-qt.spec`) with desktop file and MIME hookup.

**M2 — Workflow basics.** Auto-save (timer-based), session save/restore on
exit, recent files list, basic Find-in-Files, drag-and-drop tab reorder.
Realistic effort: a few days each.

**M3 — Navigation panels.** Function List (parsed from lexer tags),
Document Map (Scintilla minimap), File Browser (project tree as a
`QDockWidget` + `QFileSystemModel`). Each is roughly a week of
implementation plus polish.

**M4 — Editing power.** Multi-view splits (`QSplitter` of two
`ScintillaEdit` instances on the same document), column / multi-cursor
edit beyond what Scintilla gives by default, autocompletion driven by the
buffer (and optionally LSP).

**M5 — Macros and scripting.** Record/replay keystroke macros. Embedded
scripting (Python via the C API, or QtScript-style) for a `PythonScript`
equivalent.

**M6 — Plugin host.** Design a Qt-native plugin API. Plugins as shared
objects loaded with `QPluginLoader`, given a clean C++ interface
(`IEditorHost`, `IDocument`, `IMenuBuilder`) — no `HWND`, no `HMENU`, no
`WM_*`. Provide a small SDK and port one or two reference plugins.

**M7 — User Defined Languages.** Port the UDL parser/dialog. The data
format from Notepad++ XML UDLs should be readable so existing UDL files
work.

**M8 — Internationalisation.** `QTranslator`, `.ts`/`.qm` files, port
existing translations where the strings still apply.

**M9 — Printing.** `QPrinter` + `QPrintDialog` + a syntax-highlighted
print path (render Scintilla styled output).

Each of these is honestly days-to-weeks of focused work. None are
"trivial" once you account for testing across themes, HiDPI, Wayland vs
X11, and Fedora's packaging requirements.

## 7. How to contribute

The roadmap above is the contribution backlog. Each milestone — and each
sub-bullet inside a milestone — is a candidate PR. The recommended
workflow:

1. Open an issue on the project tracker describing what you want to
   tackle. Reference the milestone (e.g. "M3: Function List panel").
2. For UI work, sketch the dialog or panel first (Qt Designer `.ui` file
   is fine) so the layout can be reviewed before logic is wired up.
3. Keep the upstream `PowerEditor/src/` tree untouched. All Qt code lives
   under `PowerEditor-qt/`.
4. Build and test against Fedora 43 with Qt 6.10. Other distros are
   welcome but Fedora is the reference target.
5. Update the relevant docs (`BUILDING-LINUX.md` if you change the build,
   `PLUGINS-FEDORA.md` if you implement an equivalent for a listed
   plugin).

Bug reports are most useful with: Fedora version, Qt version
(`qmake6 -query QT_VERSION`), the binary version (`notepadpp-qt
--version`), and a minimal reproducer file when applicable.

Last updated: 2026-05-05
