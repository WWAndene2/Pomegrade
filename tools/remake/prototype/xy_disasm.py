#!/usr/bin/env python3
"""A game's ARM code at an address, and the calls a function makes (31 October, Olympia's port; the session's armdis.py
and stubs.py, kept).

  xy_disasm.py <work> x|oras ADDRESS [WORDS]       the instructions from ADDRESS (20 words by default)
  xy_disasm.py <work> x|oras --calls ADDRESS:BYTES...  each function's calls: through a stub (`ldr pc, [pc, #-4]`, its
                                                    target printed), direct outside it, and its pc-relative literals

<work> as xy_code_compare.py (dumps/xcode.bin or code.bin, xlinked/ or linked/modules.bin). Addresses in hex, from
0x100000 (.code) or 0x10000000 (the linked modules).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xy_code_compare import MD, Game  # noqa: E402


def calls(game, address, size):
    data, base = game.blob(address)
    stubs = {}
    for i in MD.disasm(data[address - base:address - base + size], address):
        if i.mnemonic in ("bl", "b", "blx") and i.op_str.startswith("#"):
            target = int(i.op_str[1:], 16)
            if game.word(target) == 0xE51FF004:
                stubs[target] = game.word(target + 4)
                print(f"{address:#x} {i.address:#x} {i.mnemonic} stub {target:#x} -> {stubs[target]:#x}")
            elif not address <= target < address + size:
                print(f"{address:#x} {i.address:#x} {i.mnemonic} direct {target:#x}")
        if i.mnemonic == "ldr" and "pc, #" in i.op_str:
            literal = i.address + 8 + int(i.op_str.split("#")[1].rstrip("]"), 16)
            print(f"{address:#x} {i.address:#x} literal {literal:#x} {game.word(literal):#x}")
    return stubs


def main(args):
    if len(args) < 3 or args[1] not in ("x", "oras"):
        print(__doc__)
        return 2
    game = Game(args[0], args[1] == "x")
    if args[2] == "--calls":
        stubs = {}
        for spec in args[3:]:
            address, size = spec.split(":")
            stubs.update(calls(game, int(address, 16), int(size)))
        print("stubs:", " ".join(f"{k:#x}={v:#x}" for k, v in sorted(stubs.items())))
        return 0
    address, words = int(args[2], 16), int(args[3]) if len(args) > 3 else 20
    data, base = game.blob(address)
    for i in MD.disasm(data[address - base:address - base + 4 * words], address):
        print(f"{i.address:#x} {i.mnemonic} {i.op_str}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
