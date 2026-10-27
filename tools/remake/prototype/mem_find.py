#!/usr/bin/env python3
"""Searches of a memory dump taken by retro_host's `mem ADDRESS LENGTH` (its log's "mem XXXXXXXX: w w w ..." lines), to find
a value the game holds at run time (27 October, ORAS_ENGINE.md 0.0: X's start camera found as X's preset 36 in memory, the
player's model and its scale, the player's position copies to teleport it).

  mem_find.py words <log> HEX...            every place holding these words in a row (0x optional; `.` any word)
  mem_find.py floats <log> F...             the same, as floats (`.` any word)
  mem_find.py cameras <log>                 every camera record (0x44 bytes: field of view 16-32 at +0x18, a far plane of a
                                            whole 1000-6000 at +0x1C): flag, number, then height, pitch, ..., distance
  mem_find.py owners <log> ADDRESS [DEPTH]  who points at this address, and at the objects holding those pointers (an
                                            object: the nearest word below the pointer that is a class table, an address in
                                            .code or the linked modules, itself pointed at), DEPTH steps up (default 3)

Take the dump on the field, e.g. `...; mem 0x08000000 0x2000000` (32 MB of the heap, about 90 MB of log), and the same in
the other game or build to compare. The heap's layout is the same from run to run of one mod and save, so an address
found here can be poked (`poke ADDRESS VALUE`) in another run of it: a lead, to check by its effect.
"""
import re
import signal
import struct
import sys

signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # quiet when piped into head


def load(path):
    words = {}
    for line in open(path, errors='replace'):
        m = re.match(r'\s*mem ([0-9A-F]{8}): (.*)', line)
        if m:
            at = int(m.group(1), 16)
            for k, w in enumerate(m.group(2).split()):
                if w[0] != '-':
                    words[at + 4 * k] = int(w, 16)
    return words


def f32(w):
    return struct.unpack('<f', struct.pack('<I', w))[0]


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def find(words, pattern):
    first = next((k for k, p in enumerate(pattern) if p is not None), None)
    for at in sorted(words):
        if first is None or words[at] != pattern[first]:
            continue
        start = at - 4 * first
        if all(p is None or words.get(start + 4 * k) == p for k, p in enumerate(pattern)):
            yield start


def is_class_table(w):
    return 0x100000 <= w < 0x900000 or 0x10000000 <= w < 0x10900000


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        sys.exit(__doc__)
    mode, words = a[0], load(a[1])
    if mode in ('words', 'floats'):
        pattern = [None if v == '.' else (int(v, 16) if mode == 'words' else bits(float(v))) for v in a[2:]]
        for at in find(words, pattern):
            print('%08X' % at, ' '.join('%08X' % words.get(at + 4 * k, 0) for k in range(max(8, len(pattern)))))
    elif mode == 'cameras':
        for at in sorted(words):
            fov, far = f32(words[at]), f32(words.get(at + 4, 0))
            if words[at] in (bits(16), bits(18), bits(24), bits(32)) and 999 < far < 6001 and far.is_integer():
                r = at - 0x18
                head = words.get(r, 0)
                values = ' '.join('%.4g' % f32(words.get(r + 4 * k, 0)) for k in range(1, 12))
                print('%08X flag %d number %d: %s' % (r, head & 0xFFFF, head >> 16, values))
    elif mode == 'owners':
        target, depth = int(a[2], 16), int(a[3]) if len(a) > 3 else 3
        back = {}
        for at, v in words.items():
            back.setdefault(v, []).append(at)
        level = [target]
        for step in range(depth):
            nxt = []
            for t in level:
                for holder in back.get(t, []):
                    base = next((b for b in range(holder, holder - 0x1000, -4) if is_class_table(words.get(b, 0)) and back.get(b)), None)
                    where = ('object %08X (class %08X) + 0x%X' % (base, words[base], holder - base)) if base is not None else 'no object found'
                    print('  ' * step + '%08X held at %08X: %s' % (t, holder, where))
                    if base is not None and base not in nxt:
                        nxt.append(base)
            level = nxt
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
