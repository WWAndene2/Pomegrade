#!/usr/bin/env python3
"""The functions an activity of the game runs (ORAS_ENGINE.md 6): the code blocks a headless run traced ("trace on" /
"trace off FILE", retro_host with POMEGRADE_INTERPRETER=1) mapped to the functions of the Ghidra program, the code modules
placed where the run loaded them (its log's "CRO "<name>" loaded at 0x..." lines) and moved to where cro_link.py links them.

  coverage_map.py <work dir> <run> <trace file> [--minus <trace file>]... [--decompile]

<work dir>   the session's work folder (session_setup.sh): functions.tsv (ghidra/Export.java's list, written at <work dir>),
             linked/modules.tsv
<run>        runs/<run>: its log.txt and the trace files
--minus      traces whose functions are taken out (an activity minus the idle field: what the activity alone runs);
             <run>/<trace> takes a trace of another run of the session

Prints one line per function: its address in the Ghidra program, where it lies (.code or a module and its offset), the
traced blocks in it; with --decompile, writes their addresses to <run>/<trace>.functions for Export.java.
"""
import bisect
import os
import re
import sys


def load_modules(work):
    linked = {}
    for line in open(os.path.join(work, "linked", "modules.tsv")):
        if not line.startswith("\t"):
            name, base, size = line.rstrip("\n").split("\t")
            linked[name] = (int(base, 16), int(size, 16))
    return linked


def loads(log):
    """the modules the run loaded and unloaded, in order: (log line number, name, address, None for an unload)"""
    out = []
    for n, line in enumerate(open(log, errors="replace")):
        m = re.search(r'CRO "([^"]+)" loaded at 0x([0-9A-F]+)', line)
        if m:
            out.append((n, m.group(1), int(m.group(2), 16)))
        m = re.search(r'Unloading CRO "([^"]+)"', line)
        if m:
            out.append((n, m.group(1), None))
    return out


def trace_window(log, trace):
    """the log lines where the trace started (the last "> trace on" before its end) and was written ("trace off: N blocks
    in .../<trace>")"""
    start = 0
    for n, line in enumerate(open(log, errors="replace")):
        if re.match(r">\s*trace on", line):
            start = n
        if "trace off" in line and line.rstrip().endswith(trace):
            return start, n
    raise SystemExit(f"no 'trace off' for {trace} in the log")


def to_program(addresses, window, module_loads, linked):
    """runtime addresses -> addresses of the Ghidra program (the .code as is, a module's at its linked base). A trace does
    not say when a block ran, so where the game loaded another module at the same address during the trace (DllField
    replaced by DllBattle at 0x6F3000 when a battle starts, run enc2) a block is given to every module that stood there,
    its place marked "name?": subtract a trace of the other activity (--minus) to drop the wrong ones."""
    start, end = window
    loaded, during = {}, []  # loaded: the modules in memory when the trace started (name -> address)
    for n, name, address in module_loads:
        if n >= start:
            if n < end and address is not None:
                during.append((name, address))  # loaded while the trace ran
        elif address is None:
            loaded.pop(name, None)
        else:
            loaded[name] = address
    placed = {}
    for name, address in list(loaded.items()) + during:
        if name not in placed.setdefault(address, []):
            placed[address].append(name)
    regions = sorted((a, a + linked[name][1], name, len(names) > 1)
                     for a, names in placed.items() for name in names if name in linked)
    out = []
    for a in addresses:
        if a < 0x630000:
            out.append((a, ".code", a - 0x100000))
            continue
        for lo, hi, name, shared in regions:
            if lo <= a < hi:
                out.append((linked[name][0] + a - lo, name + ("?" if shared else ""), a - lo))
    return out


def main():
    work, run, trace = sys.argv[1], sys.argv[2], sys.argv[3]
    minus = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == "--minus"]
    rundir = os.path.join(work, "runs", run)
    log = os.path.join(rundir, "log.txt")
    linked = load_modules(work)
    module_loads = loads(log)
    entries = sorted(int(line.split("\t")[0], 16) for line in open(os.path.join(work, "functions.tsv")))

    def functions(name):
        # <trace> of this run, or <run>/<trace> of another run of the same session
        other = os.path.join(work, "runs", name) if "/" in name else None
        rd, lg = (os.path.dirname(other), os.path.join(os.path.dirname(other), "log.txt")) if other else (rundir, log)
        ml = loads(lg) if other else module_loads
        addresses = [int(x, 16) for x in open(os.path.join(rd, os.path.basename(name)))]
        counts = {}
        for program, where, offset in to_program(addresses, trace_window(lg, os.path.basename(name)), ml, linked):
            k = bisect.bisect_right(entries, program) - 1
            if k >= 0:
                f = entries[k]
                counts.setdefault(f, [where, offset - (program - f), 0])[2] += 1
        return counts

    own = functions(trace)
    for m in minus:
        for f in functions(m):
            own.pop(f, None)
    for f in sorted(own):
        where, offset, blocks = own[f]
        print(f"{f:08X}\t{where}\t0x{offset:X}\t{blocks}")
    print(f"{len(own)} functions", file=sys.stderr)
    if "--decompile" in sys.argv:
        with open(os.path.join(rundir, trace + ".functions"), "w") as w:
            w.write(" ".join(f"0x{f:X}" for f in sorted(own)))


if __name__ == "__main__":
    main()
