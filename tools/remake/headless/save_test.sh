#!/bin/bash
# Does a save file load to the field? Several saves tested quickly (8 October): the boot to the title is played once and kept
# as an emulator state (<work>/title.state), then each save is copied into a fresh run that loads that state, presses A and
# looks at the screen. The test is only valid if the game reads the save after the title: the first time the state is made,
# the two control saves given with --good and --bad are tested and must come out LOADED and BLACK, else the script stops.
#
#   tools/remake/headless/save_test.sh <work dir> [--good <save> --bad <save>] <save>...
#
# Prints one line per save: LOADED (screen brightness over 20 and DllField loaded) or BLACK, with the run name
# (runs/st_<save file name>, its shot start.png). JIT runs (no trace); one heavy job at a time (ORAS_ENGINE.md 0, rule 2).
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
work=$(cd "$1" && pwd); shift
good=; bad=
while [ $# -gt 0 ]; do
    case $1 in --good) good=$(realpath "$2"); shift 2 ;; --bad) bad=$(realpath "$2"); shift 2 ;; *) break ;; esac
done
state=$work/title.state

verdict() {  # verdict <run name>: LOADED or BLACK
    local log=$work/runs/$1/log.txt
    local b; b=$(grep -oE "screen brightness [0-9.]+" "$log" | tail -1 | awk '{print $3}')
    if grep -q 'CRO "DllField" loaded' "$log" && python3 -c "import sys; sys.exit(0 if float('${b:-0}') > 20 else 1)"; then
        echo LOADED; else echo BLACK; fi
}

one() {  # one <save>: a run from the title state
    local name=st_$(basename "$1")
    POMEGRADE_SAVE=$(realpath "$1") POMEGRADE_STATE=$state "$here/run_local.sh" "$work" "$name" - "" \
        "wait 120;load title;mash a 15;wait 600;shot start" 60 > /dev/null 2>&1
    echo "$(verdict "$name") $1 (runs/$name)"
}

if [ ! -f "$state" ]; then
    [ -n "$good" ] && [ -n "$bad" ] || { echo "no $state yet: give --good and --bad control saves to make and check it"; exit 1; }
    "$here/run_local.sh" "$work" st_title - "" "wait 700;save title" 40 > /dev/null 2>&1
    cp "$work/runs/st_title/title.state" "$state" 2> /dev/null || { echo "the title state was not written (runs/st_title)"; exit 1; }
    g=$(one "$good"); b=$(one "$bad"); echo "control: $g"; echo "control: $b"
    if [[ $g != LOADED* || $b != BLACK* ]]; then
        rm -f "$state"; echo "the title state does not test saves (the game read the save before the title?): removed"; exit 1
    fi
fi
for s in "$@"; do one "$s"; done
