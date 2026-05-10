# LordPad

[![Release](https://img.shields.io/github/v/release/clip52/lordpad?include_prereleases)](../../releases)
[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![Linux](https://img.shields.io/badge/platform-Linux%20Qt6-orange)](#)

**Editor de código nativo Linux Qt6** — port do Notepad++ reconstruído sobre Qt 6, com Scintilla/Lexilla, suporte a LSP, integração Git, mais de **60 painéis** (REST, GraphQL, SQLite, Docker, kubectl, systemd, túneis SSH, SSHFS, cofre de senhas, etc.) e plugin host opcional em Python.

> **Versão atual: 0.9** &middot; 70+ milestones (M1–M70) &middot; última atualização: 2026-05-09

---

## Instalação

### Fedora / RHEL / openSUSE

```bash
sudo dnf install ./lordpad-0.9-1.x86_64.rpm
```

### Debian / Ubuntu / Kali

```bash
sudo apt install ./lordpad_0.9_amd64.deb
```

Os pacotes pré-compilados estão na [página de releases](../../releases). O `apt`/`dnf` resolve automaticamente as dependências (Qt6, hunspell, uchardet, openssl). O atalho aparece em **Aplicativos > Desenvolvimento** logo após a instalação.

**Pré-requisitos do alvo:**
- Qt6 ≥ 6.10 (Fedora ≥ 43, Debian trixie/Ubuntu 25+, Kali rolling)
- glibc ≥ 2.38
- 8 MB de espaço

---

## Recursos

### Editor
- Multi-cursor, smart highlight, brace matching, sticky scroll
- Auto-pair brackets/quotes, smart indent, format-on-save
- Code folding, mini-map (document map)
- Multi-view splits horizontal/vertical com drag-between-groups
- Vim mode, typewriter mode
- Themes: Light, Dark, Dracula + theme packs + theme editor

### Linguagens & LSP
- Lexilla com 90+ lexers (HTML/XML, JSON, Python, C/C++, Rust, Go, ...)
- Cliente LSP: completar, hover, signature help, símbolos doc/workspace, inlay hints, diagnostics

### Git integrado
- Log, branches, stash, blame, commit
- Fetch / pull / push
- Diff gutter (margens com +/-/~ ao vivo)
- Editor de `.gitignore`, instalação de pre-commit hooks, apply patch

### AI assistant
- Painel de chat para explicar/traduzir seleção e gerar mensagens de commit
- Ghost completion inline (toggle)

### Refactor
- Expandir/encolher seleção (smart selection)
- Renomear-no-escopo, extrair função
- Auto-correct dicionário-baseado

### Painéis (60+) divididos em 5 menus top-level

| Menu        | Conteúdo                                                                 |
|-------------|--------------------------------------------------------------------------|
| **Painéis** | Function list, Document map, File browser, Exec output, Terminal, Color palette, Image viewer/annot, Screenshot, Mermaid, Mind map, HTML/AsciiDoc preview, Doc preview, Code review/clones/call graph, Secret scanner, TODOs, Grep, Regex tester, Merge resolver, Sysinfo, Sysmon, DevTools, OpenAPI |
| **Dados**   | SQLite, DB shell, CLI DB, JSONL, JSON path, jq, MD Table, CSV chart, SQL Schema, YAML, Hex viewer, .env, Archive, Clipboard, GPG, SSL Cert |
| **Rede**    | FTP, SFTP, SSHFS, SSH Exec, Túneis SSH, Port scan, REST, GraphQL, RSS, Pastebin, Gist, Git Log, Tradução .po |
| **DevOps**  | Build watch, Cron, File watcher, Profiler, Test runner, Docker, kubectl, systemd, Log tail |
| **Util**    | Calendário, Calculadora, QR, TODO, Notas, Notebook, Pomodoro, Time tracker, Conversor, Cheatsheets, Estatísticas, **Cofre de senhas** (AES-256/PBKDF2) |

### Diálogos rápidos
Compare, CSS preview, CSV table, Markdown preview, Hex viewer, Find-in-files, Command palette, Word count, Hash, Macros, Snippets, Goto line, Find/Replace, Preferências, Atalhos, Theme editor, Plugin manager.

### Cofre de senhas
- AES-256-CBC com PBKDF2 (100k iterações) via `openssl enc`
- Master password só fica em RAM
- Copiar senha pra clipboard com auto-clear após 30s
- Trocar master re-cifra todo o vault

### Cheatsheets bundled
Bash, Python, Git, HTML, JavaScript, Docker, Regex, SQL, Vim — instalados em `/usr/share/lordpad/cheatsheets/`. Acesse pelo painel **Util > Cheatsheets**.

### Plugin host (opcional)
Python embed (precisa rebuild com `-DLORDPAD_PLUGIN_HOST=ON`). Exemplos em `/usr/share/lordpad/plugins-examples/`:
- `insert_date.py` — insere data ISO no cursor
- `uppercase_selection.py` — UPPER/lower/Title da seleção
- `wrap_quotes.py` — surround com `"`, `'`, `(...)`, `[...]`, `{...}`
- `sort_lines.py` — sort lex/numérico asc/desc
- `wordcount.py` — chars/words/lines em pop-up
- `trim_trailing_whitespace.py` — auto-trim ao salvar
- `save_log.py` — log de saves em `~/.cache/LordPad/save.log`
- `format_python.py` — invoca `black`/`ruff format` via subprocess

API completa do módulo `notepadpp`: ver [`PowerEditor-qt/plugins/examples/README.md`](PowerEditor-qt/plugins/examples/README.md).

---

## Build a partir do código

### Dependências

**Fedora:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qttools-devel qt6-qt5compat-devel \
                 cmake gcc-c++ uchardet-devel hunspell-devel python3-devel
```

**Debian/Ubuntu/Kali:**
```bash
sudo apt install qt6-base-dev qt6-tools-dev qt6-5compat-dev \
                 cmake g++ libuchardet-dev libhunspell-dev python3-dev
```

### Compilar

```bash
git clone https://github.com/clip52/notepad-fedora.git
cd notepad-fedora
mkdir build && cd build
cmake -S ../PowerEditor-qt -B .
cmake --build . -j$(nproc)
./lordpad     # roda direto do build dir
```

### Gerar pacotes

```bash
# Build de pacote distribuível (sem libpython embed)
cmake -S ../PowerEditor-qt -B . -DLORDPAD_PLUGIN_HOST=OFF
cmake --build . -j$(nproc)
cpack -G "DEB;RPM"
# Produz lordpad_0.9_amd64.deb e lordpad-0.9-1.x86_64.rpm
```

`-DLORDPAD_PLUGIN_HOST=OFF` é necessário para o pacote ser portável entre distros (senão o binário linka `libpython3.X.so` da versão exata do build host).

### Build dev com plugin host Python

```bash
cmake -S ../PowerEditor-qt -B . -DLORDPAD_PLUGIN_HOST=ON
```

---

## Integrações por subprocess

LordPad invoca utilitários CLI quando disponíveis. Instale só os que pretende usar:

```
git python3 ripgrep jq openssl gpg ssh sshfs fusermount
docker kubectl systemctl journalctl
asciidoctor tail ss gh black ruff mmdc qrencode
```

A ausência de qualquer um deles desabilita o painel correspondente, mas o editor segue funcional.

---

## Estrutura

```
notepad-fedora/
├── PowerEditor-qt/        ← código-fonte do LordPad (Qt6)
│   ├── src/               ← C++ — MainWindow, painéis, dialogs, helpers
│   ├── resources/         ← qrc, ícone, themes (qss)
│   ├── translations/      ← lordpad_pt_BR.ts
│   ├── cheatsheets/       ← .md bundled
│   ├── plugins/examples/  ← exemplos de plugin Python
│   └── CMakeLists.txt
├── scintilla/ lexilla/    ← editor widget e lexers (vendored)
├── packaging/             ← .desktop, .metainfo.xml, .spec, scripts post-install
└── docs/                  ← documentação adicional
```

---

## Licença

GPLv3+ (herdado do Notepad++ original). Scintilla é distribuído sob HPND.

---

## Contribuir

Issues e PRs são bem-vindos. Convenções:
- Strings de UI sempre em PT-BR
- Cap de 25 entradas por menu top-level
- Painéis novos seguem o pattern `QDockWidget` + `setObjectName` (para save/restore de layout)

Para escrever plugins veja [`PowerEditor-qt/plugins/examples/README.md`](PowerEditor-qt/plugins/examples/README.md).

---

**Repositório:** https://github.com/clip52/notepad-fedora
