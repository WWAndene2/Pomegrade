# usum_character.py: an Ultra Moon player (Elio or Selene, in a chosen outfit) made an Omega Ruby field character, on an
# ORAS character's body (ORAS_ENGINE.md, "Ultra Sun/Moon characters on Omega Ruby"). ORAS's body, skeleton, face sheet
# and proportions are kept (its motions then fit); its hat, hair and bag are dropped; its body texture is repainted with
# the USUM outfit's colours; the USUM hair and cap are fitted onto its Head bone. Reads the owner's dumps, never stores
# them: everything goes into the work directory (CLAUDE.md 8).
#
#   python3 -I usum_character.py extract <um.3ds> <oras.3ds> <work> [--player 1|2] [--base N] [--outfit part=i,...]
#       the outfit's parts from Ultra Moon and ORAS character N (a/0/2/1 member; default 172, Brendan, or 171, May, for
#       --player 2) into <work>: each as SMD (spica smd) and PNG textures (spica png). Parts: face hair hat tops bottoms
#       legs shoes, each the item's index in its GARC (default 0, the starting outfit). Needs remake_tool and spica
#       (REMAKE_TOOL, SPICA: paths, default on PATH) and DOTNET_ROOT for spica.
#   python3 -I usum_character.py build <work> <out> [--shirt plain|stripes] [--hair-colour R,G,B] [--cap-scale S] [--hair-drop F]
#       --shirt: the tops' colour as one (default) or as horizontal bands of its light and dark colours (Elio's starting
#       shirt); --hair-colour: the runtime hair colour (default 58,46,40, dark brown: chosen by eye, the game's table not
#       found); --cap-scale: the hat grown around the head (default 1.1: none of the hair shows through); --hair-drop: the
#       hair below the head joint shortened (default 0.5: out of the shoulders)
#       <out>.png: four views (front, three-quarter, side, back) of the result;
#       <out>_tex/: the base's body and face textures repainted, the hair texture coloured, the cap's texture;
#       <out>_head.smd: the USUM hair and cap on the base's skeleton (every vertex on its Head bone), for the in-game model.
#
# What the USUM data is (checked on Ultra Moon, 17 October, ORAS_ENGINE.md has the evidence): the player is built from parts,
# one GARC per kind (PARTS), every part a CM package whose model shares the 79-bone skeleton pc0001_00 (Selene pc0002_00);
# the hair's model space is the Neck's (its root bone is Neck_attach). Skin and hair colours are not in the textures: an
# item's _m0N mask (alpha) marks where the runtime colour goes (_m03 hair, others skin), the clothes' colours are drawn in.
# spica's PNGs are bottom-up against the SMD's UVs (rows = v, not 1 - v): the face sheet shows it, its blink motion moves
# v by +0.5 onto the closed eyes.
import sys, os, re, math, glob, shutil, subprocess
import numpy as np
from PIL import Image

# --- the USUM player's parts: GARC by player, read from the model names inside (p1_hair_..., p2_tops_...) ---------------
PARTS = {
    1: {'face': 'a/1/7/6', 'hair': 'a/1/7/7', 'bottoms': 'a/1/8/1', 'hat': 'a/1/8/2', 'legs': 'a/1/8/3', 'shoes': 'a/1/8/4', 'tops': 'a/1/8/6'},
    2: {'face': 'a/1/8/8', 'hair': 'a/1/8/9', 'bottoms': 'a/1/9/4', 'hat': 'a/1/9/5', 'legs': 'a/1/9/6', 'shoes': 'a/1/9/7', 'tops': 'a/1/9/9'},
}
BASES = {1: 172, 2: 171}  # ORAS's Brendan (rstr0002_00_fi) and May (rstr0001_00_fi), a/0/2/1

def tool(env, default): return os.environ.get(env, default)

def run(*cmd, check=True):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 and check: sys.exit(' '.join(cmd) + ' failed:\n' + r.stdout + r.stderr)
    return r.stdout + r.stderr

def options(args, names):
    out = {}; rest = []; i = 0
    while i < len(args):
        if args[i] in names and i + 1 < len(args): out[args[i]] = args[i + 1]; i += 2
        else: rest.append(args[i]); i += 1
    return out, rest

def extract(um, oras, work, player, base, outfit):
    rt, sp = tool('REMAKE_TOOL', 'remake_tool'), tool('SPICA', 'spica')
    tmp = os.path.join(work, 'garc'); os.makedirs(tmp, exist_ok=True); os.makedirs(os.path.join(work, 'tex'), exist_ok=True)
    def member(rom, garc, index, name):
        g = os.path.join(tmp, garc.replace('/', '') + '.garc'); d = g[:-5]; err = ''
        if not os.path.isdir(d):
            run(rt, 'oras-extract', rom, garc, g)
            # remake_tool garc stops at a member it takes for LZ data but can't decompress (a/1/8/4 has one): the members
            # before it are written, so only a missing member is an error (below)
            err = run(rt, 'garc', g, d, check=False)
        f = [x for x in os.listdir(d) if x.split('.')[0] == str(index)]
        if not f or f[0].endswith('.empty'): sys.exit(f'{garc} member {index}: missing or empty' + ('\n' + err if err else ''))
        src = os.path.join(d, f[0])
        run(sp, 'smd', src, os.path.join(work, name + '.smd')); run(sp, 'png', src, os.path.join(work, 'tex'))
        info = run(sp, 'info', src); m = re.search(r'^model 0 (\S+):', info, re.M)
        print(f'{name}: {garc} member {index}: {m.group(1) if m else "?"}')
    for part, garc in PARTS[player].items():
        member(um, garc, outfit.get(part, 0), part)
    member(oras, 'a/0/2/1', base, 'base')
    shutil.rmtree(tmp)  # the game's archives are not kept

# --- geometry ----------------------------------------------------------------------------------------------------------
def euler(rx, ry, rz):
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    return np.array([[cz,-sz,0],[sz,cz,0],[0,0,1]]) @ np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]]) @ np.array([[1,0,0],[0,cx,-sx],[0,sx,cx]])

class Smd:  # nodes, bind pose (world position and rotation of each bone), triangles (texture, [(pos, normal, uv, weights)])
    def __init__(self, path):
        lines = open(path).read().splitlines(); i = 0; self.nodes = {}; self.parent = {}; local = {}; self.tris = []
        while i < len(lines):
            l = lines[i].strip()
            if l == 'nodes':
                i += 1
                while lines[i].strip() != 'end':
                    m = re.match(r'(\d+) "([^"]*)" (-?\d+)', lines[i].strip()); self.nodes[int(m[1])] = m[2]; self.parent[int(m[1])] = int(m[3]); i += 1
            elif l == 'skeleton':
                i += 2
                while lines[i].strip() != 'end':
                    p = lines[i].split(); local[int(p[0])] = (np.array(list(map(float, p[1:4]))), euler(*map(float, p[4:7]))); i += 1
            elif l == 'triangles':
                i += 1
                while lines[i].strip() != 'end':
                    tex = lines[i].strip(); vs = []
                    for k in range(3):
                        p = lines[i + 1 + k].split(); n = int(p[9]) if len(p) > 9 else 0
                        w = [(int(p[10 + 2 * j]), float(p[11 + 2 * j])) for j in range(n)] or [(int(p[0]), 1.0)]
                        vs.append((np.array(list(map(float, p[1:4]))), np.array(list(map(float, p[4:7]))), (float(p[7]), float(p[8])), w))
                    self.tris.append((tex, vs)); i += 4
                continue
            i += 1
        self.local = local; self.P = {}; self.R = {}
        def go(j):
            if j in self.P: return
            t, r = local[j]
            if self.parent[j] < 0: self.P[j], self.R[j] = t.copy(), r; return
            go(self.parent[j]); q = self.parent[j]; self.P[j] = self.P[q] + self.R[q] @ t; self.R[j] = self.R[q] @ r
        for j in self.nodes: go(j)
        self.id = {v: k for k, v in self.nodes.items()}
    def dominant(self, w): return self.nodes[max(w, key=lambda x: x[1])[0]]

def tex(path): return np.asarray(Image.open(path).convert('RGBA')).astype(float)
def mask_of(texdir, name):  # an item's colour mask: its texture's name without _on, then _m0N (p1_hair_midium_on -> p1_hair_midium_m03)
    base = name[:-len('.tga.png')] if name.endswith('.tga.png') else name[:-4]
    if base.endswith('_on'): base = base[:-3]
    found = sorted(glob.glob(os.path.join(texdir, base + '_m0[0-9].tga.png')))
    return (found[0], found[0].endswith('_m03.tga.png')) if found else (None, False)

REGION = [  # the base's body: dominant bone of a triangle -> what the USUM outfit puts there
    (r'^Bag', 'hide'),                                                          # the ORAS bag: USUM's players have none
    (r'^(Waist|Spine1|Spine2|Neck|[LR]Shoulder|[LR]ArmA|[LR]ArmB)$', 'shirt'),
    (r'^(Hips|[LR]Thigh)$', 'shorts'),
    (r'^[LR](Foot|Toe)$', 'shoes'),
]
def region(bone):
    for pat, r in REGION:
        if re.search(pat, bone): return r
    return 'keep'

def build(work, out, hair_colour, cap_scale, hair_drop, stripes):
    T = os.path.join(work, 'tex')
    base, face_p, hair_p, hat_p = (Smd(os.path.join(work, n + '.smd')) for n in ('base', 'face', 'hair', 'hat'))
    texname = lambda smd, key: next(t for t, _ in smd.tris if key in t)
    # --- the outfit's colours, read from its textures (the masked skin left out) ---
    def cloth(part, key=None):
        smd = Smd(os.path.join(work, part + '.smd')); name = next(t for t, _ in smd.tris if not key or key in t)
        t = tex(os.path.join(T, name)); m, _ = mask_of(T, name); keep = t[..., 3] > 100
        if m and (keep & (tex(m)[..., 3] < 128)).any(): keep &= tex(m)[..., 3] < 128  # a mask over all of it (Selene's
        return t[keep][:, :3]                                                         # hotpants): the whole texture
    shirt = cloth('tops'); lum = shirt.mean(1)
    light = np.median(shirt[lum > np.percentile(lum, 60)], 0); dark = np.median(shirt[lum < np.percentile(lum, 20)], 0)
    plain = np.median(shirt, 0)  # a plain shirt's colour; --shirt stripes: horizontal bands of the light and dark ones
    shorts = np.median(cloth('bottoms'), 0)
    shoes = cloth('shoes'); sat = shoes.max(1) - shoes.min(1)
    coloured = (sat > 60); shoe_main = np.median(shoes[coloured], 0) if coloured.any() else np.median(shoes, 0)
    pale = (sat < 20) & (shoes.mean(1) > 200); shoe_light = np.median(shoes[pale], 0) if pale.any() else np.array([240., 240, 240])
    print('outfit colours: shirt', light.round(), dark.round(), 'shorts', shorts.round(), 'shoes', shoe_main.round(), shoe_light.round())
    os.makedirs(out + '_tex', exist_ok=True)

    # --- the base's body texture repainted, texel by texel, through the triangles that use it ---
    bodyname = texname(base, '_body'); body = tex(os.path.join(T, bodyname)); new = body.copy(); th, tw = body.shape[:2]
    torso = (base.P[base.id['Waist']][1], base.P[base.id['Neck']][1]); period = (torso[1] - torso[0]) / 6.5
    skinref = np.array([236, 196, 176.])  # the ORAS characters' skin, kept as it is
    for t, vs in base.tris:
        if t != bodyname: continue
        r = region(base.dominant(vs[0][3]))
        if r in ('keep', 'hide'): continue
        uv = np.array([[v[2][0] * tw, np.mod(v[2][1], 1) * th] for v in vs]); P = np.array([v[0] for v in vs])
        x0, y0 = np.floor(uv.min(0)).astype(int); x1, y1 = np.ceil(uv.max(0)).astype(int)
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = uv; d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9: continue
        l0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d; l1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d; l2 = 1 - l0 - l1
        m = (l0 >= -0.12) & (l1 >= -0.12) & (l2 >= -0.12)  # bleeds past the edges: none of the base's colour left on the UV seams
        xs, ys = (X[m] - 0.5).astype(int) % tw, (Y[m] - 0.5).astype(int).clip(0, th - 1)
        src = body[ys, xs, :3]; L = src.mean(1) / 255.0; free = np.linalg.norm(src - skinref, axis=1) >= 40
        if r == 'shirt':
            h = l0[m] * P[0][1] + l1[m] * P[1][1] + l2[m] * P[2][1]
            col = np.where((np.mod((h - torso[0]) / period, 1.0) < 0.32)[:, None], dark, light) if stripes else np.repeat(plain[None], len(L), 0)
            col[h < torso[0] + 0.6] = shorts                                                        # a straight hem
            shade = np.clip(0.8 + 0.6 * (L - np.median(L)), 0.78, 1.05)  # the base's folds kept, not its colours
        elif r == 'shorts':
            col = np.repeat(shorts[None], len(L), 0); shade = np.clip(0.75 + 0.9 * (L - np.median(L)), 0.6, 1.25)
        else:
            col = np.where((L > 0.7)[:, None], shoe_light, shoe_main); shade = np.clip(0.85 + 0.5 * (L - np.median(L)), 0.7, 1.1)
        new[ys[free], xs[free], :3] = (col * shade[:, None])[free].clip(0, 255)
    Image.fromarray(new.astype(np.uint8)).save(os.path.join(out + '_tex', bodyname))

    # --- the base's face sheet with the USUM player's grey irises ---
    facename = texname(base, '_face'); face = tex(os.path.join(T, facename)); f = face[..., :3]
    blue = (f[..., 2] > f[..., 0] + 25) & (f[..., 2] > 60); g = f[blue].mean(1, keepdims=True)
    f[blue] = np.concatenate([g * 0.98, g * 0.96, g * 0.95], 1); face[..., :3] = f
    Image.fromarray(face.clip(0, 255).astype(np.uint8)).save(os.path.join(out + '_tex', facename))

    # --- the USUM hair and cap on the base's Head bone: fitted on the face width, tops aligned ---
    hl_u = lambda v: face_p.R[face_p.id['Head']].T @ (v - face_p.P[face_p.id['Head']])
    hl_b = lambda v: base.R[base.id['Head']].T @ (v - base.P[base.id['Head']])
    uf = np.array([hl_u(v[0]) for _, vs in face_p.tris for v in vs]); bf = np.array([hl_b(v[0]) for t, vs in base.tris if t == facename for v in vs])
    s = np.ptp(bf[:, 1]) / np.ptp(uf[:, 1])  # local Y is across the face, local X up
    off = (bf.min(0) + bf.max(0)) / 2 - s * (uf.min(0) + uf.max(0)) / 2; off[0] = bf.max(0)[0] - s * uf.max(0)[0]
    HB = base.id['Head']
    def onto(v, hair=False):
        l = hl_u(v)
        if hair and l[0] < 0: l = np.array([l[0] * hair_drop, l[1], l[2]])  # below the head joint: the chibi's short neck
        return base.P[HB] + base.R[HB] @ (s * l + off)
    hc = base.P[HB] + base.R[HB] @ off; RH = base.R[HB] @ face_p.R[face_p.id['Head']].T
    def model_space(part):  # a part rooted at <Bone>_attach is modelled in that bone's space (Elio's hair: Neck; Selene's hair, hat: Head)
        root = part.nodes[min(part.nodes)]
        if not root.endswith('_attach'): return lambda v: v, lambda n: n
        j = face_p.id[root[:-len('_attach')]]
        return (lambda v: face_p.R[j] @ v + face_p.P[j]), (lambda n: face_p.R[j] @ n)
    hp, hn = model_space(hair_p); cp, cn = model_space(hat_p)
    hairtris = [(t, [(onto(hp(v[0]), True), RH @ hn(v[1]), v[2]) for v in vs]) for t, vs in hair_p.tris]
    # the cap a little larger than the USUM one: the hair, fitted to the chibi head, stays inside it (no clipping)
    captris = [(t, [(hc + cap_scale * (onto(cp(v[0])) - hc), RH @ cn(v[1]), v[2]) for v in vs]) for t, vs in hat_p.tris]
    print('USUM head parts: scale', round(s, 4), 'cap', cap_scale)

    hairname = hairtris[0][0]; capname = captris[0][0]
    hairtex = tex(os.path.join(T, hairname)); m, _ = mask_of(T, hairname)
    if m: a = tex(m)[..., 3:4] / 255.0; hairtex[..., :3] *= (1 - a) + a * np.array(hair_colour) / 255.0
    Image.fromarray(hairtex.clip(0, 255).astype(np.uint8)).save(os.path.join(out + '_tex', hairname))
    shutil.copy(os.path.join(T, capname), os.path.join(out + '_tex', capname))

    with open(out + '_head.smd', 'w') as o:  # the base's skeleton, the hair and cap triangles on its Head bone
        o.write('version 1\nnodes\n' + ''.join(f'{j} "{base.nodes[j]}" {base.parent[j]}\n' for j in sorted(base.nodes)) + 'end\nskeleton\ntime 0\n')
        src = open(os.path.join(work, 'base.smd')).read(); o.write(src[src.index('time 0') + 7:src.index('end', src.index('time 0'))] + 'end\ntriangles\n')
        for t, vs in hairtris + captris:
            o.write(t + '\n')
            for p, n, uv in vs: o.write(f'{HB} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {n[0]:.6f} {n[1]:.6f} {n[2]:.6f} {uv[0]:.6f} {uv[1]:.6f} 1 {HB} 1\n')
        o.write('end\n')

    # --- preview: the base without its hat, hair, bag and switchable parts, with the USUM head parts ---
    TEX = {bodyname: new, facename: face, hairname: hairtex, capname: tex(os.path.join(T, capname))}
    meshes = [(t, [(v[0], v[2]) for v in vs]) for t, vs in base.tris
              if t in (bodyname, facename) and not (t == bodyname and region(base.dominant(vs[0][3])) == 'hide')]
    meshes += [(t, [(p, uv) for p, _, uv in vs]) for t, vs in hairtris + captris]
    views = [render(meshes, TEX, y) for y in (0.0, -math.pi / 4, -math.pi / 2, math.pi)]
    Image.fromarray(np.concatenate(views, 1)).save(out + '.png')

def render(meshes, TEX, yaw, size=640):  # z-buffered, textured, orthographic, one light; a preview, not the game's shading
    allp = np.array([p for _, vs in meshes for p, _ in vs]); lo, hi = allp.min(0), allp.max(0); c = (lo + hi) / 2; h = (hi - lo)[1] * 1.08
    cy_, sy_ = math.cos(yaw), math.sin(yaw); Ry = np.array([[cy_, 0, sy_], [0, 1, 0], [-sy_, 0, cy_]])
    img = np.zeros((size, size, 3), np.uint8); img[:] = (44, 46, 54); zb = np.full((size, size), -1e9)
    Ld = np.array([-0.4, 0.6, 0.7]); Ld /= np.linalg.norm(Ld)
    for key, vs in meshes:
        Tx = TEX[key]; th, tw = Tx.shape[:2]; q = [Ry @ (p - c) for p, _ in vs]
        sp = [((x / h + 0.5) * size, (0.5 - y / h) * size, z) for x, y, z in q]
        nrm = np.cross(q[1] - q[0], q[2] - q[0]); ln = np.linalg.norm(nrm)
        if ln == 0: continue
        shade = 0.6 + 0.4 * abs(np.dot(nrm / ln, Ld))
        xs = [p[0] for p in sp]; ys = [p[1] for p in sp]
        x0, x1 = max(int(min(xs)), 0), min(int(max(xs)) + 1, size - 1); y0, y1 = max(int(min(ys)), 0), min(int(max(ys)) + 1, size - 1)
        if x1 < x0 or y1 < y0: continue
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = sp; d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9: continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        l0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d; l1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d; l2 = 1 - l0 - l1
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not m.any(): continue
        z = l0 * az + l1 * bz + l2 * cz; sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= z > sub
        u = l0 * vs[0][1][0] + l1 * vs[1][1][0] + l2 * vs[2][1][0]; v = l0 * vs[0][1][1] + l1 * vs[1][1][1] + l2 * vs[2][1][1]
        col = Tx[(np.mod(v, 1) * th).astype(int).clip(0, th - 1), (np.mod(u, 1) * tw).astype(int).clip(0, tw - 1)]  # rows = v: bottom-up PNGs
        m &= col[..., 3] > 100; sub[m] = z[m]
        reg = img[y0:y1 + 1, x0:x1 + 1]; reg[m] = (col[..., :3][m] * shade).clip(0, 255).astype(np.uint8)
    return img

def main(a):
    if len(a) >= 4 and a[0] == 'extract':
        o, rest = options(a[1:], ('--player', '--base', '--outfit'))
        player = int(o.get('--player', 1)); outfit = {k: int(v) for k, v in (x.split('=') for x in o['--outfit'].split(','))} if '--outfit' in o else {}
        bad = set(outfit) - set(PARTS[player])
        if bad: sys.exit('unknown parts: ' + ', '.join(bad))
        extract(rest[0], rest[1], rest[2], player, int(o.get('--base', BASES[player])), outfit)
    elif len(a) >= 3 and a[0] == 'build':
        o, rest = options(a[1:], ('--hair-colour', '--cap-scale', '--hair-drop', '--shirt'))
        if o.get('--shirt', 'plain') not in ('plain', 'stripes'): sys.exit('--shirt plain|stripes')
        build(rest[0], rest[1], [float(x) for x in o.get('--hair-colour', '58,46,40').split(',')],
              float(o.get('--cap-scale', 1.1)), float(o.get('--hair-drop', 0.5)), o.get('--shirt') == 'stripes')
    else:
        sys.exit(open(__file__).read().split('\nimport ')[0])

if __name__ == '__main__':
    main(sys.argv[1:])
