#!/bin/bash
# The mod to test everything ported so far on the phone (owner's request, 23 October): X/Y's world on the matrices
# sandbox/test_all.txt's warps name (its zones 538-556), the hub sandbox zone 557 on top, the engine with every port
# (X/Y's gyms, dust boxes, dust clouds, shared scripts, trainers, battles, music), a save standing in the hub, and one zip of both.
#
#   tools/remake/headless/test_all_mod.sh <work dir> <out name>
#
# Builds <work>/out/<name> (the mod), <work>/out/<name>_save, logs <work>/out/<name>*.log, and <work>/out/<name>.zip: its
# load/ and the save as sdmc/Nintendo 3DS/.../data/00000001/main, to copy into Pomegrade/3DS after deleting
# load/mods/000400000011C400 (ORAS_LITTLEROOT.md section 0, the phone test protocol).
set -euo pipefail
work=$(cd "$1" && pwd); name=$2
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$work"
T=./build-remake/remake_tool
rm -rf "out/$name" "out/${name}_save" "out/$name.zip" fscache
$T oras-xy-world dumps/oras.3ds dumps/x.3ds "out/$name" "$here/ghidra/function_names.tsv" --matrices 0,10,58,69,57,221,222,77,98,2,31 \
    --character-models 64 --story on --music on > "out/$name.log" 2>&1
grep -q "as zones 538-556" "out/$name.log" || { echo "X/Y's zones are not 538-556: sandbox/test_all.txt's warps would miss"; exit 1; }
$T oras-sandbox dumps/oras.3ds "out/$name" "$here/sandbox/test_all.txt" --onto "out/$name" --xy dumps/x.3ds > "out/${name}_sb.log" 2>&1
grep -q "^zone 557:" "out/${name}_sb.log" || { echo "the hub is not zone 557"; exit 1; }
# the zone rows the sandbox prints (its hub and the hub's house): the boot reads a/0/1/3 member 536 at exactly that size
rows=$(sed -n 's/.*(oras-engine --zone-rows \([0-9]*\)).*/\1/p' "out/${name}_sb.log")
opts=$(grep "oras-engine options" "out/$name.log" | sed 's/oras-engine options: //' | sed "s/--zone-rows [0-9]*/--zone-rows $rows/")
# shellcheck disable=SC2086
$T oras-engine dumps/oras.3ds "out/$name" $opts --character-models 64 --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 \
    --heap 0xC:0x800000 --heap 0x17:0x4000000 --xy-gyms dumps/x.3ds --xy-dust-box dumps/x.3ds --xy-dust-cloud on > "out/${name}_engine.log" 2>&1
grep -i "error" "out/${name}_engine.log" && exit 1
$T oras-save dumps/save/main "out/${name}_save" 557 17.5 18.5 --save-records 32 | tail -1
rm -rf "out/$name.pack" && mkdir -p "out/$name.pack/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001"
cp -r "out/$name/load" "out/$name.pack/"
cp "out/${name}_save" "out/$name.pack/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001/main"
[ -f dumps/save/00000001.metadata ] && cp dumps/save/00000001.metadata "out/$name.pack/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/"  # beside it, as the workflow's save
(cd "out/$name.pack" && zip -qr "../$name.zip" load sdmc)
rm -rf "out/$name.pack"
echo "mod out/$name, save out/${name}_save, zip out/$name.zip ($(du -h "out/$name.zip" | cut -f1))"
