#!/usr/bin/env python3
"""Where ORAS's code uses a value or a string: the leads to the code that sizes the overworld piece buffer
(ORAS_LITTLEROOT.md 10, item 5: about 1 MiB a piece, measured by phone runs p1 and p2).

  code_find.py <code.bin> [--imm VALUE]... [--string TEXT]... [--at ADDRESS]... [--context N]

code.bin: `remake_tool oras-code` (the ExeFS .code, decompressed, loaded at 0x100000). The code is swept as ARM (32-bit
instructions; ORAS's code is ARM, its data pools read as junk instructions and are skipped by what they match):
  --imm      every instruction with that immediate operand, and every LDR from a literal pool word holding it
  --string   every place the text lies, and every literal pool word holding its address (the code that loads it)
  --at       the instructions around an address (to read a hit's function)
Each hit is printed with N instructions of context before and after (default 8). Needs capstone (pip install capstone).
"""
import argparse
import struct

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_REG_PC

BASE = 0x100000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("code")
    ap.add_argument("--imm", action="append", default=[], type=lambda v: int(v, 0))
    ap.add_argument("--string", action="append", default=[])
    ap.add_argument("--at", action="append", default=[], type=lambda v: int(v, 0))
    ap.add_argument("--context", type=int, default=8)
    ap.add_argument("--max", type=int, default=60, help="hits printed per value")
    a = ap.parse_args()
    code = open(a.code, "rb").read()
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    md.detail = True

    def word(addr):
        off = addr - BASE
        return struct.unpack_from("<I", code, off)[0] if 0 <= off <= len(code) - 4 else None

    def show(addr, why):
        start = max(BASE, addr - 4 * a.context)
        end = min(BASE + len(code), addr + 4 * (a.context + 1))
        print(f"-- {why} at 0x{addr:X}")
        for off in range(start, end, 4):
            ins = next(md.disasm(code[off - BASE:off - BASE + 4], off), None)
            mark = ">" if off == addr else " "
            print(f"{mark} 0x{off:X}: {ins.mnemonic + ' ' + ins.op_str if ins else '(data) %08X' % word(off)}")

    # one sweep: immediates and literal loads
    wanted = set(a.imm)
    hits = {v: [] for v in wanted}
    for off in range(0, len(code) - 3, 4):
        ins = next(md.disasm(code[off:off + 4], BASE + off), None)
        if ins is None:
            continue
        for op in ins.operands:
            if op.type == ARM_OP_IMM and (op.imm & 0xFFFFFFFF) in wanted:
                hits[op.imm & 0xFFFFFFFF].append((ins.address, f"{ins.mnemonic} {ins.op_str}"))
            if op.type == ARM_OP_MEM and op.mem.base == ARM_REG_PC and ins.mnemonic.startswith("ldr"):
                pool = ins.address + 8 + op.mem.disp
                v = word(pool)
                if v in wanted:
                    hits[v].append((ins.address, f"{ins.mnemonic} {ins.op_str} (pool 0x{pool:X} = 0x{v:X})"))
    for v in a.imm:
        print(f"== value 0x{v:X}: {len(hits[v])} instructions")
        for addr, text in hits[v][:a.max]:
            show(addr, text)

    for text in a.string:
        needle = text.encode()
        places = []
        i = code.find(needle)
        while i >= 0:
            places.append(BASE + i)
            i = code.find(needle, i + 1)
        print(f"== string {text!r}: at {', '.join('0x%X' % p for p in places) or 'nowhere'}")
        for p in places:
            refs = [BASE + off for off in range(0, len(code) - 3, 4) if struct.unpack_from("<I", code, off)[0] == p]
            print(f"   0x{p:X}: pool words holding it at {', '.join('0x%X' % r for r in refs) or 'none'}")
            for r in refs[:a.max]:
                # the loads of that pool word (an LDR within 4 KB before it)
                for off in range(max(BASE, r - 4096), r, 4):
                    ins = next(md.disasm(code[off - BASE:off - BASE + 4], off), None)
                    if ins and ins.mnemonic.startswith("ldr") and any(o.type == ARM_OP_MEM and o.mem.base == ARM_REG_PC and off + 8 + o.mem.disp == r for o in ins.operands):
                        show(off, f"load of {text!r}")

    for addr in a.at:
        show(addr, "requested")


if __name__ == "__main__":
    main()
