# Material and relief recognition from a texture's pixels alone (no names): each texel is classed
# from its neighbourhood's colour, contrast and stripe direction, and the class sets its physical
# properties. Output per texture: class map (debug), ORM map (R unused, G roughness, B metalness),
# normal map (relief from the texture's own shading), transmission map (glass).
import sys, json, numpy as np
from PIL import Image
from scipy import ndimage

# Virtual Volumetric Layered Depth: how much each class really protrudes (0 = smooth surface whose
# painted shading must not become geometry: plaster, sills, metal, glass, unknown)
VOLUME = {'stone': 1.0, 'tile': 0.8, 'wood': 0.6, 'foliage': 0.6}
VOLUME_LAYERS = 4
VOLUME_FLOOR = 0.4  # fraction of the texture's strong relief (95th percentile) below which nothing rises
CLASSES = {  # name: (debug colour, roughness, metalness, relief strength, transmission)
    'glass':   ((80, 200, 255), 0.05, 0.0, 0.0, 0.85),
    'foliage': ((40, 200, 60),  0.75, 0.0, 0.4, 0.0),
    'wood':    ((150, 90, 40),  0.65, 0.0, 1.6, 0.0),
    'tile':    ((255, 200, 0),  0.45, 0.0, 1.4, 0.0),
    'stone':   ((150, 150, 150), 0.85, 0.0, 2.6, 0.0),
    'plaster': ((240, 220, 180), 0.9, 0.0, 0.6, 0.0),
    'metal':   ((60, 120, 140), 0.35, 0.7, 0.5, 0.0),
    'matte':   ((255, 0, 255),  0.8, 0.0, 0.8, 0.0),
}
NAMES = list(CLASSES)

def features(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx, mn = rgb.max(-1), rgb.min(-1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    d = np.maximum(mx - mn, 1e-6)
    hue = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * (np.pi / 3)
    luma = 0.299 * r + 0.587 * g + 0.114 * b
    m1 = ndimage.uniform_filter(luma, 9, mode='wrap'); m2 = ndimage.uniform_filter(luma ** 2, 9, mode='wrap')
    std = np.sqrt(np.maximum(m2 - m1 ** 2, 0))
    gx = ndimage.sobel(luma, 1, mode='wrap'); gy = ndimage.sobel(luma, 0, mode='wrap')
    jxx = ndimage.uniform_filter(gx * gx, 15, mode='wrap'); jyy = ndimage.uniform_filter(gy * gy, 15, mode='wrap'); jxy = ndimage.uniform_filter(gx * gy, 15, mode='wrap')
    coh = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2) / np.maximum(jxx + jyy, 1e-6)
    f = lambda x: ndimage.uniform_filter(x, 5, mode='wrap')
    # colour as a point on the hue circle scaled by saturation, brightness, contrast, stripes
    F = np.dstack([f(np.cos(hue) * sat), f(np.sin(hue) * sat), f(mx), std * 6, coh])
    return F, luma

# reference regions measured once in ORAS (texture index, x0, y0, x1, y1): the profiles
REFERENCES = {
    'stone':   [(2, 0, 110, 128, 256)],
    'wood':    [(10, 0, 150, 60, 256), (11, 0, 40, 128, 100)],
    'plaster': [(10, 0, 40, 110, 70), (10, 110, 200, 240, 250)],
    'tile':    [(10, 115, 0, 160, 120)],
    'glass':   [(6, 0, 0, 60, 64)],
    'foliage': [(4, 0, 0, 128, 128), (5, 0, 0, 128, 128)],
    'metal':   [(10, 80, 145, 256, 160)],
}
_profiles = None
def profiles():
    global _profiles
    if _profiles is None:
        _profiles = {}
        for name, regions in REFERENCES.items():
            vals = []
            for i, x0, y0, x1, y1 in regions:
                F, _ = features(np.asarray(Image.open(f'orig/{i}.png').convert('RGB')).astype(np.float32) / 255)
                vals.append(F[y0:y1, x0:x1].reshape(-1, F.shape[-1]))
            v = np.concatenate(vals)
            _profiles[name] = (v.mean(0), v.std(0) + 0.03)
    return _profiles

def classify(rgb, alpha):
    F, luma = features(rgb)
    P = profiles()
    names = list(P)
    dist = np.stack([(((F - P[n][0]) / P[n][1]) ** 2).sum(-1) for n in names])
    best = dist.argmin(0)
    cls = np.array([NAMES.index(n) for n in names])[best]
    cls[dist.min(0) > 25] = NAMES.index('matte')  # far from every profile: left matte
    cls[alpha < 0.5] = NAMES.index('matte')
    votes = np.stack([ndimage.uniform_filter((cls == k).astype(np.float32), 5, mode='wrap') for k in range(len(NAMES))])
    out = votes.argmax(0)
    out[alpha < 0.5] = NAMES.index('matte')
    return out, luma

def run(src, dst):
    im = np.asarray(Image.open(src).convert('RGBA')).astype(np.float32) / 255
    rgb, a = im[..., :3], im[..., 3]
    cls, luma = classify(rgb, a)
    table = np.array([CLASSES[n][1:] for n in NAMES], np.float32)
    rough, metal, relief, trans = (table[cls, i] for i in range(4))
    # ORAS's painted outlines (thin dark strokes along edges): drawn ridges, so read as raised,
    # not as the grooves their darkness would otherwise give
    median = ndimage.median_filter(luma, 7, mode='wrap')
    dark = (median - luma) > 0.08
    outline = dark & ~ndimage.binary_opening(dark, np.ones((4, 4)))
    outline = ndimage.binary_dilation(outline, iterations=1)
    Image.fromarray((outline * 255).astype(np.uint8)).save(f'{dst}_outline.png')
    surface = np.where(outline, median, luma)
    # relief: the texture's own shading read as height (high-pass, so flat colour areas stay flat)
    height = surface - ndimage.gaussian_filter(surface, 6, mode='wrap')
    height = ndimage.gaussian_filter(height, 0.7, mode='wrap') * relief
    height += ndimage.gaussian_filter(outline.astype(np.float32), 0.8, mode='wrap') * 0.06
    # no relief along the borders between regions (another material, an atlas cell, the joins of
    # two pieces of the map): it fades to flat within 3 texels so joins don't form a ridge
    border = np.zeros(cls.shape, bool)
    border[:, 1:] |= cls[:, 1:] != cls[:, :-1]; border[1:, :] |= cls[1:, :] != cls[:-1, :]
    big_edge = ndimage.gaussian_gradient_magnitude(ndimage.gaussian_filter(luma, 2), 1.5) > 0.06
    border |= big_edge
    border[0, :] = border[-1, :] = border[:, 0] = border[:, -1] = True
    dist = ndimage.distance_transform_edt(~border)
    height *= np.clip(dist / 3, 0, 1)
    # height map for true relief (parallax): 0 = deepest, 1 = highest; flat surfaces at 1 (nothing sinks)
    hn = height - height.max()
    span = max(-hn.min(), 1e-6)
    Image.fromarray(((1 + hn / span * min(span / 0.08, 1)) * 255).clip(0, 255).astype(np.uint8)).save(f'{dst}_height.png')
    # volume: the parts that stand out of their neighbourhood (positive local relief), only on the
    # classes that really protrude; borders already faded above, so joins stay flat
    weight = np.array([VOLUME.get(nm, 0.0) for nm in NAMES], np.float32)[cls]
    rise = np.maximum(height - ndimage.gaussian_filter(height, 4, mode='wrap'), 0)
    rise = ndimage.gaussian_filter(rise, 0.6, mode='wrap') * weight
    # scaled to the texture's own strong relief (grass rises 50 times less than stone), with a floor:
    # soft painted shading (a sill, a stain) stays under it; only marked relief reaches the layers
    scale = np.percentile(rise[rise > 0], 95) if (rise > 0).any() else 1.0
    vol = np.clip((rise / max(scale, 1e-6) - VOLUME_FLOOR) / (1 - VOLUME_FLOOR), 0, 1)
    Image.fromarray((vol * 255).astype(np.uint8)).save(f'{dst}_volume.png')
    # one cut-out per layer: layer k keeps the texels at least k/(N+1) high (the game's colours, and
    # white for the light pass)
    for k in range(1, VOLUME_LAYERS + 1):
        keep = ((vol >= k / (VOLUME_LAYERS + 1)) & (a >= 0.5)).astype(np.float32)
        Image.fromarray((np.dstack([rgb, keep]) * 255).astype(np.uint8)).save(f'{dst}_layer{k}.png')
        Image.fromarray((np.dstack([np.ones_like(rgb), keep]) * 255).astype(np.uint8)).save(f'{dst}_layerw{k}.png')
    nx = -ndimage.sobel(height, 1, mode='wrap'); ny = ndimage.sobel(height, 0, mode='wrap')
    n = np.dstack([nx * 2, ny * 2, np.ones_like(nx)]); n /= np.linalg.norm(n, axis=-1, keepdims=True)
    Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8)).save(f'{dst}_normal.png')
    orm = np.dstack([np.ones_like(rough), rough, metal])
    Image.fromarray((orm * 255).astype(np.uint8)).save(f'{dst}_orm.png')
    Image.fromarray((np.dstack([trans] * 3) * 255).astype(np.uint8)).save(f'{dst}_trans.png')
    dbg = np.array([CLASSES[nm][0] for nm in NAMES], np.uint8)[cls]
    Image.fromarray(dbg).save(f'{dst}_classes.png')
    counts = {NAMES[k]: int((cls == k).sum()) for k in range(len(NAMES)) if (cls == k).sum()}
    return counts

if __name__ == '__main__':
    report = {}
    for i in range(int(sys.argv[1])):
        report[i] = run(f'orig/{i}.png', f'mat/{i}')
    print(json.dumps(report))
