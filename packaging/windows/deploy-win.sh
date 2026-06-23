#!/usr/bin/env bash
# Deploy do lordpad.exe cross-compilado (MinGW) — junta o .exe, as DLLs do Qt6 e
# do runtime MinGW, e os plugins essenciais, numa pasta autocontida pronta para
# rodar no Windows (ou testar no Wine). Substitui o windeployqt (ausente no
# cross-build Fedora).
#
# Uso: packaging/windows/deploy-win.sh [caminho/lordpad.exe] [pasta-destino]
set -euo pipefail

SYS=/usr/x86_64-w64-mingw32/sys-root/mingw
BIN="$SYS/bin"
PLUGINS="$SYS/lib/qt6/plugins"
OBJDUMP=x86_64-w64-mingw32-objdump

EXE="${1:-build-win/lordpad.exe}"
DIST="${2:-dist-win}"

[ -f "$EXE" ] || { echo "exe não encontrado: $EXE" >&2; exit 1; }

rm -rf "$DIST"; mkdir -p "$DIST"
cp "$EXE" "$DIST/"

# Resolvedor recursivo: copia do sysroot MinGW cada DLL importada pelo PE.
# DLLs do sistema Windows (kernel32, user32, msvcrt...) não existem no sysroot,
# então são naturalmente ignoradas.
declare -A seen
resolve() {
    local f="$1" dll
    for dll in $("$OBJDUMP" -p "$f" 2>/dev/null | awk '/DLL Name:/ {print $3}'); do
        [ -n "${seen[$dll]:-}" ] && continue
        if [ -f "$BIN/$dll" ]; then
            seen[$dll]=1
            cp -n "$BIN/$dll" "$DIST/"
            resolve "$BIN/$dll"
        fi
    done
}
resolve "$EXE"

# Plugins Qt essenciais (vão em subdirs ao lado do .exe; Qt acha sozinho) +
# resolução recursiva das deps de cada plugin.
copy_plugin() {
    local sub="$1" name="$2"
    [ -f "$PLUGINS/$sub/$name" ] || return 0
    mkdir -p "$DIST/$sub"
    cp -n "$PLUGINS/$sub/$name" "$DIST/$sub/"
    resolve "$PLUGINS/$sub/$name"
}
copy_plugin platforms     qwindows.dll              # obrigatório (sem ele Qt não sobe)
copy_plugin imageformats  qsvg.dll                  # ícone SVG
copy_plugin sqldrivers    qsqlite.dll               # SqlitePanel / Qt6::Sql
copy_plugin styles        qmodernwindowsstyle.dll   # visual nativo Win11 (se houver)
for t in "$PLUGINS"/tls/*.dll; do
    [ -f "$t" ] && copy_plugin tls "$(basename "$t")" # HTTPS no Qt6::Network
done

# Dados de runtime bundlados ao lado do .exe (o app os procura em
# applicationDirPath()/cheatsheets etc. — ver CheatsheetPanel).
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [ -d "$REPO_ROOT/PowerEditor-qt/cheatsheets" ]; then
    mkdir -p "$DIST/cheatsheets"
    cp "$REPO_ROOT/PowerEditor-qt/cheatsheets/"*.md "$DIST/cheatsheets/" 2>/dev/null || true
fi
if [ -d "$REPO_ROOT/PowerEditor-qt/plugins/examples" ]; then
    mkdir -p "$DIST/plugins-examples"
    cp "$REPO_ROOT/PowerEditor-qt/plugins/examples/"* "$DIST/plugins-examples/" 2>/dev/null || true
fi

echo "Deploy concluído em '$DIST' — $(find "$DIST" -type f | wc -l) arquivos, $(du -sh "$DIST" | awk '{print $1}')"
