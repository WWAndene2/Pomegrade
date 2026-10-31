#!/usr/bin/env python3
"""What copying X functions into a ported gym module takes (31 October, Olympia's port; the session's plan.py, kept):
from ROOTS, the X functions they call that must be copied with them, and the calls left to map onto Omega Ruby.

  xy_copy_plan.py <work> MAPPED ROOTS

MAPPED: X=ORAS pairs already mapped, comma-separated hex (`1d4744=1F9544,2ff520=2FF8C0`; Olympia's are in the
DllUSGymEsper port of OrasXyGymModules.cpp). ROOTS: the X functions to copy, comma-separated hex. Reads x/edges.tsv
and x/functions.tsv (Ghidra's export, as xy_callee_map.py). Followed: calls into X's DllField (0x10200000-0x10400000)
and to other roots; left out: mapped calls and the module's own range (0x10783000-0x10787000, Olympia's). Printed:
each function to copy, its size in bytes (to the next function) and its calls still unmapped, then the calls outside
DllField to map.
"""
import bisect
import collections
import sys

FIELD = (0x10200000, 0x10400000)
MODULE = (0x10783000, 0x10787000)


def main(args):
    if len(args) != 3:
        print(__doc__)
        return 2
    work = args[0]
    edges = collections.defaultdict(set)
    for line in open(f"{work}/x/edges.tsv"):
        a, b = line.split()[:2]
        edges[int(a, 16)].add(int(b, 16))
    starts = sorted(int(line.split("\t")[0], 16) for line in open(f"{work}/x/functions.tsv"))
    mapped = {int(k, 16): int(v, 16) for k, v in (p.split("=") for p in args[1].split(",") if p)}
    roots = [int(a, 16) for a in args[2].split(",")]
    left_out = lambda c: c in mapped or MODULE[0] <= c < MODULE[1]
    seen, todo, outside = set(), list(roots), set()
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        for c in sorted(edges[f]):
            if left_out(c) or c in seen:
                continue
            if c in roots or FIELD[0] <= c < FIELD[1]:
                todo.append(c)
            else:
                outside.add(c)
    for f in sorted(seen):
        i = bisect.bisect_right(starts, f)
        size = (starts[i] if i < len(starts) else f) - f
        print(f"{f:#x} {size} " + " ".join(f"{c:#x}" for c in sorted(edges[f]) if not left_out(c) and c != f))
    print("to map:", " ".join(f"{c:#x}" for c in sorted(outside)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
