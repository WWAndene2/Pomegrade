#!/usr/bin/env python3
"""Two headless runs' sound compared (retro_host's `audio NAME`: 16-bit stereo WAV), a tenth of a second at a time: the
level (RMS) of their difference, 0 where they play the same samples. Runs from one title state with the same inputs play
the same music sample for sample (29 October: footsteps tested as a mod with a table against one with none), so what is
left is what the two mods play differently.

  audio_diff.py <a.wav> <b.wav> [--step SECONDS]
"""
import struct
import sys
import wave


def samples(path):
    w = wave.open(path)
    data = w.readframes(w.getnframes())
    return struct.unpack('<%dh' % (len(data) // 2), data), w.getframerate()


def main():
    args = sys.argv[1:]
    step = 0.1
    if '--step' in args:
        i = args.index('--step')
        step = float(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    (a, rate), (b, rate_b) = samples(args[0]), samples(args[1])
    if rate != rate_b:
        sys.exit('the two files differ in rate')
    n = min(len(a), len(b))
    chunk = max(2, int(rate * step) * 2)
    levels = []
    for k in range(0, n, chunk):
        part = [a[i] - b[i] for i in range(k, min(n, k + chunk))]
        levels.append(int((sum(v * v for v in part) / len(part)) ** 0.5))
    print(f'{n // 2 / rate:.2f} s compared ({len(a) // 2} and {len(b) // 2} frames); difference by {step} s:')
    print(' '.join(str(v) for v in levels))
    print('the same' if not any(levels) else f'different in {sum(1 for v in levels if v)} of {len(levels)} steps')


if __name__ == '__main__':
    main()
