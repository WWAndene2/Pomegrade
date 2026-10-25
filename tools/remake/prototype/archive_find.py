#!/usr/bin/env python3
"""A game's archives searched (25 October: how X's effect files, a/0/6/6, and its per-area model lists, a/1/4/9, were
found by bytes a run had dumped from memory, ORAS_ENGINE.md 0.0).

  archive_find.py bytes <game.3ds> <hex>           the archives and members holding these bytes
  archive_find.py count <game.3ds> <low> <high>    the archives with that many members (GARC's FATO count)

Each a/X/Y/Z archive is extracted with remake_tool (REMAKE_TOOL, default build-remake/remake_tool), its members searched
as stored, LZ11-decompressed, and inside 2-letter containers ("TM", "GR", ...: the container itself is searched whole).
Files go to a temporary folder, removed after.
"""
import os
import struct
import subprocess
import sys
import tempfile


def lz11(b):
    if b[0] != 0x11:
        raise ValueError('not LZ11')
    n, i = b[1] | b[2] << 8 | b[3] << 16, 4
    if n == 0:
        n, i = int.from_bytes(b[4:8], 'little'), 8
    out = bytearray()
    while len(out) < n:
        flags = b[i]; i += 1
        for k in range(8):
            if len(out) >= n:
                break
            if not flags & (0x80 >> k):
                out.append(b[i]); i += 1
                continue
            t = b[i] >> 4
            if t == 0:
                c, d, i = (((b[i] & 15) << 4) | (b[i + 1] >> 4)) + 0x11, (((b[i + 1] & 15) << 8) | b[i + 2]) + 1, i + 3
            elif t == 1:
                c = (((b[i] & 15) << 12) | (b[i + 1] << 4) | (b[i + 2] >> 4)) + 0x111
                d, i = (((b[i + 2] & 15) << 8) | b[i + 3]) + 1, i + 4
            else:
                c, d, i = t + 1, (((b[i] & 15) << 8) | b[i + 1]) + 1, i + 2
            for _ in range(c):
                out.append(out[-d])
    return bytes(out)


def archives(tool, game):
    listing = subprocess.run([tool, 'oras-list', game], capture_output=True, text=True, check=True).stdout.splitlines()[1:]
    return [line.split()[1] for line in listing if line.split()[1].startswith('a/')]


def member_count(data):
    at = data.find(b'OTAF')
    return struct.unpack_from('<H', data, at + 8)[0] if data[:4] == b'CRAG' and at >= 0 else None


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ('bytes', 'count'):
        sys.exit(__doc__)
    tool = os.environ.get('REMAKE_TOOL', 'build-remake/remake_tool')
    mode, game = sys.argv[1], sys.argv[2]
    with tempfile.TemporaryDirectory() as tmp:
        garc = os.path.join(tmp, 'archive.garc')
        for path in archives(tool, game):
            subprocess.run([tool, 'oras-extract', game, path, garc], capture_output=True, check=True)
            data = open(garc, 'rb').read()
            count = member_count(data)
            if mode == 'count':
                if count is not None and int(sys.argv[3], 0) <= count <= int(sys.argv[4], 0):
                    print(path, count, len(data), flush=True)
                continue
            wanted = bytes.fromhex(sys.argv[3])
            hits = ['the archive as stored'] if wanted in data else []
            if count is not None:
                members = os.path.join(tmp, path.replace('/', '_'))
                if subprocess.run([tool, 'garc', garc, members], capture_output=True).returncode != 0:
                    print(path, '(its members not read: remake_tool garc refused it)', file=sys.stderr, flush=True)
                    os.makedirs(members, exist_ok=True)
                for name in sorted(os.listdir(members), key=lambda f: int(f.split('.')[0])):
                    m = open(os.path.join(members, name), 'rb').read()
                    if m[:1] == b'\x11':
                        try:
                            m = lz11(m)
                        except (ValueError, IndexError):
                            pass
                    if wanted in m:
                        hits.append('member %s (%d bytes, %r)' % (name.split('.')[0], len(m), m[:4]))
            if hits:
                print(path, '; '.join(hits[:8]), flush=True)


if __name__ == '__main__':
    main()
