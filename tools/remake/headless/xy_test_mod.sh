#!/bin/bash
# A test mod with X/Y's world and a sandbox zone on top, the engine built with the options X/Y's world asks for, and a
# save standing in the sandbox's first zone (23 October: the recipe of runs wh, ws, sg, typed by hand before).
#
#   tools/remake/headless/xy_test_mod.sh <work dir> <out name> <sandbox description> [ZONE X Z] [-- extra oras-engine options]
#
# Builds <work>/out/<name> (the mod) and <work>/out/<name>_save (the save, at ZONE X Z, default 542 14.5 20.5: the
# sandbox's first zone after X/Y's matrix 0), logs in <work>/out/<name>*.log. Then:
#   python3 tools/remake/headless/session.py start <work> S --mod out/<name> --save out/<name>_save   (about 9 min)
#   python3 tools/remake/headless/session.py do <work> S "save field; ..."                          (commands: retro_host)
# X/Y's world: matrix 0 (Vaniville and around), its story and music. The sandbox's zones follow X/Y's: their count is
# added to --zone-rows (3 more zones than the world's here; a description of another size: change ROWS below).
set -euo pipefail
work=$(cd "$1" && pwd); name=$2; description=$(cd "$(dirname "$3")" && pwd)/$(basename "$3"); shift 3
zone=542; x=14.5; z=20.5; ROWS=3
if [ $# -ge 3 ] && [ "$1" != "--" ]; then zone=$1; x=$2; z=$3; shift 3; fi
[ "${1:-}" = "--" ] && shift
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$work"
T=./build-remake/remake_tool
rm -rf "out/$name" fscache
$T oras-xy-world dumps/oras.3ds dumps/x.3ds "out/$name" "$here/ghidra/function_names.tsv" --matrices 0 --character-models 64 --story on --music on > "out/$name.log" 2>&1
$T oras-sandbox dumps/oras.3ds "out/$name" "$description" --onto "out/$name" --xy dumps/x.3ds > "out/${name}_sb.log" 2>&1
opts=$(grep "oras-engine options" "out/$name.log" | sed 's/oras-engine options: //')
rows=$(echo "$opts" | sed -n 's/.*--zone-rows \([0-9]*\).*/\1/p')
opts=$(echo "$opts" | sed "s/--zone-rows $rows/--zone-rows $((rows + ROWS))/")
# shellcheck disable=SC2086
$T oras-engine dumps/oras.3ds "out/$name" $opts --character-models 64 --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000 "$@" > "out/${name}_engine.log" 2>&1
$T oras-save dumps/save/main "out/${name}_save" "$zone" "$x" "$z" --save-records 32 | tail -1
grep -i "error" "out/${name}_engine.log" && exit 1
echo "mod out/$name, save out/${name}_save; engine log out/${name}_engine.log"
