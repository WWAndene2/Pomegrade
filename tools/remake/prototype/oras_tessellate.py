# oras_tessellate.py: a 3DS character model's triangles multiplied, each split into level x level triangles laid on a
# curved surface, by the DS core's polygon multiplier method ("PNhong": Phong tessellation blended with circular PN
# triangles at the share measured per level; melonDS-android-lib/src/GPU3D_PolygonMultiplier.cpp), in bind pose. Points
# on an edge depend on that edge's two corners only, so neighbours never crack apart; UVs and skinning weights are
# interpolated, a vertex keeping its 4 strongest bones (the PICA's limit), so the result still animates.
#
#   python3 -I oras_tessellate.py <model.smd> <tex dir> <out prefix> [levels, default 1,2,3]
#       <model.smd>: `spica smd` of a character (an a/0/2/1 member), <tex dir>: its `spica png` textures;
#       <out prefix>.png: four views per level (front, three-quarter, side, back), lit per vertex, textures sampled
#       bilinearly as the PICA does. A preview: what the game accepts (2,042 triangles for Brendan; x9 is 18,378, x16
#       32,672) was not tried. Level n gives n*n times the triangles: x4, x9, x16, x25.
#
# Checked (17 October, Brendan, a/0/2/1 172): no crack at x9; the shoulder strap, two layers close together, comes out
# mottled from x9 (not fixed).
import sys, os, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usum_character as U
from PIL import Image

SHARE = {1: 0, 2: 0, 3: 0.50, 4: 0.75, 5: 0.85, 6: 0.90, 7: 0.95, 8: 0.95}  # PNhongShare, GPU3D_PolygonMultiplier.cpp
SHAPE = 0.75                                                                 # Phong ShapeFactor

def unit(v):
    l = np.linalg.norm(v); return v / l if l > 1e-12 else v

def patch(P, N):  # circular PN control points (SetupPatch with circular edges)
    def edge(i, j):
        d = P[j] - P[i]; t = d - np.dot(d, N[i]) * N[i]; scale = 1 / 3
        ang = math.acos(max(-1, min(1, np.dot(N[i], N[j])))); chord = np.linalg.norm(d); tl = np.linalg.norm(t)
        if 1e-6 < ang < 2.1 and tl > 1e-12: scale = (4 / 3) * math.tan(ang / 4) * chord / (2 * math.sin(ang / 2)) / tl
        return P[i] + scale * t
    b210, b120, b021, b012, b102, b201 = edge(0, 1), edge(1, 0), edge(1, 2), edge(2, 1), edge(2, 0), edge(0, 2)
    e = (b210 + b120 + b021 + b012 + b102 + b201) / 6; b111 = e + (e - (P[0] + P[1] + P[2]) / 3) * 0.75
    def en(i, j):
        d = P[j] - P[i]; s = N[i] + N[j]; l2 = np.dot(d, d); v = 2 * np.dot(d, s) / l2 if l2 > 0 else 0
        return unit(s - v * d)
    return (b210, b120, b021, b012, b102, b201, b111, en(0, 1), en(1, 2), en(2, 0))

def point(P, N, pt, share, u, v, w):
    flat = u * P[0] + v * P[1] + w * P[2]
    proj = [flat - np.dot(flat - P[i], N[i]) * N[i] for i in range(3)]
    pos = (1 - SHAPE) * flat + SHAPE * (u * proj[0] + v * proj[1] + w * proj[2]); nrm = u * N[0] + v * N[1] + w * N[2]
    if share > 0:
        b210, b120, b021, b012, b102, b201, b111, n110, n011, n101 = pt
        pn = (P[0] * u ** 3 + P[1] * v ** 3 + P[2] * w ** 3 + 3 * b210 * u * u * v + 3 * b120 * u * v * v + 3 * b201 * u * u * w
              + 3 * b021 * v * v * w + 3 * b102 * u * w * w + 3 * b012 * v * w * w + 6 * b111 * u * v * w)
        pnn = N[0] * u * u + N[1] * v * v + N[2] * w * w + n110 * u * v + n011 * v * w + n101 * u * w
        pos = pos + share * (pn - pos); nrm = nrm + share * (unit(pnn) - unit(nrm))
    return pos, unit(nrm)

def multiply(tris, level):  # tris: (tex, [(pos, normal, uv, weights)]) -> the same, level*level times as many
    if level <= 1: return tris
    out = []
    for t, vs in tris:
        P = [np.asarray(v[0], float) for v in vs]; N = [unit(np.asarray(v[1], float)) for v in vs]
        pt = patch(P, N) if SHARE[level] > 0 else None
        grid = {}
        for i in range(level + 1):
            for j in range(level + 1 - i):
                u, v_ = i / level, j / level; w = 1 - u - v_
                pos, nrm = point(P, N, pt, SHARE[level], u, v_, w)
                uv = tuple(u * np.array(vs[0][2]) + v_ * np.array(vs[1][2]) + w * np.array(vs[2][2]))
                wt = {}
                for k, bw in zip((u, v_, w), (vs[0][3], vs[1][3], vs[2][3])):
                    for b, x in bw: wt[b] = wt.get(b, 0) + k * x
                grid[i, j] = (pos, nrm, uv, sorted(wt.items(), key=lambda x: -x[1])[:4])  # a vertex keeps 4 bones at most (PICA)
        for i in range(level):
            for j in range(level - i):
                out.append((t, [grid[i, j], grid[i + 1, j], grid[i, j + 1]]))
                if i + j + 1 < level: out.append((t, [grid[i + 1, j], grid[i + 1, j + 1], grid[i, j + 1]]))
    return out

def render(tris, TEX, yaw, size=640, frame=None):  # Gouraud: lit per vertex from its normal, the light interpolated
    allp = np.array([v[0] for _, vs in tris for v in vs]); lo, hi = frame if frame else (allp.min(0), allp.max(0))
    c = (lo + hi) / 2; h = (hi - lo)[1] * 1.08
    cy_, sy_ = math.cos(yaw), math.sin(yaw); Ry = np.array([[cy_, 0, sy_], [0, 1, 0], [-sy_, 0, cy_]])
    img = np.zeros((size, size, 3), np.uint8); img[:] = (44, 46, 54); zb = np.full((size, size), -1e9)
    Ld = np.array([-0.4, 0.6, 0.7]); Ld /= np.linalg.norm(Ld)
    for key, vs in tris:
        Tx = TEX[key]; th, tw = Tx.shape[:2]; q = [Ry @ (v[0] - c) for v in vs]
        lit = [0.45 + 0.55 * max(0.0, float(np.dot(Ry @ v[1], Ld))) for v in vs]
        sp = [((x / h + 0.5) * size, (0.5 - y / h) * size, z) for x, y, z in q]
        xs = [p[0] for p in sp]; ys = [p[1] for p in sp]
        x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, size - 1); y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, size - 1)
        if x1 < x0 or y1 < y0: continue
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = sp; d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9: continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d; l1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d; l2 = 1 - l0 - l1
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not m.any(): continue
        z = l0 * az + l1 * bz + l2 * cz; sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= z > sub
        u = l0 * vs[0][2][0] + l1 * vs[1][2][0] + l2 * vs[2][2][0]; v = l0 * vs[0][2][1] + l1 * vs[1][2][1] + l2 * vs[2][2][1]
        fx = np.mod(u, 1) * tw - 0.5; fy = np.mod(v, 1) * th - 0.5; ix = np.floor(fx).astype(int); iy = np.floor(fy).astype(int)
        ax_ = (fx - ix)[..., None]; ay_ = (fy - iy)[..., None]; x0_, x1_ = ix % tw, (ix + 1) % tw; y0_, y1_ = iy % th, (iy + 1) % th
        col = (Tx[y0_, x0_] * (1 - ax_) + Tx[y0_, x1_] * ax_) * (1 - ay_) + (Tx[y1_, x0_] * (1 - ax_) + Tx[y1_, x1_] * ax_) * ay_  # bilinear, wrapping, as the PICA samples
        m &= col[..., 3] > 100; sub[m] = z[m]; shade = l0 * lit[0] + l1 * lit[1] + l2 * lit[2]
        reg = img[y0:y1 + 1, x0:x1 + 1]; reg[m] = (col[..., :3][m] * shade[m][:, None]).clip(0, 255).astype(np.uint8)
    return img

if __name__ == '__main__':
    smd, texdir, out = sys.argv[1:4]; levels = [int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else '1,2,3').split(',')]
    s = U.Smd(smd); TEX = {}
    base = [(t, vs) for t, vs in s.tris if 'sw_parts' not in t and os.path.exists(os.path.join(texdir, t))]
    for t, _ in base: TEX.setdefault(t, U.tex(os.path.join(texdir, t)))
    allp = np.array([v[0] for _, vs in base for v in vs]); frame = (allp.min(0), allp.max(0))
    rows = []
    for lv in levels:
        tr = multiply(base, lv); print(f'level {lv}: {len(tr)} triangles')
        rows.append(np.concatenate([render(tr, TEX, y, 640, frame) for y in (0.0, -math.pi / 4, -math.pi / 2, math.pi)], 1))
    Image.fromarray(np.concatenate(rows, 0)).save(out + '.png')
