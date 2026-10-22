#!/bin/bash
# Decompiles functions of the session's Ghidra program (session_setup.sh) into <out dir>/decomp.c, with their names and
# roles from function_names.tsv as ApplyNames gave them: the addresses given, and with --callers / --callees ADDRESS the
# functions that call it or that it calls (from the call graph <work>/edges.tsv, CallGraph.java). Read-only: the program
# is not changed. Ghidra is a heavy job: not alongside a run or a build (ORAS_ENGINE.md 0, rule 2).
#
#   tools/remake/ghidra/decompile.sh <work dir> <out dir> [--listing] [ADDRESS | --callers ADDRESS | --callees ADDRESS]...
#
# --listing: the functions disassembled instead, into <out dir>/listing.s (Disassembly.java: names of the addresses reached,
# literal words also read as floats), for code the decompiler drops (VFP float instructions).
#
# ADDRESS in hex, 0x optional (0x453E08, 102cd2b4). Prints each function's name and size in lines.
#
# POMEGRADE_GHIDRA_GAME=x: X's program instead (session_setup.sh ghidra-x: ghidra_proj_x, xcode.bin, the call graph in <work>/x/)
set -uo pipefail
work=$(cd "$1" && pwd); out=$2; shift 2
here=$(cd "$(dirname "$0")" && pwd)
project=ghidra_proj; name=oras; program=code.bin; edges=$work/edges.tsv; [ -f "$edges" ] || edges=$work/fresh/edges.tsv
[ "${POMEGRADE_GHIDRA_GAME:-}" = x ] && { project=ghidra_proj_x; name=x; program=xcode.bin; edges=$work/x/edges.tsv; }
norm() { printf '%08x' "0x${1#0x}"; }
addresses=(); script=Export.java; result=decomp.c
while [ $# -gt 0 ]; do
    case $1 in
        --callers) a=$(norm "$2"); addresses+=($(awk -F'\t' -v a="$a" 'tolower($2) == a {print $1}' "$edges" | sort -u)); shift 2 ;;
        --callees) a=$(norm "$2"); addresses+=($(awk -F'\t' -v a="$a" 'tolower($1) == a {print $2}' "$edges" | sort -u)); shift 2 ;;
        --listing) script=Disassembly.java; result=listing.s; shift ;;
        *) addresses+=($(norm "$1")); shift ;;
    esac
done
[ ${#addresses[@]} -gt 0 ] || { echo "no function to decompile"; exit 1; }
mkdir -p "$out"
"$work/ghidra_11.4.2_PUBLIC/support/analyzeHeadless" "$work/$project" $name -process $program -noanalysis -readOnly \
    -scriptPath "$here" -postScript "$script" "$(cd "$out" && pwd)" $(printf '0x%s ' "${addresses[@]}") > "$out/ghidra.log" 2>&1
[ -s "$out/$result" ] || { echo "Ghidra wrote nothing: $out/ghidra.log"; tail -5 "$out/ghidra.log"; exit 1; }
awk '/^(\/\/|;) ---- /{if (name) print name, n " lines"; name = $3 " " ($4 == "in" ? $5 : $4); n = 0; next} {n++} END {if (name) print name, n " lines"}' "$out/$result"
echo "$out/$result"
