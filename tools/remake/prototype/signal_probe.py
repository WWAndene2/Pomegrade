#!/usr/bin/env python3
"""Continuity test (owner's request, 8 October): signals put in at one end, looked for at the other, to see where the game
carries data without reading the code that does it. A signal is a 4-byte value unique to one place of the source; one run
can carry many, each telling its own place.

  signal_probe.py inject <in> <out> <map.tsv> PLACE...
        writes one signal at each PLACE of a copy of <in>, and the signals with their places to <map.tsv>
        PLACE: <offset>[:<size>] in the file (hex with 0x, or decimal; size 4 by default, the signal repeated to fill it),
        or b<block>:<offset>[:<size>] in an ORAS save, whose checksums are then written again (oras_save.py)
  signal_probe.py find <dump> <map.tsv> [--base ADDRESS]
        every place each signal of <map.tsv> lights up in <dump> (a file, or a memory dump made by retro_host's
        "dump ADDRESS LENGTH FILE": --base ADDRESS gives addresses as the game sees them)
  signal_probe.py trace <source> <dump> PLACE [--chunk N] [--base ADDRESS]
        no injection: the bytes already at PLACE of <source>, cut into N-byte chunks (16 by default), each looked for in
        <dump>; a chunk found once is unique and says where those bytes are held (chunks of zeros or repeats are skipped)

Signals are 0xC51Gnnnn-like values chosen not to occur in the input file nor (when given with --avoid) in a dump of the
untouched run, so a hit is the signal and not chance: inject checks the input; find reports a signal found more than 8
times as noise. A signal may break the game where the value matters (a zone, a size): test fields one group at a time.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def number(text):
    return int(text, 0)


def place(text):
    """(block or None, offset, size)"""
    parts = text.split(":")
    block = None
    if parts[0].startswith("b"):
        block = int(parts[0][1:])
        parts = parts[1:]
    offset = number(parts[0])
    size = number(parts[1]) if len(parts) > 1 else 4
    return block, offset, size


def signals(data, count, avoid=b""):
    """count 4-byte values that occur in neither data nor avoid"""
    out, k = [], 0
    while len(out) < count:
        v = 0xC5160000 | (k & 0xFFFF) | ((k >> 16) << 12)
        k += 1
        b = struct.pack("<I", v)
        if b not in data and b not in avoid:
            out.append(v)
    return out


def cmd_inject(src, out, mapfile, places, avoid=None):
    data = bytearray(open(src, "rb").read())
    av = open(avoid, "rb").read() if avoid else b""
    ps = [place(p) for p in places]
    save = any(b is not None for b, _, _ in ps)
    if save:
        import oras_save
        data = oras_save.read(src)
    sig = signals(bytes(data), len(ps), av)
    rows = []
    for (block, offset, size), v, text in zip(ps, sig, places):
        at = offset
        if block is not None:
            _, _, boff, bsize, _ = oras_save.block_at(data, block)
            if offset + size > bsize:
                sys.exit(f"{text}: past the end of block {block} ({bsize} bytes)")
            at = boff + offset
        if at + size > len(data):
            sys.exit(f"{text}: past the end of the file")
        fill = (struct.pack("<I", v) * ((size + 3) // 4))[:size]
        data[at:at + size] = fill
        rows.append(f"0x{v:08X}\t{text}\tfile offset 0x{at:X}, {size} bytes")
    if save:
        oras_save.write_checksums(data)
    open(out, "wb").write(data)
    open(mapfile, "w").write("\n".join(rows) + "\n")
    print(f"{len(rows)} signals written to {out}" + (" (save checksums rewritten)" if save else "") + f"; map {mapfile}")


def hits(blob, needle, limit=9):
    out, at = [], blob.find(needle)
    while at >= 0 and len(out) < limit:
        out.append(at)
        at = blob.find(needle, at + 1)
    return out


def cmd_find(dump, mapfile, base=0):
    blob = open(dump, "rb").read()
    for line in open(mapfile):
        if not line.strip():
            continue
        v, text = line.split("\t")[:2]
        h = hits(blob, struct.pack("<I", int(v, 16)))
        where = "not found" if not h else ("noise (more than 8)" if len(h) > 8 else " ".join(f"0x{base + a:08X}" for a in h))
        print(f"{v}\t{text}\t{where}")


def cmd_trace(src, dump, text, chunk=16, base=0):
    data = open(src, "rb").read()
    blob = open(dump, "rb").read()
    block, offset, size = place(text)
    if block is not None:
        import oras_save
        _, _, boff, _, _ = oras_save.block_at(bytearray(data), block)
        offset += boff
    for at in range(offset, offset + size - chunk + 1, chunk):
        piece = data[at:at + chunk]
        if len(set(piece)) <= 2:
            continue
        h = hits(blob, piece)
        where = "not found" if not h else ("noise (more than 8)" if len(h) > 8 else " ".join(f"0x{base + a:08X}" for a in h))
        print(f"source +0x{at - offset:04X} ({piece[:8].hex()}..)\t{where}")


def main():
    a = sys.argv[1:]
    opt = {}
    for key in ("--base", "--chunk", "--avoid"):
        if key in a:
            i = a.index(key)
            opt[key] = a[i + 1]
            del a[i:i + 2]
    base = number(opt.get("--base", "0"))
    if len(a) >= 5 and a[0] == "inject":
        cmd_inject(a[1], a[2], a[3], a[4:], opt.get("--avoid"))
    elif len(a) == 3 and a[0] == "find":
        cmd_find(a[1], a[2], base)
    elif len(a) == 4 and a[0] == "trace":
        cmd_trace(a[1], a[2], a[3], number(opt.get("--chunk", "16")), base)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
