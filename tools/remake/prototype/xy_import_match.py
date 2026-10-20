#!/usr/bin/env python3
"""Which imports of an X/Y code module (CRO) have the same function in Omega Ruby: the measure of how far X/Y's own
modules (its gyms' DllUSGym*.cro, 3e lane B) can be moved into Omega Ruby by mapping their imports, not rewriting them.

  xy_import_match.py <x code.bin> <x linked dir> <oras code.bin> <oras linked dir> <x static.crs> <module.cro>...

code.bin: a game's ExeFS .code, decompressed, at 0x100000; linked dir: prototype/cro_link.py's output for that game (its
modules.bin from 0x10000000, modules.tsv). Each import of a module (|static|: an address of X's .code; another module: an
export of it, located through the linked X modules) is compared, as the first 24 instructions of the function there (to
its first return if sooner), with every place of Omega Ruby's .code and linked modules: equal when every instruction is
equal but for a branch's offset (bl / b / blx to an address) and a literal pool load's offset (ldr rX, [pc, #k]). Printed:
per module, each import's X address, then "same" (one place in Omega Ruby: its address), "several" (the count) or "none".
An odd address is Thumb code: its first 16 bytes compared raw. An address in X's
data segments is reported as data (a vtable or type info, to map by what it holds). A match is a lead, not a proof: a short function (a getter) matches many places; check before using one.
"""
import struct
import sys


from cro_dis import Cro

LINKED = 0x10000000
CODE = 0x100000
WINDOW = 24
DATA = 0x53C000  # X's .code: code to 0x53BEDC, then its read-only data and data (static.crs segments)


def key(data, at, base):
    """the masked instructions of the function at address at (data loaded at base), or None if outside"""
    off = at - base
    if off < 0 or off + 4 > len(data):
        return None
    words = []
    for k in range(WINDOW):
        if off + 4 * k + 4 > len(data):
            break
        w = struct.unpack_from("<I", data, off + 4 * k)[0]
        if (w & 0x0E000000) == 0x0A000000:      # b / bl (any condition): the offset masked
            w &= 0xFF000000
        elif (w & 0xFE000000) == 0xFA000000:    # blx immediate
            w &= 0xFE000000
        elif (w & 0x0F7F0000) == 0x051F0000:    # ldr rX, [pc, #k]: the offset masked
            w &= 0xFFFFF000
        words.append(w)
        if w in (0xE12FFF1E,) or (w & 0xFFFF8000) == 0xE8BD8000:  # bx lr, pop {..., pc}
            break
    return tuple(words)


def index(data, base, size):
    """masked keys of every 4-byte aligned place, by their first 4 words (to narrow the search)"""
    table = {}
    for off in range(0, len(data) - 16, 4):
        k = key(data, base + off, base)
        if k and len(k) >= 4:
            table.setdefault(k[:4], []).append(base + off)
    return table


def main():
    xcode, xlinked, ocode, olinked, xstatic = sys.argv[1:6]
    mods = sys.argv[6:]
    x = {"code": bytearray(open(xcode, "rb").read()), "mods": bytearray(open(xlinked + "/modules.bin", "rb").read())}
    o = {"code": bytearray(open(ocode, "rb").read()), "mods": bytearray(open(olinked + "/modules.bin", "rb").read())}
    xbase = {}
    for line in open(xlinked + "/modules.tsv"):
        p = line.rstrip("\n").split("\t")
        if p[0]:
            xbase[p[0]] = int(p[1], 16)
    static = Cro(xstatic)
    cache = {}
    oidx = [(o["code"], CODE, index(o["code"], CODE, 0)), (o["mods"], LINKED, index(o["mods"], LINKED, 0))]

    def x_key(addr):
        data, base = (x["code"], CODE) if addr < LINKED else (x["mods"], LINKED)
        return key(data, addr, base)

    def find_thumb(addr):
        """a Thumb function (odd address): its first 16 bytes, raw, in Omega Ruby's .code"""
        k = bytes(x["code"][addr - 1 - CODE:addr - 1 - CODE + 16])
        hits, i = [], o["code"].find(k)
        while i >= 0 and len(hits) < 8:
            if i % 2 == 0:
                hits.append(CODE + i + 1)
            i = o["code"].find(k, i + 1)
        return hits

    def find(k):
        hits = []
        for data, base, table in oidx:
            for at in table.get(k[:4], []):
                if key(data, at, base) == k:
                    hits.append(at)
        return hits

    for path in mods:
        m = Cro(path)
        h = m.h
        same = several = none = data = 0
        print(f"== {m.name}")
        for i in range(h["ImportModuleNum"]):
            name_off, idx_off, idx_num, anon_off, anon_num = struct.unpack_from("<5I", m.data, h["ImportModuleTableOffset"] + 20 * i)
            module = m.cstr(name_off)
            targets = []
            if module == "|static|":
                for j in range(anon_num):
                    tag, _ = struct.unpack_from("<II", m.data, anon_off + 8 * j)
                    targets.append(static.tag(tag))
            else:
                other = Cro(f"{path.rsplit('/', 1)[0]}/{module}.cro")
                for j in range(idx_num):
                    index_, _ = struct.unpack_from("<II", m.data, idx_off + 8 * j)
                    off = other.exports_indexed[index_] if index_ < len(other.exports_indexed) else None
                    targets.append(None if off is None else xbase[module] + off)
                for j in range(anon_num):
                    tag, _ = struct.unpack_from("<II", m.data, anon_off + 8 * j)
                    off = other.tag(tag)
                    targets.append(None if off is None else xbase[module] + off)
            for t in targets:
                if t is None:
                    print(f"  {module}: ? (unresolved)")
                    none += 1
                    continue
                if CODE + len(x["code"]) > t >= DATA:
                    print(f"  {module} 0x{t:08X}: data (to map by its content)")
                    data += 1
                    continue
                if t not in cache and t & 1:
                    cache[t] = find_thumb(t)
                if t not in cache:
                    k = x_key(t)
                    cache[t] = find(k) if k and len(k) >= 4 else None
                hits = cache[t]
                if hits is None:
                    print(f"  {module} 0x{t:08X}: data or too short")
                    none += 1
                elif len(hits) == 1:
                    print(f"  {module} 0x{t:08X}: same 0x{hits[0]:08X}")
                    same += 1
                elif hits:
                    print(f"  {module} 0x{t:08X}: several ({len(hits)})")
                    several += 1
                else:
                    print(f"  {module} 0x{t:08X}: none")
                    none += 1
        print(f"  {m.name}: {same} same, {several} several, {none} none, {data} data")


if __name__ == "__main__":
    main()
