#!/usr/bin/env python3
"""run_local.sh with a cache of checkpoints (owner's request, 8 October): a run starts from the furthest point a previous run
already reached instead of from the boot.

  run_cached.py <work dir> <name> <mod dir|-> "<zone x z>|" "<script>" [seconds]      (as run_local.sh; same environment)

In the script, "checkpoint NAME" marks a point worth keeping (the title, the field loaded, the player in front of a trainer).
The first run plays everything and saves the emulator's state at each checkpoint into <work>/cache/; a later run whose script
begins the same way loads the furthest cached checkpoint ("wait 120; load", the core must have booted first) and plays only
what follows. A checkpoint's key is everything that leads to it: the core and retro_host builds, the game image, the mod's
files, the save (POMEGRADE_SAVE or dumps/save/main, and the zone x z it is moved to), POMEGRADE_INTERPRETER, and every command
before it; change any and the state is made again. Prints what it reused and saved.

Limits (retro_host.cpp, run_local.sh): a state does not restore an asset load in progress, so put checkpoints where the game
waits (a few seconds after a screen settles); a state loaded under the interpreter was made under it too (the key holds the
mode). Checked on 8 October: a state saved on the title screen tests the save given afterwards (save_test.sh's controls: the
game reads the save after the title).
"""
import hashlib
import os
import shutil
import subprocess
import sys


def file_key(path):
    st = os.stat(path)
    return f"{path}:{st.st_size}:{int(st.st_mtime)}"


def dir_key(path):
    h = hashlib.sha256()
    for root, _, files in sorted(os.walk(path)):
        for f in sorted(files):
            p = os.path.join(root, f)
            h.update(p.encode())
            h.update(open(p, "rb").read())
    return h.hexdigest()


def main():
    if len(sys.argv) < 6:
        sys.exit(__doc__)
    work, name, mod, move, script = sys.argv[1:6]
    secs = sys.argv[6] if len(sys.argv) > 6 else "120"
    work = os.path.abspath(work)
    here = os.path.dirname(os.path.abspath(__file__))
    save = os.environ.get("POMEGRADE_SAVE", "dumps/save/main")
    save_path = save if os.path.isabs(save) else os.path.join(work, save)
    base = hashlib.sha256("\n".join([
        file_key(os.path.join(work, "build-azahar/bin/Release/azahar_libretro.so")),
        file_key(os.path.join(work, "retro_host")),
        file_key(os.path.join(work, "dumps/oras.3ds")),
        dir_key(mod) if mod != "-" else "-",
        hashlib.sha256(open(save_path, "rb").read()).hexdigest(), move,
        os.environ.get("POMEGRADE_INTERPRETER", ""),
    ]).encode()).hexdigest()

    commands = [c.strip() for c in script.split(";") if c.strip()]
    cache = os.path.join(work, "cache")
    os.makedirs(cache, exist_ok=True)
    keys = {}  # command index of each checkpoint -> (name, key)
    for i, c in enumerate(commands):
        if c.startswith("checkpoint "):
            prefix = "\n".join(x for x in commands[:i] if not x.startswith("checkpoint "))
            keys[i] = (c.split()[1], hashlib.sha256((base + "\n" + prefix).encode()).hexdigest()[:24])

    start, state = 0, None
    for i in sorted(keys, reverse=True):
        path = os.path.join(cache, keys[i][1] + ".state")
        if os.path.exists(path):
            start, state = i + 1, path
            print(f"reusing checkpoint {keys[i][0]} ({keys[i][1]}): {sum(1 for c in commands[:i] if not c.startswith('checkpoint '))} commands skipped")
            break
    if state is None:
        print("no checkpoint cached for this beginning: playing from the boot")

    body = []
    for i in range(start, len(commands)):
        c = commands[i]
        body.append(f"save cp_{keys[i][0]}" if i in keys else c)
    if state:
        body = ["wait 120", "load cp_resume"] + body
    env = dict(os.environ)
    tmp = None
    if state:
        tmp = os.path.join(cache, "cp_resume.state")
        shutil.copyfile(state, tmp)
        env["POMEGRADE_STATE"] = tmp
    move_arg = "" if state else move  # a resumed run's save is already in the state
    r = subprocess.run([os.path.join(here, "run_local.sh"), work, name, mod, move_arg, ";".join(body), secs], env=env)
    if tmp:
        os.remove(tmp)

    run = os.path.join(work, "runs", name)
    for i, (cp, key) in keys.items():
        made = os.path.join(run, f"cp_{cp}.state")
        if os.path.exists(made) and not os.path.exists(os.path.join(cache, key + ".state")):
            shutil.copyfile(made, os.path.join(cache, key + ".state"))
            print(f"checkpoint {cp} cached ({key})")
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
