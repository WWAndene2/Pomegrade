#!/usr/bin/env python3
"""One game kept running for a whole test, driven command batch by command batch, with files swapped while it runs
(23 October: tests took a run per step, each booting the game again, and a run per file change).

  session.py start <work> <name> --mod <mod dir> [--save FILE] [--move "ZONE X Z"] [--hot PATH]...
  session.py do    <work> <name> "<commands>" [--timeout SECONDS]
  session.py swap  <work> <name> PATH=FILE | PATH:N=FILE ...
  session.py stop  <work> <name>
  session.py list  <work>

start   boots the game once (fast_run.sh: from the mod's title state, made on the first boot of a mod) to the field and
        leaves it waiting (retro_host "interactive"); the run is <work>/runs/<name>.
do      runs retro_host commands (';'-separated: wait, hold, press, until, burst, shot, save, load, mem, ...) and prints
        only what they reported (the core's chatter dropped); shots become PNG files, and two or more are put on a sheet,
        <run>/sheet_<batch>.png, whose path is printed: read the sheet, not each shot.
swap    replaces a game file (PATH) or one archive member (PATH:N) in the running game's mod folder (named with --hot
        at start: given whole in romfs/, its patch dropped). NOT WORKING YET for the battle sequences (23 October,
        session s3): a/0/3/4's members swapped, then "load" (which rebuilds LayeredFS from the disk: its log says so),
        and the next battle played the old intro. Likely, not checked: the game reads that archive at boot and keeps
        it, and "load" restores that memory. Until a file is shown to be read after the state, change it with a new
        session (start).
stop    ends the game.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PROGRAM = "000400000011C400"
# the core's chatter: kept are the modules loaded and unloaded (LoadCRO, UnloadCRO), errors and the host's own lines
NOISE = re.compile(r"SetProbeResponseParam|Delay generator|Path not found|FetchVariable|LayeredFS|cro_helper\.cpp|STUBBED|"
                   r"Network isn't|ClCertA|different revision")


def paths(work, name):
    run = os.path.join(work, "runs", name)
    return run, os.path.join(run, "play.fifo"), os.path.join(run, "log.txt"), os.path.join(work, "sessions", name + ".json")


def tool(work):
    return os.path.join(work, "build-remake", "remake_tool")


def materialize(work, mod, path, out):
    subprocess.run([tool(work), "mod-file", os.path.join(work, "dumps", "oras.3ds"), mod, path, out], check=True,
                   stdout=subprocess.DEVNULL)


def ready_count(log):
    try:
        return sum(1 for l in open(log, errors="replace") if l.startswith("[ready frame"))
    except FileNotFoundError:
        return 0


def start(a):
    work = os.path.abspath(a.work)
    run, fifo, log, state = paths(work, a.name)
    prepared = os.path.join(work, "sessions", a.name + "_mod")
    shutil.rmtree(prepared, ignore_errors=True)
    shutil.copytree(a.mod, prepared, symlinks=True)
    base = os.path.join(prepared, "load", "mods", PROGRAM)
    hot = {}
    for path in a.hot:
        out = os.path.join(base, "romfs", path)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        materialize(work, a.mod, path, out + ".tmp")
        data = open(out + ".tmp", "rb").read()
        os.remove(out + ".tmp")
        hot[path] = len(data)
        open(out, "wb").write(data)
        bps = os.path.join(base, "romfs_ext", path + ".bps")
        if os.path.exists(bps):
            os.remove(bps)
    os.makedirs(os.path.dirname(state), exist_ok=True)
    json.dump({"mod": os.path.abspath(a.mod), "hot": hot, "batches": 0}, open(state, "w"))
    env = dict(os.environ)
    if a.save:
        env["POMEGRADE_SAVE"] = os.path.abspath(a.save)
    cmd = [os.path.join(HERE, "fast_run.sh"), work, a.name, prepared, a.move or "",
           "draw every 15;interactive " + fifo, "100000"]
    subprocess.Popen(cmd, env=env, stdout=open(os.path.join(work, "sessions", a.name + ".out"), "w"),
                     stderr=subprocess.STDOUT, start_new_session=True)
    t0 = time.time()
    while time.time() - t0 < 900:
        if os.path.exists(fifo) and ready_count(log) > 0:
            print(f"{a.name}: ready after {time.time() - t0:.0f} s (run {run}); hot files: {', '.join(hot) or 'none'}")
            return
        time.sleep(2)
    raise SystemExit(f"{a.name}: not ready after 900 s; see {log}")


def do(a):
    work = os.path.abspath(a.work)
    run, fifo, log, state = paths(work, a.name)
    if not os.path.exists(fifo):
        raise SystemExit(f"{a.name}: no session (start it first)")
    info = json.load(open(state))
    before, lines = ready_count(log), sum(1 for _ in open(log, errors="replace"))
    t0 = time.time()
    with open(fifo, "w") as f:
        f.write("\n".join(c.strip() for c in a.commands.split(";") if c.strip()) + "\n")
    while ready_count(log) <= before:
        if time.time() - t0 > a.timeout:
            print(f"(still running after {a.timeout} s: its output so far)")
            break
        time.sleep(0.5)
    shots = []
    for l in open(log, errors="replace").readlines()[lines:]:
        l = l.rstrip("\n")
        if NOISE.search(l) or l.startswith("[ready frame"):
            continue
        m = re.match(r"shot \S*?([^/\s]+)\.ppm at frame", l)
        if m:
            shots.append(m.group(1))
            continue
        print(l)
    for n in shots:
        ppm = os.path.join(run, n + ".ppm")
        if os.path.exists(ppm):
            subprocess.run([sys.executable, "-c", "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])",
                            ppm, ppm[:-4] + ".png"], check=True)
            os.remove(ppm)
    info["batches"] += 1
    json.dump(info, open(state, "w"))
    if len(shots) >= 2:
        sheet = os.path.join(run, f"sheet_{info['batches']}.png")
        only = os.path.commonprefix(shots)
        subprocess.run([sys.executable, os.path.join(HERE, "..", "prototype", "shot_sheet.py"), run, sheet, "--only", only],
                       check=True, stdout=subprocess.DEVNULL)
        print(f"{len(shots)} shots: {sheet}")
    elif shots:
        print(f"shot: {os.path.join(run, shots[0] + '.png')}")
    print(f"({time.time() - t0:.0f} s)")


def swap(a):
    work = os.path.abspath(a.work)
    run, fifo, log, state = paths(work, a.name)
    info = json.load(open(state))
    base = os.path.join(run, "Azahar", "load", "mods", PROGRAM, "romfs")
    for change in a.changes:
        target, file = change.split("=", 1)
        path, _, member = target.partition(":")
        if path not in info["hot"]:
            raise SystemExit(f"swap: {path} was not named with --hot at start (its size is fixed at boot)")
        live = os.path.join(base, path)
        data = open(file, "rb").read()
        if member:
            scratch = os.path.join(work, "sessions", a.name + "_swap")
            os.makedirs(scratch, exist_ok=True)
            subprocess.run([tool(work), "garc-set", live, os.path.join(scratch, "new"), member, file], check=True,
                           stdout=subprocess.DEVNULL)
            data = open(os.path.join(scratch, "new"), "rb").read()
            shutil.rmtree(scratch)
        open(live + ".tmp", "wb").write(data)
        os.replace(live + ".tmp", live)
        same = len(data) == info["hot"][path]
        info["hot"][path] = len(data)
        print(f"{path}{':' + member if member else ''}: {file}, {len(data)} bytes: "
              + ("the same size, read as it is from the game's next read" if same else "counts from the next \"load NAME\""))
    json.dump(info, open(state, "w"))


def stop(a):
    work = os.path.abspath(a.work)
    run, fifo, log, state = paths(work, a.name)
    if os.path.exists(fifo):
        with open(fifo, "w") as f:
            f.write("quit\n")
    for p in (state, os.path.join(work, "sessions", a.name + ".out")):
        if os.path.exists(p):
            os.remove(p)
    shutil.rmtree(os.path.join(work, "sessions", a.name + "_mod"), ignore_errors=True)
    print(f"{a.name}: stopped")


def listing(a):
    folder = os.path.join(os.path.abspath(a.work), "sessions")
    for f in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        if f.endswith(".json"):
            info = json.load(open(os.path.join(folder, f)))
            print(f"{f[:-5]}: mod {info['mod']}, hot {', '.join(info['hot']) or 'none'}, {info['batches']} batches")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="what", required=True)
    s = sub.add_parser("start"); s.add_argument("work"); s.add_argument("name"); s.add_argument("--mod", required=True)
    s.add_argument("--save"); s.add_argument("--move"); s.add_argument("--hot", action="append", default=[])
    d = sub.add_parser("do"); d.add_argument("work"); d.add_argument("name"); d.add_argument("commands")
    d.add_argument("--timeout", type=int, default=900)
    w = sub.add_parser("swap"); w.add_argument("work"); w.add_argument("name"); w.add_argument("changes", nargs="+")
    t = sub.add_parser("stop"); t.add_argument("work"); t.add_argument("name")
    l = sub.add_parser("list"); l.add_argument("work")
    a = ap.parse_args()
    {"start": start, "do": do, "swap": swap, "stop": stop, "list": listing}[a.what](a)


if __name__ == "__main__":
    main()
