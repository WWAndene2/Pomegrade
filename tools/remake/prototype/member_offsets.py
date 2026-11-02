#!/usr/bin/env python3
"""Which member of an Omega Ruby class matches a member of X's, by how the code uses it (2 November: the field's
module object X +0x198 = Omega Ruby +0x1A0, its player X +0x158 = +0x164; OrasXyGymModules.cpp's CopyWords).

  member_offsets.py <work dir> <x lo> <x hi> <oras lo> <oras hi> --x OFF... --oras OFF...

<work dir>: xlinked/modules.bin and linked/modules.bin (cro_link.py, from 0x10000000); [lo, hi): the linked code to
scan in each (DllField: X 0x101F9000-0x10300000, Omega Ruby 0x10242000-0x10350000). For each offset, every
`ldr rX, [rY, #OFF]` and the offsets then read from rX within five instructions: two members holding the same class show
the same offsets read from them, in about the same proportions. Inferred, not proof: check a match in the code.
"""
import argparse
import collections

import capstone
from capstone.arm import ARM_OP_MEM

BASE = 0x10000000


def disasm(path, lo, hi):
    d = open(path, 'rb').read()
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md.detail = True
    out = []
    for a in range(lo, hi, 4):
        one = list(md.disasm(d[a - BASE:a - BASE + 4], a, 1))
        out.append(one[0] if one else None)
    return out


def signature(ins, offsets):
    then = {o: collections.Counter() for o in offsets}
    count = collections.Counter()
    for i, x in enumerate(ins):
        if not x or x.mnemonic != 'ldr' or len(x.operands) != 2:
            continue
        m = x.operands[1]
        if m.type != ARM_OP_MEM or m.mem.disp not in offsets or m.mem.index != 0:
            continue
        r = x.operands[0].reg
        count[m.mem.disp] += 1
        for y in ins[i + 1:i + 6]:
            if y and len(y.operands) >= 2 and y.operands[-1].type == ARM_OP_MEM and y.operands[-1].mem.base == r:
                then[m.mem.disp][y.operands[-1].mem.disp] += 1
                break
    return count, then


def main():
    num = lambda s: int(s, 0)
    ap = argparse.ArgumentParser()
    ap.add_argument('work')
    for k in ('x_lo', 'x_hi', 'oras_lo', 'oras_hi'):
        ap.add_argument(k, type=num)
    ap.add_argument('--x', type=num, nargs='+', required=True)
    ap.add_argument('--oras', type=num, nargs='+', required=True)
    a = ap.parse_args()
    for game, path, lo, hi, offs in (('X', 'xlinked', a.x_lo, a.x_hi, a.x), ('ORAS', 'linked', a.oras_lo, a.oras_hi, a.oras)):
        count, then = signature(disasm(f'{a.work}/{path}/modules.bin', lo, hi), offs)
        for o in offs:
            print(game, hex(o), count[o], [(hex(k), v) for k, v in then[o].most_common(6)])


if __name__ == '__main__':
    main()
