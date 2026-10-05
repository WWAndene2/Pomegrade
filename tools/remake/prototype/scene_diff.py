# scratch tool: Platinum's terrain against the ORAS rebuild, half tile by half tile, for paths and water
# (what lies over each half tile's centre in each scene); prints the mismatches and draws a diff map
import os
import json, re, sys
from PIL import Image, ImageDraw
S = os.environ.get('REMAKE_WORK', '.')
name, gc0, gr0 = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
plat = json.load(open(S + '/slice/cover2.json')); oras = json.load(open(S + f'/slice/{name}_cover.json'))
base = lambda ts: {re.sub(r'_lm\d+$', '', t) for t in ts}
def pclass(ts):
    t = base(ts)
    if 'lake' in t: return 'water'
    if t & {'nsand', 'hage'}: return 'path'
    return 'other'
def oclass(ts):
    t = set(ts)
    if 'water1' in t: return 'water'
    if 'chip_soil' in t: return 'path' if not t & {'chip_kusa_b', 'chip_kusa_a'} else 'path+ground'
    return 'other'
img = Image.new('RGB', (80 * 8, 80 * 8)); d = ImageDraw.Draw(img)
bad = {}
for r in range(80):
    for c in range(80):
        k = f'{2 * gc0 + c},{2 * gr0 + r}'
        p, o = pclass(plat.get(k, [])), oclass(oras.get(k, []))
        if p == o or (p == 'other' and o == 'other'): col = (60, 160, 60) if p == 'other' else (220, 200, 120) if p == 'path' else (80, 140, 230)
        else: col = (230, 40, 40); bad[(p, o)] = bad.get((p, o), 0) + 1
        d.rectangle([c * 8, r * 8, c * 8 + 7, r * 8 + 7], fill=col)
img.save(S + f'/slice/{name}_diff.png')
print('mismatches (Platinum, ORAS): count ->', bad if bad else 'none')
