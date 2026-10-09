# The stylised mix: the game's own look (base) carries the colours; the path tracer only supplies
# where and how much light arrives (light = path-traced image / base colours), which is then
# applied with an art direction: a soft two-tone step, cool lavender shadows, warm sunlight,
# a little coloured bounce, the textures' relief, and the vibrance kept.
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
from scipy import ndimage

def load(p): return np.asarray(Image.open(p).convert('RGB')).astype(np.float32) / 255
def to_lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
def smoothstep(a, b, x): t = np.clip((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)
LUMA = np.array([0.2126, 0.7152, 0.0722], np.float32)

D = sys.argv[8] if len(sys.argv) > 8 else '.'
base_srgb = load(f'{D}/pass_base.png'); base = to_lin(base_srgb)
pt = to_lin(load(sys.argv[1])) / float(sys.argv[2]) if sys.argv[1] != '-' else None  # undo the render's exposure
sky_mask = load(f'{D}/pass_flat.png').min(-1) > 0.995                # no surface in the geometry pass (its white background)
relief = load(f'{D}/pass_relief.png').mean(-1); flat = load(f'{D}/pass_flat.png').mean(-1)

# the light, from the path tracer's render of white surfaces: shadows, sky, occlusion
light = ndimage.median_filter(to_lin(load(sys.argv[5])) / float(sys.argv[2]), size=(3, 3, 1))  # its grain
l = (light * LUMA).sum(-1)
lit_level = np.percentile(l[~sky_mask], 92)                     # a sunlit surface = 1
l /= lit_level
# the bounce's colour, from the textured render: only its tint, blurred to its low frequencies
if pt is not None:
    tinted = pt / np.maximum(base, 0.03)
    tint = tinted / np.maximum((tinted * LUMA).sum(-1, keepdims=True), 1e-4)
    tint = np.where((base.max(-1) < 0.05)[..., None], 1.0, tint)
    light_tint = ndimage.gaussian_filter(np.clip(tint, 0.6, 1.6), (12, 12, 0))
else:
    light_tint = np.ones_like(base)
# --- world-anchored light additions (v5) --------------------------------------------------------
EXTRAS = sys.argv[10].split(',') if len(sys.argv) > 10 else []
if len(sys.argv) > 9:
    from world import Camera, depth_of, fbm, SUN, smoothstep as ss
    cam = Camera(sys.argv[9])
    zdepth = depth_of(D); P = cam.world_positions(zdepth)
    ground = ~sky_mask
    # relief self-shadow (true relief): the textures' own small shadows under the sun
    import os
    if os.path.exists(f'{D}/pass_pomshadow.png'):
        l = l * (0.75 + 0.25 * load(f'{D}/pass_pomshadow.png').mean(-1))
    # cloud shadows: a cloud layer 600 units up, seen from each point along the sun
    H = 600.0
    k = (H - P[..., 1]) / SUN[1]
    cx, cz = P[..., 0] + SUN[0] * k, P[..., 2] + SUN[2] * k
    cloud = ss(0.52, 0.66, fbm(cx / 420 + 3.15, cz / 420 + 2.95, 5, seed=11))  # offset chosen so the main house stays in the sun, ~30% of the ground shaded
    if 'clouds' in EXTRAS: l = l * (1 - 0.42 * cloud * ground)
    # dappled light: where a tree's canopy (around 45 units up) sits between the sun and the ground,
    # spots of sun come through the gaps between the leaves
    mid = load(f'{D}/pass_matid.png'); trees = (mid[..., 0] > 0.5) & (mid[..., 1] < 0.5) & (mid[..., 2] < 0.5)
    cell = 8.0; canopy = set(zip(*(np.floor(P[trees][:, [0, 2]] / cell).astype(int).T))) if trees.any() else set()
    kc = (45.0 - P[..., 1]) / SUN[1]
    ax, az = P[..., 0] + SUN[0] * kc, P[..., 2] + SUN[2] * kc
    under = np.zeros(l.shape, bool)
    if canopy:
        gx, gz = np.floor(ax / cell).astype(int), np.floor(az / cell).astype(int)
        keys = gx * 100003 + gz
        cset = np.array([a * 100003 + b for a, b in canopy])
        under = np.isin(keys, cset) & ground & ~trees & (P[..., 1] < 20)
    spots = ss(0.62, 0.7, fbm(ax / 9, az / 9, 3, seed=23)) * under
    if 'dapple' in EXTRAS: l = np.where(under, np.maximum(l * 0.85, spots * 1.05), l)
    TREES = trees
else:
    TREES = None

# art direction
SHADOW = np.array([0.55, 0.57, 0.68], np.float32)   # shade: darker, cooler
SUN = np.array([1.04, 1.0, 0.94], np.float32)       # warm sunlight
# added colour, so the shift shows even on pure colours (a saturated green has no blue to tint)
SHADOW_LIFT = np.array([0.012, 0.02, 0.07], np.float32)
SUN_LIFT = np.array([0.03, 0.015, 0.0], np.float32)
step = smoothstep(float(sys.argv[6]), float(sys.argv[7]), l)  # soft two-tone: light or shade, a gentle edge
tone = SHADOW * (1 - step[..., None]) + SUN * step[..., None]
# occlusion kept, softened: deep corners darken a little more, never to black
tone *= (0.8 + 0.2 * smoothstep(0.1, 0.45, l))[..., None]
# coloured bounce: the light's own tint (the grass greening walls, warm ground), kept subtle
tone *= 1 + 0.3 * (light_tint - 1) * (1 - 0.5 * step[..., None])  # stronger in the shade
# relief of the textures under the sun, only where the sun shines
relief_shade = np.clip(relief / np.maximum(flat, 0.05), 0.6, 1.4)
tone *= (1 + (relief_shade - 1) * 1.25 * (0.35 + 0.65 * step))[..., None]  # relief stronger, a little in the shade too

# leaf translucency: foliage lit from behind (in shade or facing away from the sun) glows green-gold
if TREES is not None and 'translucency' in EXTRAS:
    n = load(f'{D}/pass_normal.png') * 2 - 1
    facing = (n * cam.view(SUN)).sum(-1)
    back = TREES * np.clip(1 - step - 0.3 * np.clip(facing, 0, 1), 0, 1)
    tone = tone + (back * 0.5)[..., None] * np.array([0.55, 0.85, 0.2], np.float32)
out = base * tone + (SHADOW_LIFT * (1 - step[..., None]) + SUN_LIFT * step[..., None]) * (0.4 + (base * LUMA).sum(-1, keepdims=True))
# vibrance: saturation up where colours are muted, highlights kept
lum = (out * LUMA).sum(-1, keepdims=True)
out = lum + (out - lum) * 1.28
out_srgb = to_srgb(out)
sky = load(sys.argv[3]) if len(sys.argv) > 3 else None
if sky is not None: out_srgb = np.where(sky_mask[..., None], sky, out_srgb)
Image.fromarray((out_srgb * 255 + 0.5).astype(np.uint8)).save(sys.argv[4] if len(sys.argv) > 4 else 'stylised.png')
print('lit level', lit_level)
