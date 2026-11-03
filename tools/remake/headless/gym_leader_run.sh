#!/bin/bash
# A gym leader's battle played headless (3 November, the gyms' checks): a save standing on TILE X Z (facing up, height H
# for a raised floor: the leader's record's float at word 22, `oras-inspect zone 538 --mod`), then A pressed every second
# in ROUNDS rounds of 15, a shot after each (s0 the start, s1..sN), all in one fast_run (its title state per mod).
#
#   tools/remake/headless/gym_leader_run.sh <work dir> <run name> <mod name> <tile x> <tile z> [height] [rounds]
#
# <mod name>: out/<mod name> built by xy_gym_mod.sh (--weak TEAM for a quick win). Prints the unmapped accesses' count;
# the shots are runs/<run name>/s*.png. Checked on Viola, Grant, Korrina, Valerie (3 November).
set -euo pipefail
work=$(cd "$1" && pwd); run=$2; mod=$3; x=$4; z=$5; height=${6:-0}; rounds=${7:-7}
here=$(cd "$(dirname "$0")" && pwd)
heights=(); [ "$height" != 0 ] && heights=(--height "$height")
(cd "$work" && ./build-remake/remake_tool oras-save dumps/save/main "out/${mod}_leader_save" 538 "$x" "$z" --save-records 32 "${heights[@]}" \
    --characters-from dumps/oras.3ds "out/$mod" | tail -1)
s="draw;wait 3;shot s0;draw every 4;press up;wait 20;draw off"
for k in $(seq 1 "$rounds"); do
    for i in $(seq 1 15); do s="$s;press a;wait 60"; done
    s="$s;draw;wait 3;shot s$k;draw off"
done
rm -rf "$work/runs/$run"
POMEGRADE_SAVE="$work/out/${mod}_leader_save" "$here/fast_run.sh" "$work" "$run" "$work/out/$mod" "" "$s" $((rounds * 15 * 70 / 60 + 300)) > /dev/null 2>&1 || true
echo "unmapped accesses: $(grep -c unmapped "$work/runs/$run/log.txt" || true); shots in runs/$run"
