#!/usr/bin/env python3
"""What each BCH holds, by its content sections: a lone BCH, or every file of a TM / GR / BM / AD container (26 October:
the field's effect files a/0/6/4, its entries 0xAD-0xBC found to be models and animations the game never loads, and X's
swirl files, ORAS_ENGINE.md 0.0).

  bch_sections.py <file> [FIRST [LAST]]

The file as `remake_tool garc` writes a member (LZ11 decompressed here when it is), or a BCH. A container's files from
FIRST to LAST (its numbers) are listed. Each BCH: its version (0x07 X/Y, 0x21 Omega Ruby) and the non-empty ones of its
15 content sections (each a pointer table, a count and a name tree, 12 bytes from the header's contents pointer +8):
models, materials, shaders, textures, LUTs, lights, cameras, fogs, skeletal / material / visibility / light / camera /
fog animations, scenes.
"""
import struct
import sys

SECTIONS = ['models', 'materials', 'shaders', 'textures', 'luts', 'lights', 'cameras', 'fogs', 'skeletal animations',
            'material animations', 'visibility animations', 'light animations', 'camera animations', 'fog animations', 'scenes']


def lz11(b):
    n, i = b[1] | b[2] << 8 | b[3] << 16, 4
    if n == 0:
        n, i = int.from_bytes(b[4:8], 'little'), 8
    out = bytearray()
    while len(out) < n:
        flags = b[i]
        i += 1
        for k in range(8):
            if len(out) >= n:
                break
            if not flags & (0x80 >> k):
                out.append(b[i])
                i += 1
                continue
            t = b[i] >> 4
            if t == 0:
                c, d, i = (((b[i] & 15) << 4) | (b[i + 1] >> 4)) + 0x11, (((b[i + 1] & 15) << 8) | b[i + 2]) + 1, i + 3
            elif t == 1:
                c = (((b[i] & 15) << 12) | (b[i + 1] << 4) | (b[i + 2] >> 4)) + 0x111
                d, i = (((b[i + 2] & 15) << 8) | b[i + 3]) + 1, i + 4
            else:
                c, d, i = t + 1, (((b[i] & 15) << 8) | b[i + 1]) + 1, i + 2
            for _ in range(c):
                out.append(out[-d])
    return bytes(out)


def describe(b):
    if b[:4] != b'BCH\0':
        return f'not a BCH ({b[:4]!r}, {len(b)} bytes)'
    contents = struct.unpack_from('<I', b, 8)[0]
    held = [f'{SECTIONS[k]} {n}' for k in range(15) for n in [struct.unpack_from('<I', b, contents + 12 * k + 4)[0]] if n]
    return f'BCH 0x{b[4]:02X}, {len(b)} bytes: ' + (', '.join(held) or 'nothing')


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    d = open(sys.argv[1], 'rb').read()
    if d[:1] == b'\x11':
        d = lz11(d)
    if d[:3] == b'BCH':
        print(describe(d))
        return
    count = struct.unpack_from('<H', d, 2)[0]
    offsets = struct.unpack_from('<%dI' % (count + 1), d, 4)
    first = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
    last = int(sys.argv[3], 0) if len(sys.argv) > 3 else count - 1
    print(f'{d[:2].decode(errors="replace")} container, {count} files')
    for k in range(first, min(last, count - 1) + 1):
        print(f'  {k:#06x} {describe(d[offsets[k]:offsets[k + 1]])}')


if __name__ == '__main__':
    main()
