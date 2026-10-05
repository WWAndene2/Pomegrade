#!/usr/bin/env python3
"""The mods of the phone test bench (the app's N3dsModBench) that look for the code bounding an overworld map piece to about
1 MiB (ORAS_LITTLEROOT.md 10, item 5: a piece of 1,044,480 bytes shows, one of 1,052,672 hangs the field).

  bench_mods.py <code.bin> <p1 mod dir> <p2 mod dir> <out bench dir>

code.bin: `remake_tool oras-code` (the .code, decompressed, at 0x100000). p1 / p2 mod dirs: load/mods/<title>/ of the two padded
v24 mods (romfs_ext/...). Candidates: every instruction of the .code that puts 0x100000 in a register, `mov rX, #0x100000` or
an `ldr` from a literal pool word holding it (flag tests, tst/orr/bic/and with that bit, are not sizes and are left out). Each
is doubled to 0x200000 by an IPS patch of the code (Azahar applies load/mods/<title>/exefs/code.ips to the decompressed code).

Bench mods, run in name order (results: one line each in results.txt on the phone):
  00_p1           p1 alone: must show the field (the bench works)
  01_p2           p2 alone: must not (the bound holds)
  02_p2_all       p2 with every candidate doubled: the field means the bound is among them
  1k_p2_bitK      p2 with the candidates whose number has bit K set   } the field in one of each pair gives bit K of the
  2k_p2_notK      p2 with the candidates whose number has bit K clear } bounding candidate's number (candidates.txt)
Several candidates doubled at once may break the game for another reason: a pair with no field on either side says so.
"""
import os
import shutil
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_REG_PC

BASE = 0x100000
OLD, NEW = 0x100000, 0x200000


def candidates(code):
    md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
    md.detail = True
    sites = {}  # code offset -> (old word, new word, what)
    for off in range(0, len(code) - 3, 4):
        ins = next(md.disasm(code[off:off + 4], BASE + off), None)
        if ins is None:
            continue
        name = ins.mnemonic
        if name.startswith("mov") and not name.startswith("movs") and len(ins.operands) == 2 and ins.operands[1].type == ARM_OP_IMM \
                and (ins.operands[1].imm & 0xFFFFFFFF) == OLD:
            word = struct.unpack_from("<I", code, off)[0]
            # ARM data-processing immediate: 8-bit value rotated; 0x100000 is 1 rotated, 0x200000 is 2 with the same rotation
            if word & 0xFF != 1:
                continue
            new = (word & ~0xFF) | 2
            check = next(md.disasm(struct.pack("<I", new), BASE + off), None)
            if check is None or (check.operands[1].imm & 0xFFFFFFFF) != NEW:
                continue
            sites[off] = (word, new, f"0x{BASE + off:X} {name} {ins.op_str}")
        elif name.startswith("ldr") and len(ins.operands) == 2 and ins.operands[1].type == ARM_OP_MEM \
                and ins.operands[1].mem.base == ARM_REG_PC:
            pool = off + 8 + ins.operands[1].mem.disp
            if 0 <= pool <= len(code) - 4 and struct.unpack_from("<I", code, pool)[0] == OLD:
                what = f"pool 0x{BASE + pool:X} (loaded by {name} {ins.op_str} at 0x{BASE + off:X})"
                if pool in sites:
                    what = sites[pool][2] + f", 0x{BASE + off:X}"
                sites[pool] = (OLD, NEW, what)
    return [(off, *sites[off]) for off in sorted(sites)]


def ips(patches):
    out = bytearray(b"PATCH")
    for off, _, new, _ in patches:
        if off >= 0x1000000 or off == 0x454F46:  # IPS offsets are 24-bit; "EOF" would end the file early
            raise SystemExit(f"offset 0x{off:X} cannot be written as IPS")
        out += struct.pack(">I", off)[1:] + struct.pack(">H", 4) + struct.pack("<I", new)
    return bytes(out + b"EOF")


def mod(out, name, base, patches):
    target = os.path.join(out, name)
    shutil.copytree(base, target)
    if patches:
        os.makedirs(os.path.join(target, "exefs"), exist_ok=True)
        open(os.path.join(target, "exefs", "code.ips"), "wb").write(ips(patches))


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    code = open(sys.argv[1], "rb").read()
    p1, p2, out = sys.argv[2:5]
    found = candidates(code)
    if not found:
        sys.exit("no candidate found")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    bits = max(1, (len(found) - 1).bit_length())
    with open(os.path.join(out, "candidates.txt"), "w") as f:
        for n, (off, old, new, what) in enumerate(found):
            f.write(f"{n} {n:0{bits}b} {what}\n")
    mod(out, "00_p1", p1, [])
    mod(out, "01_p2", p2, [])
    mod(out, "02_p2_all", p2, found)
    for k in range(bits):
        mod(out, f"1{k:02d}_p2_bit{k}", p2, [c for n, c in enumerate(found) if n >> k & 1])
        mod(out, f"2{k:02d}_p2_not{k}", p2, [c for n, c in enumerate(found) if not n >> k & 1])
    print(f"{len(found)} candidates, {bits} bits: {3 + 2 * bits} bench mods in {out} (candidates.txt maps a number to its code)")


if __name__ == "__main__":
    main()
