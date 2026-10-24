#!/usr/bin/env python3
"""X/Y's code against Omega Ruby's, to port X/Y's code (ORAS_ENGINE.md 0.0, step 1 B2: the dust clouds were mapped with
these, 23-24 October). A work directory laid out by headless/session_setup.sh (steps tool, ghidra and ghidra-x):
dumps/code.bin and dumps/xcode.bin (each game's .code at 0x100000), linked/ and xlinked/ (prototype/cro_link.py: every
code module at its linked address from 0x10000000, Omega Ruby's DllField at 0x10242000, X's at 0x101F9000),
functions.tsv / edges.tsv and x/functions.tsv / x/edges.tsv (ghidra/Export.java and CallGraph.java: each function's
start and size, each call).

  xy_code_compare.py <work> offsets X_ADDRESS ORAS_ADDRESS [WORDS]
      the two functions aligned by their instructions' mnemonics (difflib, WORDS instructions each, 400 by default): the
      ratio, then each aligned pair whose memory offset ([rN, #k]) differs. A function's fields moved between the games
      show here (X's 0x1027E964 = Omega Ruby's 0x4525BC: + 0x2C -> + 0x30, + 0x30 -> + 0x34: one word added)
  xy_code_compare.py <work> calls X_ADDRESS ORAS_ADDRESS [WORDS]
      the two functions aligned the same way: each aligned pair of calls (bl / b / blx), a module's stub
      (`ldr pc, [pc, #-4]` and its word) followed to its target. Maps what one game calls to what the other calls at the
      same place (X's step check 0x102265DC against Omega Ruby's 0x10263014: 55 calls paired)
  xy_code_compare.py <work> closure X_ADDRESS... [--stop X_ADDRESS...]
      the X functions reachable from these through X's call graph, going on only through X's DllField (0x10200000-
      0x10400000) and not through the --stop ones: what a port of them takes (prototype/xy_import_match.py --at then
      tells which of those Omega Ruby has)
  xy_code_compare.py <work> pairs X_LOW X_HIGH ORAS_LOW ORAS_HIGH [WORDS]
      every X function starting in [X_LOW, X_HIGH) against every Omega Ruby one in [ORAS_LOW, ORAS_HIGH), each X one's
      best twin and its ratio: a copied block's functions found in the other game (X's dust cloud event 0x10239668-
      0x10239B94 against Omega Ruby's 0x10230000-0x10240000: none above 0.46, ported whole)
  xy_code_compare.py <work> sites x|oras OFFSET_A OFFSET_B
      the functions of that game's DllField where a load `ldr rX, [rY, #OFFSET_A]` is followed (within 4 instructions)
      by a load from [rX, #OFFSET_B]: where a field of an object reached through another is read (0x48 0x10: the map
      object's + 0x10), to find a site with a twin in the other game and read the offset there

Addresses in hex (0x optional). Reads only; prints only. A result is a lead to read, not a proof: an aligned pair can be
two different instructions that happen to share a mnemonic.
"""
import bisect
import difflib
import re
import struct
import sys

from capstone import CS_ARCH_ARM, CS_MODE_ARM, Cs

MD = Cs(CS_ARCH_ARM, CS_MODE_ARM)
MD.skipdata = True
LINKED = 0x10000000
CODE = 0x100000


class Game:
    """one game's code and linked modules, and its function list"""
    def __init__(self, work, x):
        self.code = open(f"{work}/dumps/{'xcode.bin' if x else 'code.bin'}", "rb").read()
        self.linked = open(f"{work}/{'xlinked' if x else 'linked'}/modules.bin", "rb").read()
        self.work, self.x = work, x

    def blob(self, address):
        return (self.linked, LINKED) if address >= LINKED else (self.code, CODE)

    def word(self, address):
        data, base = self.blob(address)
        return struct.unpack_from("<I", data, address - base)[0]

    def disassemble(self, address, words):
        """the function's instructions, to its first return past half of `words` (or `words` of them)"""
        data, base = self.blob(address)
        out = []
        for i in MD.disasm(data[address - base:address - base + 4 * words], address):
            out.append(i)
            if i.mnemonic == "pop" and "pc" in i.op_str and len(out) > words // 2:
                break
        return out

    def target(self, address):
        """a call's target, a stub (`ldr pc, [pc, #-4]`, its word) followed"""
        data, base = self.blob(address)
        if 0 <= address - base < len(data) - 8 and self.word(address) == 0xE51FF004:
            return self.word(address + 4)
        return address

    def functions(self):
        path = f"{self.work}/x/functions.tsv" if self.x else f"{self.work}/functions.tsv"
        return sorted(int(line.split()[0], 16) for line in open(path))

    def edges(self):
        path = f"{self.work}/x/edges.tsv" if self.x else f"{self.work}/edges.tsv"
        calls = {}
        for line in open(path):
            a, b = line.split()
            calls.setdefault(int(a, 16), set()).add(int(b, 16))
        return calls


def aligned(xgame, ogame, xa, oa, words):
    X, O = xgame.disassemble(xa, words), ogame.disassemble(oa, words)
    matcher = difflib.SequenceMatcher(None, [i.mnemonic for i in X], [i.mnemonic for i in O], autojunk=False)
    pairs = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            pairs.extend(zip(X[i1:i2], O[j1:j2]))
    return matcher.ratio(), pairs


def offsets(work, xa, oa, words):
    ratio, pairs = aligned(Game(work, True), Game(work, False), xa, oa, words)
    print(f"ratio {ratio:.2f}")
    for a, b in pairs:
        ma, mb = re.findall(r"#(-?0x[0-9a-f]+|-?\d+)\]", a.op_str), re.findall(r"#(-?0x[0-9a-f]+|-?\d+)\]", b.op_str)
        if ma and mb and ma != mb:
            print(f"{a.address:#x} {a.mnemonic} {a.op_str:30s} | {b.address:#x} {b.op_str}")


def calls(work, xa, oa, words):
    xg, og = Game(work, True), Game(work, False)
    ratio, pairs = aligned(xg, og, xa, oa, words)
    print(f"ratio {ratio:.2f}")
    for a, b in pairs:
        if a.mnemonic in ("bl", "b", "blx") and a.op_str.startswith("#"):
            ta, tb = int(a.op_str[1:], 16), int(b.op_str[1:], 16)
            fa, fb = xg.target(ta), og.target(tb)
            sa = f"{ta:#x}->{fa:#x}" if fa != ta else f"{ta:#x}"
            sb = f"{tb:#x}->{fb:#x}" if fb != tb else f"{tb:#x}"
            print(f"{a.address:#x} {sa:26s} | {b.address:#x} {sb}")


def closure(work, roots, stops):
    edges = Game(work, True).edges()
    seen, todo = set(), list(roots)
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        if 0x10200000 <= f < 0x10400000 and f not in stops:
            todo.extend(edges.get(f, ()))
    print(" ".join(f"{a:#x}" for a in sorted(seen)))


def pairs(work, xr, orr, words):
    xg, og = Game(work, True), Game(work, False)
    xs = [a for a in xg.functions() if xr[0] <= a < xr[1]]
    os_ = [a for a in og.functions() if orr[0] <= a < orr[1]]
    shapes = {a: [i.mnemonic for i in og.disassemble(a, words)] for a in os_}
    for a in xs:
        mine = [i.mnemonic for i in xg.disassemble(a, words)]
        best = max(((difflib.SequenceMatcher(None, mine, s, autojunk=False).ratio(), b) for b, s in shapes.items()), default=(0, 0))
        print(f"{a:#x} {best[1]:#x} {best[0]:.2f}")


def sites(work, game, first, second):
    g = Game(work, game == "x")
    lo, hi = 0x10200000, 0x102F0000 if game == "x" else 0x10340000
    ins = list(MD.disasm(g.linked[lo - LINKED:hi - LINKED], lo))
    starts = g.functions()
    found = set()
    for k, i in enumerate(ins):
        m = re.match(r"(r\d+), \[r\d+, #" + first + r"\]$", i.op_str)
        if i.mnemonic == "ldr" and m:
            for j in ins[k + 1:k + 5]:
                if re.search(r"\[" + m[1] + r", #" + second + r"\]", j.op_str):
                    found.add(starts[bisect.bisect_right(starts, j.address) - 1])
                    break
    print(" ".join(f"{a:#x}" for a in sorted(found)))


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    work, command, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    hexes = lambda values: [int(v, 16) for v in values]
    if command in ("offsets", "calls"):
        xa, oa = hexes(args[:2])
        words = int(args[2]) if len(args) > 2 else 400
        (offsets if command == "offsets" else calls)(work, xa, oa, words)
    elif command == "pairs":
        ranges = hexes(args[:4])
        pairs(work, ranges[:2], ranges[2:], int(args[4]) if len(args) > 4 else 200)
    elif command == "closure":
        stop = args.index("--stop") if "--stop" in args else len(args)
        closure(work, hexes(args[:stop]), set(hexes(args[stop + 1:])))
    elif command == "sites":
        game, first, second = args[0], args[1], args[2]
        norm = lambda v: hex(int(v, 16))
        sites(work, game, norm(first), norm(second))
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
