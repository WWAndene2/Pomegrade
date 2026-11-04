# Example-based terrain synthesis (image quilting, Efros & Freeman 2001, applied to elevation): a new range laid out
# by hand is filled with patches of real Copernicus DEM (30 m) from three Alpine regions, chosen to fit the layout's
# large shapes and the patches already placed; overlaps are joined along minimum-error seams.
import numpy as np, tifffile, sys, os
# synth.py <Copernicus DEM dir> <out dir> [tile names]: writes synth_h.npy (metres), layout.npy, synth_preview.png
DEM, OUT = sys.argv[1], sys.argv[2]
TILES = sys.argv[3:] or ['N46_00_E007_00', 'N46_00_E008_00', 'N46_00_E011_00']   # Valais, Bernese Oberland, Dolomites
os.makedirs(OUT, exist_ok=True)
from scipy.ndimage import gaussian_filter
rng = np.random.default_rng(3)
SRC = [tifffile.imread(os.path.join(DEM, t + '.tif')).astype(np.float32) for t in TILES]
N, P, O = 1024, 192, 64            # output size, patch size, overlap (cells of ~30 m: the output spans ~30 km)
STEP = P - O
LOW = 48                           # large-shape scale for the layout match (cells)
SRC_LOW = [gaussian_filter(s, LOW) for s in SRC]
# relief detail = real terrain minus its large shapes; lakes (flat water in the DEM) marked so they are never used
SRC_DET = [s - l for s, l in zip(SRC, SRC_LOW)]
SRC_FLAT = [gaussian_filter((np.hypot(*np.gradient(s)) < 0.05).astype(np.float32), 3) > 0.3 for s in SRC]

# --- the layout: a curved main crest, a side spur, valleys and a lowland in one corner (metres) -----------------
yy, xx = np.mgrid[0:N, 0:N] / N
def crest(px, py, width, height):
    # distance to a polyline crest, height falling off with a broad mountain profile
    d = np.full((N, N), 9.0)
    for (x0, y0), (x1, y1) in zip(zip(px, py), zip(px[1:], py[1:])):
        vx, vy = x1 - x0, y1 - y0; t = np.clip(((xx - x0) * vx + (yy - y0) * vy) / (vx * vx + vy * vy), 0, 1)
        d = np.minimum(d, np.hypot(xx - x0 - t * vx, yy - y0 - t * vy))
    return height * np.exp(-(d / width) ** 1.6)
L = 700 + crest([0.05, 0.3, 0.55, 0.8, 0.98], [0.75, 0.55, 0.5, 0.35, 0.15], 0.17, 2800)
L = np.maximum(L, 700 + crest([0.5, 0.55, 0.62], [0.52, 0.75, 0.97], 0.09, 1900))
nz = gaussian_filter(rng.normal(size=(N, N)), 60); L = L + nz / nz.std() * 180
L = gaussian_filter(L, 40)

def orient(a, k):
    a = np.rot90(a, k % 4, axes=(-2, -1))
    return a[..., ::-1] if k >= 4 else a

def candidates(n):
    out, lows = [], []
    while len(out) < n:
        s = rng.integers(len(SRC)); H, W = SRC[s].shape
        y, x = rng.integers(0, H - P), rng.integers(0, W - P); k = rng.integers(8)
        if SRC_FLAT[s][y:y + P, x:x + P].mean() > 0.01: continue
        out.append(orient(SRC_DET[s][y:y + P, x:x + P], k)); lows.append(orient(SRC_LOW[s][y:y + P, x:x + P], k))
    return np.array(out), np.array(lows)

def seam_mask(err):
    # minimum-error vertical cut through an overlap (rows x O): mask = 1 where the new patch is kept
    h, w = err.shape; cost = err.copy()
    for i in range(1, h):
        left = np.r_[np.inf, cost[i - 1, :-1]]; right = np.r_[cost[i - 1, 1:], np.inf]
        cost[i] += np.minimum(np.minimum(left, cost[i - 1]), right)
    mask = np.zeros((h, w)); j = int(np.argmin(cost[-1]))
    for i in range(h - 1, -1, -1):
        mask[i, j:] = 1
        if i:
            lo, hi = max(0, j - 1), min(w, j + 2); j = lo + int(np.argmin(cost[i - 1, lo:hi]))
    return gaussian_filter(mask, 3.0)  # a slightly feathered cut, so no single-cell step remains

out = np.zeros((N, N), np.float32); placed = np.zeros((N, N), bool)
for py in range(0, N - O, STEP):
    for px in range(0, N - O, STEP):
        y1, x1 = min(py + P, N), min(px + P, N); h, w = y1 - py, x1 - px
        cand, low = candidates(900)
        cand, low = cand[:, :h, :w], low[:, :h, :w]
        tgt_low = L[py:y1, px:x1]
        have = placed[py:y1, px:x1]
        # detail is matched on the overlap; the large shape must slope the same way as the layout's here
        e_overlap = ((cand[:, have] - out[py:y1, px:x1][have]) ** 2).mean(axis=1) if have.any() else 0
        gy_t, gx_t = np.gradient(tgt_low); gy_c, gx_c = np.gradient(low, axis=(1, 2))
        e_layout = ((gy_c - gy_t) ** 2 + (gx_c - gx_t) ** 2).mean(axis=(1, 2)) * 400
        best = int(np.argmin(e_overlap + e_layout))
        patch = cand[best]
        m = np.ones((h, w))
        if px > 0:
            m[:, :O] = np.minimum(m[:, :O], seam_mask((patch[:, :O] - out[py:y1, px:px + O]) ** 2))
        if py > 0:
            m[:O, :] = np.minimum(m[:O, :], seam_mask(((patch[:O, :] - out[py:py + O, px:x1]) ** 2).T).T)
        region = out[py:y1, px:x1]
        out[py:y1, px:x1] = np.where(have, region * (1 - m) + patch * m, patch)
        placed[py:y1, px:x1] = True
det = out
out = L + det  # the layout's large shape, real relief on it
np.save(os.path.join(OUT, 'synth_h.npy'), out); np.save(os.path.join(OUT, 'layout.npy'), L)
from PIL import Image
def shade(a, cell=30.0):
    gy, gx = np.gradient(a / cell); s = np.clip(0.55 + (-gx * 0.6 + gy * 0.6) / np.sqrt(1 + gx * gx + gy * gy) * 0.7, 0, 1)
    return (s * 255).astype(np.uint8)
Image.fromarray(np.concatenate([shade(L), shade(out)], 1)).save(os.path.join(OUT, 'synth_preview.png'))
print('ok', out.min(), out.max())
