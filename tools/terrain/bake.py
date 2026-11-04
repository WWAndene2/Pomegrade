# Baked lighting for a heightfield: sun shadows (ray-march toward the sun) and ambient occlusion (horizon angles in
# 16 directions), so relief reads through cast shadows and dark hollows, as in the game.
import numpy as np, numba, base64, re, sys, os
@numba.njit(parallel=True, cache=True)
def shadow(h, cell, sx, sy, tan_el, steps):
    n, m = h.shape; out = np.ones((n, m), np.float32)
    for y in numba.prange(n):
        for x in range(m):
            h0 = h[y, x]; lit = 1.0
            for k in range(1, steps):
                px = x + sx * k; py = y + sy * k
                ix, iy = int(px), int(py)
                if ix < 0 or iy < 0 or ix >= m - 1 or iy >= n - 1: break
                if h[iy, ix] > h0 + tan_el * k * cell:
                    lit = 0.0; break
            out[y, x] = lit
    return out
@numba.njit(parallel=True, cache=True)
def occlusion(h, cell, steps):
    n, m = h.shape; out = np.zeros((n, m), np.float32)
    for y in numba.prange(n):
        for x in range(m):
            h0 = h[y, x]; acc = 0.0
            for d in range(16):
                a = d * 2 * np.pi / 16; dx = np.cos(a); dy = np.sin(a); best = 0.0
                k = 1.0
                while k < steps:
                    ix, iy = int(x + dx * k), int(y + dy * k)
                    if ix < 0 or iy < 0 or ix >= m or iy >= n: break
                    t = (h[iy, ix] - h0) / (k * cell)
                    if t > best: best = t
                    k *= 1.25
                acc += np.arctan(best)
            out[y, x] = 1.0 - acc / 16 / (np.pi / 2)
    return out
def bake(h, cell, sun=(-0.62, 0.6, 0.42)):
    s = np.array(sun, float); s /= np.linalg.norm(s)
    horiz = np.hypot(s[0], s[1]); sx, sy = s[0] / horiz, s[1] / horiz
    sh = shadow(h.astype(np.float64), cell, sx, sy, s[2] / horiz, 600)
    from scipy.ndimage import gaussian_filter
    sh = gaussian_filter(sh, 1.0)                       # soft shadow edge
    ao = occlusion(h.astype(np.float64), cell, 120)
    return sh.astype(np.float32), np.clip(ao, 0, 1).astype(np.float32)
if __name__ == '__main__':
    # bake.py <scene dir>: adds SH/AO to its data.js and FSH/FAO to its fine.js
    os.chdir(sys.argv[1])
    EXAG = 2.0 / 1.4
    s = open('data.js').read(); N = 1024
    H = np.frombuffer(base64.b64decode(re.search(r'window.H="([^"]+)"', s).group(1)), np.float32).reshape(N, N)
    sh, ao = bake(H * 170 * EXAG, 1000 / (N - 1))
    s = re.sub(r'window.(SH|AO)="[^"]*";', '', s) + 'window.SH="%s";window.AO="%s";' % (
        base64.b64encode(sh.tobytes()).decode(), base64.b64encode(ao.tobytes()).decode())
    open('data.js', 'w').write(s)
    f = open('fine.js').read(); FN = 1024
    meta = eval(re.search(r'window.FINE=(\{[^}]+\})', f).group(1))
    FH = np.frombuffer(base64.b64decode(re.search(r'window.FH="([^"]+)"', f).group(1)), np.float32).reshape(FN, FN)
    sh2, ao2 = bake(FH * EXAG, meta['size'] / (FN - 1))
    f = re.sub(r'window.(FSH|FAO)="[^"]*";', '', f) + 'window.FSH="%s";window.FAO="%s";' % (
        base64.b64encode(sh2.tobytes()).decode(), base64.b64encode(ao2.tobytes()).decode())
    open('fine.js', 'w').write(f)
    from PIL import Image
    Image.fromarray((np.dstack([sh * ao] * 3)[::2, ::2] * 255).astype(np.uint8)).save('bake_preview.png')
    print('shadow', sh.mean(), 'ao', ao.mean(), 'fine', sh2.mean(), ao2.mean())
