#!/usr/bin/env python3
"""The footstep sounds of Omega Ruby and X, ground by ground, as WAV files to listen to (30 October: the owner, choosing
a sound for the ground that has none). Both games pick a step's sound from a table in DllField by the ground under the
foot, the kind of move and the foot (OrasXyFootsteps.h): Omega Ruby's 13 grounds x 9 kinds at file offset 0xF2B10,
X's 17 grounds x 23 kinds at 0xEE804. This reads the walk and run rows (Omega Ruby's kinds 0 and 2, X's 0 and 1), each
sound id through the code's category table (Omega Ruby's 0x592728, X's 0x54E904) to its sound, and writes the wave
its bank plays (DSP-ADPCM or PCM in a CWAV, decoded here) at its own rate: the sample the step plays, not the
sequence rendered (pitch, volume and envelope left out).

  footstep_sounds.py <oras code.bin> <x code.bin> <sango_sound.bcsar> <xy_sound.bcsar> <oras DllField.cro> \\
                     <x DllField.cro> <out dir>

Writes <out dir>/<sound name>.wav and <out dir>/footsteps.json (each game's grounds: the walk and run sounds of each
foot, by name). The archives: remake_tool oras-extract; the modules likewise (DllField.cro).
"""
import json
import os
import struct
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sound_compare import Csar  # noqa: E402

CODE_BASE = 0x100000


def category_item(code, table, sound_id):
    """the archive item a sound id names through a code's category table, or None"""
    category, n = sound_id >> 16, sound_id & 0xFFFF
    if category >= 0x17:
        return None
    count, lst = struct.unpack_from('<II', code, table - CODE_BASE + 8 * category)
    if not lst or n >= count:
        return None
    return struct.unpack_from('<I', code, lst - CODE_BASE + 4 * n)[0]


def cwav_pcm(data):
    """a CWAV's first channel as 16-bit samples, and its rate"""
    if data[:4] != b'CWAV':
        raise ValueError('not a CWAV')
    blocks = {}
    for k in range(struct.unpack_from('<H', data, 0x10)[0]):
        kind, _, at, size = struct.unpack_from('<HHII', data, 0x14 + 12 * k)
        blocks[kind] = at
    info, data_block = blocks[0x7000] + 8, blocks[0x7001] + 8
    encoding, loop, _, rate, loop_start, loop_end = struct.unpack_from('<BBHIII', data, info)
    table = info + 0x14  # after the encoding, loop, rate, loop start, loop end and a reserved word
    channel = table + struct.unpack_from('<I', data, table + 8)[0]
    sample_at = data_block + struct.unpack_from('<I', data, channel + 4)[0]
    frames = loop_end
    if encoding == 1:
        return list(struct.unpack_from('<%dh' % frames, data, sample_at)), rate
    if encoding == 0:
        return [(b - 256 if b > 127 else b) << 8 for b in data[sample_at:sample_at + frames]], rate
    if encoding != 2:
        raise ValueError('encoding %d' % encoding)
    adpcm = channel + struct.unpack_from('<I', data, channel + 0x0C)[0]
    coefs = struct.unpack_from('<16h', data, adpcm)
    yn1, yn2 = struct.unpack_from('<hh', data, adpcm + 0x22)
    out, at = [], sample_at
    while len(out) < frames:
        header = data[at]
        scale, index = 1 << (header & 0xF), header >> 4
        c1, c2 = coefs[2 * index], coefs[2 * index + 1]
        for b in data[at + 1:at + 8]:
            for nibble in (b >> 4, b & 0xF):
                if nibble >= 8:
                    nibble -= 16
                s = ((nibble * scale) << 11) + 1024 + c1 * yn1 + c2 * yn2
                s = max(-32768, min(32767, s >> 11))
                yn2, yn1 = yn1, s
                out.append(s)
                if len(out) >= frames:
                    break
            if len(out) >= frames:
                break
        at += 8
    return out, rate


def sound_wave(csar, sound):
    """the CWAV bytes the sound's bank plays first, or None"""
    waves = sorted(csar.sound_waves(sound))
    if not waves:
        return None
    warc, index = waves[0]
    data = csar.file_bytes(struct.unpack_from('<I', csar.d, csar.warcs[warc])[0])
    if not data or data[:4] != b'CWAR':
        return None
    sections = {struct.unpack_from('<H', data, 0x14 + 12 * k)[0]: struct.unpack_from('<I', data, 0x18 + 12 * k)[0]
                for k in range(struct.unpack_from('<H', data, 0x10)[0])}
    info, file = sections[0x6800] + 8, sections[0x6801] + 8
    _, _, at, size = struct.unpack_from('<HHII', data, info + 4 + 12 * index)
    return data[file + at:file + at + size]


def main():
    if len(sys.argv) != 8:
        sys.exit(__doc__)
    oras_code, x_code = (open(p, 'rb').read() for p in sys.argv[1:3])
    oras, xy = Csar(sys.argv[3]), Csar(sys.argv[4])
    oras_field, x_field = (open(p, 'rb').read() for p in sys.argv[5:7])
    out = sys.argv[7]
    os.makedirs(out, exist_ok=True)
    games = {
        'oras': (oras_code, 0x592728, oras, oras_field, 0xF2B10, 13, 9, {'walk': 0, 'run': 2}),
        'x': (x_code, 0x54E904, xy, x_field, 0xEE804, 17, 23, {'walk': 0, 'run': 1}),
    }
    result = {}
    written = set()
    for game, (code, table, csar, field, at, grounds, kinds, moves) in games.items():
        rows = []
        for ground in range(grounds):
            row = {'ground': ground}
            for move, kind in moves.items():
                names = []
                for foot in range(2):
                    sound_id = struct.unpack_from('<I', field, at + ground * kinds * 8 + kind * 8 + foot * 4)[0]
                    item = category_item(code, table, sound_id) if sound_id else None
                    if item is None or item >> 24 != 1:
                        names.append(None)
                        continue
                    sound = item & 0xFFFFFF
                    name = csar.name(sound) or 'sound_%d' % sound
                    file = '%s_%s.wav' % (game, name)
                    if file not in written:
                        cwav = sound_wave(csar, sound)
                        if cwav:
                            samples, rate = cwav_pcm(cwav)
                            with wave.open(os.path.join(out, file), 'wb') as w:
                                w.setnchannels(1)
                                w.setsampwidth(2)
                                w.setframerate(rate)
                                w.writeframes(struct.pack('<%dh' % len(samples), *samples))
                            written.add(file)
                    names.append({'id': '0x%X' % sound_id, 'name': name, 'file': file if file in written else None})
                row[move] = names
            rows.append(row)
        result[game] = rows
    with open(os.path.join(out, 'footsteps.json'), 'w') as f:
        json.dump(result, f, indent=1)
    print('%d sounds written to %s' % (len(written), out))


if __name__ == '__main__':
    main()
