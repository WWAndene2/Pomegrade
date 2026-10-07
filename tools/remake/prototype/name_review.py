#!/usr/bin/env python3
"""Finds the names of ghidra/function_names.tsv to read again, and summarises the decompiled functions to read them fast
(ORAS_ENGINE.md 6, "The names reviewed": the method of the 7 October review, which changed 99 names).

  name_review.py grew <functions before> <functions after> <function_names.tsv>
        named functions whose size changed between two Export.java function lists (functions.tsv): named from code
        that was cut short or that changed (after FixNoReturn, 24 besides the natives)
  name_review.py suspects <function_names.tsv> <edges.tsv>
        names marked guess whose subsystem prefix matches none of their named callers and callees (edges.tsv from
        ghidra/CallGraph.java); Util_, Sys_ and Mem_ are left out as used everywhere (105 on 7 October, 19 wrong)
  name_review.py placeholders <function_names.tsv>
        names made of a subsystem guessed from a trace and an address (Warp_LargeUpdate_..., Util_Accessor_...), which
        say nothing reliable; import stubs and thunks named after their target are not placeholders
  name_review.py summary <decomp.c>
        for each function of an Export.java decompilation: its current role, its size, the named functions it calls,
        the strings it uses and its first statements, one block each

The first three print one address per line, as 0x..., ready for Export.java:
  analyzeHeadless <work>/ghidra_proj oras -process code.bin -noanalysis -readOnly -scriptPath tools/remake/ghidra \\
      -postScript Export.java <out> $(name_review.py suspects ...)
A listed name is not wrong by being listed: read the summary (and the code where it does not settle it), keep what fits,
rename from what the code does, and write in the role what it was guessed before.
"""
import collections
import re
import sys

GENERIC = {"Util", "Sys", "Mem"}
PLACEHOLDER = re.compile(r"^(Warp_LargeUpdate|Warp_CheckedStep|Warp_AllocInit|Warp_Helper|Thunk_Warp_Helper|"
                         r"Res_ReleaseHandles|Thunk_Res_ReleaseHandles|Gfx_ProcessEntries|Util_Accessor)_[0-9A-F]{6,8}$")


def key(addr):
    return addr.strip().lower().replace("0x", "").lstrip("0")


def names(path):
    out = {}
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")
        out[key(f[0])] = (f[1], f[3] if len(f) > 3 else "")
    return out


def sizes(path):
    return {key(l.split("\t")[0]): int(l.split("\t")[1]) for l in open(path) if l.strip()}


def emit(addrs):
    for a in sorted(addrs, key=lambda x: int(x, 16)):
        print("0x" + a.upper())


def cmd_grew(before, after, names_path):
    b, a, n = sizes(before), sizes(after), names(names_path)
    emit(x for x in n if x in a and x in b and a[x] != b[x] and not n[x][0].startswith("Script_Native_"))


def cmd_suspects(names_path, edges_path):
    n = names(names_path)
    neighbours = collections.defaultdict(set)
    for line in open(edges_path):
        caller, callee = (key(x) for x in line.split())
        if caller != callee:
            neighbours[caller].add(callee)
            neighbours[callee].add(caller)
    prefix = lambda name: name.split("_")[0]
    found = []
    for addr, (name, status) in n.items():
        if status != "guess" or prefix(name) in GENERIC:
            continue
        around = [prefix(n[x][0]) for x in neighbours[addr] if x in n and prefix(n[x][0]) not in GENERIC]
        if len(around) >= 2 and prefix(name) not in around:
            found.append(addr)
            print(f"{name}: neighbours {dict(collections.Counter(around))}", file=sys.stderr)
    emit(found)


def cmd_placeholders(names_path):
    emit(a for a, (name, _) in names(names_path).items() if PLACEHOLDER.match(name))


def cmd_summary(decomp):
    for block in open(decomp).read().split("// ---- ")[1:]:
        head = block.split("\n")[0]
        m = re.search(r"/\*(.*?)\*/", block, re.S)
        role = " ".join(m.group(1).split()) if m else ""
        body = block[m.end():] if m else block
        calls = sorted(set(re.findall(r"\b([A-Z][a-z]+_[A-Za-z0-9_]+)\(", body)))
        strings = sorted(set(re.findall(r"\bs_([A-Za-z0-9_]+)_[0-9a-f]{8}\b", body)))
        declaration = re.compile(r"\s*(undefined|u?int|code|char|bool|u?short|float|byte)\d* \*?\w")
        statements = " ".join(l.strip() for l in body.split("\n") if l.strip() and not declaration.match(l))
        print(f"### {head} | {body.count(chr(10))} lines\n  role: {role[:240]}\n  calls: {', '.join(calls)[:400]}\n"
              f"  strings: {', '.join(strings)[:200]}\n  code: {statements[:300]}")


def main():
    a = sys.argv[1:]
    commands = {"grew": (cmd_grew, 3), "suspects": (cmd_suspects, 2), "placeholders": (cmd_placeholders, 1),
                "summary": (cmd_summary, 1)}
    if not a or a[0] not in commands or len(a) - 1 != commands[a[0]][1]:
        print(__doc__)
        sys.exit(2)
    commands[a[0]][0](*a[1:])


if __name__ == "__main__":
    main()
