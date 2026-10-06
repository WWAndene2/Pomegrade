#!/usr/bin/env python3
"""A RomFS code module (CRO) with words changed.

  cro_patch.py <module.cro> <out dir> OFFSET=VALUE...

Only for Azahar: a console's loader checks the module against the hashes of .crr/static.crr (which do not hash the whole file:
the CRR does not hold the file's SHA-256, checked on DllField.cro), Azahar does not (azahar/src/core/hle/service/ldr_ro/
cro_helper.cpp, VerifyHash: a TODO returning success). So the module's hashes are left as they were. Each OFFSET is a file offset in the CRO
holding a u32 (it must hold the expected old value: OFFSET=OLD:NEW refuses otherwise). Writes <out dir>/<module name>, which
`remake_tool oras-patch` turns into the mod's BPS patch.

For the overworld piece buffer (ORAS_LITTLEROOT.md 10, item 5): DllField.cro holds 0x100000 at 0xF1118, 0xF1148, 0xF1208,
0xF1268, 0xF1298, 0xF12C8 and 0xF1328, each 4 words after 720.0 and 18.0 (a piece's width and a tile's, in units): the first
suspect for the piece buffer's size. Also at 0xF49C0 and 0xF49C4, beside 0xB0000 and 0x400000 (heap sizes, perhaps).
"""
import os
import struct
import sys


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    cro_path, out_dir = sys.argv[1:3]
    cro = bytearray(open(cro_path, "rb").read())
    for arg in sys.argv[3:]:
        off, value = arg.split("=")
        off = int(off, 0)
        expected = None
        if ":" in value:
            expected, value = (int(v, 0) for v in value.split(":"))
        else:
            value = int(value, 0)
        current = struct.unpack_from("<I", cro, off)[0]
        if expected is not None and current != expected:
            sys.exit(f"0x{off:X} holds 0x{current:X}, not 0x{expected:X}: refused")
        struct.pack_into("<I", cro, off, value)
        print(f"0x{off:X}: 0x{current:X} -> 0x{value:X}")
    os.makedirs(out_dir, exist_ok=True)
    open(os.path.join(out_dir, os.path.basename(cro_path)), "wb").write(cro)


if __name__ == "__main__":
    main()
