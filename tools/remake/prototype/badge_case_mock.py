#!/usr/bin/env python3
"""A mock-up of Omega Ruby's badge case with a Kalos drawer (31 October, owner's request: a prototype, approximate):
the case's lower screen is Omega Ruby's own layout (a/1/0/9 member 0, tranercase_lower.bclyt) drawn with its textures;
switching drawer plays the lid's animation (tranercase_lower_tcaselid_low.bclan: the lid's scale Y 1 -> -1 over 20
frames, about its top edge) backwards, puts X's badges and outlines in the badge panes at the places X's own layout
(a/1/1/3) gives them, then opens the lid again. The upper screen is not the game's (its panes are shown by animations and
the leaders' drawings are elsewhere): the ball pattern, the chosen badge and its leader.

  badge_case_mock.py <work> <out.html>

<work> laid out by headless/session_setup.sh (dumps/oras.3ds, dumps/x.3ds, build-remake/remake_tool). The page embeds
the games' textures: it is the owner's (CLAUDE.md 8), never committed or published without the owner's word.

Read here and nowhere else in the tools: the case archive's member 0 is a count, then 0x40-byte names, then the files
(found by their own magic and size: CLIM's footer, CLYT / CLAN's header), in the names' order. A CLYT pane (pan1, pic1,
wnd1): flags (bit 0 visible) at +8, origin at +9 (x + 3 y, 0 left / top), alpha at +10, name at +12, translation,
rotation, scale and size from +0x24 (Layout.cpp); a pic1's material at +0x5C; a wnd1's frame count at +0x5C, its
content's offset at +0x60 (the material at the content's +0x10), its frame table's at +0x64 (each frame: material,
flip). A material: name (20 bytes), colour at +0x18, flags at +0x30 (texture maps in bits 0-1), its first texture at
+0x34. Window frames are drawn from their corner texture mirrored at the four corners, its last row and column
stretched along the edges, the content as its texture's centre texel (the corner texels are transparent): an
approximation, not the console's own drawing.
"""
import base64
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

LOWER = 'tranercase_lower.bclyt'
NAMES = {
    'hoenn': [['Badge Roche', 'Roxanne'], ['Badge Poing', 'Bastien'], ['Badge Dynamo', 'Voltère'], ['Badge Chaleur', 'Adriane'],
              ['Badge Balance', 'Norman'], ['Badge Plume', 'Alizée'], ['Badge Esprit', 'Lévy et Tatia'], ['Badge Pluie', 'Marc']],
    'kalos': [['Badge Insecte', 'Violette'], ['Badge Mur', 'Lino'], ['Badge Lutte', 'Cornélia'], ['Badge Plante', 'Amaro'],
              ['Badge Volt', 'Lem'], ['Badge Nymphe', 'Valériane'], ['Badge Psy', 'Astera'], ['Badge Iceberg', 'Urup']],
}


def split_case(member, out):
    """the case archive's files, written under out by name"""
    d = open(member, 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]
    names = [d[4 + i * 0x40:4 + i * 0x40 + 0x40].split(b'\0')[0].decode() for i in range(n)]
    for magic, ext, footer in ((b'CLIM', '.bclim', True), (b'CLYT', '.bclyt', False), (b'CLAN', '.bclan', False)):
        named = [x for x in names if x.endswith(ext)]
        found = [m.start() for m in re.finditer(magic + b'\xff\xfe', d)]
        if len(named) != len(found):
            raise SystemExit(f'{member}: {len(named)} {ext} names, {len(found)} files')
        for name, at in zip(named, found):
            size = struct.unpack_from('<I', d, at + 0xC)[0]
            start = at + 0x28 - size if footer else at
            open(os.path.join(out, name), 'wb').write(d[start:start + size])


def lyt(path):
    d = open(path, 'rb').read()
    u8 = lambda o: d[o]; u16 = lambda o: struct.unpack_from('<H', d, o)[0]; u32 = lambda o: struct.unpack_from('<I', d, o)[0]
    fl = lambda o: struct.unpack_from('<f', d, o)[0]
    name = lambda o, n: d[o:o + n].split(b'\0')[0].decode('latin1')
    o = u16(6); textures, mats, root, stack, last = [], [], None, [], None
    while o < len(d):
        kind, size = d[o:o + 4].decode('latin1'), u32(o + 4)
        if kind == 'txl1':
            n = u16(o + 8); textures = [name(o + 12 + u32(o + 12 + 4 * i), 64) for i in range(n)]
        elif kind == 'mat1':
            n = u16(o + 8)
            for i in range(n):
                a = o + u32(o + 12 + 4 * i); flags = u32(a + 0x30)
                colour = list(d[a + 0x18:a + 0x1C])
                tex = u16(a + 0x34) if flags & 3 else None
                mats.append({'name': name(a, 20), 'tex': textures[tex] if tex is not None else None, 'colour': colour})
        elif kind in ('pan1', 'pic1', 'wnd1', 'bnd1', 'txt1'):
            p = {'kind': kind, 'name': name(o + 12, 16), 'visible': bool(u8(o + 8) & 1), 'alpha': u8(o + 10), 'origin': u8(o + 9),
                 'x': fl(o + 0x24), 'y': fl(o + 0x28), 'rot': fl(o + 0x34), 'sx': fl(o + 0x3C), 'sy': fl(o + 0x40),
                 'w': fl(o + 0x44), 'h': fl(o + 0x48), 'children': []}
            if kind == 'pic1':
                p['mat'] = mats[u16(o + 0x5C)]
                if u16(o + 0x5E):
                    uv = struct.unpack_from('<8f', d, o + 0x60); p['uv'] = uv
            if kind == 'wnd1':
                # 0x4C: 16 bytes (inflation, frame size), 0x5C frame count, 0x60 content's offset, 0x64 the frame table's
                frames = u8(o + 0x5C); c = o + u32(o + 0x60); ft = o + u32(o + 0x64)
                p['content'] = mats[u16(c + 0x10)]
                p['frames'] = [{'mat': mats[u16(o + u32(ft + 4 * i))], 'flip': u8(o + u32(ft + 4 * i) + 2)} for i in range(frames)]
            if stack: stack[-1]['children'].append(p)
            else: root = p
            last = p
        elif kind == 'pas1': stack.append(last)
        elif kind == 'pae1': stack.pop()
        elif kind == 'lyt1': screen = (fl(o + 0xC), fl(o + 0x10))
        o += size
    return {'screen': screen, 'root': root}


def flat(p, acc):
    acc[p['name']] = {k: p[k] for k in ('x', 'y', 'w', 'h', 'sx', 'sy')}
    for c in p['children']:
        flat(c, acc)
    return acc


def main():
    work, out = sys.argv[1], sys.argv[2]
    tool = os.path.join(work, 'build-remake', 'remake_tool')
    run = lambda *a: subprocess.run([tool, *a], check=True, stdout=subprocess.DEVNULL)
    tex, layouts = {}, {}
    with tempfile.TemporaryDirectory() as tmp:
        for game, rom, path in (('o', 'oras.3ds', 'a/1/0/9'), ('x', 'x.3ds', 'a/1/1/3')):
            folder = os.path.join(tmp, game)
            os.makedirs(os.path.join(folder, 'files'))
            run('oras-extract', os.path.join(work, 'dumps', rom), path, os.path.join(folder, 'case.garc'))
            run('garc', os.path.join(folder, 'case.garc'), os.path.join(folder, 'members'))
            member = next(os.path.join(folder, 'members', f) for f in os.listdir(os.path.join(folder, 'members')) if f.startswith('0.'))
            split_case(member, os.path.join(folder, 'files'))
            tex[game] = {}
            for name in sorted(os.listdir(os.path.join(folder, 'files'))):
                if not name.endswith('.bclim'):
                    continue
                png = os.path.join(folder, name + '.png')
                try:
                    run('bclim-png', os.path.join(folder, 'files', name), png)
                except subprocess.CalledProcessError:
                    print(f'{game} {name}: not decoded, left out')
                    continue
                tex[game][name] = 'data:image/png;base64,' + base64.b64encode(open(png, 'rb').read()).decode()
            layouts[game] = lyt(os.path.join(folder, 'files', LOWER))
    lid = next(c for c in layouts['o']['root']['children'] if c['name'] == 'n')
    data = {'tex': tex,
            'layout': {'hoenn': {'root': layouts['o']['root'], 'panes': flat(layouts['o']['root'], {})},
                       'kalos': {'panes': flat(layouts['x']['root'], {})}},
            'lid': {'hoenn': 'tcaselid_low_n.bclim', 'kalos': 'tcaselid_low.bclim'}, 'lidPane': lid, 'names': NAMES}
    page = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'badge_case_mock.html')).read()
    open(out, 'w').write(page.replace('/*DATA*/null', json.dumps(data)))
    print(f'{out}: {len(tex["o"])} + {len(tex["x"])} textures')


if __name__ == '__main__':
    main()
