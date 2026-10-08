#!/bin/bash
# run_local.sh the fast way (8 October): from a title state made with the same mod and save, drawing off once the Continue
# menu is passed. A title state holds the save the game read at boot and the mod it booted with (run sand5: a state made
# without the mod, then loaded with it and a save moved to zone 538, showed Littleroot), so one is made per mod, save and
# move, kept in <work>/states/<key>/title.state (run_local.sh copies a state under its own file name, which "load title"
# names) and reused: the first run of a test boots once more (about 4 min), every rerun reaches the field in about 2 min (run sand7: the walk to a trainer and his line in 2 min 49 s, 10-12 min before).
#
#   tools/remake/headless/fast_run.sh <work dir> <name> <mod dir|-> "<zone x z>|" "<script after the field loads>" [seconds]
#
# The script runs after "wait 120;load title;mash a 8;draw off;wait 900" (the field up, drawing off: a shot draws 3 frames
# first; "field" and the freeze warning need drawing, run_local.sh). POMEGRADE_SAVE as for run_local.sh.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
work=$(cd "$1" && pwd); name=$2; mod=$3; move=$4; script=$5; secs=${6:-80}
save=${POMEGRADE_SAVE:-$work/dumps/save/main}
key=$( { [ "$mod" != - ] && find "$mod" -type f -print0 | sort -z | xargs -0 sha256sum; sha256sum "$save"; echo "$move";
         sha256sum "$work/build-azahar/bin/Release/azahar_libretro.so"; } | sha256sum | cut -c1-16)
state=$work/states/$key/title.state
if [ ! -f "$state" ]; then
    echo "no title state for this mod, save and move yet: booting once to make it ($key)"
    "$here/run_local.sh" "$work" "title_$key" "$mod" "$move" "wait 700;save title" 40 > /dev/null 2>&1
    mkdir -p "$work/states/$key"
    cp "$work/runs/title_$key/title.state" "$state" 2> /dev/null || { echo "the title state was not written (runs/title_$key)"; exit 1; }
fi
POMEGRADE_STATE=$state "$here/run_local.sh" "$work" "$name" "$mod" "$move" "wait 120;load title;mash a 8;draw off;wait 900;$script" "$secs"
