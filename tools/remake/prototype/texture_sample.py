# scratch tool: what colour a scene shows from above at every 1/R tile, read from its textures (the topmost
# surface whose texel there is opaque; the texture's wrap modes applied), then that colour's nearest class.
# argv: gltf, R, output name, offset x, offset z (the scene moved onto Platinum's world grid), window gc0 gr0
import os
import json, base64, io, re, sys
import numpy as np
from PIL import Image
S = os.environ.get('REMAKE_WORK', '.')
GLTF, R, OUT, OX, OZ, GC0, GR0 = sys.argv[1], int(sys.argv[2]), sys.argv[3], float(sys.argv[4]), float(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7])
U = 18.0 / R
j = json.loads(open(GLTF).read())
buf = base64.b64decode(j['buffers'][0]['uri'].split(',', 1)[1])
def acc(i):
    a = j['accessors'][i]; v = j['bufferViews'][a['bufferView']]; off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    size = np.dtype(dt).itemsize * n; stride = v.get('byteStride', 0) or size
    raw = np.frombuffer(buf, dtype=np.uint8, count=stride * (a['count'] - 1) + size, offset=off)
    out = np.empty((a['count'], n), dtype=dt)
    for k in range(a['count']): out[k] = np.frombuffer(raw[k * stride:k * stride + size].tobytes(), dtype=dt)
    return out
images = {}
def texture(mat):
    m = j['materials'][mat]; info = m.get('pbrMetallicRoughness', {}).get('baseColorTexture')
    if not info: return None
    t = j['textures'][info['index']]
    if t['source'] not in images:
        img = j['images'][t['source']]
        data = base64.b64decode(img['uri'].split(',', 1)[1]) if 'uri' in img else buf[j['bufferViews'][img['bufferView']].get('byteOffset', 0):][:j['bufferViews'][img['bufferView']]['byteLength']]
        smp = j['samplers'][t['sampler']] if 'sampler' in t else {}
        images[t['source']] = (np.array(Image.open(io.BytesIO(data)).convert('RGBA')), smp.get('wrapS', 10497), smp.get('wrapT', 10497))
    return images[t['source']]
def wrap(x, mode):
    if mode == 33071: return min(max(x, 0.0), 1.0 - 1e-6)
    if mode == 33648: f = x % 2.0; return f if f < 1 else 2.0 - f - 1e-6
    return x % 1.0
hits = {}   # (c, r) in the window -> list of (y, rgba)
for mesh in j['meshes']:
    for p in mesh['primitives']:
        if 'material' not in p or 'TEXCOORD_0' not in p['attributes']: continue
        tex = texture(p['material'])
        if tex is None: continue
        img, ws, wt = tex; h, w = img.shape[:2]
        P = acc(p['attributes']['POSITION']).astype(np.float64); P[:, 0] += OX; P[:, 2] += OZ
        T = acc(p['attributes']['TEXCOORD_0']).astype(np.float64)
        I = acc(p['indices']).reshape(-1).astype(np.int64).reshape(-1, 3)
        for t in I:
            a, b, c = P[t[0]], P[t[1]], P[t[2]]
            d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
            if abs(d) < 1e-9: continue
            xs = (a[0], b[0], c[0]); zs = (a[2], b[2], c[2])
            c0 = max(int(np.floor(min(xs) / U - 0.5)) + 1, GC0 * R); c1 = min(int(np.floor(max(xs) / U - 0.5)), (GC0 + 40) * R - 1)
            r0 = max(int(np.floor(min(zs) / U - 0.5)) + 1, GR0 * R); r1 = min(int(np.floor(max(zs) / U - 0.5)), (GR0 + 40) * R - 1)
            for gr in range(r0, r1 + 1):
                for gc in range(c0, c1 + 1):
                    x = (gc + 0.5) * U; z = (gr + 0.5) * U
                    l1 = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / d
                    l2 = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / d
                    l3 = 1 - l1 - l2
                    if min(l1, l2, l3) < -1e-6: continue
                    y = l1 * a[1] + l2 * b[1] + l3 * c[1]
                    u = l1 * T[t[0]][0] + l2 * T[t[1]][0] + l3 * T[t[2]][0]; v = l1 * T[t[0]][1] + l2 * T[t[1]][1] + l3 * T[t[2]][1]
                    px = img[int(wrap(v, wt) * h), int(wrap(u, ws) * w)]
                    if px[3] < 128: continue
                    hits.setdefault((gc - GC0 * R, gr - GR0 * R), []).append((y, tuple(int(k) for k in px[:3])))
N = 40 * R
colours = np.zeros((N, N, 3), dtype=np.uint8)
for (c, r), lst in hits.items(): colours[r, c] = max(lst)[1]
Image.fromarray(colours).resize((N * (640 // N), N * (640 // N)), Image.NEAREST).save(f'{S}/slice/{OUT}_colours.png')
np.save(f'{S}/slice/{OUT}_colours.npy', colours)
print(OUT, len(hits), 'samples coloured')
