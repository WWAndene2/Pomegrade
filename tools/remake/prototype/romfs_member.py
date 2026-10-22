#!/usr/bin/env python3
"""The archive member a RomFS read lands in: from the offsets of the headless runs' read trace (POMEGRADE_ROMFS_TRACE,
headless/fs_open_trace.inc) to "a/0/3/3 member 2", to know which file the game loaded at a given moment (ORAS_ENGINE.md
0.1: the battle intro's layout and name textures were found this way, 22 October).

  romfs_member.py <game.3ds> <offset>... [--romfs-base OFFSET]

An offset of the trace plus the RomFS's place in the cartridge image is an image offset: 0x344000 for Omega Ruby (checked
on DllBattle.cro's read: 1,089,536 bytes at trace offset 0x2DAD0, the file at image 0x371AD0); another cartridge needs its
own (--romfs-base; find it the same way, from a file whose bytes are known). The archive holding the offset is the last
GARC ("CRAG") starting before it; its members are read from its FATB (each entry: a bit mask of sub-members, then
{start, end, length} per bit set, relative to the data offset at header +0x10).
"""
import argparse
import mmap
import struct


def member(mm, image):
    garc = mm.rfind(b"CRAG", 0, image + 1)
    if garc < 0:
        return None
    header = struct.unpack_from("<H", mm, garc + 4)[0]
    fato_size, count = struct.unpack_from("<IH", mm, garc + header + 4)
    data = struct.unpack_from("<I", mm, garc + 0x10)[0]
    q = garc + header + fato_size + 12
    for i in range(count):
        bits = struct.unpack_from("<I", mm, q)[0]
        q += 4
        for k in range(32):
            if bits >> k & 1:
                start, end, _ = struct.unpack_from("<III", mm, q)
                q += 12
                if garc + data + start <= image < garc + data + end:
                    return garc, count, i if k == 0 else f"{i}_{k}", end - start, image - (garc + data + start)
    return garc, count, None, 0, 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("game")
    ap.add_argument("offsets", nargs="+", type=lambda v: int(v, 0))
    ap.add_argument("--romfs-base", type=lambda v: int(v, 0), default=0x344000, help="Omega Ruby's by default")
    a = ap.parse_args()
    with open(a.game, "rb") as f:
        mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        for off in a.offsets:
            hit = member(mm, off + a.romfs_base)
            if hit is None or hit[2] is None:
                print(f"0x{off:X}: in no archive member" + (f" (after the GARC at image 0x{hit[0]:X})" if hit else ""))
            else:
                garc, count, index, length, into = hit
                print(f"0x{off:X}: GARC at image 0x{garc:X} ({count} members), member {index} ({length} bytes), 0x{into:X} into it")


if __name__ == "__main__":
    main()
