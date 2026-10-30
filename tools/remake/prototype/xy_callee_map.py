#!/usr/bin/env python3
"""Omega Ruby's function for an X function, found through its callers (30 October, the gyms' ports: an import of X's
module whose own code has no copy in Omega Ruby, xy_import_match.py "none", "several" or "near", is often the same job
changed; what the two games' twin callers call at the same place answers it).

  xy_callee_map.py <work> X_ADDRESS...

<work> as xy_code_compare.py. For each X function: its callers in X (x/edges.tsv, a call through a module's stub
`ldr pc, [pc, #-4]` included), each caller's copy in Omega Ruby (xy_only_code.Twins over every Omega Ruby function,
its .code and linked modules: the best aligned of the 5 nearest by shapes, a ratio of 0.75 or more), the two aligned by
their mnemonics (xy_code_compare.aligned), and the Omega Ruby call paired with each call of the X function. Printed:
per X function, each Omega Ruby target and its votes (callers agreeing), then the callers read. A lead to read, not a
proof: one vote is one caller's alignment.
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xy_code_compare import Game, aligned  # noqa: E402
from xy_only_code import LINKED, CODE, Twins, functions, shapes  # noqa: E402


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    work = sys.argv[1]
    targets = [int(a, 16) for a in sys.argv[2:]]
    xg, og = Game(work, True), Game(work, False)
    xfun = functions(f'{work}/x/functions.tsv')
    ofun = functions(f'{work}/functions.tsv')
    twins = Twins(ofun, [(og.code, CODE), (og.linked, LINKED)])
    callers = collections.defaultdict(set)
    for line in open(f'{work}/x/edges.tsv'):
        a, b = (int(p, 16) for p in line.split()[:2])
        callers[b].add(a)
    # the stubs of X's modules: `ldr pc, [pc, #-4]` then the target
    stubs = collections.defaultdict(set)
    for off in range(0, len(xg.linked) - 7, 4):
        if struct.unpack_from('<I', xg.linked, off)[0] == 0xE51FF004:
            stubs[struct.unpack_from('<I', xg.linked, off + 4)[0]].add(LINKED + off)
    for target in targets:
        reached = {target} | stubs[target]
        votes, read = collections.Counter(), []
        for caller in sorted(set().union(*(callers[r] for r in reached))):
            if caller not in xfun:
                continue
            data, base = (xg.linked, LINKED) if caller >= LINKED else (xg.code, CODE)
            sh = shapes(data, caller, xfun[caller], base)
            if len(sh) < 3:
                continue
            twin, ratio = twins.best(sh)
            if twin is None or ratio < 0.75:
                read.append('0x%X (no copy)' % caller)
                continue
            _, pairs = aligned(xg, og, caller, twin, min(400, max(xfun[caller], 8)))
            found = set()
            for a, b in pairs:
                if a.mnemonic in ('bl', 'b', 'blx') and a.op_str.startswith('#') and b.op_str.startswith('#'):
                    if xg.target(int(a.op_str[1:], 16)) in reached:
                        found.add(og.target(int(b.op_str[1:], 16)))
            for f in found:
                votes[f] += 1
            read.append('0x%X -> 0x%X %.2f%s' % (caller, twin, ratio, '' if found else ' (call not aligned)'))
        print('X 0x%X: %s' % (target, ', '.join('0x%X x%d' % (f, n) for f, n in votes.most_common(4)) or 'nothing found'))
        print('    callers: ' + ('; '.join(read[:8]) + (' ...' if len(read) > 8 else '') if read else 'none in x/edges.tsv'))


if __name__ == '__main__':
    main()
