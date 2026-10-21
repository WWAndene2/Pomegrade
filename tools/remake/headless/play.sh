#!/bin/bash
# A game kept running, driven step by step (22 October): fast_run.sh (or run_local.sh) started in the background with a script
# ending "interactive <run>/play.fifo"; then each call sends retro_host commands (';'-separated, as a script's) and waits
# until they ran, printing the log lines they wrote (shots land in the run's folder as <name>.png). "quit" ends the game.
#
#   tools/remake/headless/play.sh <work dir> <run name> "<commands>" [seconds to wait, 600]
set -uo pipefail
work=$(cd "$1" && pwd); run=$work/runs/$2; commands=$3; limit=${4:-600}
log=$run/log.txt; fifo=$run/play.fifo
for i in $(seq 1 600); do [ -p "$fifo" ] && grep -q "^\[ready frame" "$log" 2>/dev/null && break; sleep 1; done
[ -p "$fifo" ] || { echo "play: no game waiting at $fifo"; exit 1; }
before=$(grep -c "^\[ready frame" "$log"); lines=$(wc -l < "$log")
printf '%s\n' "$commands" | tr ';' '\n' > "$fifo"
[ "$commands" = quit ] && exit 0
for i in $(seq 1 "$limit"); do [ "$(grep -c "^\[ready frame" "$log")" -gt "$before" ] && break; sleep 1; done
tail -n +"$((lines + 1))" "$log" | grep -vE "SetProbeResponseParam|Delay generator|Path not found"
for f in "$run"/*.ppm; do [ -e "$f" ] && python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[1][:-4] + '.png')" "$f" && rm "$f"; done
