#!/usr/bin/env python3
"""Checks every NVN slot of the Mali Scarlet module (scarlet_mali/source/nvn_slots.hpp) before a build.

For each Slot{variable, got}: main.elf (tools/switch/nso2elf.py's output) must hold an R_AARCH64_RELATIVE
relocation at `got` whose addend is `variable`, and the loader's name table (Ghidra address and NVN function
name per line, Ghidra address = offset + 0x100000) gives the function the variable holds, printed for review.
Probe v9 shipped two slots 0x100 off that swapped the game's stencil functions (3 November); this catches that.

Usage: check_nvn_slots.py main.elf names.txt scarlet_mali/source/nvn_slots.hpp   (exit 1 if a slot is wrong)
"""
import re
import struct
import sys

GHIDRA_BASE = 0x100000
R_AARCH64_RELATIVE = 0x403


def main(elf_path, names_path, slots_path):
    data = open(elf_path, 'rb').read()
    relocations = {}
    for offset in range(0, len(data) - 24, 8):
        where, info, addend = struct.unpack_from('<QQq', data, offset)
        if info == R_AARCH64_RELATIVE:
            relocations[where] = addend
    names = {}
    for line in open(names_path):
        fields = line.split()
        if len(fields) >= 2:
            names[int(fields[0], 16) - GHIDRA_BASE] = fields[1]
    wrong = 0
    slots = re.findall(r'Slot (k\w+)\{(0x[0-9a-fA-F]+), (0x[0-9a-fA-F]+)\}', open(slots_path).read())
    for name, variable, got in slots:
        variable, got = int(variable, 16), int(got, 16)
        ok = relocations.get(got) == variable
        wrong += not ok
        print(f"{'OK ' if ok else 'BAD'} {name}: {names.get(variable, 'not in the name table')}")
    return 1 if wrong or not slots else 0


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    sys.exit(main(*sys.argv[1:]))
