#!/usr/bin/env python3
"""X/Y's own field code: the functions of X's DllField that Omega Ruby has no copy of, grouped by what calls them
(30 October, DEVELOPMENT_NOTE.md 3e working order step 2: the static search for X/Y's features run in engine code, so
the list of what is left to port is whole before building).

  xy_only_code.py <work> [--min N] [--names TSV]

<work> laid out by headless/session_setup.sh (as xy_code_compare.py): dumps/code.bin, dumps/xcode.bin, linked/ and
xlinked/ (prototype/cro_link.py), functions.tsv and x/functions.tsv, x/edges.tsv (ghidra/Export.java, CallGraph.java).

How: each X DllField function (0x101F9000-0x10338000) is read as its instructions' shapes (condition, class and opcode;
registers, constants and offsets left out: the compilers allocate registers apart, schedule apart, and the field's
members moved, +0x50 -> +0x60, prototype/xy_module_layout.tsv); its five nearest Omega Ruby functions (any linked
module) by the shapes' counts are aligned with it (difflib), and a ratio of 0.75 or more is a copy
(a function of fewer than 3 instructions is counted as shared). The others are X's own. They are grouped by X's call
graph into clusters (an X-own function and the X-own functions it calls); a cluster's root is a member no other member calls, and its entry says how the game reaches it:
"called by <X function>" from shared code (the hook a port needs), "data" (its address is a word of X's modules: a
vtable, a native's table, a callback; a vtable's C++ class named from its type info), or "unreached" (neither found). Printed: one line per cluster of at least N
functions (default 1), largest first: root, functions, instructions, entry; then the totals. The script natives among a cluster's
functions are named (X's {name, function} tables found in its modules). --names: a TSV of `address<TAB>name` (X
addresses) printed beside the roots (the ones read so far).

A lead to read, not a proof: a function can match a different one of the same shape; a copy changed beyond 0.75 is
counted as X's own; the shapes hide registers, constants and offsets.
"""
import collections
import struct
import sys

LINKED, CODE = 0x10000000, 0x100000
X_FIELD = (0x101F9000, 0x10338000)
WINDOW = 24


def masked(w):
    """the instruction's shape: condition, class and opcode bits (27-20, 7-4); registers and constants left out (the
    two compilers allocate registers apart and the field's members moved)"""
    return masked_strict(w) & 0xFFF000F0


def masked_strict(w):
    if (w & 0x0E000000) == 0x0A000000:      # b / bl: offset
        return w & 0xFF000000
    if (w & 0xFE000000) == 0xFA000000:      # blx immediate
        return w & 0xFE000000
    if (w & 0x0C000000) == 0x04000000 and w >> 28 != 0xF:  # ldr / str / ldrb / strb, immediate or pc-relative: 12 bits
        return w & 0xFFFFF000 if not (w >> 25) & 1 else w
    if (w & 0x0E000090) == 0x00000090 and (w >> 22) & 1 and (w & 0x60):  # ldrh / strh / ldrsb / ldrsh / ldrd: 8 bits
        return w & 0xFFFFF0F0
    if (w & 0x0E000E00) == 0x0C000A00:      # vldr / vstr: 8 bits
        return w & 0xFFFFFF00
    return w


def key(data, base, at):
    off = at - base
    if off < 0 or off + 4 > len(data):
        return None
    words = []
    for k in range(WINDOW):
        if off + 4 * k + 4 > len(data):
            break
        w = struct.unpack_from('<I', data, off + 4 * k)[0]
        words.append(masked(w))
        if w == 0xE12FFF1E or (w & 0xFFFF8000) == 0xE8BD8000:
            break
    return tuple(words)


def shapes(data, at, size, base=LINKED):
    """the function's instructions as shapes (masked), at most 400"""
    off = at - base
    return [masked(struct.unpack_from('<I', data, off + 4 * k)[0]) for k in range(min(size, 400)) if off + 4 * k + 4 <= len(data)]


DIMENSIONS = 4096


def bucket(shape):
    """a shape's place among the counts: its opcode bits (27-20) and bits 7-4, 4096 places (the condition left out)"""
    return ((shape >> 20) & 0xFF) << 4 | (shape >> 4) & 0xF


class Twins:
    """Omega Ruby's functions (given as {address: instructions}, read from blobs [(data, base), ...]) searched for the
    copy of an X function: the 5 nearest by their shapes' counts (cosine 0.8), aligned (difflib)"""
    def __init__(self, ofun, blobs):
        import numpy as np
        self.np = np
        self.addresses = sorted(ofun)
        self.shapes = []
        for a in self.addresses:
            data, base = next((d, b) for d, b in blobs if b <= a < b + len(d))
            self.shapes.append(shapes(data, a, ofun[a], base))
        matrix = np.zeros((len(self.addresses), DIMENSIONS), dtype=np.float32)
        for i, sh in enumerate(self.shapes):
            for w in sh:
                matrix[i, bucket(w)] += 1
        norms = np.linalg.norm(matrix, axis=1)
        norms[norms == 0] = 1
        self.matrix = matrix / norms[:, None]

    def best(self, sh):
        """(Omega Ruby's address, ratio) of the best aligned of the 5 nearest, or (None, 0)"""
        import difflib
        np = self.np
        vector = np.zeros(DIMENSIONS, dtype=np.float32)
        for w in sh:
            vector[bucket(w)] += 1
        vector /= np.linalg.norm(vector) or 1
        scores = self.matrix @ vector
        found = (None, 0.0)
        for i in np.argsort(-scores)[:5]:
            other = self.shapes[i]
            if scores[i] < 0.8 or not other or not 0.6 <= len(other) / len(sh) <= 1.6:
                continue
            ratio = difflib.SequenceMatcher(None, sh, other, autojunk=False).ratio()
            if ratio > found[1]:
                found = (self.addresses[i], ratio)
        return found


TWINS = {}


def twin(xdata, at, size, odata, ofun):
    """whether Omega Ruby's modules hold a copy of X's function (Twins, a ratio of 0.75 or more); a function of fewer
    than 3 instructions is too short to tell, counted as shared"""
    if 'twins' not in TWINS:
        TWINS['twins'] = Twins(ofun, [(odata, LINKED)])
    sh = shapes(xdata, at, size)
    if len(sh) < 3:
        return True
    return TWINS['twins'].best(sh)[1] >= 0.75


def class_of(data, slot):
    """the C++ class whose vtable holds this slot, by its type info (the word before a vtable's first slot points at
    {vptr, char *mangled name}): the nearest such word up to 64 words back, its name ("N5field10PlayerGridE"), or None"""
    def word(a):
        return struct.unpack_from('<I', data, a - LINKED)[0] if LINKED <= a < LINKED + len(data) - 3 else None

    for k in range(0, 256, 4):
        info = word(slot - k)
        name_at = word(info + 4) if info is not None else None
        if name_at is None or not LINKED <= name_at < LINKED + len(data):
            continue
        raw = data[name_at - LINKED:name_at - LINKED + 96].split(b'\0')[0]
        if len(raw) > 4 and raw[:1] in b'N0123456789' and all(32 < c < 127 for c in raw):
            return raw.decode()
    return None


def functions(path):
    """each function's start and its size in instructions (the TSV's second column is its bytes, Ghidra's body)"""
    out = {}
    for line in open(path):
        parts = line.split()
        out[int(parts[0], 16)] = (int(parts[1]) if len(parts) > 1 else 0) // 4
    return out


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    work = args[0]
    minimum = int(args[args.index('--min') + 1]) if '--min' in args else 1
    names = {}
    if '--names' in args:
        for line in open(args[args.index('--names') + 1]):
            if line.strip() and not line.startswith('#'):
                a, n = line.rstrip('\n').split('\t', 1)
                names[int(a, 16)] = n
    olinked = open(f'{work}/linked/modules.bin', 'rb').read()
    xlinked = open(f'{work}/xlinked/modules.bin', 'rb').read()
    xfun = {a: n for a, n in functions(f'{work}/x/functions.tsv').items() if X_FIELD[0] <= a < X_FIELD[1]}
    ofun = {a: n for a, n in functions(f'{work}/functions.tsv').items() if a >= LINKED}
    own = {a for a in xfun if not twin(xlinked, a, xfun[a], olinked, ofun)}
    calls, callers = collections.defaultdict(set), collections.defaultdict(set)
    starts = sorted(functions(f'{work}/x/functions.tsv'))
    import bisect

    def function_of(address):
        i = bisect.bisect_right(starts, address) - 1
        return starts[i] if i >= 0 else None

    for line in open(f'{work}/x/edges.tsv'):
        a, b = (int(p, 16) for p in line.split()[:2])
        a = function_of(a) if a not in xfun else a
        calls[a].add(b)
        callers[b].add(a)
    # the script natives' names: X's tables of {char *name, function} pairs (OrasXyNatives.h)
    for off in range(0, len(xlinked) - 7, 4):
        name_at, function = struct.unpack_from('<II', xlinked, off)
        if function in own and LINKED <= name_at < LINKED + len(xlinked) and function not in names:
            raw = xlinked[name_at - LINKED:name_at - LINKED + 48].split(b'\0')[0]
            if 3 <= len(raw) < 48 and raw.replace(b'_', b'').isalnum() and raw[:1].isalpha() or raw[:1] == b'_':
                names[function] = 'native ' + raw.decode()
    # words of X's modules pointing at a function (vtables, native tables, callbacks), and the C++ class of a vtable
    referenced = {}
    for off in range(0, len(xlinked) - 3, 4):
        w = struct.unpack_from('<I', xlinked, off)[0]
        if w in own and w not in referenced:
            referenced[w] = class_of(xlinked, LINKED + off)
    # clusters: union of an own function and the own functions it calls
    parent = {a: a for a in own}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for a in own:
        for b in calls[a]:
            if b in own:
                parent[find(a)] = find(b)
    clusters = collections.defaultdict(list)
    for a in own:
        clusters[find(a)].append(a)
    rows = []
    for members in clusters.values():
        mset = set(members)
        roots = [a for a in members if not (callers[a] & mset)] or [min(members)]
        entries = []
        for r in sorted(roots):
            outside = sorted(c for c in callers[r] if c not in mset)
            if outside:
                entries.append('0x%X called by %s' % (r, ' '.join('0x%X' % c for c in outside[:3]) + (' ...' if len(outside) > 3 else '')))
            elif r in referenced:
                entries.append('0x%X data' % r + (' (%s)' % referenced[r] if referenced[r] else ''))
            else:
                entries.append('0x%X unreached' % r)
            if r in names:
                entries[-1] += ' (%s)' % names[r]
        size = sum(xfun.get(a, 0) for a in members)
        natives = sorted(names[a][7:] for a in members if names.get(a, '').startswith('native '))
        if natives:
            entries.insert(0, '%d native(s): %s' % (len(natives), ' '.join(natives[:8]) + (' ...' if len(natives) > 8 else '')))
        rows.append((len(members), size, entries))
    rows.sort(key=lambda r: (-r[1], -r[0]))
    shown = 0
    for count, size, entries in rows:
        if count >= minimum:
            shown += 1
            print('%4d functions %6d instructions  %s' % (count, size, '; '.join(entries[:4]) + (' ...' if len(entries) > 4 else '')))
    print('X DllField: %d functions, %d with a copy in Omega Ruby, %d X\'s own in %d clusters (%d shown), %d instructions'
          % (len(xfun), len(xfun) - len(own), len(own), len(clusters), shown, sum(xfun.get(a, 0) for a in own)))


if __name__ == '__main__':
    main()
