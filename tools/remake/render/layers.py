# Depth, layers and contrast on top of the stylised render (no new path tracing): sky light on
# up-facing surfaces, a warm rim on silhouettes facing the sun, a fine contour line on depth breaks,
# a soft glow on highlights, aerial perspective and a slight far blur, then an S-curve.
import numpy as np
from PIL import Image
from scipy import ndimage
def load(p): return np.asarray(Image.open(p).convert('RGB')).astype(np.float32) / 255
def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def smoothstep(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)
LUMA = np.array([0.2126, 0.7152, 0.0722], np.float32)

import sys
D = sys.argv[1]
img = to_lin(load(f'{D}/stylised.png'))
d = load(f'{D}/pass_depth.png'); z = (d[..., 0] * 255 + d[..., 1]) / 255 * 1500
sky = z < 1
n = load(f'{D}/pass_normal.png') * 2 - 1                      # view-space normals
# world up and the afternoon sun (azimuth 160, elevation 32) in the camera's view space
cam = np.array([float(v) for v in sys.argv[2].split(',')]); pos, tgt = cam[:3], cam[3:]
zc = pos - tgt; zc /= np.linalg.norm(zc)
xc = np.cross([0, 1, 0], zc); xc /= np.linalg.norm(xc); yc = np.cross(zc, xc)
view = lambda v: np.array([v @ xc, v @ yc, v @ zc])
az, el = np.radians(160), np.radians(32)
UP = view(np.array([0.0, 1.0, 0.0])); SUN = view(np.array([np.cos(el) * np.cos(az), np.sin(el), np.cos(el) * np.sin(az)]))
zs = np.where(sky, z[~sky].max() * 1.1, z)
depth01 = np.clip((zs - z[~sky].min()) / (z[~sky].max() - z[~sky].min()), 0, 1)

# layer 1, sky light: surfaces facing up catch a little of the blue sky
up = np.clip((n * UP).sum(-1), 0, 1)
img = img + img * (np.array([0.02, 0.05, 0.14]) * up[..., None] ** 2)

# layer 2, rim light: the silhouette edges turned towards the sun get a thin warm line
behind = ndimage.maximum_filter(zs, size=5) - zs           # something further behind nearby
silhouette = smoothstep(6, 30, behind) * ~sky
facing = np.clip((n[..., :2] * SUN[:2] / np.linalg.norm(SUN[:2])).sum(-1), 0, 1)
rim = ndimage.gaussian_filter(silhouette * facing, 0.8)
img = img + rim[..., None] * np.array([0.28, 0.2, 0.1]) * (0.3 + (img * LUMA).sum(-1, keepdims=True))

# layer 3, contour: a fine line where depth breaks, the local colour darkened (anime line art)
front = ndimage.minimum_filter(zs, size=3)
line = smoothstep(4, 18, zs - front) * ~sky                 # the far side of each break
line = np.maximum(line, smoothstep(10, 40, ndimage.maximum_filter(zs, size=3) - zs) * 0.6 * ~sky)
img = img * (1 - 0.45 * np.clip(line, 0, 1)[..., None])

# layer 4, glow: the brightest highlights bleed a soft halo
bright = np.clip((img * LUMA).sum(-1) - 0.55, 0, None)[..., None] * img
img = img + ndimage.gaussian_filter(bright, (9, 9, 0)) * 0.6

# depth: aerial perspective (further = lighter, bluer, less contrast) and a slight far blur
haze = np.array([0.62, 0.74, 0.92]) ** 2.2
far = (depth01 ** 1.4 * 0.22)[..., None] * ~sky[..., None]
img = img * (1 - far) + haze * far
blurred = ndimage.gaussian_filter(img, (1.6, 1.6, 0))
dof = smoothstep(0.7, 1.0, depth01)[..., None] * 0.7
img = img * (1 - dof) + blurred * dof

# contrast: an S-curve on brightness, colours kept
out = to_srgb(img)
lum = (out * np.array([0.299, 0.587, 0.114])).sum(-1, keepdims=True)
curved = lum + (smoothstep(0.0, 1.0, lum) - lum) * 0.7
out = out * (curved / np.maximum(lum, 1e-4))
out = np.where(sky[..., None], load(f'{D}/stylised.png'), out)
Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)).save(f'{D}/final.png')
