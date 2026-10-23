#!/usr/bin/env python3
"""The archive member a RomFS read lands in: from the offsets of the headless runs' read trace (POMEGRADE_ROMFS_TRACE,
headless/fs_open_trace.inc) to "a/0/3/3 member 2", to know which file the game loaded at a given moment (ORAS_ENGINE.md
0.1: the battle intro's layout and name textures were found this way, 22 October).

  romfs_member.py <game.3ds> <offset>... [--romfs-base OFFSET]
  romfs_member.py <game.3ds> --log <run>/log.txt [--after FRAME] [--romfs-base OFFSET]
      every ROMFS_READ line of a run's log (23 October: listed one by one, a battle's reads ran to 800 lines), grouped:
      each archive with the members read and how many reads; --after skips the lines before the first "at frame N" >= FRAME
      (a shot's line), to see what a moment loads. Files the mod gives (romfs/, romfs_ext/) are not read from the image,
      so not traced: test with the game's own file to see its reads

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


def summary(mm, log, after, base):
    import re
    reads, started = [], after == 0
    for line in open(log, errors="replace"):
        if not started:
            m = re.search(r"at frame (\d+)", line)
            started = bool(m and int(m.group(1)) >= after)
            continue
        m = re.match(r"ROMFS_READ (\d+) (\d+)", line)
        if m:
            reads.append(int(m.group(1)))
    archives = {}
    for off in reads:
        hit = member(mm, off + base)
        key = hit[0] if hit else None
        a = archives.setdefault(key, {"count": hit[1] if hit else 0, "reads": 0, "members": set()})
        a["reads"] += 1
        if hit and hit[2] is not None:
            a["members"].add(hit[2])
    for key, a in sorted(archives.items(), key=lambda kv: -kv[1]["reads"]):
        where = f"GARC at image 0x{key:X} ({a['count']} members)" if key is not None else "outside any GARC"
        members = sorted(a["members"], key=lambda m: (len(str(m)), str(m)))
        shown = ", ".join(str(m) for m in members[:20]) + (f" ... ({len(members)})" if len(members) > 20 else "")
        print(f"{a['reads']:6} reads  {where}" + (f": members {shown}" if members else ": header only"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("game")
    ap.add_argument("offsets", nargs="*", type=lambda v: int(v, 0))
    ap.add_argument("--log")
    ap.add_argument("--after", type=int, default=0)
    ap.add_argument("--romfs-base", type=lambda v: int(v, 0), default=0x344000, help="Omega Ruby's by default")
    a = ap.parse_args()
    with open(a.game, "rb") as f:
        mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        if a.log:
            summary(mm, a.log, a.after, a.romfs_base)
            return
        for off in a.offsets:
            hit = member(mm, off + a.romfs_base)
            if hit is None or hit[2] is None:
                print(f"0x{off:X}: in no archive member" + (f" (after the GARC at image 0x{hit[0]:X})" if hit else ""))
            else:
                garc, count, index, length, into = hit
                print(f"0x{off:X}: GARC at image 0x{garc:X} ({count} members), member {index} ({length} bytes), 0x{into:X} into it")


if __name__ == "__main__":
    main()
