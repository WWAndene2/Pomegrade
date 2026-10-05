# scratch tool: Platinum's terrain, tile by tile, into scene elements -- every texture has an explicit role
# (an unknown one stops it), roles have a fixed precedence, and a control map is drawn.
# in: slice/cover.json (textures over each tile, from tilemats.py); out: slice/<name>_vis.txt, slice/<name>_control.png
import json, os, re, sys
from collections import Counter
from PIL import Image, ImageDraw, ImageFont

S = os.environ.get('REMAKE_WORK', '.')
# texture (without its _lmNN suffix) -> role
ROLE = {
    'conttree_b': 'forest', 'conttree_t': 'forest', 'tree01': 'tree', 'tree04_2': 'tree', 'tree04': 'tree',
    'nsand': 'path', 'hage': 'path',                                 # hage: a bald (dirt) patch
    'nsandp': None,                                                  # a path's outline, drawn over the grass beside it
    'imped': 'fence', 'fenter': 'fence',
    'nhana': 'flower',
    'lake': 'water', 'puddle': 'water', 'lakep': 'frame', 'puddlep': 'frame',
    'puddle_b': None,                                                # the base under a pond: what lies on it decides
    's_snow': 'pale', 's_snow02': 'pale', 's_snow03': 'pale', 's_snow04': 'pale', 's_sonwp': 'pale',
    'ngrass': 'grass', 'nectgr': 'tallgrass', 'allpeak': 'ledge',
    'h_kage': 'house', 't1_s01_1': 'house', 't1_s01_2': 'house', 't1_h01': 'house', 'door': 'house', 'light': 'house',
    'tshadow': None, 'seaside3': None, 's_snow_lm': 'pale',
}
ORDER = ['house', 'water', 'frame', 'fence', 'flower', 'tallgrass', 'ledge', 'path', 'forest', 'tree', 'pale', 'grass']
CHAR = {'house': 'H', 'water': '~', 'frame': 'f', 'fence': 'F', 'flower': '*', 'tallgrass': 'g', 'ledge': 'L', 'path': ':',
        'pale': 's', 'forest': 'T', 'tree': 't', 'grass': '.'}
COLOUR = {'H': (200, 60, 60), '~': (70, 140, 230), 'f': (130, 90, 50), 'F': (250, 250, 250), '*': (240, 120, 200), 'g': (30, 110, 40),
          'L': (120, 60, 20), ':': (230, 200, 120), 's': (200, 240, 220), 'T': (20, 70, 30), 't': (50, 120, 50), '.': (120, 220, 140)}

cov = json.load(open(S + '/slice/cover.json'))
unknown = Counter()
def role_of(tex):
    base = re.sub(r'_lm\d+$', '', tex)
    if base not in ROLE: unknown[base] += 1; return None
    return ROLE[base]

def classify(gc0, gr0, name):
    grid = []
    for r in range(40):
        row = ''
        for c in range(40):
            roles = {role_of(t) for t in cov.get(f'{gc0 + c},{gr0 + r}', [])} - {None}
            row += next((CHAR[x] for x in ORDER if x in roles), '.')
        grid.append(row)
    # flower beds: tiles a fence encloses (not reachable from the edge without crossing one) hold flowers
    out = [list(x) for x in grid]
    seen = set(); todo = [(r, c) for r in range(40) for c in range(40) if r in (0, 39) or c in (0, 39)]
    while todo:
        r, c = todo.pop()
        if (r, c) in seen or not (0 <= r < 40 and 0 <= c < 40) or grid[r][c] == 'F': continue
        seen.add((r, c)); todo += [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]
    beds = 0
    for r in range(40):
        for c in range(40):
            if (r, c) not in seen and grid[r][c] != 'F': out[r][c] = '*'; beds += 1
    grid = [''.join(x) for x in out]
    open(f'{S}/slice/{name}_vis.txt', 'w').write('\n'.join(grid) + '\n')
    # control map: a 16-pixel square a tile, with the tile grid every 5
    img = Image.new('RGB', (40 * 16, 40 * 16)); d = ImageDraw.Draw(img)
    for r in range(40):
        for c in range(40):
            d.rectangle([c * 16, r * 16, c * 16 + 15, r * 16 + 15], fill=COLOUR[grid[r][c]])
    for k in range(0, 41, 5):
        d.line([k * 16, 0, k * 16, 640], fill=(0, 0, 0)); d.line([0, k * 16, 640, k * 16], fill=(0, 0, 0))
    img.save(f'{S}/slice/{name}_control.png')
    counts = Counter(ch for row in grid for ch in row)
    print(name, dict(counts), f'{beds} bed tiles filled with flowers')

# paths at half-tile precision: a half tile is path when Platinum's path texture covers its centre
cov2 = json.load(open(S + '/slice/cover2.json'))
def paths2(gc0, gr0, name):
    rows = []
    for r in range(80):
        rows.append(''.join(':' if {'nsand', 'hage'} & {re.sub(r'_lm\d+$', '', t) for t in cov2.get(f'{2 * gc0 + c},{2 * gr0 + r}', [])} else '.' for c in range(80)))
    open(f'{S}/slice/{name}_path2.txt', 'w').write('\n'.join(rows) + '\n')
    print(name, sum(row.count(':') for row in rows), 'half tiles of path')
paths2(92, 856, 'twinleaf'); paths2(92, 816, 'route201')

# better: the paths from the colour Platinum shows (texsample.py), sand-coloured half tiles; Platinum's path
# outline (nsandp) is sand on its inner half, which texture names can't tell
import numpy as np
def paths_from_colour(name, sample):
    col = np.load(f'{S}/slice/{sample}_colours.npy').astype(int)
    r, g, b = col[..., 0], col[..., 1], col[..., 2]
    sand = (r > 200) & (g > 160) & (b < 200) & (r > b + 50) & (r >= g - 10)
    rows = [''.join(':' if sand[y, x] else '.' for x in range(80)) for y in range(80)]
    open(f'{S}/slice/{name}_path2.txt', 'w').write('\n'.join(rows) + '\n')
    print(name, int(sand.sum()), 'half tiles of path, from the colours')
if os.path.exists(f'{S}/slice/plat_tw_colours.npy'): paths_from_colour('twinleaf', 'plat_tw')

classify(92, 856, 'twinleaf')
classify(92, 816, 'route201')
if unknown:
    print('UNKNOWN textures (give them a role):', dict(unknown)); sys.exit(1)
print(open(S + '/slice/twinleaf_vis.txt').read())
