#!/usr/bin/env python3
"""A battle sequence (SESD: an intro, a move's or a ball's animation) listed record by record, to read what X/Y's and Omega
Ruby's sequences do and check one converted by OrasXySequence.h (ORAS_ENGINE.md 0.1, the battle intros).

  sesd.py <DllBattle.cro> oras|x <sequence>...

<sequence>: a member of a/0/3/4 (Omega Ruby) or a/0/3/6 (X), as `remake_tool garc` writes them (N.unknown). The format, read
22 October: "SESD", u32 2, u32 0, u32 the last frame; then records {u32 first frame, u32 last frame, u16, u16, u16 command,
its arguments}, closed by the word 0xFFFFFFFF. A record carries no length: each command's argument size is the u32 at its
number in DllBattle's table (Omega Ruby: 213 commands at file offset 0xE3800, X: 186 at 0xD91F8), which is why the
module of the sequence's own game is given. Omega Ruby's numbers are X's with 27 commands inserted (OrasXySequence.cpp's
table converts). A sequence that does not end on the mark after its last record is reported, not guessed further.
"""
import struct
import sys

TABLES = {"oras": (0xE3800, 213), "x": (0xD91F8, 186)}


def sizes(cro, game):
    at, count = TABLES[game]
    data = open(cro, "rb").read()
    return struct.unpack_from("<%dI" % count, data, at)


def records(seq, size):
    """(offset, first frame, last frame, u16, u16, command, arguments) of each record, and whether the end mark closes it"""
    out, p = [], 0x10
    while p + 4 <= len(seq):
        if struct.unpack_from("<I", seq, p)[0] == 0xFFFFFFFF:
            return out, p + 4 == len(seq)
        if p + 14 > len(seq):
            break
        first, last, a, b, command = struct.unpack_from("<IIHHH", seq, p)
        if command >= len(size):
            break
        out.append((p, first, last, a, b, command, seq[p + 14:p + 14 + size[command]]))
        p += 14 + size[command]
    return out, False


def main():
    if len(sys.argv) < 4 or sys.argv[2] not in TABLES:
        sys.exit(__doc__)
    size = sizes(sys.argv[1], sys.argv[2])
    for path in sys.argv[3:]:
        seq = open(path, "rb").read()
        if seq[:4] != b"SESD":
            print(f"{path}: not a SESD sequence")
            continue
        out, whole = records(seq, size)
        print(f"{path}: last frame {struct.unpack_from('<I', seq, 12)[0]}, {len(out)} records"
              + ("" if whole else f", NOT closed by the end mark after the last one read (stopped at 0x{out[-1][0] if out else 0x10:X})"))
        for p, first, last, a, b, command, args in out:
            print(f"  0x{p:04X} frames {first}-{last} {a:04X} {b:04X} command 0x{command:02X}: {args.hex(' ')}")


if __name__ == "__main__":
    main()
