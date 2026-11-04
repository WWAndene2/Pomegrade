#!/usr/bin/env python3
"""A measuring patch for ORAS's .code: an instruction becomes a call to a routine in the .code's free bytes that writes, each
time it runs, r0, the caller's return address and words of r0's object into a log the rig's `mem` reads back, then runs
the instruction itself (4 November: which camera Gfx_CameraUpdateProjection's perspective branch builds last, run ws_log;
the answer was a default camera from 0x14DF8C, not the field's).

  hook_log_ips.py <code.bin> <out code.ips> --hook ADDRESS [--lr-at OFFSET] [--field OFFSET]... [--cave A] [--log A]

--hook    the instruction to hook. It is copied into the routine and run there, so it must not read pc or lr; the call
          overwrites lr, so the function must have saved its own (pushed it) before that point: both are the caller's to check
--lr-at   where the function's saved lr lies above sp at the hook (push {r4, r5, lr} + vpush {d8-d10} + sub sp, #12: 44);
          logged as the second word, else 0
--field   a word of r0's object to log (an offset, several allowed), after r0 and the return address; the log holds 4 words
--cave, --log  the routine and the log: by default 0x579F44 and 0x579F70, free in Omega Ruby 1.4's .code (zeros between the
          sound routine ending at 0x579F43 and 0x579F80, OrasEngine.cpp); checked to be zero here. Not for a mod that
          uses those bytes: the IPS is a measurement, applied alone (load/mods/000400000011C400/exefs/code.ips)

Then, after a run reached the place:  mem 0x579F70 16  (fast_run.sh script) prints the log.
"""
import argparse
import struct

BASE = 0x100000


def main():
    p = argparse.ArgumentParser()
    p.add_argument("code")
    p.add_argument("out")
    p.add_argument("--hook", type=lambda s: int(s, 0), required=True)
    p.add_argument("--lr-at", type=lambda s: int(s, 0))
    p.add_argument("--field", type=lambda s: int(s, 0), action="append", default=[])
    p.add_argument("--cave", type=lambda s: int(s, 0), default=0x579F44)
    p.add_argument("--log", type=lambda s: int(s, 0), default=0x579F70)
    a = p.parse_args()
    code = open(a.code, "rb").read()
    word = lambda at: struct.unpack_from("<I", code, at - BASE)[0]
    if len(a.field) > 2:
        raise SystemExit("the log holds 4 words: r0, the return address and 2 fields")
    hooked = word(a.hook)
    if (hooked >> 16) & 0xF == 15 or (hooked >> 12) & 0xF == 15 or (hooked & 0xF) == 15 or (hooked & 0xF) == 14:
        raise SystemExit("the hooked instruction 0x%08X may read pc or lr: pick another" % hooked)
    routine = [0xE92D0006]                                   # push {r1, r2}
    lit_slot = len(routine)
    routine.append(0)                                        # ldr r1, =log (filled below)
    routine.append(0xE5810000)                               # str r0, [r1]
    if a.lr_at is not None:
        routine += [0xE59D2000 | (a.lr_at + 8), 0xE5812004]  # ldr r2, [sp, #lr + 8]; str r2, [r1, #4]
    for k, off in enumerate(a.field):
        routine += [0xE5902000 | off, 0xE5812000 | (8 + 4 * k)]  # ldr r2, [r0, #off]; str r2, [r1, #8 + 4k]
    routine += [0xE8BD0006, hooked, 0xE12FFF1E]              # pop {r1, r2}; the hooked instruction; bx lr
    lit_at = a.cave + 4 * len(routine)
    routine[lit_slot] = 0xE59F1000 | (lit_at - (a.cave + 4 * lit_slot) - 8)
    routine.append(a.log)
    end = a.cave + 4 * len(routine)
    if end > a.log or any(code[a.cave - BASE:a.log + 16 - BASE]):
        raise SystemExit("the routine (to 0x%X) and the log need zeros from 0x%X to 0x%X" % (end, a.cave, a.log + 16))
    bl = 0xEB000000 | (((a.cave - a.hook - 8) >> 2) & 0xFFFFFF)
    body = b"".join(struct.pack("<I", w) for w in routine)
    ips = b"PATCH"
    ips += (a.hook - BASE).to_bytes(3, "big") + (4).to_bytes(2, "big") + struct.pack("<I", bl)
    ips += (a.cave - BASE).to_bytes(3, "big") + len(body).to_bytes(2, "big") + body + b"EOF"
    open(a.out, "wb").write(ips)
    print("hook 0x%X (0x%08X) -> routine 0x%X-0x%X, log 0x%X: r0, return address, %s"
          % (a.hook, hooked, a.cave, end - 1, a.log, ", ".join("+0x%X" % f for f in a.field) or "no field"))


if __name__ == "__main__":
    main()
