#!/bin/bash
# Do story flags and works survive a save and a reload, in both ranges (the game's below flag 0xD00 / work 0x4178, and
# oras-engine --story large's in save block 47)? (20 October, Agent C's step 1, DEVELOPMENT_NOTE.md 3e.) A mod with zone 6's
# init script setting three flags and three works (once, guarded by work 0x4171) and the engine's --story large:
#   1. the owner's save (zone 6), Continue, the field (the script runs), memory dumped, then the menu's Save;
#   2. the file it wrote read: each value in save block 19 (EventWork's data) or block 47 at its place;
#   3. a boot with that file, memory dumped at the title (the save read, no zone script run yet): the same values;
#   4. control: a boot with the owner's save, the same places 0.
# Prints PASS or FAIL with the values. 18 minutes measured (20 October: three boots, the first played to the field and the
# save menu): over AGENT_PROCESS.md 3.1's 15, so run it only when the save layout or --story large changes.
#
#   tools/remake/headless/story_save_test.sh <work dir>
#
# After session_setup.sh (remake_tool, the core, dumps/save/main in zone 6); runs in <work>/runs/story_save_*.
set -uo pipefail
work=$(cd "$1" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
tool="$work/build-remake/remake_tool"
dir="$work/story_save"
rm -rf "$dir" && mkdir -p "$dir"
# flags 0xCF0, 0xD05, 0x28F0; works 0x4170 = 0x1357, 0x4180 = 0x2468, 0x4300 = 0x3579; 0x4171 = 1 the guard
cat > "$dir/init6.amx" << 'EOF'
pubvar g_ai_flag 8
pubvar g_interactive_flag 4
pubvar g_mode 0
code
    halt.p 0
main:
    proc
    push.p.c 16753
    sysreq.n WorkGet 4
    jnz done
    push.p.c 3312
    sysreq.n FlagSet 4
    push.p.c 3333
    sysreq.n FlagSet 4
    push.p.c 10480
    sysreq.n FlagSet 4
    push.p.c 4951
    push.p.c 16752
    sysreq.n WorkSet 8
    push.p.c 9320
    push.p.c 16768
    sysreq.n WorkSet 8
    push.p.c 13689
    push.p.c 17152
    sysreq.n WorkSet 8
    push.p.c 1
    push.p.c 16753
    sysreq.n WorkSet 8
done:
    zero.pri
    retn
data
    cell -1 0 0
EOF
"$tool" amx-asm "$dir/init6.amx" "$dir/init6.bin" "$here/../ghidra/function_names.tsv" > /dev/null || { echo "FAIL: the script does not assemble"; exit 1; }
"$tool" oras-zone-script "$work/dumps/oras.3ds" "$dir/mod" 6 - "$dir/init6.bin" > /dev/null || { echo "FAIL: the zone"; exit 1; }
"$tool" oras-engine "$work/dumps/oras.3ds" "$dir/mod" --story large > /dev/null || { echo "FAIL: the engine"; exit 1; }
cd "$work"
saved="runs/story_save_1/Azahar/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001/main"
# 1. the save menu as in ORAS_ENGINE.md 6 (run sv2): X, right, down held 10 frames, A on Save, A to confirm
"$here/run_local.sh" "$work" story_save_1 "$dir/mod" "" "wait 700;mash a 8;wait 900;draw every 15;wait 60;dump 0x08000000 0x6000000 field.bin;press x;wait 60;press right;wait 20;hold down 10;wait 20;press a;wait 120;press a;wait 300;press a;wait 300;press a;wait 300" 1400 > /dev/null 2>&1
cp "$saved" "$dir/main_after" 2> /dev/null || { echo "FAIL: no save written"; exit 1; }
# 3. and 4.: the title, the save read
POMEGRADE_SAVE="$dir/main_after" "$here/run_local.sh" "$work" story_save_2 "$dir/mod" "" "wait 700;dump 0x08000000 0x6000000 title.bin" 1200 > /dev/null 2>&1
"$here/run_local.sh" "$work" story_save_3 "$dir/mod" "" "wait 700;dump 0x08000000 0x6000000 title.bin" 1200 > /dev/null 2>&1
python3 -I - "$dir/main_after" runs/story_save_1/field.bin runs/story_save_2/title.bin runs/story_save_3/title.bin << 'EOF'
import struct, sys
want = {'flag 0xCF0': 1, 'flag 0xD05': 1, 'flag 0x28F0': 1, 'work 0x4170': 0x1357, 'work 0x4171': 1, 'work 0x4180': 0x2468, 'work 0x4300': 0x3579}
def values(d, works, flags, works47, flags47):
    # works 0x4000-0x4177 u16 from `works`, flags 1-0xCFF bits from `flags`; block 47: works 0x4178 up, then flags 0xD00 up
    work = lambda w: struct.unpack_from('<H', d, works + 2 * (w - 0x4000) if w < 0x4178 else works47 + 2 * (w - 0x4178))[0]
    flag = lambda f: (d[flags + f // 8] >> (f % 8)) & 1 if f < 0xD00 else (d[flags47 + (f - 0xD00) // 8] >> ((f - 0xD00) % 8)) & 1
    return {k: (flag if k.startswith('flag') else work)(int(k.split()[1], 16)) for k in want}
f = open(sys.argv[1], 'rb').read()
# the file: block 19 (EventWork's data, 0x14A00) and block 47 (0x20600), oras_save.py blocks
fromFile = values(f, 0x14A00, 0x14A00 + 0x2F0, 0x20600, 0x20600 + 1000)
field = open(sys.argv[2], 'rb').read()
# memory: EventWork's data found in the field's dump by the two works side by side (0x1357, 1 at 0x4170-0x4171), the copy
# in SaveData (the last one: the save buffer's copies come before it; run svC, 20 October); block 47 at EventWork + 0xA550
at = [i for i in range(0, len(field) - 4, 2) if field[i:i + 4] == struct.pack('<HH', 0x1357, 1)]
if not at: print('FAIL: the values not in memory after the script'); sys.exit(1)
base = at[-1] - 2 * 0x170
mem = lambda d: values(d, base, base + 0x2F0, base - 4 + 0xA550, base - 4 + 0xA550 + 1000)
reloaded, control = mem(open(sys.argv[3], 'rb').read()), mem(open(sys.argv[4], 'rb').read())
print('file     ', fromFile); print('reloaded ', reloaded); print('control  ', control)
ok = fromFile == want and reloaded == want and all(v == 0 for v in control.values())
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
EOF
