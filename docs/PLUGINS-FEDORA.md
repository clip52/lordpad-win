# Plugins on Fedora — what `notepadpp-qt` ships and what to use instead

Milestone 1 of `notepadpp-qt` does **not** ship plugin parity with
upstream Notepad++. The reason is structural: Notepad++'s plugin ABI is
defined in `PluginInterface.h` and is fundamentally Win32 — plugins are
handed an `HWND` for the editor, an `HMENU` for the menu bar, and they
communicate with the host via `WM_*` messages and `SendMessage`. None of
that maps to Qt without redesigning the contract from scratch.

A future milestone (see `PORTING.md`, M6) will design a Qt-native plugin
API: shared objects loaded with `QPluginLoader`, a clean C++ host
interface, no Win32 types in the headers. Until that exists, the table
below tells you, for each popular Notepad++ plugin, what already exists
on Linux today and what a future in-tree implementation would look like.

The format for each entry is:

- **What it does** in upstream Notepad++.
- **Linux/Qt equivalents** you can use right now.
- **M1 status** under `notepadpp-qt`.
- **Future implementation path** if/when this is brought in-tree.

---

## Compare

- **What it does.** Side-by-side diff of two open documents, with line
  highlighting for additions/removals/changes.
- **Linux equivalents.** `meld` (GUI, the closest UX match), `kdiff3`,
  `vimdiff`, and `git diff --no-index file_a file_b` for terminal use.
- **M1 status.** Not built-in. Recommend `meld` for now; it integrates
  well with Fedora and handles three-way merges too.
- **Future path.** Wrap an external diff (`git diff --no-index` or
  `diff -u`) via `QProcess`, then render hunks using Scintilla margin
  markers and a dual-`ScintillaEdit` split. Realistic milestone-sized
  feature, not a quick afternoon.

## NppFTP

- **What it does.** Edit files over FTP/SFTP from inside the editor,
  with a tree panel for the remote filesystem.
- **Linux equivalents.** KIO virtual filesystems (open `sftp://host/`
  directly in Dolphin or Kate), GNOME's GVFS (`Files` → "Connect to
  Server"), or `sshfs` to mount the remote tree and edit it as a normal
  local path. For scripted transfers, `lftp` is excellent.
- **M1 status.** Not built-in. `sshfs` is the lowest-friction workflow:
  mount once, open files normally with `notepadpp-qt`.
- **Future path.** `QNetworkAccessManager` covers FTP; SFTP needs
  `libssh2` (or a third-party `QSshClient`). A `QDockWidget` with a
  `QTreeView` over a custom `QAbstractItemModel` gives the panel UI.

## XML Tools

- **What it does.** Pretty-print XML, evaluate XPath expressions,
  validate against XSD/DTD.
- **Linux equivalents.** `xmllint` (from `libxml2`, already on most
  Fedora installs) for formatting and validation; `xmlstarlet` for
  XPath; the VS Code XML extension for an integrated experience.
- **M1 status.** Not built-in. `xmllint --format -` piped through a
  shell is the quickest fix for "make this XML readable".
- **Future path.** `QXmlStreamReader` for parsing, `libxml2` (already a
  Fedora system library) for XPath and XSD validation. Pretty-print is
  effectively a re-emit pass; XPath needs a small results panel.

## JSON Viewer / JSTool

- **What it does.** Pretty-print JSON, collapse/expand a tree view of
  the document.
- **Linux equivalents.** `jq` (the de-facto standard for JSON on the
  command line), `fx` (interactive TUI), `gron` (flatten JSON to
  greppable lines), VS Code's built-in JSON support, KDE's `KJsonView`.
- **M1 status.** Not built-in. `jq . file.json` covers pretty-print in
  one command.
- **Future path.** Cheapest plugin to bring in-tree: `QJsonDocument` →
  `toJson(QJsonDocument::Indented)` for formatting, `QTreeView` over a
  `QAbstractItemModel` walking the parsed document for the tree panel.

## NppExec

- **What it does.** Run shell commands, capture stdout/stderr in a
  bottom panel, hyperlink compiler errors back into the source.
- **Linux equivalents.** Any terminal emulator (every Linux user has
  one), VS Code's task runner, `make`/`just`/`ninja` from a terminal
  next to the editor.
- **M1 status.** Not built-in.
- **Future path.** `QDockWidget` at the bottom with a `QProcess` driving
  a read-only `ScintillaEdit` (or `QPlainTextEdit`) for output. Click-to-
  jump on `path:line:col` patterns is a regex pass over the buffer.

## PythonScript

- **What it does.** Embed a Python REPL with access to an editor API
  (`notepad`, `editor` objects) so users can script the editor in
  Python.
- **Linux equivalents.** VS Code's Python extension, Jupyter for
  notebooks, IPython for a standalone REPL — none of which script the
  editor itself.
- **M1 status.** Not built-in.
- **Future path.** Either embed CPython through the C API and expose a
  small wrapper around Scintilla, or use PyQt6's existing bindings and
  ship a Python console widget that drives the host through the same
  internal API the menus use. Depends on M6 (plugin host) landing first
  so the API surface is stable.

## AutoSave

- **What it does.** Periodically save modified buffers in the
  background.
- **Linux equivalents.** Most modern editors (gedit, Kate, VS Code) have
  this built-in.
- **M1 status.** Not built-in.
- **Future path.** Trivial — a single `QTimer` in `MainWindow` that
  iterates open documents and calls the existing save path on those
  flagged dirty. Slated for **M2**.

## NppSnippets / QuickText

- **What it does.** Expand short triggers into pre-defined snippets,
  with placeholder navigation.
- **Linux equivalents.** Qt's `QCompleter` covers static word
  completion. UltiSnips/LSP-style dynamic snippets (with tab stops and
  mirrored placeholders) need a real snippet engine.
- **M1 status.** Not built-in.
- **Future path.** A small snippet engine reading Notepad++'s existing
  XML snippet format, integrated with `QCompleter` for the trigger UI.
  Tab stops require a custom selection model on top of Scintilla.

## NppMenuSearch

- **What it does.** Fuzzy search across all menu items, like VS Code's
  command palette (Ctrl+Shift+P).
- **Linux equivalents.** Most modern editors ship a command palette out
  of the box.
- **M1 status.** Not built-in.
- **Future path.** Genuinely small feature: walk the `QMenuBar` tree
  once at startup to build a flat list of `QAction*`, drive a
  `QListView` filtered by `QLineEdit` text with a fuzzy matcher (a
  small subsequence-scoring function). One evening of work.

## Customize Toolbar

- **What it does.** Show/hide toolbars, choose which toolbar buttons
  appear.
- **Linux equivalents.** Built into Qt itself.
- **M1 status.** Partial — Qt's `QMainWindow` automatically provides a
  context menu listing all `QToolBar`s and `QDockWidget`s, so toggling
  visibility works out of the box. Per-button customisation does not.
- **Future path.** A small "Customize toolbar..." dialog exposing the
  existing `QAction` registry, drag-and-drop reorder backed by
  `QSettings` so the layout persists.

## HexEditor

- **What it does.** View and edit files as hex + ASCII.
- **Linux equivalents.** `ghex` (GNOME), `okteta` (KDE), `bless`,
  `hexyl` (read-only, terminal). All available in Fedora repos.
- **M1 status.** Not built-in.
- **Future path.** Non-trivial. Scintilla is not well-suited to binary
  buffers (it assumes text and a coding system). Realistic
  implementation is a separate `QAbstractScrollArea`-based hex widget
  toggled per-tab, not an in-place mode of the existing editor.

## DSpellCheck

- **What it does.** Inline spell checking with squiggly underlines and
  suggestions on right-click.
- **Linux equivalents.** Hunspell directly, Enchant as an abstraction
  layer (used by GNOME), KSpell on KDE. Hunspell dictionaries are
  packaged in Fedora as `hunspell-pt-BR`, `hunspell-en-US`,
  `hunspell-es`, etc.
- **M1 status.** Not built-in.
- **Future path.** Link `libhunspell` directly, run checks on visible
  ranges (Scintilla exposes them), draw squiggles via Scintilla
  indicators (`INDIC_SQUIGGLE`). Right-click suggestions plug into the
  existing Scintilla context menu. Dictionary discovery via Fedora's
  `/usr/share/hunspell/`.

## MarkdownPanel / MarkdownViewer++

- **What it does.** Live HTML preview of the Markdown document being
  edited, side-by-side.
- **Linux equivalents.** ReText, Ghostwriter, Marker, VS Code's
  Markdown preview, Obsidian — many options.
- **M1 status.** Not built-in.
- **Future path.** `QtWebEngineView` in a side dock rendering output
  from `cmark` (or `cmark-gfm` for GitHub-flavoured), refreshed on edit
  with a debounce timer. Genuinely small feature once `qt6-qtwebengine-devel`
  is acceptable as an optional dependency.

---

## Summary

For M1, the answer to "where is plugin X?" is almost always "use the
native Linux tool for that job". Linux has mature standalone equivalents
for every plugin in the table above, and shelling out is friction-free
on a Unix system in a way it never was on Windows.

The plugin host comes back in M6 once there is a Qt-native API to plug
into. At that point the entries above with low-effort future paths
(JSON, AutoSave, MenuSearch, MarkdownPanel) are the most likely
candidates for in-tree reference plugins.

Last updated: 2026-05-05
