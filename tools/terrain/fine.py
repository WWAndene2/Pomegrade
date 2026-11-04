# 2 m detail around the ground view: the 30 m synthesised range, upsampled, plus walker-scale relief (< ~40 m) quilted
# from swissALTI3D pieces; each piece is turned so its gullies run down the slope it lands on; detail fades to zero at
# the window's border so the window meets the coarse terrain without a seam.
import numpy as np, re, base64, sys, os
# fine.py <swissALTI3D mosaic.npy> <scene dir>: reads the scene's data.js, writes its fine.js and fine_preview.png
MOSAIC, SCENE = sys.argv[1], sys.argv[2]
from scipy.ndimage import gaussian_filter, map_coordinates
rng = np.random.default_rng(11)
SW = np.load(MOSAIC).astype(np.float32)          # 5000 x 5000 at 2 m
LOWS = 10                                              # detail = below ~20 m sigma (scales under ~60 m)
SW_LOW = gaussian_filter(SW, LOWS); SW_DET = SW - SW_LOW
gy_s, gx_s = np.gradient(gaussian_filter(SW, 25))
SW_SLOPE = np.hypot(gx_s, gy_s) / 2.0                 # tan of the slope at ~50 m scale
# sources: no valley floors (village, roads, fields) and no glacier tops (too smooth)
SW_OK = gaussian_filter(((SW_SLOPE > 0.25) & (SW < 3600)).astype(np.float32), 20) > 0.97

# the coarse terrain (data.js of the range) and the ground view's position on it
s = open(os.path.join(SCENE, 'data.js')).read(); N = 1024
H = np.frombuffer(base64.b64decode(re.search(r'window.H="([^"]+)"', s).group(1)), np.float32).reshape(N, N)
GV = eval(re.search(r'window.GV=(\[[^\]]+\])', s).group(1))
SIZE, HEIGHT = 1000.0, 170.0; M_PER_UNIT = 30.7       # scene units: 1 = 30.7 m
F = 1024; CELL_M = 2.0; WIN_UNITS = F * CELL_M / M_PER_UNIT   # 2 km window = ~66.7 units
cx, cz = GV[0], GV[1]
# the window centred ahead of the camera, toward its target, so the view's foreground is all fine
dx, dz = GV[2] - cx, GV[3] - cz; L = np.hypot(dx, dz); cx += dx / L * WIN_UNITS * 0.35; cz += dz / L * WIN_UNITS * 0.35
xs = cx + (np.arange(F) / (F - 1) - 0.5) * WIN_UNITS; zs = cz + (np.arange(F) / (F - 1) - 0.5) * WIN_UNITS
gx = (xs / SIZE + 0.5) * (N - 1); gz = (zs / SIZE + 0.5) * (N - 1)
GZ, GX = np.meshgrid(gz, gx, indexing='ij')
base = map_coordinates(H.astype(np.float64), [GZ, GX], order=3) * HEIGHT * (M_PER_UNIT * 1.0)  # metres (x1.4 exaggeration kept)
tgy, tgx = np.gradient(gaussian_filter(base, 12))

P, O = 96, 32; STEP = P - O
def orient(a, k):
    a = np.rot90(a, k % 4, axes=(-2, -1)); return a[..., ::-1] if k >= 4 else a
def rot_vec(gy, gx, k):
    # how a (gy, gx) direction maps under orient(k): rot90 counter-clockwise k times, then a column flip
    for _ in range(k % 4): gy, gx = -gx, gy
    return (gy, -gx) if k >= 4 else (gy, gx)
H5, W5 = SW.shape
def candidates(n):
    out, dirs = [], []
    while len(out) < n:
        y, x = rng.integers(0, H5 - P), rng.integers(0, W5 - P)
        if not SW_OK[y + P // 2, x + P // 2]: continue
        k = rng.integers(8)
        out.append(orient(SW_DET[y:y + P, x:x + P], k))
        g = rot_vec(gy_s[y + P // 2, x + P // 2], gx_s[y + P // 2, x + P // 2], k); dirs.append(g)
    d = np.array(dirs); d /= np.linalg.norm(d, axis=1, keepdims=True) + 1e-9
    return np.array(out), d
def seam(err):
    h, w = err.shape; c = err.copy()
    for i in range(1, h):
        c[i] += np.minimum(np.minimum(np.r_[np.inf, c[i - 1, :-1]], c[i - 1]), np.r_[c[i - 1, 1:], np.inf])
    m = np.zeros((h, w)); j = int(np.argmin(c[-1]))
    for i in range(h - 1, -1, -1):
        m[i, j:] = 1
        if i: lo, hi = max(0, j - 1), min(w, j + 2); j = lo + int(np.argmin(c[i - 1, lo:hi]))
    return gaussian_filter(m, 2.0)
det = np.zeros((F, F), np.float32); placed = np.zeros((F, F), bool)
for py in range(0, F - O, STEP):
    for px in range(0, F - O, STEP):
        y1, x1 = min(py + P, F), min(px + P, F); h, w = y1 - py, x1 - px
        cand, d = candidates(700); cand = cand[:, :h, :w]
        t = np.array([tgy[py:y1, px:x1].mean(), tgx[py:y1, px:x1].mean()]); tn = np.linalg.norm(t)
        e_dir = (1 - d @ (t / tn)) * 2000 if tn > 1e-3 else 0       # gullies down the slope
        have = placed[py:y1, px:x1]
        e_ov = ((cand[:, have] - det[py:y1, px:x1][have]) ** 2).mean(axis=1) if have.any() else 0
        best = cand[int(np.argmin(e_ov + e_dir))]
        m = np.ones((h, w))
        if px > 0: m[:, :O] = np.minimum(m[:, :O], seam((best[:, :O] - det[py:y1, px:px + O]) ** 2))
        if py > 0: m[:O, :] = np.minimum(m[:O, :], seam(((best[:O, :] - det[py:py + O, px:x1]) ** 2).T).T)
        r = det[py:y1, px:x1]; det[py:y1, px:x1] = np.where(have, r * (1 - m) + best * m, best); placed[py:y1, px:x1] = True
# fade the detail out toward the window border (seamless with the coarse terrain around it)
yy, xx = np.mgrid[0:F, 0:F] / (F - 1); edge = np.minimum.reduce([xx, yy, 1 - xx, 1 - yy])
fade = np.clip(edge / 0.12, 0, 1); fade = fade * fade * (3 - 2 * fade)
fine = base + det * 1.4 * fade                          # metres, with the scene's x1.4 vertical exaggeration
json_meta = dict(cx=float(cx), cz=float(cz), size=float(WIN_UNITS), F=F)
open(os.path.join(SCENE, 'fine.js'), 'w').write('window.FINE=%s;window.FH="%s";window.FFADE="%s";' % (
    str(json_meta).replace("'", '"'),
    base64.b64encode((fine / (M_PER_UNIT)).astype(np.float32).tobytes()).decode(),   # in scene units
    base64.b64encode(fade.astype(np.float32).tobytes()).decode()))
from PIL import Image
g2y, g2x = np.gradient(fine / CELL_M)
sh = np.clip(0.55 + (-g2x * 0.6 + g2y * 0.6) / np.sqrt(1 + g2x ** 2 + g2y ** 2) * 0.7, 0, 1)
Image.fromarray((sh * 255).astype(np.uint8)).save(os.path.join(SCENE, 'fine_preview.png'))
print('ok', json_meta, det.std(), base.min(), base.max())
