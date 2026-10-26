#!/usr/bin/env python3
"""X/Y's sounds against Omega Ruby's same-named ones, by their data (26 October, step 2's inventory, ORAS_ENGINE.md 0.0):
which are the same (merged: Omega Ruby's played) and which differ (X/Y's own to port).

  sound_compare.py <sango_sound.bcsar> <xy_sound.bcsar> [--list FILE]

The archives: `remake_tool oras-extract <game> sound/sango_sound.bcsar <out>` (X/Y: sound/xy_sound.bcsar). A sound is
compared through what it plays: its sequence file and, for a sequence (details 0x2203), each bank it names and each wave
archive those banks name. Two things are set aside, the two games' archives differing there for every file: the files'
version word (+8: 0x0100 in X/Y, 0x0101 in Omega Ruby) and a bank's wave archive ids (0x05xxxxxx, each game numbering
its archives apart), compared by the archive's contents. Streams (0x2201, the music) are counted apart: oras-xy-world
--music ports them. --list writes the names of the different ones, one a line.

The INFO tables read (Bcsar.h): sounds (0x2100: file, player, details; the name's string at option bit 0), banks
(0x2101: file, then a reference to its wave archive id table), wave archives (0x2103: file), files (0x2106: 0x220C an
internal file, a FILE-section reference to it; 0x220D an external one).
"""
import collections
import hashlib
import struct
import sys


class Csar:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        if d[:4] != b'CSAR':
            sys.exit(f'{path}: not a sound archive')
        sections = {}
        for k in range(struct.unpack_from('<H', d, 0x10)[0]):
            kind, _, at, _ = struct.unpack_from('<HHII', d, 0x14 + 12 * k)
            sections[kind] = at
        self.strg, self.info, self.file = sections[0x2000], sections[0x2001] + 8, sections[0x2002] + 8
        self.tables = {kind: self.info + at for kind, _, at in (struct.unpack_from('<HHI', d, self.info + 8 * k) for k in range(8))}
        self.sounds, self.banks, self.warcs, self.files = (self.entries(t) for t in (0x2100, 0x2101, 0x2103, 0x2106))
        body = self.strg + 8
        strings = body + struct.unpack_from('<I', d, body + 4)[0]
        self.names = []
        for i in range(struct.unpack_from('<I', d, strings)[0]):
            _, _, at, size = struct.unpack_from('<HHII', d, strings + 4 + 12 * i)
            self.names.append(d[strings + at:strings + at + size].split(b'\0')[0].decode())

    def entries(self, kind):
        at = self.tables[kind]
        return [at + struct.unpack_from('<HHI', self.d, at + 4 + 8 * i)[2] for i in range(struct.unpack_from('<I', self.d, at)[0])]

    def file_bytes(self, number):
        """an internal file's bytes, None for an external or missing one"""
        if number >= len(self.files):
            return None
        e = self.files[number]
        if struct.unpack_from('<H', self.d, e)[0] != 0x220C:
            return None
        _, _, at, size = struct.unpack_from('<HHII', self.d, e + struct.unpack_from('<I', self.d, e + 4)[0])
        return None if at == 0xFFFFFFFF else self.d[self.file + at:self.file + at + size]

    def name(self, sound):
        e = self.sounds[sound]
        return self.names[struct.unpack_from('<I', self.d, e + 0x18)[0]] if struct.unpack_from('<I', self.d, e + 0x14)[0] & 1 else ''

    def kind(self, sound):
        return struct.unpack_from('<H', self.d, self.sounds[sound] + 0xC)[0]

    def warc_hash(self, number):
        if number >= len(self.warcs):
            return b'?'
        data = self.file_bytes(struct.unpack_from('<I', self.d, self.warcs[number])[0])
        return hashlib.md5(unversioned(data)).digest() if data else b'-'

    def key(self, sound):
        """what the sound plays, game-independent: its file's hash, then each bank's (wave archive ids as their contents)"""
        d, e = self.d, self.sounds[sound]
        out = [self.kind(sound), digest(self.file_bytes(struct.unpack_from('<I', d, e)[0]))]
        if self.kind(sound) == 0x2203:
            details = e + struct.unpack_from('<I', d, e + 0x10)[0]
            banks = details + struct.unpack_from('<I', d, details + 4)[0]
            for bank in struct.unpack_from('<%dI' % struct.unpack_from('<I', d, banks)[0], d, banks + 4):
                if (bank & 0xFFFFFF) >= len(self.banks):
                    out.append(('bank?', bank))
                    continue
                data = self.file_bytes(struct.unpack_from('<I', d, self.banks[bank & 0xFFFFFF])[0])
                out.append(digest(self.bank_contents(data)) if data else None)
        return out

    def bank_contents(self, data):
        data, out, i = unversioned(data), bytearray(), 0
        while i < len(data):
            w = struct.unpack_from('<I', data, i)[0] if i + 4 <= len(data) else None
            if w is not None and i % 4 == 0 and w >> 24 == 5 and (w & 0xFFFFFF) < len(self.warcs):
                out += self.warc_hash(w & 0xFFFFFF)
                i += 4
            else:
                out.append(data[i])
                i += 1
        return bytes(out)


def unversioned(data):
    return data[:8] + b'\0\0\0\0' + data[12:] if data else data


def digest(data):
    return hashlib.md5(unversioned(data)).hexdigest() if data else None


def main():
    if len(sys.argv) not in (3, 5) or (len(sys.argv) == 5 and sys.argv[3] != '--list'):
        sys.exit(__doc__)
    oras, xy = Csar(sys.argv[1]), Csar(sys.argv[2])
    first = {}
    for i in range(len(oras.sounds)):
        first.setdefault(oras.name(i), i)
    counts, different = collections.Counter(), []
    for i in range(len(xy.sounds)):
        name = xy.name(i)
        if not name or name not in first:
            counts['no Omega Ruby sound of the name'] += 1
        elif xy.kind(i) == 0x2201:
            counts['stream (music)'] += 1
        elif xy.key(i) == oras.key(first[name]):
            counts['the same'] += 1
        else:
            counts['different'] += 1
            different.append(name)
    for what, n in counts.most_common():
        print(f'{n:5} {what}')
    families = collections.Counter('_'.join(n.split('_')[:3]) for n in different)
    print('different, by family:', ', '.join(f'{f} {n}' for f, n in families.most_common(12)))
    if len(sys.argv) == 5:
        open(sys.argv[4], 'w').write('\n'.join(different) + '\n')


if __name__ == '__main__':
    main()
