# Trinity model (.trmsh mesh layout, .trmbf vertex/index buffers, .trmtr material texture paths) -> a JSON mesh and
# PNG textures, loaded by tools/terrain/render/trmodel.js. Layout read on plants_me_tree_fir02 (4 November): one
# vertex accessor (attribute kind, format, offset; stride), position RGB32F, normal and tangent RGBA16F, uv RG32F,
# u16 indices, one index range per material.
import sys, os, struct, json, re, base64
import numpy as np
sys.path.insert(0, os.path.dirname(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import pack, bntx
import flatbuffer_reader as fb
from PIL import Image
P = pack.Packs()
FMT = {51: ('f', 3), 43: ('e', 4), 48: ('f', 2), 20: ('B', 4), 54: ('f', 4)}

def strings(b):
    return re.findall(rb'[A-Za-z0-9_./]{4,}\.(?:trmsh|trmbf|trmtr|bntx)', b)

def texture(path, out):
    b = P.file(path)
    if b is None: return None
    t = bntx.parse(b)[0]
    if t.fmt >> 8 in (0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x20):
        im = bntx.to_image(t)
    else:
        bw, bh, bpp = bntx.BLOCK[t.fmt >> 8]
        gob = 1 << t.bh_log2
        while gob > 1 and t.h <= (gob // 2) * 8: gob //= 2
        raw = bntx.deswizzle(t.data[t.mip_off[0]:t.mip_off[0] + t.size], t.w, t.h, bpp, gob)
        im = Image.frombytes({4: 'RGBA', 1: 'L', 2: 'LA'}[bpp], (t.w, t.h), raw[:t.w * t.h * bpp])
    im.save(out); return im

def export(base_dir, name, out_dir, mesh=None, out_name=None):
    os.makedirs(out_dir, exist_ok=True)
    d = f'{base_dir}/{name}'
    msh = P.file(f'{d}/{mesh or name}.trmsh'); tr = fb.root(msh)
    mbf = fb.root(P.file(f'{d}/{mesh or name}.trmbf'))
    mesh = tr.tables(1)[0]; acc = mesh.tables(3)[0]
    stride = acc.tables(1)[0].scalar(0, '<I')
    attrs = [(a.scalar(1, '<I'), a.scalar(3, '<I'), a.scalar(4, '<I')) for a in acc.tables(0)]
    mb = mbf.tables(1)[0]
    ib = mb.tables(0)[0].bytes_(0); vb = mb.tables(1)[0].bytes_(0)
    nv = len(vb) // stride
    raw = np.frombuffer(vb, np.uint8).reshape(nv, stride)
    out = {}
    for kind, fmt, off in attrs:
        t, n = FMT[fmt]; sz = struct.calcsize(t) * n
        arr = np.frombuffer(raw[:, off:off + sz].tobytes(), t).reshape(nv, n).astype(np.float32)
        out[{1: 'position', 2: 'normal', 3: 'tangent', 6: 'uv', 5: 'color'}.get(kind, f'a{kind}')] = arr
    idx = np.frombuffer(ib, np.uint16 if nv <= 65536 else np.uint32).astype(np.uint32)
    # materials: their index range, and their textures from the .trmtr
    mtr = P.file(f'{d}/{name}.trmtr')
    tex_refs = [s.decode() for s in strings(mtr)]
    groups = []
    for mt in mesh.tables(4):
        count, start = mt.scalar(0, '<I'), mt.scalar(1, '<I')
        f = mt.off(3); a = mt.pos + f; t = a + struct.unpack_from('<I', msh, a)[0]
        l = struct.unpack_from('<I', msh, t)[0]; mname = msh[t + 4:t + 4 + l].decode()
        groups.append({'material': mname, 'start': start, 'count': count})
    textures = {}
    for ref in tex_refs:
        tname = ref.split('/')[-1][:-5]
        path = os.path.normpath(f'{d}/{ref}').replace('\\', '/')
        if texture(path, f'{out_dir}/{tname}.png') is not None: textures[tname] = f'{tname}.png'
    js = {k: base64.b64encode(v.tobytes()).decode() for k, v in out.items()}
    js['dims'] = {k: v.shape[1] for k, v in out.items()}
    js['index'] = base64.b64encode(idx.tobytes()).decode()
    js['groups'] = groups; js['textures'] = textures
    json.dump(js, open(f'{out_dir}/{out_name or name}.json', 'w'))
    return nv, len(idx), groups, list(textures), out['position'].min(0), out['position'].max(0)

if __name__ == '__main__':
    # model.py <out dir> <model name>[:<mesh name>] ...   (models under field_graphic/terrain_model)
    out_dir = sys.argv[1]
    for arg in sys.argv[2:]:
        name, _, mesh = arg.partition(':')
        r = export('field_graphic/terrain_model', name, out_dir, mesh=mesh or None, out_name=mesh or None)
        print(arg, r[0], 'vertices', r[1], 'indices', [g['material'] for g in r[2]])
