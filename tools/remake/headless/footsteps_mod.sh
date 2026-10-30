#!/bin/bash
# The footstep test mod (owner's request, 30 October): X/Y's whole world (matrices 0-399, its footsteps by texture,
# OrasFootstepGround.h), the test field sandbox/footsteps.txt on top (zone 898), the music muted, and a save standing in
# the field with Fly (party slot 0, move 3), every badge and every Fly place visited, in one zip.
#
#   tools/remake/headless/footsteps_mod.sh <work dir> <out name>
#
# Needs in <work>: dumps/oras.3ds, dumps/x.3ds, dumps/save/main, build-remake/remake_tool. Builds <work>/out/<name>,
# <work>/out/<name>_save, logs <work>/out/<name>*.log and <work>/out/<name>.zip (load/ and the save, as test_all_mod.sh;
# the phone test protocol: ORAS_LITTLEROOT.md section 0). About 15 minutes here.
set -euo pipefail
work=$(cd "$1" && pwd); name=$2
here=$(cd "$(dirname "$0")/.." && pwd)
cd "$work"
T=./build-remake/remake_tool
rm -rf "${work:?}/out/$name" "out/${name}_save" "out/$name.zip" fscache
mkdir -p out
python3 "$here/map/fly_background.py" "$here/map/hoenn_xy_map.png" "out/${name}_fly_hoenn.rgba"
python3 "$here/map/fly_background.py" "$here/map/kalos_xy_map.png" "out/${name}_fly_kalos.rgba" --flip
$T oras-xy-world dumps/oras.3ds dumps/x.3ds "out/$name" "$here/ghidra/function_names.tsv" --matrices "$(seq -s, 0 399)" \
    --character-models 64 --story off --hoenn-story off --music mute > "out/$name.log" 2>&1
$T oras-sandbox dumps/oras.3ds "out/$name" "$here/sandbox/footsteps.txt" --onto "out/$name" --xy dumps/x.3ds > "out/${name}_sb.log" 2>&1
grep -q "^zone 898:" "out/${name}_sb.log" || { echo "the test field is not zone 898"; exit 1; }
# the zone rows the sandbox prints: the boot reads a/0/1/3 member 536 at exactly that size
rows=$(sed -n 's/.*(oras-engine --zone-rows \([0-9]*\)).*/\1/p' "out/${name}_sb.log")
opts=$(grep "oras-engine options" "out/$name.log" | sed 's/oras-engine options: //' | sed "s/--zone-rows [0-9]*/--zone-rows $rows/")
# shellcheck disable=SC2086
$T oras-engine dumps/oras.3ds "out/$name" $opts --character-models 64 --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 \
    --heap 0xC:0x800000 --heap 0x17:0x4000000 --xy-gyms dumps/x.3ds --xy-dust-box dumps/x.3ds --xy-dust-cloud on \
    --xy-flower-grass dumps/x.3ds --fly-map-hoenn "out/${name}_fly_hoenn.rgba" --fly-map-kalos "out/${name}_fly_kalos.rgba" \
    --fly-places-hoenn "$here/map/hoenn_xy_places.txt" --fly-places-kalos "$here/map/kalos_xy_places.txt" > "out/${name}_engine.log" 2>&1
grep -i "error" "out/${name}_engine.log" && exit 1
# the save: in the field, Fly, the badges, and the Fly places' visited flags (both regions, as the run of 30 October)
P="$here/prototype/oras_save.py"
visited="$(printf '0x%x,' $(seq 0xAC6 0xAF9) 0xAFC 0xAFD 0xAFE 0xAFF 0xB00 0xB01 $(seq 0xB03 0xB08) 0xB0F 0xB10 \
    $(seq 0xB51 0xB54) 0xBA0 0xBA1 0xBA2 0x17A0 0x17A1 0x17A2 $(seq 0x17A4 0x17B3))"
$T oras-save dumps/save/main "out/${name}_s1" 898 20.5 19.5 --save-records 32 | tail -1
python3 "$P" move "out/${name}_s1" "out/${name}_s2" 0 3 19
python3 "$P" badges "out/${name}_s2" "out/${name}_s3" 0xFF
python3 "$P" flag "out/${name}_s3" "out/${name}_save" "${visited%,}"
rm -f "out/${name}_s1" "out/${name}_s2" "out/${name}_s3"
data="sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data"
rm -rf "out/$name.pack" && mkdir -p "out/$name.pack/$data/00000001"
cp -r "out/$name/load" "out/$name.pack/"
cp "out/${name}_save" "out/$name.pack/$data/00000001/main"
[ -f dumps/save/00000001.metadata ] && cp dumps/save/00000001.metadata "out/$name.pack/$data/"
(cd "out/$name.pack" && zip -qr "../$name.zip" load sdmc)
rm -rf "out/$name.pack"
echo "mod out/$name, save out/${name}_save, zip out/$name.zip ($(du -h "out/$name.zip" | cut -f1))"
