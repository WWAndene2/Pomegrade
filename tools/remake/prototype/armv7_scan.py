#!/usr/bin/env python3
"""ARM words the 3DS's ARM11 (ARMv6K) cannot run, in a file or a range of one (3 November: the mods' code patches used
movw / movt / ubfx, which Azahar's JIT runs and its interpreter stops at, "Decode failure"; ArmCode.h has the ARMv6K way).

  armv7_scan.py <file> [offset length]   each ARMv7-only instruction (movw, movt, ubfx, sbfx, bfi, bfc, rbit, mls)

Data read as code gives false hits: scan code, e.g. a cave's words (oras-engine's log names them) or a module's code
segment. Needs capstone.
"""
import sys

import capstone

ARMV7 = {'movw', 'movt', 'ubfx', 'sbfx', 'bfi', 'bfc', 'rbit', 'mls'}


def main():
    if len(sys.argv) not in (2, 4):
        sys.exit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    start, length = (int(sys.argv[2], 0), int(sys.argv[3], 0)) if len(sys.argv) == 4 else (0, len(data))
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    hits = 0
    for o in range(start, min(start + length, len(data)) - 3, 4):
        for i in md.disasm(data[o:o + 4], o):
            if i.mnemonic.split('.')[0] in ARMV7:
                print(f'{o:#x} {i.mnemonic} {i.op_str}')
                hits += 1
    print(f'{hits} ARMv7-only word(s)')


if __name__ == '__main__':
    main()
