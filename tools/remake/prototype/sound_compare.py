#!/usr/bin/env python3
"""X/Y's sounds against Omega Ruby's same-named ones, by their data (26 October, step 2's inventory, ORAS_ENGINE.md 0.0):
which are the same (merged: Omega Ruby's played) and which differ (X/Y's own to port).

  sound_compare.py <sango_sound.bcsar> <xy_sound.bcsar> [--list FILE] [--why] [--appended GROWN.bcsar]

The archives: `remake_tool oras-extract <game> sound/sango_sound.bcsar <out>` (X/Y: sound/xy_sound.bcsar). A sound is
compared through what it plays: its sequence file and, for a sequence (details 0x2203), each bank it names and each wave
archive those banks name. Two things are set aside, the two games' archives differing there for every file: the files'
version word (+8: 0x0100 in X/Y, 0x0101 in Omega Ruby) and a bank's wave archive ids (0x05xxxxxx, each game numbering
its archives apart), compared by the archive's contents. Streams (0x2201, the music) are counted apart: oras-xy-world
--music ports them. --list writes the names of the different ones, one a line. --why says, for each different one, what
differs (29 October, the instrument-level comparison): its sequence, its banks (the instruments), and of the waves (the
samples) its banks name, how many X/Y's are not found in any of Omega Ruby's wave archives (by their bytes): those are
what a port must bring, the rest Omega Ruby already holds. --appended checks an archive oras-xy-world grew (the mod's
sango_sound.bcsar: remake_tool mod-file): each of its sounds past Omega Ruby's that names one of X/Y's plays X/Y's data
(its key as above), and Omega Ruby's own sounds are unchanged, read here apart from the C++ that wrote it.

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
        # in 32 bits, as the 3DS adds them: a grown archive's tables, past their entries, reach them with offsets that wrap
        return [(at + struct.unpack_from('<HHI', self.d, at + 4 + 8 * i)[2]) & 0xFFFFFFFF for i in range(struct.unpack_from('<I', self.d, at)[0])]

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

    def waves(self, number):
        """a wave archive's waves (CWAR: INFO's references to the CWAVs in FILE), as their hashes"""
        data = self.file_bytes(struct.unpack_from('<I', self.d, self.warcs[number])[0]) if number < len(self.warcs) else None
        if not data or data[:4] != b'CWAR':
            return []
        sections = {struct.unpack_from('<H', data, 0x14 + 12 * k)[0]: struct.unpack_from('<I', data, 0x18 + 12 * k)[0]
                    for k in range(struct.unpack_from('<H', data, 0x10)[0])}
        info, file = sections[0x6800] + 8, sections[0x6801] + 8
        out = []
        for i in range(struct.unpack_from('<I', data, info)[0]):
            _, _, at, size = struct.unpack_from('<HHII', data, info + 4 + 12 * i)
            out.append(hashlib.md5(unversioned(data[file + at:file + at + size])).digest())
        return out

    def sound_waves(self, sound):
        """the waves a sequence's banks name: (wave archive, index) pairs read from each bank's wave id table"""
        d, e = self.d, self.sounds[sound]
        if self.kind(sound) != 0x2203:
            return set()
        details = e + struct.unpack_from('<I', d, e + 0x10)[0]
        banks = details + struct.unpack_from('<I', d, details + 4)[0]
        out = set()
        for bank in struct.unpack_from('<%dI' % struct.unpack_from('<I', d, banks)[0], d, banks + 4):
            data = self.file_bytes(struct.unpack_from('<I', d, self.banks[bank & 0xFFFFFF])[0]) if (bank & 0xFFFFFF) < len(self.banks) else None
            for i in range(0, len(data or b'') - 7, 4):
                w, index = struct.unpack_from('<II', data, i)
                if w >> 24 == 5 and (w & 0xFFFFFF) < len(self.warcs) and index < 0x10000:
                    out.add((w & 0xFFFFFF, index))
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
    args = sys.argv[1:]
    why = '--why' in args
    if why:
        args.remove('--why')
    appended = None
    if '--appended' in args:
        i = args.index('--appended')
        appended = args[i + 1]
        del args[i:i + 2]
    if len(args) not in (2, 4) or (len(args) == 4 and args[2] != '--list'):
        sys.exit(__doc__)
    oras, xy = Csar(args[0]), Csar(args[1])
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
    if len(args) == 4:
        open(args[3], 'w').write('\n'.join(different) + '\n')
    if why:
        held = {h for n in range(len(oras.warcs)) for h in oras.waves(n)}
        xwaves = {n: xy.waves(n) for n in range(len(xy.warcs))}
        kinds, new = collections.Counter(), set()
        for name in different:
            x, o = next(i for i in range(len(xy.sounds)) if xy.name(i) == name), first[name]
            kx, ko = xy.key(x), oras.key(o)
            parts = [p for p, a, b in (('sequence', kx[1], ko[1]), ('banks', kx[2:], ko[2:])) if a != b]
            missing = {(w, i) for w, i in xy.sound_waves(x) if i < len(xwaves[w]) and xwaves[w][i] not in held}
            new |= {xwaves[w][i] for w, i in missing}
            what = ' and '.join(parts) or 'kind'
            kinds[what + (', new waves' if missing else '')] += 1
            print(f'{name}: {what} differ; {len(missing)} of its {len(xy.sound_waves(x))} waves not in Omega Ruby')
        for what, n in kinds.most_common():
            print(f'{n:5} {what}')
        print(f'{len(new)} different waves to bring')
    if appended:
        grown = Csar(appended)
        x_of = {}
        for i in range(len(xy.sounds)):
            x_of.setdefault(xy.name(i), i)
        old = sum(grown.key(i) == oras.key(i) and grown.name(i) == oras.name(i) for i in range(len(oras.sounds)))
        same = wrong = 0
        for i in range(len(oras.sounds), len(grown.sounds)):
            x = x_of.get(grown.name(i))
            if x is None or xy.kind(x) != 0x2203:
                continue
            if grown.key(i) == xy.key(x):
                same += 1
            else:
                wrong += 1
                print('not X/Y\'s data:', grown.name(i))
        print(f'appended: {same} of X/Y\'s sound effects play its data, {wrong} not; {old} of Omega Ruby\'s {len(oras.sounds)} unchanged')


if __name__ == '__main__':
    main()
