#!/usr/bin/env python3
"""A RomFS code module (CRO) with words changed, and the module list (.crr/static.crr) holding its new hash.

  cro_patch.py <module.cro> <static.crr> <out dir> OFFSET=VALUE...

A console's loader takes a CRO only when its SHA-256 is among the hashes the CRR lists, so the module's hash is replaced in the
CRR. Azahar does not check it (azahar/src/core/hle/service/ldr_ro/cro_helper.cpp, VerifyHash: a TODO returning success). Each OFFSET is a file offset in the CRO
holding a u32 (it must hold the expected old value: OFFSET=OLD:NEW refuses otherwise). Writes <out dir>/<module name> and
<out dir>/static.crr, which `remake_tool oras-patch` turns into the mod's BPS patches.

For the overworld piece buffer (ORAS_LITTLEROOT.md 10, item 5): DllField.cro holds 0x100000 at 0xF1118, 0xF1148, 0xF1208,
0xF1268, 0xF1298, 0xF12C8 and 0xF1328, each 4 words after 720.0 and 18.0 (a piece's width and a tile's, in units): the first
suspect for the piece buffer's size. Also at 0xF49C0 and 0xF49C4, beside 0xB0000 and 0x400000 (heap sizes, perhaps).
"""
import hashlib
import os
import struct
import sys


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    cro_path, crr_path, out_dir = sys.argv[1:4]
    cro = bytearray(open(cro_path, "rb").read())
    crr = bytearray(open(crr_path, "rb").read())
    old_hash = hashlib.sha256(cro).digest()
    for arg in sys.argv[4:]:
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
    new_hash = hashlib.sha256(cro).digest()
    count = crr.count(old_hash)
    if count != 1:
        sys.exit(f"the CRR lists the module's hash {count} times, not once: refused")
    crr[crr.find(old_hash):crr.find(old_hash) + 32] = new_hash
    os.makedirs(out_dir, exist_ok=True)
    open(os.path.join(out_dir, os.path.basename(cro_path)), "wb").write(cro)
    open(os.path.join(out_dir, "static.crr"), "wb").write(crr)
    print(f"hash {old_hash.hex()[:16]}... -> {new_hash.hex()[:16]}... in the CRR")


if __name__ == "__main__":
    main()
