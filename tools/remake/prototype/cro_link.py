#!/usr/bin/env python3
"""Every code module of the game linked at once, for Ghidra (ORAS_ENGINE.md 1): each CRO placed at its own address from
0x10000000 (0x1000-aligned, in the order given), its internal relocations applied there and its imports resolved to the
.code (loaded at 0x100000), to the static module or to the other modules, as Azahar's loader does
(azahar/src/core/hle/service/ldr_ro/cro_helper.cpp). The game itself maps a module wherever its heap gives room (DllField at
0x6F3000 on the field): these addresses are this tool's, chosen so that all 145 coexist in one program.

  cro_link.py <out dir> <static.crs> <module.cro>...

static.crs is the program's own module table: its segments are the .code's absolute addresses (code 0x100000, rodata
0x57A000, data 0x5EC000), which the modules' imports from "|static|" name by segment and offset.

Writes <out dir>/modules.bin (the linked image from 0x10000000) and <out dir>/modules.tsv (name, base, size, then one line
per named or indexed export: name, address). Branch relocations (types 10, 28, 29: left unimplemented by Azahar) are left
as the file has them and counted.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cro_dis import Cro  # noqa: E402

BASE = 0x10000000


def main():
    out, static, paths = sys.argv[1], Cro(sys.argv[2]), sys.argv[3:]
    mods = [Cro(p) for p in paths]
    base = {}
    at = BASE
    for m in mods:
        base[m.name] = at
        end = max(off + size for off, size, _ in m.segments)
        at = (at + max(end, len(m.data)) + 0xFFF) & ~0xFFF
    by_name = {m.name: m for m in mods}
    image = bytearray(at - BASE)
    unresolved = branches = 0

    def put(m, target, value):
        struct.pack_into("<I", m.out, target, value & 0xFFFFFFFF)

    def relocate(m, target, kind, value):
        """a relocation of kind at the module's offset target, to the absolute address value"""
        nonlocal branches
        if kind in (2, 38):
            put(m, target, value)
        elif kind == 3:
            put(m, target, value - (base[m.name] + target))
        else:
            branches += 1

    for m in mods:
        m.out = bytearray(m.data)
        m.out.extend(bytes(max(0, max(off + size for off, size, _ in m.segments) - len(m.out))))
        h = m.h
        # internal relocations, at this module's base (cro_dis applies them at base 0)
        for i in range(h["InternalRelocationNum"]):
            tag, kind, seg, _p1, _p2, addend = struct.unpack_from("<IBBBBI", m.data, h["InternalRelocationTableOffset"] + 12 * i)
            target = m.tag(tag)
            if target is None or seg >= len(m.segments):
                continue
            relocate(m, target, kind, base[m.name] + m.segments[seg][0] + addend)
        # imports by module: indexed exports and anonymous segment tags
        for k in range(h["ImportModuleNum"]):
            name_off, idx_off, idx_num, anon_off, anon_num = struct.unpack_from("<5I", m.data, h["ImportModuleTableOffset"] + 20 * k)
            module = m.cstr(name_off)
            other = by_name.get(module)
            entries = [(other.exports_indexed[index] if other and index < len(other.exports_indexed) else None, batch)
                       for index, batch in (struct.unpack_from("<II", m.data, idx_off + 8 * i) for i in range(idx_num))]
            for i in range(anon_num):
                tag, batch = struct.unpack_from("<II", m.data, anon_off + 8 * i)
                if module == "|static|":
                    entries.append(("code", static.tag(tag), batch))
                    continue
                entries.append((other.tag(tag) if other else None, batch))
            for entry in entries:
                if entry[0] == "code":
                    value, batch = entry[1], entry[2]
                else:
                    offset, batch = entry
                    value = base[module] + offset if offset is not None and module in base else None
                for target, kind, addend in m.batch(batch):
                    if target is None:
                        continue
                    if value is None:
                        unresolved += 1
                        continue
                    relocate(m, target, kind, value + addend)
        start = base[m.name] - BASE
        image[start:start + len(m.out)] = m.out

    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "modules.bin"), "wb").write(image)
    with open(os.path.join(out, "modules.tsv"), "w") as w:
        for m in mods:
            w.write(f"{m.name}\t0x{base[m.name]:X}\t0x{len(m.out):X}\n")
            for address, label in sorted(m.labels.items()):
                w.write(f"\t{label}\t0x{base[m.name] + address:X}\n")
    print(f"{len(mods)} modules linked from 0x{BASE:X} to 0x{at:X}; {unresolved} import relocations unresolved, "
          f"{branches} branch relocations left as stored")


if __name__ == "__main__":
    main()
