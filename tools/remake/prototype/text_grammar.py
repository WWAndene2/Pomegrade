#!/usr/bin/env python3
"""Each line's grammar word in a Gen 6 game text file (3 November: the word after the line's length in the line table,
GameText.h; French trainer class names: 0x1 feminine "la Championne", 0x4 elided "l'Écolier", 0 masculine "le").

  text_grammar.py <text member file> [line...]   lines and their word (all lines by default)

The file: `remake_tool oras-member <game> a/0/7/<language> <member> -1 <out>` (Omega Ruby's French a/0/7/4, its class
names member 21; X's a/0/7/5 member 20). The words only: the text itself is `remake_tool oras-text`.
"""
import struct
import sys


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    d = open(sys.argv[1], 'rb').read()
    count = struct.unpack_from('<H', d, 2)[0]
    section = struct.unpack_from('<I', d, 0xC)[0]
    lines = [int(v) for v in sys.argv[2:]] or range(count)
    for i in lines:
        _, length, word = struct.unpack_from('<IHH', d, section + 4 + 8 * i)
        print(f'{i}\t{length}\t{word:#x}')


if __name__ == '__main__':
    main()
