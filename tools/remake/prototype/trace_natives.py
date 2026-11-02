#!/usr/bin/env python3
"""The script natives a run called, from a retro_host `trace on` / `trace off FILE` log (2 November: X's quiz room
called 25 natives on arrival, the mod 11, ORAS_ENGINE.md 0.0).

  trace_natives.py <modules.bin> <table lo> <table hi> <runtime base> <trace.txt>

modules.bin: the CROs linked at 0x10000000 (cro_link.py). [table lo, table hi): the linked addresses of a module's
native table ({char *name, function} pairs; amx_natives.py tables lists them). runtime base: where that module's
linked base runs in the game (DllField: Omega Ruby 0x10242000 -> 0x6F3000, X 0x101F9000 -> 0x6A5000), given as the
offset to add, runtime minus linked. Prints the natives whose function starts a traced block.
"""
import re
import struct
import sys

BASE = 0x10000000


def natives(modules, lo, hi):
    m = open(modules, 'rb').read()
    out = {}
    for o in range(lo - BASE, hi - BASE - 8, 4):
        p, f = struct.unpack_from('<II', m, o)
        if BASE <= p < BASE + len(m):
            s = m[p - BASE:p - BASE + 48].split(b'\0')[0]
            if 3 <= len(s) < 40 and re.fullmatch(rb'_?[A-Z][A-Za-z0-9_]+', s):
                out[f] = s.decode()
    return out


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    modules, lo, hi, delta, trace = sys.argv[1], *(int(v, 0) for v in sys.argv[2:5]), sys.argv[5]
    table = natives(modules, lo, hi)
    hit = {int(l.split()[0], 16) for l in open(trace) if l.strip()}
    names = sorted({table[f] for f in table if (f + delta) & 0xFFFFFFFF in hit})
    print(f'{len(table)} natives in the table, {len(names)} called')
    print(' '.join(names))


if __name__ == '__main__':
    main()
