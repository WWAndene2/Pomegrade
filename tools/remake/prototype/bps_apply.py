#!/usr/bin/env python3
"""A BPS patch applied to its source file: to read what a mod holds where it ships a patch, not the file (26 October: the
mod's a/0/3/9 and a/0/2/3, romfs_ext/*.bps, rebuilt to read their pieces' and props' entries).

  bps_apply.py <source> <patch.bps> <out>

The source is the game's own file (`remake_tool oras-extract <game> <path> <out>`). The format (BPS1): three numbers
(source size, target size, metadata size) then commands until the 12-byte footer, each a number whose low two bits are
the action and the rest the length - 1: 0 source read (the source at the output's position), 1 target read (bytes in the
patch), 2 source copy and 3 target copy (each a signed offset, bit 0 its sign, moving its own cursor). A number: 7 bits a
byte, the high bit ends it, each further byte adding one step more (beat's encoding). The checksums are not checked:
the output's size against the patch's target size is.
"""
import sys


def number(patch, at):
    value, shift = 0, 1
    while True:
        byte = patch[at]
        at += 1
        value += (byte & 0x7F) * shift
        if byte & 0x80:
            return value, at
        shift <<= 7
        value += shift


def apply(source, patch):
    if patch[:4] != b'BPS1':
        sys.exit('not a BPS patch')
    at = 4
    _, at = number(patch, at)
    target_size, at = number(patch, at)
    metadata, at = number(patch, at)
    at += metadata
    out, source_at, target_at, end = bytearray(), 0, 0, len(patch) - 12
    while at < end:
        command, at = number(patch, at)
        action, length = command & 3, (command >> 2) + 1
        if action == 0:
            out += source[len(out):len(out) + length]
        elif action == 1:
            out += patch[at:at + length]
            at += length
        else:
            offset, at = number(patch, at)
            offset = -(offset >> 1) if offset & 1 else offset >> 1
            if action == 2:
                source_at += offset
                out += source[source_at:source_at + length]
                source_at += length
            else:
                target_at += offset
                for _ in range(length):
                    out.append(out[target_at])
                    target_at += 1
    if len(out) != target_size:
        sys.exit(f'the output is {len(out)} bytes, the patch says {target_size}')
    return bytes(out)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    out = apply(open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read())
    open(sys.argv[3], 'wb').write(out)
    print(f'{sys.argv[3]}: {len(out)} bytes')


if __name__ == '__main__':
    main()
