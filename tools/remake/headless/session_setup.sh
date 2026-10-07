#!/bin/bash
# Prepares a cloud session to run and read Omega Ruby itself, in minutes per test instead of an hour per workflow run
# (ORAS_ENGINE.md 1, "How a session works"). Everything goes into a work folder outside the repository: the owner's dumps are
# never committed (CLAUDE.md 8).
#
#   tools/remake/headless/session_setup.sh <work dir> [dumps|tool|core|ghidra|all]...
#
# dumps   the owner's files from Google Drive, by the ids given in the environment (each is in the Drive folder "Pokemon
#         Project - Radiant Platinum"; ask the owner for a share link when one is missing):
#           POMEGRADE_ORAS_DRIVE_ID      the Omega Ruby cartridge (.3ds, decrypted), 1.9 GB  -> dumps/oras.3ds
#           POMEGRADE_PLATINUM_DRIVE_ID  Platinum (.nds)                                     -> dumps/platinum.nds
#           POMEGRADE_SAVE_DRIVE_ID      the owner's save (main.zip: main, 00000001.metadata) -> dumps/save/
# tool    remake_tool (tools/remake), and the game's code decompressed for disassembly -> dumps/code.bin
# core    Azahar's libretro core with the headless patches (pomegrade_report.inc, patch_core.py: thread report, memory
#         dump, GDB stub), built from a copy of azahar/ (about 30 min on 4 cores, once), and retro_host
# ghidra  Ghidra 11.4.2 and the whole game's code analysed in one program: the .code and the 145 code modules linked by
#         prototype/cro_link.py (about 15 min) -> ghidra_proj/, linked/
# all     the four, in that order (the default)
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd)
work=$(mkdir -p "$1" && cd "$1" && pwd); shift
steps=("${@:-all}")
[ "${steps[0]}" = all ] && steps=(dumps tool core ghidra)
cd "$work"

drive() { # drive <id> <out>: a file shared by link (curl through the session's proxy; gdown refuses the 1.9 GB file)
    curl --location --silent --show-error --output "$2" "https://drive.usercontent.google.com/download?id=$1&export=download&confirm=t"
}

for step in "${steps[@]}"; do
case $step in
dumps)
    mkdir -p dumps
    : "${POMEGRADE_ORAS_DRIVE_ID:?set POMEGRADE_ORAS_DRIVE_ID (the .3ds in the Drive folder)}"
    : "${POMEGRADE_PLATINUM_DRIVE_ID:?set POMEGRADE_PLATINUM_DRIVE_ID}"
    : "${POMEGRADE_SAVE_DRIVE_ID:?set POMEGRADE_SAVE_DRIVE_ID (main.zip)}"
    [ -s dumps/oras.3ds ] || drive "$POMEGRADE_ORAS_DRIVE_ID" dumps/oras.3ds
    [ -s dumps/platinum.nds ] || drive "$POMEGRADE_PLATINUM_DRIVE_ID" dumps/platinum.nds
    drive "$POMEGRADE_SAVE_DRIVE_ID" dumps/save.zip && rm -rf dumps/save && mkdir -p dumps/save && unzip -q -o dumps/save.zip -d dumps/save
    # the zip may hold the folder 00000001 or its content: keep main and its metadata side by side
    main=$(find dumps/save -type f -name main | head -n 1); [ -n "$main" ] && [ "$(dirname "$main")" != dumps/save ] && mv "$(dirname "$main")"/* dumps/save/
    ls -l dumps dumps/save ;;
tool)
    cmake -S "$repo/tools/remake" -B build-remake -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
    ninja -C build-remake remake_tool
    ./build-remake/remake_tool oras-code dumps/oras.3ds dumps/code.bin ;;
core)
    git -C "$repo" submodule update --init --recursive --depth 1 azahar > /dev/null
    rm -rf core-src && mkdir -p core-src/tools/remake && cp -a "$repo/azahar" core-src/ && cp -a "$repo/tools/remake/headless" core-src/tools/remake/
    (cd core-src && cat tools/remake/headless/pomegrade_report.inc >> azahar/src/citra_libretro/citra_libretro.cpp && python3 tools/remake/headless/patch_core.py)
    cmake -S core-src/azahar -B build-azahar -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_LIBRETRO=ON -DENABLE_OPENGL=OFF \
        -DENABLE_VULKAN=OFF -DENABLE_TESTS=OFF > /dev/null
    ninja -C build-azahar citra_libretro
    g++ -std=c++17 -O2 -I "$repo/azahar/externals/libretro-common/libretro-common/include" "$repo/tools/remake/headless/retro_host.cpp" -ldl -o retro_host
    ls -l build-azahar/bin/Release/azahar_libretro.so retro_host ;;
ghidra)
    if [ ! -d ghidra_11.4.2_PUBLIC ]; then
        curl -sSL -o ghidra.zip https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_11.4.2_build/ghidra_11.4.2_PUBLIC_20250826.zip
        unzip -q ghidra.zip && rm ghidra.zip
    fi
    python3 -c "import capstone" 2>/dev/null || pip install --quiet capstone
    mkdir -p cro
    ./build-remake/remake_tool oras-list dumps/oras.3ds | grep -oE "[A-Za-z0-9_]+\.cro" | while read -r m; do
        [ -s "cro/$m" ] || ./build-remake/remake_tool oras-extract dumps/oras.3ds "$m" "cro/$m" > /dev/null
    done
    ./build-remake/remake_tool oras-extract dumps/oras.3ds static.crs cro/static.crs > /dev/null
    python3 "$repo/tools/remake/prototype/cro_link.py" linked cro/static.crs cro/*.cro
    rm -rf ghidra_proj && mkdir -p ghidra_proj
    ./ghidra_11.4.2_PUBLIC/support/analyzeHeadless "$work/ghidra_proj" oras -import dumps/code.bin -loader BinaryLoader \
        -loader-baseAddr 0x100000 -processor ARM:LE:32:v6 -max-cpu 4 -scriptPath "$repo/tools/remake/ghidra" \
        -preScript AddModules.java "$work/linked/modules.bin" "$work/linked/modules.tsv" \
        -postScript FixNoReturn.java -postScript ApplyNames.java "$repo/tools/remake/ghidra/function_names.tsv" > ghidra_setup.log 2>&1
    # the function list the coverage tools read (coverage_map.py), with the names applied
    ./ghidra_11.4.2_PUBLIC/support/analyzeHeadless "$work/ghidra_proj" oras -process code.bin -noanalysis -readOnly \
        -scriptPath "$repo/tools/remake/ghidra" -postScript Export.java "$work" > /dev/null 2>&1
    grep -E "REPORT" ghidra_setup.log | tail -3 ;;
*) echo "unknown step $step (dumps, tool, core, ghidra, all)"; exit 2 ;;
esac
done
