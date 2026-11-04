# scene_data.py <synth_h.npy> <scene dir>: the scene's per-cell data for render/scene.html, as data.js:
#   H   height, 0.05..1 (scene height = H * 170 units, x1.4 of real; the renderer exaggerates to x2)
#   R   rivers (drainage area of the relief), F / M forest and meadow masks (low-frequency noise)
#   S   material slope (relief smoothed over ~3 cells: grass covers the small gullies)
#   SN  snow: altitude line moved by sun exposure, wind (curvature) and drift; slides off steep faces; holds in couloirs
#   GV  the ground-level view: a gentle meadow 180-300 cells from the highest crest, looking at it
# The map is 1000 units wide (~30.7 km: 1024 cells of the 30 m synthesis).
import sys, os, base64
import numpy as np, numba
from scipy.ndimage import gaussian_filter, laplace
N = 1024; HEIGHT = 170.0; CELL = 1000 / N

@numba.njit(cache=True)
def accum(h):
    n = h.shape[0]; flat = h.ravel(); order = np.argsort(-flat); area = np.ones(n * n)
    for t in range(n * n):
        i = order[t]; y, x = i // n, i % n; best = i; bs = 0.0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                y2, x2 = y + dy, x + dx
                if 0 <= y2 < n and 0 <= x2 < n and (dy or dx):
                    s = (flat[i] - flat[y2 * n + x2]) / (1.4142 if dy * dx else 1.0)
                    if s > bs: bs = s; best = y2 * n + x2
        if best != i: area[best] += area[i]
    return area.reshape(n, n)

def main(src, scene):
    os.makedirs(scene, exist_ok=True)
    h = np.load(src).astype(np.float64)
    h = (h - h.min()) / (h.max() - h.min()) * 0.95 + 0.05
    rng = np.random.default_rng(5)
    river = gaussian_filter((accum(gaussian_filter(h, 1.5)) > 20000).astype(float), 1.2)
    gy, gx = np.gradient(gaussian_filter(h, 3.0) * HEIGHT / CELL); S = np.hypot(gx, gy)
    norm = lambda a: (a - a.min()) / (a.max() - a.min())
    forest = norm(gaussian_filter(rng.normal(size=(N, N)), 28))
    meadow = norm(gaussian_filter(rng.normal(size=(N, N)), 12))
    # snow
    hs = gaussian_filter(h, 1.0) * HEIGHT; gy, gx = np.gradient(hs, CELL)
    slope_deg = np.degrees(np.arctan(np.hypot(gx, gy)))
    sun = np.array([-0.5, 0.55, 0.67]); sun /= np.linalg.norm(sun)      # south-west, 35 degrees (+y is south)
    nrm = np.dstack([-gx, -gy, np.ones_like(gx)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    insol = np.clip((nrm * sun).sum(2), 0, 1)
    curv = -laplace(gaussian_filter(hs, 3.0))                            # > 0 on crests, < 0 in hollows
    drift = gaussian_filter(np.random.default_rng(9).normal(size=(N, N)), 6); drift /= drift.std()
    line = 0.66 + 0.10 * (insol - 0.5) + 0.02 * drift + 0.03 * np.clip(curv * 0.8, -1, 1)
    snow = np.clip((h - line) / 0.035, 0, 1) * np.clip((48 - slope_deg) / 14, 0, 1)
    snow = np.maximum(snow, np.clip((h - line + 0.05) / 0.04, 0, 1) * np.clip(-curv * 1.2, 0, 1) * np.clip((55 - slope_deg) / 10, 0, 1))
    SN = gaussian_filter(snow, 0.8)
    # ground view
    cand = np.argwhere((S < 0.18) & (h > 0.32) & (h < 0.45) & (SN < 0.01))
    top = np.unravel_index(np.argmax(gaussian_filter(h, 20)), h.shape)
    d = np.hypot(cand[:, 0] - top[0], cand[:, 1] - top[1]); ok = cand[(d > 180) & (d < 300)]
    gy0, gx0 = ok[len(ok) // 2]
    to_u = lambda c: float((c / (N - 1) - 0.5) * 1000)
    gv = [to_u(gx0), to_u(gy0), to_u(top[1]), to_u(top[0])]
    f32 = lambda a: base64.b64encode(np.ascontiguousarray(a, dtype=np.float32).tobytes()).decode()
    arrays = dict(H=h, R=river, F=forest, M=meadow, S=S, SN=SN)
    open(os.path.join(scene, 'data.js'), 'w').write('window.N=%d;' % N + ''.join('window.%s="%s";' % (k, f32(v)) for k, v in arrays.items()) + 'window.GV=%s;' % gv)
    print('snow cover %.3f, ground view %s' % ((SN > 0.5).mean(), [round(v, 1) for v in gv]))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
