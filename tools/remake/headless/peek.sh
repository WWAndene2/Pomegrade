#!/bin/bash
# A look at a headless run while it goes on (run_local.sh turns the screenshots into PNG only when the run ends): its
# speed (retro_host's "speed:" line, once a minute), the last commands of the script it reached, and its screenshots so
# far as PNG, to open with an image viewer.
#
#   tools/remake/headless/peek.sh <work dir> <run name>
set -uo pipefail
run=$1/runs/$2
[ -f "$run/log.txt" ] || { echo "no run at $run"; exit 1; }
pid=$(pgrep -f "^\./retro_host .* runs/$2 " | head -1)
if [ -n "$pid" ]; then echo "host: running for $(ps -o etimes= -p "$pid" | tr -d ' ') s"; else echo "host: not running"; fi
grep "speed:" "$run/log.txt" | tail -1
grep "^>" "$run/log.txt" | tail -3
for f in "$run"/*.ppm; do
    [ -e "$f" ] || continue
    [ "${f%.ppm}.png" -nt "$f" ] || python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[1][:-4] + '.png')" "$f"
    echo "shot: ${f%.ppm}.png"
done
grep RESULT "$run/log.txt"
exit 0
