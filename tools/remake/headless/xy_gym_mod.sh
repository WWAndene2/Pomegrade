#!/bin/bash
# An X/Y gym's test mod (30 October, the gyms' ports): X/Y's world on one matrix, the engine with X/Y's gym modules
# (--xy-gyms), and a save standing in the gym, its first zone being mod zone 538.
#
#   tools/remake/headless/xy_gym_mod.sh <work dir> <out name> <X matrix> <x> <z> [--weak TEAM] [--characters N] [--height H]
#
# The gyms' matrices and tiles: Grant 58 (20.5 17.5, --height 414), Viola 51 (20.5 64.5), Korrina 60 (40.5 40.5),
# Ramos 64 (60.5 82.5, X's spawn; the zone does not show yet), Valerie 67 (15.5 9.5, X's spawn). --height H: the
# player's height in pixels (oras-save --height), for a tile on a raised floor: a character's height is its record's
# float at word 22, so standing beside one gives it (Valerie's platform: 180).
# --weak TEAM: the trainer's team made one Magikarp at level 1 (oras-sandbox `team TEAM`; Grant's is 1026), for a won
# battle. --characters N grows the field's heap 0x5D and the save's character records with it (N + 1).
# Then: POMEGRADE_SAVE=<work>/out/<name>_save fast_run.sh <work> <run> <work>/out/<name> "" "<script>".
# A fault's state is read without the interpreter (it crashes at boot with this core): `dump ADDRESS LENGTH FILE` after
# the fault in the same JIT run (the heap from 0x08000000, the linear heap from 0x14000000), then the module's object
# found by its vtable's address (the module's load address, in the log's "LoadCRO ... loaded at", plus the vtable's
# offset in the ported module).
set -euo pipefail
work=$(cd "$1" && pwd); name=$2; matrix=$3; x=$4; z=$5; shift 5
weak=""; characters=""; height=()
while [ $# -gt 0 ]; do
    case $1 in
        --weak) weak=$2; shift 2 ;;
        --characters) characters=$2; shift 2 ;;
        --height) height=(--height "$2"); shift 2 ;;
        *) echo "unknown option $1"; exit 1 ;;
    esac
done
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$work"
T=./build-remake/remake_tool
rm -rf "${work:?}/out/$name" "out/${name}_save"
mkdir -p out
$T oras-xy-world dumps/oras.3ds dumps/x.3ds "out/$name" "$here/ghidra/function_names.tsv" --matrices "$matrix" \
    --character-models 64 --story on > "out/$name.log" 2>&1
grep -q "^zone 538:" "out/$name.log" || { echo "the matrix's first zone is not 538"; exit 1; }
if [ -n "$weak" ]; then
    printf 'team %s\npokemon 129 1\nend\n' "$weak" > "out/${name}_weak.txt"
    $T oras-sandbox dumps/oras.3ds "out/$name" "out/${name}_weak.txt" --onto "out/$name" > "out/${name}_sb.log" 2>&1
fi
opts=$(grep "oras-engine options" "out/$name.log" | sed 's/oras-engine options: //')
extra=(); records=32
[ -n "$characters" ] && { extra=(--characters "$characters"); records=$((characters + 1)); }
# shellcheck disable=SC2086
$T oras-engine dumps/oras.3ds "out/$name" $opts --xy-gyms dumps/x.3ds "${extra[@]}" --character-models 64 --memory 124 \
    --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000 > "out/${name}_engine.log" 2>&1
$T oras-save dumps/save/main "out/${name}_save" 538 "$x" "$z" --save-records "$records" "${height[@]}" --characters-from dumps/oras.3ds "out/$name" | tail -1
echo "mod out/$name, save out/${name}_save"
