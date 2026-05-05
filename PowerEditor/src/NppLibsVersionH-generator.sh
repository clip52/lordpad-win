#!/usr/bin/env bash
# Linux/Unix port of NppLibsVersionH-generator.bat (Fedora cross-compile).
set -euo pipefail

cd "$(dirname "$0")"

OUT="./NppLibsVersion.h"

extract_quoted() {
    grep -F "$2" "$1" | sed -E 's/.*"([^"]+)".*/"\1"/' | head -n1
}

sciVer=$(extract_quoted "../../scintilla/win32/ScintRes.rc"   '#define VERSION_SCINTILLA ' || true)
lexVer=$(extract_quoted "../../lexilla/src/LexillaVersion.rc" '#define VERSION_LEXILLA '   || true)
boostVer=$(grep -F '#define BOOST_LIB_VERSION ' ../../boostregex/boost/version.hpp 2>/dev/null \
           | sed -E 's/.*"([^"]+)".*/"\1"/' | head -n1 || true)

: "${sciVer:=\"N/A\"}"
: "${lexVer:=\"N/A\"}"
: "${boostVer:=\"N/A\"}"

echo "Scintilla version detected: $sciVer"
echo "Lexilla version detected: $lexVer"
echo "Boost Regex version detected: $boostVer"

{
    echo "// NppLibsVersion.h"
    echo "// - maintained by NppLibsVersionH-generator.sh"
    echo "#define NPP_SCINTILLA_VERSION   $sciVer"
    echo "#define NPP_LEXILLA_VERSION     $lexVer"
    echo "#define NPP_BOOST_REGEX_VERSION $boostVer"
} > "$OUT"
