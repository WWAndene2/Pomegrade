#!/usr/bin/env python3
"""What one 3DS Pokemon game holds against another, for bringing one's maps into the other (ORAS_ENGINE.md 7: X beside
Omega Ruby). Reads both RomFS with remake_tool (oras-list, oras-extract, garc); writes nothing but its report.

  game_census.py <remake_tool> <oras.3ds> <x.3ds> [archives.tsv]

Prints, for each kind of map data (zones, pieces, matrices, area packs), the archive and member count in each game and
together; the largest zone file against ORAS's zone buffer; the files of a zone container; and the script natives the
second game's zone scripts call that ORAS has not registered (names from ghidra/function_names.tsv). With archives.tsv,
also every a/x/y/z archive of both games: size, entries and the leading two-letter magics of its members.
"""
import collections
import os
import re
import struct
import subprocess
import sys
import tempfile

# Where each game keeps its map data (found by the members' magics: ORAS_ENGINE.md 2, 3 and 7)
KINDS = {  # kind: (magic, ORAS archive, X archive)
    'zones (ZO)': (b'ZO', 'a/0/1/3', 'a/0/1/2'),
    'map pieces (GR)': (b'GR', 'a/0/3/9', 'a/0/4/1'),
    'matrices (MM)': (b'MM', 'a/0/4/0', 'a/0/4/2'),
    'area packs (AD)': (b'AD', 'a/0/1/4', 'a/0/1/3'),
}
ZONE_BUFFER = 0x4A58   # a zone file decompressed larger is fatal in ORAS (ORAS_ENGINE.md 2: the word at 0x3D9884)


def members(tool, game, archive, tmp):
    """the archive's members, decompressed, in member order"""
    path = os.path.join(tmp, 'archive.bin')
    out = os.path.join(tmp, 'members')
    subprocess.run([tool, 'oras-extract', game, archive, path], check=True, capture_output=True)
    subprocess.run(['rm', '-rf', out], check=True)
    subprocess.run([tool, 'garc', path, out], check=True, capture_output=True)
    def order(name):
        m = re.match(r'(\d+)(?:_(\d+))?\.', name)
        return (int(m.group(1)), int(m.group(2) or 0))
    files = sorted(os.listdir(out), key=order)
    data = [open(os.path.join(out, f), 'rb').read() for f in files]
    subprocess.run(['rm', '-rf', out, path], check=True)
    return data


def container_files(blob):
    """a two-letter container's files (u16 count after the magic, then count + 1 u32 offsets)"""
    count = struct.unpack_from('<H', blob, 2)[0]
    offs = [struct.unpack_from('<I', blob, 4 + 4 * i)[0] for i in range(count + 1)]
    return [blob[offs[i]:offs[i + 1]] for i in range(count)]


def amx_natives(blob):
    """the native hashes of every AMX script inside blob (magic 0xF1E0 at +4, natives table at +0x24 to +0x28)"""
    found = set()
    for k in range(0, len(blob) - 0x2C, 4):
        if struct.unpack_from('<H', blob, k + 4)[0] != 0xF1E0:
            continue
        size = struct.unpack_from('<I', blob, k)[0]
        if not 56 <= size <= len(blob) - k:
            continue
        defsize = struct.unpack_from('<H', blob, k + 10)[0]
        nat, lib = struct.unpack_from('<II', blob, k + 0x24)
        if not defsize or not k + nat <= k + lib <= k + size:
            continue
        found.update(struct.unpack_from('<I', blob, k + o + 4)[0] for o in range(nat, lib, defsize))
    return found


def zone_natives(zones):
    """per native hash, how many zones call it (their events and script files); and each zone's set of natives"""
    calls = collections.Counter()
    per_zone = []
    for z in zones:
        files = container_files(z)
        natives = amx_natives(files[1]) | amx_natives(files[2])
        per_zone.append(natives)
        calls.update(natives)
    return calls, per_zone


def known_natives():
    """the script natives ORAS registers, by hash (ghidra/function_names.tsv, named from the game's own tables)"""
    names = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ghidra', 'function_names.tsv')
    known = set()
    for line in open(names, encoding='utf-8'):
        m = re.search(r'hash 0x([0-9A-F]{8})', line)
        if m and 'Script_Native' in line:
            known.add(int(m.group(1), 16))
    return known


def archive_list(tool, game):
    listing = subprocess.run([tool, 'oras-list', game], check=True, capture_output=True, text=True).stdout
    return [line.split()[1] for line in listing.splitlines()[1:] if re.fullmatch(r'a/\d/\d/\d', line.split()[1])]


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    tool, oras, other = sys.argv[1:4]
    with tempfile.TemporaryDirectory() as tmp:
        data = {}
        print('kind\tORAS\tX\ttogether')
        for kind, (magic, a_oras, a_x) in KINDS.items():
            m_o = [m for m in members(tool, oras, a_oras, tmp) if m[:2] == magic]
            m_x = [m for m in members(tool, other, a_x, tmp) if m[:2] == magic]
            data[kind] = (m_o, m_x)
            print(f'{kind}\t{len(m_o)} ({a_oras})\t{len(m_x)} ({a_x})\t{len(m_o) + len(m_x)}')
        z_o, z_x = data['zones (ZO)']
        print(f'largest zone file: ORAS {max(map(len, z_o))}, X {max(map(len, z_x))} bytes; ORAS zone buffer {ZONE_BUFFER}')
        print('zone container files: ORAS', collections.Counter(len(container_files(z)) for z in z_o),
              '| X', collections.Counter(len(container_files(z)) for z in z_x))
        n_o, p_o = zone_natives(z_o)
        n_x, p_x = zone_natives(z_x)
        known = known_natives()
        missing = sorted((h for h in n_x if h not in n_o and h not in known), key=lambda h: -n_x[h])
        hit = sum(bool(z & set(missing)) for z in p_x)
        print(f'zones with scripts: ORAS {sum(map(bool, p_o))}, X {sum(map(bool, p_x))}; '
              f'natives called: ORAS {len(n_o)}, X {len(n_x)}')
        print(f"X natives ORAS does not register: {len(missing)}; X's zones calling at least one: {hit} of {len(z_x)}; "
              'most called (zones): ' + ', '.join(f'0x{h:08X} ({n_x[h]})' for h in missing[:10]))
        if len(sys.argv) > 4:
            with open(sys.argv[4], 'w') as out:
                out.write('game\tarchive\tbytes\tentries\tmagics\n')
                for name, game in (('ORAS', oras), ('X', other)):
                    for a in archive_list(tool, game):
                        try:
                            ms = members(tool, game, a, tmp)
                        except subprocess.CalledProcessError:
                            out.write(f'{name}\t{a}\t\tunreadable\t\n')
                            continue
                        magics = collections.Counter(m[:2].decode('latin-1') if m[:2].isalpha() else '?' for m in ms)
                        out.write(f'{name}\t{a}\t{sum(map(len, ms))}\t{len(ms)}\t' +
                                  ' '.join(f'{k}:{v}' for k, v in magics.most_common(4)) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
