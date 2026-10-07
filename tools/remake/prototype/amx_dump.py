#!/usr/bin/env python3
"""The Pawn (AMX) scripts in a memory dump of the running game, their contexts and their native call frames
(ORAS_ENGINE.md 6: how TalkMdlMsg_Seq's first argument was read live, without the flaky GDB stub).

Take the dump while the thing to read is on screen (retro_host script command, the application heap):
  run_local.sh <work> <name> - "<zone x z>" "...; dump 0x08000000 0x6000000 heap.bin" 300
then:
  amx_dump.py <heap.bin> [--base 0x08000000] [--count N] [--script SIZE]
- every script image: the AMX header (magic 0xF1E0, defsize 8) with its size and sections;
- the contexts pointing to it (+4 holds the image; +0x98 the native mask, +0x48 the context itself once loaded by
  Script_LoadFieldScript, +0x88 the _Suspend wait);
- in its data section, every call frame of N cells (--count, in bytes as Pawn pushes it: 76 for TalkMdlMsg_Seq's 19
  arguments): a word equal to N followed by the arguments, first argument first. A native's frame stays in the script's
  stack after the call; its first argument is the deepest, so later shorter calls leave it intact.
--script keeps only the images of that size (Littleroot's zone script: 6438).
"""
import argparse
import struct


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("--base", type=lambda v: int(v, 0), default=0x08000000)
    ap.add_argument("--count", type=lambda v: int(v, 0), default=76)
    ap.add_argument("--script", type=int, default=0)
    a = ap.parse_args()
    b = open(a.dump, "rb").read()
    n = len(b) // 4
    words = struct.unpack(f"<{n}I", b[:n * 4])
    signed = lambda x: x - (1 << 32) if x & 0x80000000 else x

    images = []
    at = b.find(b"\xe0\xf1")
    while at >= 0:
        if (at - 4) % 4 == 0 and at >= 4:
            size, magic, fver, aver, flags, defsize, cod, dat, hea, stp = struct.unpack_from("<IHBBHHIIII", b, at - 4)
            if defsize == 8 and 0 < cod < dat <= hea < stp < 0x200000 and (not a.script or size == a.script):
                images.append((a.base + at - 4, size, flags, cod, dat, hea, stp))
        at = b.find(b"\xe0\xf1", at + 1)

    for base, size, flags, cod, dat, hea, stp in images:
        print(f"script image 0x{base:X}: size {size}, flags 0x{flags:X}, code 0x{cod:X}, data 0x{dat:X}, heap 0x{hea:X}, "
              f"stack top 0x{stp:X}")
        for i, w in enumerate(words):
            if w == base and i >= 1:
                ctx = a.base + 4 * (i - 1)
                field = lambda off: words[(ctx - a.base + off) // 4] if 0 <= ctx - a.base + off < 4 * n else None
                print(f"  context 0x{ctx:X}: mask (+0x98) 0x{field(0x98):X}, self (+0x48) "
                      f"{'yes' if field(0x48) == ctx else 'no'}, wait (+0x88) {field(0x88)}")
        cells = a.count // 4
        start, end = base - a.base + dat, min(base - a.base + stp, 4 * n - 4 * (cells + 1))
        for off in range(start, end, 4):
            if words[off // 4] == a.count:
                args = [signed(words[off // 4 + 1 + k]) for k in range(cells)]
                print(f"  frame at data + 0x{off - (base - a.base) - dat:X}: {args}")


if __name__ == "__main__":
    main()
