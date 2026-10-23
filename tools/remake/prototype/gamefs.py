#!/usr/bin/env python3
"""The game's files and a mod's, browsed and searched from one place (23 October: archives were extracted by hand into
scratch folders again and again, members found by guessing, and a mod's copy of a file compared by eye).

  gamefs.py <work> ls [PREFIX] [--mod MOD] [--game x]   RomFS files: size, what they are (GARC and its member count, CRO,
                                                         ...) and, with --mod, what the mod does to each (patched,
                                                         replaced, added)
  gamefs.py <work> members PATH [--mod MOD] [--game x]  an archive's members: size, type, first name found in it
  gamefs.py <work> get PATH[:N] OUT [--mod MOD]         a file, or one member, as the game (or the mod) has it
  gamefs.py <work> find PATH (--text S | --hex H) [--mod MOD] [--game x]
                                                         the members holding a text (ASCII or UTF-16) or bytes
  gamefs.py <work> diff PATH --mod MOD                  the members the mod changes, adds or drops, with their sizes
  gamefs.py <work> where OFFSET... [--game x]           the file and member at an image offset (POMEGRADE_ROMFS_TRACE
                                                         offsets: add the RomFS base, romfs_member.py)

--game x reads X (<work>/dumps/x.3ds) instead of Omega Ruby. An archive is extracted once per content into
<work>/fscache/<its hash>/ (remake_tool garc: members decompressed) and reused; the cache can be deleted at any time.
"""
import argparse
import hashlib
import os
import re
import signal
import struct
import subprocess
import sys

PROGRAM = "000400000011C400"


class Fs:
    def __init__(self, work, game, mod):
        self.work = os.path.abspath(work)
        self.tool = os.path.join(self.work, "build-remake", "remake_tool")
        self.rom = os.path.join(self.work, "dumps", "x.3ds" if game == "x" else "oras.3ds")
        self.mod = mod
        if mod and game == "x":
            raise SystemExit("--mod is an Omega Ruby mod")

    def files(self, prefix=""):
        out = subprocess.run([self.tool, "romfs-ls", self.rom, prefix], check=True, capture_output=True, text=True).stdout
        return [(p, int(s), int(o, 16)) for p, s, o in (l.split("\t") for l in out.splitlines())]

    def read(self, path, mod=True):
        out = os.path.join(self.work, "fscache", "_read")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        if mod and self.mod:
            subprocess.run([self.tool, "mod-file", self.rom, self.mod, path, out], check=True, stdout=subprocess.DEVNULL)
        else:
            subprocess.run([self.tool, "oras-extract", self.rom, path, out], check=True, stdout=subprocess.DEVNULL)
        data = open(out, "rb").read()
        os.remove(out)
        return data

    def members(self, path, mod=True):
        data = self.read(path, mod)
        if data[:4] != b"CRAG":
            raise SystemExit(f"{path} is not a GARC ({data[:4]!r})")
        folder = os.path.join(self.work, "fscache", hashlib.sha256(data).hexdigest()[:16])
        if not os.path.isdir(folder):
            src = folder + ".garc"
            os.makedirs(os.path.dirname(src), exist_ok=True)
            open(src, "wb").write(data)
            subprocess.run([self.tool, "garc", src, folder + ".tmp"], check=True, stdout=subprocess.DEVNULL)
            os.rename(folder + ".tmp", folder)
            os.remove(src)
        found = {}
        for f in os.listdir(folder):
            key = f.split(".")[0]
            found[key] = os.path.join(folder, f)
        return dict(sorted(found.items(), key=lambda kv: [int(x) for x in kv[0].split("_")]))

    def mod_state(self, path):
        if not self.mod:
            return ""
        base = os.path.join(self.mod, "load", "mods", PROGRAM)
        if os.path.exists(os.path.join(base, "romfs", path)):
            return "replaced"
        if os.path.exists(os.path.join(base, "romfs_ext", path + ".bps")):
            return "patched"
        return ""


def stored(garc):
    """each member's first stored byte (0x10/0x11: LZ compressed), from the raw GARC's FATB, keyed as the members are"""
    hs = struct.unpack_from("<H", garc, 4)[0]
    data = struct.unpack_from("<I", garc, 0x10)[0]
    fatb = hs + struct.unpack_from("<I", garc, hs + 4)[0]
    q, out = fatb + 12, {}
    for i in range(struct.unpack_from("<I", garc, fatb + 8)[0]):
        bits = struct.unpack_from("<I", garc, q)[0]
        q += 4
        for k in range(32):
            if bits >> k & 1:
                start = struct.unpack_from("<I", garc, q)[0]
                q += 12
                out[str(i) if k == 0 else f"{i}_{k}"] = garc[data + start] in (0x10, 0x11)
    return out


def kind(data):
    if data[:4] == b"CRAG":
        return f"GARC, {struct.unpack_from('<H', data, 0x1C + 8)[0] if len(data) > 0x26 else '?'} members"
    if data[0x80:0x84] == b"CRO0":
        return "CRO module"
    for magic in (b"CGFX", b"BCH", b"SESD", b"CLIM", b"DARC", b"CSTM", b"CWAR", b"CSAR", b"BL", b"ZO"):
        if data.startswith(magic):
            return magic.decode()
    return ""


def first_name(data):
    m = re.search(rb"[A-Za-z][A-Za-z0-9_]{5,40}", data[:0x4000])
    return m.group(0).decode() if m else ""


def main():
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # piped into head: stop quietly
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("work")
    ap.add_argument("what", choices=["ls", "members", "get", "find", "diff", "where"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--mod")
    ap.add_argument("--game", choices=["oras", "x"], default="oras")
    ap.add_argument("--text")
    ap.add_argument("--hex")
    a = ap.parse_args()
    fs = Fs(a.work, a.game, a.mod)

    if a.what == "ls":
        listed = fs.files(a.args[0] if a.args else "")
        game_paths = {p for p, _, _ in listed}
        for p, size, off in listed:
            head = fs.read(p, mod=False)[:0x84] if size < (1 << 31) else b""
            print(f"{p:40} {size:>11} {kind(head):22} {fs.mod_state(p)}")
        if fs.mod:
            romfs = os.path.join(fs.mod, "load", "mods", PROGRAM, "romfs")
            for root, _, names in os.walk(romfs):
                for n in names:
                    p = os.path.relpath(os.path.join(root, n), romfs)
                    if p not in game_paths and p.startswith(a.args[0] if a.args else ""):
                        print(f"{p:40} {os.path.getsize(os.path.join(root, n)):>11} {'':22} added")
    elif a.what == "members":
        # stored: LZ or plain as the archive holds it (23 October: X/Y's camera appended plain to a/0/3/2, whose every
        # member is LZ and read through the decompressor, hung the battle loader); a member stored unlike most is flagged
        packed = stored(fs.read(a.args[0]))
        usual = sum(packed.values()) * 2 > len(packed)
        for key, f in fs.members(a.args[0]).items():
            d = open(f, "rb").read()
            flag = "" if packed.get(key) == usual else ("  <- stored plain, the archive's others LZ" if usual else "  <- stored LZ, the archive's others plain")
            print(f"{key:>8} {len(d):>9} {'LZ' if packed.get(key) else 'plain':5} {f.rsplit('.', 1)[-1]:8} {first_name(d)}{flag}")
    elif a.what == "get":
        path, _, member = a.args[0].partition(":")
        data = open(fs.members(path)[member], "rb").read() if member else fs.read(path)
        open(a.args[1], "wb").write(data)
        print(f"{a.args[1]}: {len(data)} bytes")
    elif a.what == "find":
        if not (a.text or a.hex):
            raise SystemExit("find: --text or --hex")
        needles = [bytes.fromhex(a.hex)] if a.hex else [a.text.encode(), a.text.encode("utf-16-le")]
        for key, f in fs.members(a.args[0]).items():
            d = open(f, "rb").read()
            hits = [d.find(n) for n in needles if d.find(n) >= 0]
            if hits:
                print(f"{key:>8} {len(d):>9} at 0x{min(hits):X}  {first_name(d)}")
    elif a.what == "diff":
        if not fs.mod:
            raise SystemExit("diff: --mod")
        game, mod = fs.members(a.args[0], mod=False), fs.members(a.args[0], mod=True)
        for key in sorted(set(game) | set(mod), key=lambda k: [int(x) for x in k.split("_")]):
            g = open(game[key], "rb").read() if key in game else None
            m = open(mod[key], "rb").read() if key in mod else None
            if g != m:
                state = "added" if g is None else "dropped" if m is None else "changed"
                print(f"{key:>8} {state:8} {len(g) if g is not None else '-':>9} -> {len(m) if m is not None else '-':>9} {first_name(m or g)}")
    elif a.what == "where":
        files = sorted(fs.files(), key=lambda t: t[2])
        for off in (int(x, 0) for x in a.args):
            hit = [t for t in files if t[2] <= off < t[2] + t[1]]
            if not hit:
                print(f"0x{off:X}: in no file")
                continue
            p, size, start = hit[0]
            line = f"0x{off:X}: {p} + 0x{off - start:X}"
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import mmap
            import romfs_member
            with open(fs.rom, "rb") as f:
                mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
                m = romfs_member.member(mm, off)
                if m and m[0] == start and m[2] is not None:
                    line += f", member {m[2]} + 0x{m[4]:X}"
            print(line)


if __name__ == "__main__":
    main()
