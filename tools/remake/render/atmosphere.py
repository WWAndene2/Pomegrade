# Sky and atmosphere, after the layers (v5): painted anime clouds in the sky, light rays through the
# trees' foliage, and pollen floating in the sun. Clouds and pollen are placed in the world (fixed
# seeds), so they would stay put if the camera moved.
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
from scipy import ndimage
from world import load, to_lin, to_srgb, smoothstep, fbm, Camera, depth_of, SUN, LUMA

D = sys.argv[1]; cam = Camera(sys.argv[2])
img = to_lin(load(f'{D}/final.png'))
sky = load(f'{D}/pass_flat.png').min(-1) > 0.995
z = depth_of(D)
H, W = img.shape[:2]

# --- clouds: sky directions mapped onto a cloud dome, painted in two tones with soft edges ---------
ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
nx = (xs + 0.5) / W * 2 - 1; ny = 1 - (ys + 0.5) / H * 2
a = W / H
dirs = (nx[..., None] * cam.t * a * cam.x + ny[..., None] * cam.t * cam.y - cam.z)
dirs /= np.linalg.norm(dirs, axis=-1, keepdims=True)
up = np.clip(dirs[..., 1], 0.02, 1)
u, v = dirs[..., 0] / up * 0.9, dirs[..., 2] / up * 0.9           # a flat cloud layer seen from below
# large rounded masses: low frequencies only, with a few puffs on their edges
shape = fbm(u * 0.55 + 2.0, v * 0.55 + 5.0, 3, seed=41) + 0.08 * fbm(u * 2.2, v * 2.2, 2, seed=43)
shape = shape + 0.12 * (1 - up)                                    # denser towards the horizon
cover = ndimage.gaussian_filter(smoothstep(0.55, 0.6, shape), 1.2)  # crisp painted edges, anti-aliased
# two-tone shading: the side towards the sun lit, the underside lavender
lit = smoothstep(0.45, 0.6, fbm(u * 0.55 + 2.0 + 0.04, v * 0.55 + 5.0 - 0.06, 3, seed=41) + 0.08) * smoothstep(0.62, 0.7, shape)
top = np.array([1.0, 0.98, 0.94]) ** 2.2; belly = np.array([0.78, 0.80, 0.95]) ** 2.2
cloud_col = belly * (1 - lit[..., None]) + top * lit[..., None]
img = np.where(sky[..., None], img * (1 - cover[..., None] * 0.92) + cloud_col * cover[..., None] * 0.92, img)

# --- light rays: the sunlit foliage and the sky behind the trees, smeared along the light's -------
# direction on screen, kept faint and only in the air above the ground
mid = load(f'{D}/pass_matid.png')
trees = (mid[..., 0] > 0.5) & (mid[..., 1] < 0.5) & (mid[..., 2] < 0.5)
src = ((trees & ((img * LUMA).sum(-1) > 0.25)) | sky).astype(np.float32)
src *= ndimage.binary_dilation(trees, iterations=25)                # only light around the trees
sx0, sy0, _ = cam.project(cam.pos + np.array([0.0, 0.0, 0.0]))
d2 = cam.view(-SUN)[:2]; d2 = d2 / (np.linalg.norm(d2) + 1e-6)     # the light's travel, on screen
d2 = np.array([d2[0], -d2[1]])
rays = np.zeros_like(src)
for i in range(1, 60):
    rays += ndimage.shift(src, (d2[1] * i * 2, d2[0] * i * 2), order=1, mode='constant') * (1 - i / 60)
rays /= 30
rays = ndimage.gaussian_filter(rays, 3) * ~sky
img = img + rays[..., None] * np.array([1.0, 0.85, 0.55]) * 0.10

# --- pollen: small points floating in the sunlit air, around the trees and the houses --------------
rng = np.random.default_rng(5)
n = 260
pts = np.stack([rng.uniform(-420, 420, n), rng.uniform(6, 55, n), rng.uniform(-440, 360, n)], -1)
px, py, pz = cam.project(pts)
near_trees = ndimage.binary_dilation(trees, iterations=60)
lum = (img * LUMA).sum(-1)
glow = np.zeros((H, W), np.float32)
for x, y, d in zip(px, py, pz):
    if d <= 1 or not np.isfinite(x) or not np.isfinite(y): continue
    xi, yi = int(x), int(y)
    if not (0 <= xi < W and 0 <= yi < H): continue
    if z[yi, xi] > 1 and z[yi, xi] < d: continue                       # hidden behind something
    if not near_trees[yi, xi] or lum[yi, xi] < 0.12: continue           # only in the sunlit air near trees
    r = max(0.5, 120 / d)                                               # nearer = bigger
    yy, xx = np.ogrid[max(0, yi - 4):min(H, yi + 5), max(0, xi - 4):min(W, xi + 5)]
    glow[yy, xx] += np.exp(-(((xx - x) ** 2 + (yy - y) ** 2) / (2 * r * r)))
glow = np.clip(glow, 0, 1)
img = img + glow[..., None] * np.array([1.0, 0.88, 0.55]) * 0.22

Image.fromarray((to_srgb(img) * 255 + 0.5).astype(np.uint8)).save(f'{D}/final_v5.png')
print('rays max', rays.max().round(3), 'pollen points', int((glow > 0.5).sum()))
