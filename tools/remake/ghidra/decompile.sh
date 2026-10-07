#!/bin/bash
# Decompiles functions of the session's Ghidra program (session_setup.sh) into <out dir>/decomp.c, with their names and
# roles from function_names.tsv as ApplyNames gave them: the addresses given, and with --callers / --callees ADDRESS the
# functions that call it or that it calls (from the call graph <work>/edges.tsv, CallGraph.java). Read-only: the program
# is not changed. Ghidra is a heavy job: not alongside a run or a build (ORAS_ENGINE.md 0, rule 2).
#
#   tools/remake/ghidra/decompile.sh <work dir> <out dir> [ADDRESS | --callers ADDRESS | --callees ADDRESS]...
#
# ADDRESS in hex, 0x optional (0x453E08, 102cd2b4). Prints each function's name and size in lines.
set -uo pipefail
work=$(cd "$1" && pwd); out=$2; shift 2
here=$(cd "$(dirname "$0")" && pwd)
edges=$work/edges.tsv; [ -f "$edges" ] || edges=$work/fresh/edges.tsv
norm() { printf '%08x' "0x${1#0x}"; }
addresses=()
while [ $# -gt 0 ]; do
    case $1 in
        --callers) a=$(norm "$2"); addresses+=($(awk -F'\t' -v a="$a" 'tolower($2) == a {print $1}' "$edges" | sort -u)); shift 2 ;;
        --callees) a=$(norm "$2"); addresses+=($(awk -F'\t' -v a="$a" 'tolower($1) == a {print $2}' "$edges" | sort -u)); shift 2 ;;
        *) addresses+=($(norm "$1")); shift ;;
    esac
done
[ ${#addresses[@]} -gt 0 ] || { echo "no function to decompile"; exit 1; }
mkdir -p "$out"
"$work/ghidra_11.4.2_PUBLIC/support/analyzeHeadless" "$work/ghidra_proj" oras -process code.bin -noanalysis -readOnly \
    -scriptPath "$here" -postScript Export.java "$(cd "$out" && pwd)" $(printf '0x%s ' "${addresses[@]}") > "$out/ghidra.log" 2>&1
[ -s "$out/decomp.c" ] || { echo "Ghidra wrote nothing: $out/ghidra.log"; tail -5 "$out/ghidra.log"; exit 1; }
awk '/^\/\/ ---- /{if (name) print name, n " lines"; name = $3 " " $5; n = 0; next} {n++} END {if (name) print name, n " lines"}' "$out/decomp.c"
echo "$out/decomp.c"
