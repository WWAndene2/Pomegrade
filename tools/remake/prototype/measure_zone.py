# How an ORAS map piece lays a ground zone and its blade strip against the tile lattice: the measurements behind
# TownShapes' StairZone(outerPull) and OutlineStrip (ORAS_LITTLEROOT.md 9d). Input: `remake_tool mesh-json <piece> out.json`.
#   python3 measure_zone.py <mesh.json> [zone texture (chip_kusa_b)] [strip texture (chip_alpha)]
# Prints, for the zone's fill: how far its border vertices sit from the lattice, by corner kind (straight run, the zone's own
# corner, an inner corner), and whether tiles with the same 3x3 neighbourhood have the same border; for the strip: where
# its tips (texture v 0.30) and roots (v 0.50) sit around each border corner, by kind (depth along the normal or bisector,
# into the zone positive). Littleroot (piece 6) gives: own corners pulled 0.12 tile in, inner 0.00, straight 0.01; tips
# 0.23 / 0.32 / 0.13 in and roots 0.25 / 0.22 / 0.36 out (straight / own corner / inner).
import collections, itertools, json, math, sys
import numpy as np

T = 18.0                       # a tile, in units; a piece spans -360..360
meshes = {m['tex']: m for m in json.load(open(sys.argv[1]))}
zone_tex = sys.argv[2] if len(sys.argv) > 2 else 'chip_kusa_b'
strip_tex = sys.argv[3] if len(sys.argv) > 3 else 'chip_alpha'
if zone_tex not in meshes: sys.exit('no mesh shows %s; textures: %s' % (zone_tex, ', '.join(sorted(meshes))))
fill = meshes[zone_tex]
FV = [tuple(v[:2]) for v in fill['v']]
tris = lambda m: [tuple(m['t'][i:i + 3]) for i in range(0, len(m['t']), 3)]
FT = tris(fill)

def inside(x, z):
    for a, b, c in FT:
        (ax, az), (bx, bz), (cx, cz) = FV[a], FV[b], FV[c]
        den = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
        if abs(den) < 1e-9: continue
        l1 = ((bz - cz) * (x - cx) + (cx - bx) * (z - cz)) / den
        l2 = ((cz - az) * (x - cx) + (ax - cx) * (z - cz)) / den
        if l1 >= -1e-6 and l2 >= -1e-6 and 1 - l1 - l2 >= -1e-6: return True
    return False

# the fill's border: edges of one triangle only (vertices welded to a tenth of a unit), as chains of corners
key = lambda i: (round(FV[i][0], 1), round(FV[i][1], 1))
use = collections.Counter()
for t in FT:
    for e in range(3): use[tuple(sorted((key(t[e]), key(t[(e + 1) % 3]))))] += 1
border = [e for e, n in use.items() if n == 1]
neighbours = collections.defaultdict(list)
for a, b in border: neighbours[a].append(b); neighbours[b].append(a)
corners = [c for c in neighbours if len(neighbours[c]) == 2]
lattice = lambda p: np.array([round((p[0] + 360) / T) * T - 360, round((p[1] + 360) / T) * T - 360])

def frame(c):
    # the corner's kind, and the unit direction out of the zone (normal on a straight run, bisector at a corner); taken on the
    # lattice points, so a corner the game has pulled in still reads as the corner it was cut from
    a, b = neighbours[c]
    lc, la, lb = lattice(c), lattice(a), lattice(b)
    ua, ub = (la - lc), (lb - lc)
    if np.linalg.norm(ua) < 1e-6 or np.linalg.norm(ub) < 1e-6: return None, None
    ua, ub = ua / np.linalg.norm(ua), ub / np.linalg.norm(ub)
    if ua @ ub < -math.cos(math.radians(30)):
        m = np.array([-ub[1], ub[0]])
        kind = 'straight'
    else:
        m = (ua + ub) / np.linalg.norm(ua + ub)  # into the angle between the sides
        kind = 'own corner' if inside(*(lc + m * T * 0.45)) else 'inner corner'
    if inside(*(lc + m * T * 0.45)): m = -m       # out of the zone
    return kind, m

kinds = {}
fill_moves = collections.defaultdict(list)
for c in corners:
    kind, m = frame(c)
    if kind is None: continue
    kinds[c] = (kind, m)
    fill_moves[kind].append(-((np.array(c) - lattice(c)) @ m) / T)
print('%s: %d border edges, %d corners' % (zone_tex, len(border), len(kinds)))
print('fill corners moved into the zone, from their lattice point (tiles; median, 10%-90%):')
for kind, v in sorted(fill_moves.items()):
    print('   %-12s n=%3d  %+.3f  [%+.3f .. %+.3f]' % (kind, len(v), np.median(v), np.percentile(v, 10), np.percentile(v, 90)))

# a kit or a rule? border tiles with the same 3x3 neighbourhood: is their border the same?
N = 40
zone = [[inside(-360 + (c + .5) * T, -360 + (r + .5) * T) for c in range(N)] for r in range(N)]
per = collections.defaultdict(list)
for (ax, az), (bx, bz) in border:
    for i in range(7):
        x, z = ax + (bx - ax) * i / 6, az + (bz - az) * i / 6
        c, r = int((x + 360) // T), int((z + 360) // T)
        per[(r, c)].append(((x + 360) / T - c, (z + 360) / T - r))
cfg = lambda r, c: tuple(int(zone[min(max(r + dr, 0), N - 1)][min(max(c + dc, 0), N - 1)]) for dr in (-1, 0, 1) for dc in (-1, 0, 1))
groups = collections.defaultdict(list)
for (r, c) in per:
    if 0 <= r < N and 0 <= c < N: groups[cfg(r, c)].append((r, c))
def apart(p, q):
    P, Q = np.array(p), np.array(q)
    d = np.sqrt(((P[:, None] - Q[None]) ** 2).sum(-1))
    return (d.min(1).mean() + d.min(0).mean()) / 2
same = [apart(per[a], per[b]) for k in groups for a, b in itertools.combinations(groups[k], 2)]
if same: print('border shape of tiles with the same neighbourhood: median %.3f tile apart (a kit or a rule would give ~0)' % np.median(same))

# the strip: each tip and root against its nearest border corner, by kind
if strip_tex in meshes:
    V = meshes[strip_tex]['v']
    cs = list(kinds)
    C = np.array(cs)
    depth = collections.defaultdict(list)
    for v in V:
        tip, root = abs(v[4] - 0.302) < 0.02, abs(v[4] - 0.496) < 0.02
        if not (tip or root): continue
        p = np.array(v[:2])
        j = int(np.argmin(np.hypot(*(C - p).T)))
        if math.hypot(*(C[j] - p)) > T: continue            # a decal, not this zone's strip
        kind, m = kinds[cs[j]]
        depth[(kind, 'tips' if tip else 'roots')].append(-((p - C[j]) @ m) / T)
    print('%s strip, depth into the zone from the nearest border corner (tiles; median, 10%%-90%%):' % strip_tex)
    for (kind, which), v in sorted(depth.items()):
        print('   %-12s %-5s n=%3d  %+.3f  [%+.3f .. %+.3f]' % (kind, which, len(v), np.median(v), np.percentile(v, 10), np.percentile(v, 90)))
else:
    print('no mesh shows %s: no strip measured' % strip_tex)
