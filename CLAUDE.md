# CLAUDE.md — LordPad

LordPad 0.9 — editor de código/texto **nativo Linux em Qt6**, port do Notepad++
sobre **Scintilla (edição Qt) + Lexilla**. Renomeado de `notepadpp-qt` em 2026-05.
UI 100% **PT-BR** por padrão.

As fontes Win32 originais em `PowerEditor/src/` ficam **intocadas** como referência.
Todo o trabalho novo vive no codebase paralelo `PowerEditor-qt/`.

## Layout

- `PowerEditor-qt/` — projeto Qt6 (CMake). É aqui que se mexe.
  - `src/` — módulos da aplicação (`*.cpp/*.h`), padrão um arquivo por feature.
  - `src/dialogs/`, `src/panels/` — diálogos e painéis dock.
  - `cheatsheets/*.md` — cheatsheets bundled (instalam em `/usr/share/lordpad/cheatsheets/`).
  - `plugins/`, `plugins/examples/` — plugins Python de exemplo.
  - `resources/` — ícone (`icons/lordpad.svg`) + `lordpad.qrc`.
- `PowerEditor/`, `scintilla/`, `lexilla/` — upstream + patches mínimos e escopados.
- `build/` — diretório de build out-of-tree (não versionado).

## Build

```bash
# configurar (dev: plugin host Python ligado)
cmake -B build -S PowerEditor-qt -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# smoke test headless (exit 143 = SIGTERM do timeout = OK)
QT_QPA_PLATFORM=offscreen timeout 5 ./build/lordpad
```

- C++17, CMake ≥ 3.20, `project(lordpad VERSION 0.9)`.
- Deps Fedora 43: `qt6-qtbase-devel qt6-qttools-devel qt6-qt5compat-devel
  cmake gcc-c++ uchardet-devel pkgconfig`. Opcionais por feature:
  Hunspell (spell), `python3-embed` (plugin host).
- Atalho local do user: `~/.local/bin/lordpad → build/lordpad`.

## Empacotamento (.deb / .rpm)

```bash
cmake -B build -S PowerEditor-qt -DCMAKE_BUILD_TYPE=Release -DLORDPAD_PLUGIN_HOST=OFF
cmake --build build -j$(nproc)
cd build && cpack -G "DEB;RPM"
# → lordpad_0.9_amd64.deb  e  lordpad-0.9-1.x86_64.rpm
```

- **Sempre empacotar com `-DLORDPAD_PLUGIN_HOST=OFF`.** Ligado, o binário linka
  `libpython3.X.so` do host de build e quebra em distros com Python diferente
  (caso real: libpython3.14 do Fedora 43 vs 3.13 do Kali). Default é `ON` só para dev.
- DEB usa `Depends` com alternativas `…t64 | …` para casar nomes de lib Qt6
  entre Debian/Ubuntu/Kali (sufixo `t64`) e Fedora.
- Alvo: Qt6 ≥ 6.10, glibc ≥ 2.38.

## Arquitetura — padrão de módulo

Cada feature é autocontida: `src/<Nome>.{h,cpp}` expondo uma API mínima
`attach()`/`install()`. `MainWindow` só inclui o header, guarda um ponteiro
membro e liga as ações de menu. Ao adicionar uma feature, siga esse padrão e
acrescente os fontes à lista explícita `APP_SOURCES` no `CMakeLists.txt`.

## Decisões fixadas (não reverter sem motivo)

- **UI em PT-BR**, sem entradas duplicadas. Menus top-level com **≤ 25 entradas** cada.
- **5 menus de painéis**: `Painéis / Dados / Rede / DevOps / Util`. Não voltar a juntar.
- **Diálogos de conteúdo**: usar `QDialog + QTextBrowser` (layout multi-coluna,
  largura > altura). Evitar `QMessageBox` para conteúdo grande (ex.: About).
- **Cofre de senhas**: AES-256-CBC via `openssl enc -pbkdf2 -iter 100000`
  (subprocess) — sem dependência de build extra.

## Gotchas

- **Lexer HTML/XML**: Lexilla registra o lexer de HTML como `"hypertext"`, não
  `"html"`; a tradução é feita em `tryCreateLexer` (`src/LexerMap.cpp`). XML usa
  lexer `"xml"` separado. O estilo 1 do HTML é TAG (não comentário) — há overlay
  de paleta por-lexer para não pintar tags com cor de comentário.
- **Cheatsheets**: `CheatsheetPanel` descobre `*.md` por `QDirIterator` em runtime
  e o CMake instala a pasta inteira por glob — adicionar um `.md` novo **não exige**
  mudança de código nem de CMake.
- **Scintilla**: usar as APIs `*Full` (`SCI_FINDTEXTFULL`, `SCI_FORMATRANGEFULL`,
  `Sci_TextToFindFull`) — as variantes não-Full foram removidas do upstream atual.
- **LexUser.cxx** (User-Defined Languages do Notepad++) é Win32-only e fica
  **excluído** do build Linux.

## Ambiente de teste

Fedora 43 (dev) e Kali rolling (`srv.scorp.digital`: Qt6 6.10, Python 3.13,
libs com sufixo `t64`).
