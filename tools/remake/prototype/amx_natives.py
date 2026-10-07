#!/usr/bin/env python3
"""The natives ORAS's Pawn (AMX) scripts call, named by the game's own tables (ORAS_ENGINE.md 6, "Script commands").

A script calls the game with `sysreq.n` by a native's name hash (`h = h * 0x83 ^ c`, Script_LinkNativeImports 0x506118);
the game links each script against tables of {char *name, function} pairs, first match wins:
  - for every script, at load (Script_Load 0x3AAE3C, Script_LoadFieldScript 0x3BD498): Pawn's core 0x5A6394, console
    0x6179E4 (printf), float 0x5A630C, _Suspend 0x5A6758, the field's 0x57A860;
  - then one table per bit of the script's mask (Script_RegisterNativeTablesByMask 0x3FBC80), the DllField ones through
    the .code's import stubs 0x19C8C8-0x19C910 to their getters; a zone script's mask is word <zone> of the 536-word table
    0x587D58 (read by Zone_LoadZoneScript 0x3FF538).
The table addresses below were read from that code on 7 October (code.bin from `remake_tool oras-code`, the modules
linked by cro_link.py at 0x10000000); `tables` reads every row from the dumps, so nothing of the game is stored here.

  amx_natives.py hash NAME...                                   the hash of a name
  amx_natives.py tables <code.bin> <linked/modules.bin> [out]   every row: table, bit, function, hash, name (TSV)
  amx_natives.py names <tables.tsv> [listing]                   an `oras-script` listing (a file or stdin) with each
                                                                #HASH followed by its name
  amx_natives.py check <tables.tsv> <code.bin> <remake_tool> <oras.3ds>
        every zone script's native calls (536 zones, main and init) against the tables its zone's mask and the load
        register; prints each call not covered (none on 7 October: 16,321 calls)
"""
import collections
import struct
import subprocess
import sys

CODE_BASE, MODULES_BASE = 0x100000, 0x10000000
ALWAYS = {0x5A6394: "core", 0x6179E4: "console", 0x5A630C: "float", 0x5A6758: "_Suspend", 0x57A860: "field"}
BY_BIT = {1: 0x1033A234, 2: 0x10339594, 4: 0x1033A99C, 8: 0x1033AFBC, 0x10: 0x1033A9FC, 0x20: 0x1033958C,
          0x40: 0x1033AA9C, 0x80: 0x1033AAAC, 0x100: 0x1033AB44, 0x200: 0x5885CC, 0x400: 0x1033ACDC}
ZONE_MASKS, ZONES = 0x587D58, 536


def name_hash(name):
    h = 0
    for c in name.encode():
        h = (h * 0x83 ^ (c if c < 0x80 else c - 0x100)) & 0xFFFFFFFF
    return h


class Image:
    """code.bin and the linked modules, read by address"""
    def __init__(self, code, modules):
        self.parts = [(CODE_BASE, open(code, "rb").read()), (MODULES_BASE, open(modules, "rb").read())]

    def _at(self, addr):
        for base, data in self.parts:
            if base <= addr < base + len(data):
                return data, addr - base
        raise ValueError(f"0x{addr:X} lies in neither image")

    def word(self, addr):
        data, off = self._at(addr)
        return struct.unpack_from("<I", data, off)[0]

    def text(self, addr):
        data, off = self._at(addr)
        return data[off:data.index(b"\0", off)].decode("latin-1")


def read_table(img, table):
    rows, k = [], 0
    while img.word(table + 8 * k):
        rows.append((img.text(img.word(table + 8 * k)), img.word(table + 8 * k + 4)))
        k += 1
    return rows


def cmd_tables(code, modules, out=None):
    img = Image(code, modules)
    lines = []
    for table, label in ALWAYS.items():
        lines += [(table, "load:" + label, fn, name) for name, fn in read_table(img, table)]
    for bit, table in BY_BIT.items():
        lines += [(table, f"bit:0x{bit:X}", fn, name) for name, fn in read_table(img, table)]
    text = "".join(f"0x{t:X}\t{w}\t0x{fn:X}\t{name_hash(n):08X}\t{n}\n" for t, w, fn, n in lines)
    (open(out, "w") if out else sys.stdout).write(text)
    print(f"{len(lines)} rows in {len(ALWAYS) + len(BY_BIT)} tables", file=sys.stderr)


def load_tables(path):
    rows = [l.rstrip("\n").split("\t") for l in open(path) if l.strip()]
    by_hash = collections.defaultdict(list)
    for table, where, fn, h, name in rows:
        by_hash[h].append((int(table, 16), where, fn, name))
    return by_hash


def cmd_names(tables, listing=None):
    by_hash = load_tables(tables)
    for line in (open(listing) if listing else sys.stdin):
        line = line.rstrip("\n")
        for h in set(w[1:] for w in line.split() if w.startswith("#") and len(w) == 9):
            if h in by_hash:
                line = line.replace("#" + h, f"#{h} {by_hash[h][0][3]}")
        print(line)


def cmd_check(tables, code, tool, rom):
    by_hash = load_tables(tables)
    data = open(code, "rb").read()
    masks = struct.unpack_from(f"<{ZONES}I", data, ZONE_MASKS - CODE_BASE)
    calls = bad = 0
    for zone in range(ZONES):
        for kind in ("", "init"):
            out = subprocess.run([tool, "oras-script", rom, str(zone)] + ([kind] if kind else []),
                                 capture_output=True, text=True).stdout
            for line in out.splitlines():
                words = line.split()
                if len(words) != 3 or words[0] != "native":
                    continue
                calls += 1
                h = words[2].lstrip("#")
                ok = any(where.startswith("load:") or int(where[4:], 16) & masks[zone] for _, where, _, _ in by_hash[h])
                if not ok:
                    bad += 1
                    known = [r[3] for r in by_hash[h]] or "(no table)"
                    print(f"zone {zone}{kind}: #{h} {known} not covered by mask 0x{masks[zone]:X}")
    print(f"{calls} native calls, {bad} not covered by their zone's mask and the load")


def main():
    a = sys.argv[1:]
    if a[:1] == ["hash"] and len(a) > 1:
        for n in a[1:]:
            print(f"{name_hash(n):08X}\t{n}")
    elif a[:1] == ["tables"] and len(a) in (3, 4):
        cmd_tables(*a[1:])
    elif a[:1] == ["names"] and len(a) in (2, 3):
        cmd_names(*a[1:])
    elif a[:1] == ["check"] and len(a) == 5:
        cmd_check(*a[1:])
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
