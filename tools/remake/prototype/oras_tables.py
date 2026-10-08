#!/usr/bin/env python3
"""ORAS's game tables read the way the code reads them (ORAS_ENGINE.md 2 and 6): what took throwaway scripts on 7 October.

  oras_tables.py files <file>                     the files of a two-letter container (ZO, EN, ...): magic, count, sizes
  oras_tables.py encounters <work dir> <zone>     a zone's wild encounter tables (its own member's file 3)
  oras_tables.py trainer <work dir> <id>...       a trainer's record and team
  oras_tables.py native-zones <work dir> NAME     the zones whose files hold the hash of a script native (a zone script
                                                  that imports it; amx_natives.py names the natives)

<work dir> is the session's work folder (session_setup.sh): the archives are extracted once into <work>/tables/ with its
build-remake/remake_tool from dumps/oras.3ds, so nothing of the game is stored here.

What is read in the code and what is only seen in the data:
  - container (BinLinker_GetFile 0x12FB40, read): 2 letters, u16 count, count + 1 u32 offsets from +4.
  - encounters (Field_WildEncounter_StepCheck 0x102CD2B4, SelectTable 0x102BFBA4, PickSlotAndLevel 0x102DCA88, read):
    file 3 of the zone's member of a/0/1/3; byte +8 the horde chance; tables of kinds 0-8 at +0xE, +0x3E, +0x6E, +0x7A,
    +0x8E, +0xA2, +0xAE, +0xBA, +0xC6 (kind 0 grass; the others' meaning not read); a slot is 4 bytes: species bits 0-10,
    form bits 11-15 (31 empty), minimum level, maximum level. A table's slot count is taken to run to the next table's
    offset (inferred), the last to the end of the file.
  - trainers (Trainer_ReadData 0x453E08, Trainer_BuildTeams 0x454434, read): member <id> of a/0/3/6, 24 bytes: +0 the
    team's format, u16 +2 the class (a/0/3/7), +7 the number of Pokemon; the team is member <id> of a/0/3/8, entries of
    8, 16, 10 or 18 bytes for formats 0-3, species u16 +4, form u8 +6; u16 +2 of an entry is taken for the level (seen:
    the rival's starters at 5, not read in the code).
"""
import os
import struct
import subprocess
import sys

KIND_OFFSETS = [0x0E, 0x3E, 0x6E, 0x7A, 0x8E, 0xA2, 0xAE, 0xBA, 0xC6]
# slots and weights of each kind (Field_WildEncounter_PickSlotMode0-6, read 8 October); the names are inferred from the weights
# and slot counts, which match the game's known methods, except the water kind (3), which the step check picks on a tile flag
KIND_NAMES = ["grass", "grass 2", "kind 2", "surf (water)", "rock smash?", "rod 1", "rod 2", "rod 3", "hordes (3 x 5)"]  # rods: listed together (Encounter_ListSpeciesByCategory)
KIND_WEIGHTS = [[10] * 9 + [5, 4, 1]] * 2 + [[60, 35, 5], [50, 30, 15, 4, 1], [50, 30, 15, 4, 1]] + [[60, 35, 5]] * 3 + [[60, 35, 5]]
TEAM_ENTRY = {0: 8, 1: 16, 2: 10, 3: 18}


def container_files(data):
    """the files of a two-letter container, as BinLinker_GetFile reads them"""
    count = struct.unpack_from("<H", data, 2)[0]
    offsets = struct.unpack_from(f"<{count + 1}I", data, 4)
    return [data[offsets[i]:offsets[i + 1]] for i in range(count)]


def archive(work, path):
    """the members of an archive of the RomFS (a/0/1/3 ...), extracted once to <work>/tables/<path> by remake_tool"""
    out = os.path.join(work, "tables", path.replace("/", ""))
    if not os.path.isdir(out):
        os.makedirs(out)
        tool, rom = os.path.join(work, "build-remake", "remake_tool"), os.path.join(work, "dumps", "oras.3ds")
        garc = out + ".garc"
        subprocess.run([tool, "oras-extract", rom, path, garc], check=True, stdout=subprocess.DEVNULL)
        subprocess.run([tool, "garc", garc, out], check=True, stdout=subprocess.DEVNULL)
        os.remove(garc)
    return lambda i: open(os.path.join(out, f"{i}.unknown"), "rb").read()


def cmd_files(path):
    data = open(path, "rb").read()
    files = container_files(data)
    print(f"{data[:2].decode('latin-1')} container, {len(files)} files")
    for i, f in enumerate(files):
        print(f"  file {i}: {len(f)} bytes")


def cmd_encounters(work, zone):
    zone = int(zone)
    data = container_files(archive(work, "a/0/1/3")(zone))[3]
    if not data:
        print(f"zone {zone}: no encounter file")
        return
    print(f"zone {zone}: encounter file {len(data)} bytes, horde chance byte +8 = {data[8]}")
    ends = KIND_OFFSETS[1:] + [len(data) - len(data) % 4]
    for kind, (start, end) in enumerate(zip(KIND_OFFSETS, ends)):
        slots = []
        for at in range(start, min(end, len(data)) - 3, 4):
            half, low, high = struct.unpack_from("<HBB", data, at)
            if half >> 11 == 31 or half == 0:
                continue
            slots.append(f"{half & 0x7FF}" + (f"/{half >> 11}" if half >> 11 else "") + f" L{low}-{high}")
        if slots:
            print(f"  kind {kind} {KIND_NAMES[kind]} (+0x{start:X}, weights {KIND_WEIGHTS[kind]}): " + ", ".join(slots))


def cmd_trainer(work, ids):
    records, teams = archive(work, "a/0/3/6"), archive(work, "a/0/3/8")
    for i in map(int, ids):
        t, team = records(i), teams(i)
        if len(t) < 24:
            print(f"trainer {i}: {len(t)}-byte record (a placeholder)")
            continue
        fmt, count = t[0], t[7]
        size = TEAM_ENTRY.get(fmt)
        print(f"trainer {i}: class {struct.unpack_from('<H', t, 2)[0]}, format {fmt}, {count} Pokemon; record {t.hex()}")
        if size is None or size * count != len(team):
            print(f"  team: {len(team)} bytes, does not match {count} x the format's entry")
            continue
        for k in range(count):
            e = team[k * size:(k + 1) * size]
            print(f"  {k + 1}: species {struct.unpack_from('<H', e, 4)[0]} form {e[6]} level {struct.unpack_from('<H', e, 2)[0]}"
                  f"  ({e.hex()})")


def cmd_native_zones(work, name):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from amx_natives import name_hash
    h = struct.pack("<I", name_hash(name))
    zones = archive(work, "a/0/1/3")
    found = [z for z in range(536) if h in zones(z)]
    print(f"{name} (0x{name_hash(name):08X}): {len(found)} zones: " + " ".join(map(str, found)))


def main():
    a = sys.argv[1:]
    if len(a) == 2 and a[0] == "files":
        cmd_files(a[1])
    elif len(a) == 3 and a[0] == "encounters":
        cmd_encounters(a[1], a[2])
    elif len(a) >= 3 and a[0] == "trainer":
        cmd_trainer(a[1], a[2:])
    elif len(a) == 3 and a[0] == "native-zones":
        cmd_native_zones(a[1], a[2])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
