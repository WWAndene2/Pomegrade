#!/usr/bin/env python3
"""Fly opens the soaring mode (point 38 of Delta_Emerald_Development.md): a code.ips that Azahar applies from the mod folder
(load/mods/000400000011C400/code.ips, ncch_container.cpp ApplyCodePatch).

  fly_soaring_ips.py <code.bin> <out code.ips>

The party menu's return function 0x3ef7b8, on action 3 with code 0x13 (Fly) and once its check 0x3dc534 passed, builds the
Fly event at 0x3ef858. The patch puts there what the bag's return function 0x3ef000 does for the Eon Flute: the soaring
event's constructor 0x3d19f8(*(param_2 + 0x18), param_1), then *param_1 + 0x1d = 3 (an event runs), then the function's own
return (its pop, read at 0x3ef820). Checked headless on 11 October: Fly, the town map, then the soaring mode (the map's
destination is not used). <code.bin>: the game's ExeFS code, decompressed (remake_tool oras-code), loaded at 0x100000.
"""
import struct
import sys

BASE = 0x100000
AT = 0x3EF858


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    code = open(sys.argv[1], "rb").read()
    word = lambda a: struct.unpack_from("<I", code, a - BASE)[0]
    # the bytes replaced must be the game's (the Fly event's construction): refuse another version or an already patched file
    if word(AT) != 0xE5949018 or word(0x3EF840) != 0xE594001C:
        sys.exit("unexpected code at 0x3ef858: not the code this patch was made on (the owner's Omega Ruby), or already patched")
    bl = lambda pc, target: 0xEB000000 | (((target - (pc + 8)) >> 2) & 0xFFFFFF)
    ins = [0xE5940018,              # ldr r0, [r4, #0x18]
           0xE1A01006,              # mov r1, r6
           bl(AT + 8, 0x3D19F8),    # bl the soaring event's constructor
           0xE5960000,              # ldr r0, [r6]
           0xE3A01003,              # mov r1, #3
           0xE5C0101D,              # strb r1, [r0, #0x1d]
           word(0x3EF820)]          # pop {r4-sl, pc}, the function's return
    data = b"".join(struct.pack("<I", i) for i in ins)
    off = AT - BASE
    open(sys.argv[2], "wb").write(b"PATCH" + off.to_bytes(3, "big") + len(data).to_bytes(2, "big") + data + b"EOF")
    print(f"{sys.argv[2]}: {len(data)} bytes at 0x{AT:X}")


if __name__ == "__main__":
    main()
