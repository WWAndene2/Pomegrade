#!/usr/bin/env python3
"""How much a still scene changes over time, from shots of one headless run (30 October, the projection maps: X's cloud
shadows drift over Pokemon Village, DEVELOPMENT_NOTE.md 3e step 5). Each shot's top screen (400 x 240) is blurred
(Gaussian, radius 6, so the grass's and trees' small sway weighs little) and compared with the first.

  time_lapse_diff.py <run dir> [--diff OUT.png]

<run dir> holds the shots a.png, b.png, ... (fast_run.sh's `shot a; wait 600; shot b; ...`, the player not moving).
Prints, for each later shot, the mean and the largest absolute difference with a.png (0-255); --diff writes the last
shot minus the first, x6 around grey, at twice the size. Measured 30 October in X's Pokemon Village (zone 318, shots 600
frames apart): 6.39, 1.51, 5.40, with no drifting shadow seen in the shots; whether that is the projection or the
grass's wind is not settled (the same run of the mod was not finished).
"""
import os
import string
import sys

import numpy as np
from PIL import Image, ImageFilter


def load(path):
    image = Image.open(path).convert('L').crop((0, 0, 400, 240)).filter(ImageFilter.GaussianBlur(6))
    return np.asarray(image).astype(float)


def main():
    args = sys.argv[1:]
    if not args or args[0].startswith('-'):
        sys.exit(__doc__)
    run = args[0]
    out = args[args.index('--diff') + 1] if '--diff' in args else None
    shots = [os.path.join(run, c + '.png') for c in string.ascii_lowercase if os.path.exists(os.path.join(run, c + '.png'))]
    if len(shots) < 2:
        sys.exit('fewer than two shots (a.png, b.png...) in ' + run)
    first = load(shots[0])
    for path in shots[1:]:
        delta = np.abs(load(path) - first)
        print('a-%s mean |diff| %.2f max %.0f' % (os.path.basename(path)[0], delta.mean(), delta.max()))
    if out:
        last = load(shots[-1])
        Image.fromarray(np.clip(128 + (last - first) * 6, 0, 255).astype('uint8')).resize((800, 480)).save(out)


if __name__ == '__main__':
    main()
