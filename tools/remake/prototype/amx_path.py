#!/usr/bin/env python3
"""The path a running Pawn (AMX) script takes, read live from the game (2 November, Clemont's gym: where the quiz
room's arrival script stops, ORAS_ENGINE.md 0.0). The script's code cells are read by the game's AMX interpreter
(opcode, then its operands), so a `readers` log over the script's code segment is its path, cell by cell.

  1. the script image's address: amx_dump.py on a heap dump taken while the script runs (its header, cod, dat);
  2. a retro_host script with POMEGRADE_INTERPRETER=1 (the JIT bypasses the readers log):
       readers <image + cod> <image + dat>   ...   readers off rd.txt
     (the log stops at 100000 reads: keep the window short);
  3. amx_path.py rd.txt <image + cod> [--listing <remake_tool oras-script listing>]

Prints each label entered (Lxxxxxx, from the listing; without it, each jump: a cell read not right after the last),
with the read's index, and the last cell read: where the script stopped or is waiting. A cip is the cell's offset in the
code segment, as the listing numbers them.
"""
import argparse
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('reads')
    ap.add_argument('code', type=lambda s: int(s, 0), help="the code segment's address in memory")
    ap.add_argument('--listing')
    a = ap.parse_args()
    labels = set()
    if a.listing:
        labels = {int(m.group(1), 16) for m in re.finditer(r'^L([0-9A-F]{6}):', open(a.listing).read(), re.M)}
    last = None
    n = 0
    for line in open(a.reads):
        f = line.split()
        if len(f) < 2:
            continue
        cip = int(f[0], 16) - a.code
        if cip < 0:
            continue
        n += 1
        if a.listing:
            if cip in labels and cip != last:
                print(f'{n:6} L{cip:06X}')
        elif last is not None and cip not in (last, last + 4):
            print(f'{n:6} {last:#x} -> {cip:#x}')
        last = cip
    print(f'{n} reads; last cip {last:#x}' if last is not None else 'no read in the code segment')


if __name__ == '__main__':
    main()
