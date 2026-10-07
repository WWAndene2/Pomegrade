#!/bin/bash
# One headless run of Omega Ruby in the session (after session_setup.sh): a mod, the owner's save (moved if asked) and a
# retro_host script, the log and any screenshot in <work>/runs/<name>. About 2 min to boot to the field, 6 for a walk.
#
#   tools/remake/headless/run_local.sh <work dir> <name> <mod dir|-> "<zone x z>|" "<script>" [seconds] [gdb]
#
# (env) POMEGRADE_SAVE  a save file to start from instead of <work>/dumps/save/main
# mod dir   a folder holding load/mods/000400000011C400 (oras-region, oras-engine, oras-append-test ... write one); - for none
# zone x z  the save moved there first (oras-save); empty to keep the owner's position (Littleroot, zone 6)
# script    retro_host commands separated by ';' (retro_host.cpp: wait N, hold KEYS N, press KEYS, mash KEYS N, field [N],
#           screen, shot NAME, report, mem ADDRESS LENGTH, watch FRAMES [KEYS] ADDRESS..., dump ADDRESS LENGTH FILE,
#           gdb PORT, trace on / trace off FILE (with POMEGRADE_INTERPRETER=1), save NAME / load NAME). A save state can only be loaded after the core has booted ("wait 120" first)
#           and does not restore a pending asset load: to freeze again, replay from the start.
# gdb       given as the 7th argument: run in the background and attach gdb-multiarch with the commands of the file
#           <work>/runs/<name>.gdb (see gdb_attach notes below); the script must hold "gdb 24689" after its first "wait".
#
# The log is capped at 2 GB: a game looping on an unmapped read once wrote 22 GB and filled the disk.
set -uo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd)
work=$(cd "$1" && pwd); name=$2; mod=$3; move=$4; script=$5; secs=${6:-120}; gdb=${7:-}
cd "$work"
run=runs/$name
rm -rf "$run"; mkdir -p "$run/Azahar/load/mods"
[ "$mod" != - ] && cp -r "$mod/load/mods/000400000011C400" "$run/Azahar/load/mods/"
save="$run/Azahar/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001"
# POMEGRADE_SAVE: another save file than the owner's dumps/save/main (one the owner made in the game where a run must start,
# 7 October: in front of a trainer, as a save moved by oras-save into another matrix does not load and one moved into a zone
# finds no characters there)
start=${POMEGRADE_SAVE:-dumps/save/main}
mkdir -p "$save"; cp "$start" "$save/main"; cp dumps/save/00000001.metadata "$save.metadata"
[ -n "$move" ] && ./build-remake/remake_tool oras-save "$start" "$save/main" $move > "$run/save.txt"
printf '%s\n' "$script" | tr ';' '\n' > "$run/script.txt"
host() { (ulimit -f 4000000; ./retro_host build-azahar/bin/Release/azahar_libretro.so dumps/oras.3ds "$run" "$secs" "$PWD/$run/script.txt" > "$run/log.txt" 2>&1); }
if [ -z "$gdb" ]; then
    host
else
    # gdb_attach: the stub answers once the game has booted; "set osabi none" (its "3DS" OS type crashes gdb), commands in a
    # file sourced in batch mode. Known flakiness: gdb sometimes believes the target is running and refuses "continue"
    # (retry the run); "set remote interrupt-on-connect" crashes the core; a breakpoint's "commands ... continue" block does
    # not resume in batch mode, so log with a "while" loop of "continue" + "printf" instead
    host & pid=$!
    for i in $(seq 1 120); do grep -q "gdb stub on port" "$run/log.txt" 2>/dev/null && break; sleep 5; done
    timeout 900 gdb-multiarch -q -batch -ex "set architecture arm" -ex "set osabi none" -ex "set tcp connect-timeout 60" \
        -ex "target remote :24689" -ex "source $work/runs/$name.gdb" > "$run/gdb.txt" 2>&1
    kill $pid 2>/dev/null; wait $pid 2>/dev/null
    grep -v "^warning" "$run/gdb.txt" | tail -20
fi
for f in "$run"/*.ppm; do [ -e "$f" ] && python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[1][:-4] + '.png')" "$f"; done
grep RESULT "$run/log.txt" || echo "RESULT: the host stopped without a result"
grep -A1 "^thread 1 " "$run/log.txt" | head -2 | cut -c1-120
