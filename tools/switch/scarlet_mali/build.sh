#!/bin/sh
# Builds the Mali Scarlet module (subsdk9) and lays it out as an Eden mod:
#   <out>/Mali Scarlet/exefs/subsdk9
# copied into Punica/Switch/Mods/0100A3D008C5C000/ on the phone (Eden's mod folder for Scarlet).
# Needs git and Docker; compiles with devkitPro's devkitA64 image, using exlaunch (shadowninja108, GPL-2.0)
# at the commit tested on 3 November. The game's own main.npdm is kept: only subsdk9 is added.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
work=${1:-"$here/build"}
out=${2:-"$work/mod"}
exlaunch_commit=f9f4b0d

mkdir -p "$work"
if [ ! -d "$work/exlaunch" ]; then
    git clone -q https://github.com/shadowninja108/exlaunch "$work/exlaunch"
fi
git -C "$work/exlaunch" checkout -q "$exlaunch_commit"
git -C "$work/exlaunch" checkout -q -- source/program
cp "$here/source/"* "$work/exlaunch/source/program/"
# exlaunch's own settings header, with the module's name for its log prefix
git -C "$work/exlaunch" show "$exlaunch_commit:source/program/setting.hpp" |
    sed 's/#define EXL_MODULE_NAME "exlaunch"/#define EXL_MODULE_NAME "scarlet_mali"/' > "$work/exlaunch/source/program/setting.hpp"
docker run --rm -v "$work/exlaunch:/src" -w /src devkitpro/devkita64:latest \
    make PROGRAM_ID=0100A3D008C5C000 LOAD_KIND=Module -j4
mkdir -p "$out/Mali Scarlet/exefs"
cp "$work/exlaunch/deploy/subsdk9" "$out/Mali Scarlet/exefs/subsdk9"
echo "built: $out/Mali Scarlet/exefs/subsdk9"
