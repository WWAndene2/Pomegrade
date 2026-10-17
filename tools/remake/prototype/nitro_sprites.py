# nitro_sprites.py: DS 2D sprites as PNG: NCGR tiles with an NCLR palette, and Platinum's encrypted trainer sprites.
#
#   python3 -I nitro_sprites.py ncgr <file.ncgr> <file.nclr> <out.png> [--decrypt pt|dp] [--zoom N]
#   python3 -I nitro_sprites.py trainer <dir> <index> <out.png> [--zoom N]
#       <dir>: a trainer sprite archive unpacked (`remake_tool unpack <platinum.nds> poketool/trgra/trfgra.narc <dir>`,
#       trbgra.narc for the back sprites); trainer <index> is members 5*index to 5*index+4. Its image is member
#       5*index+4 (a linear NCGR, encrypted) with the palette of member 5*index+1. Members 5*index and +2/+3 (1D tiles,
#       NCER cells, NANR animation) are the same picture cut into OAM cells, not assembled here.
#
# The encryption (checked on Platinum, 17 October: trainers 0-2 front and back come out clean with "pt", noise with
# "dp"): the image data, as 16-bit words, XORed with the low half of a linear congruential sequence (x * 0x41C64E6D +
# 0x6073), seeded with the first word and run forward in Platinum, seeded with the last word and run backward in
# Diamond/Pearl (the community's description; only the Platinum direction was checked here). Trainer 0 is Lucas.
import sys, struct
import numpy as np
from PIL import Image


def nclr(path):  # the palette, as RGB rows (BGR555 colours)
    d = open(path, 'rb').read()
    i = d.index(b'TTLP')
    _, _, size, off = struct.unpack_from('<IIII', d, i + 8)
    c = np.frombuffer(d[i + 8 + off: i + 8 + off + size], '<u2')
    return np.stack([(c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3], -1).astype(np.uint8)


def decrypt(words, mode):
    w = words.astype(np.uint32)
    out = np.zeros_like(w)
    order = range(len(w)) if mode == 'pt' else range(len(w) - 1, -1, -1)
    seed = int(w[0] if mode == 'pt' else w[-1])
    for i in order:
        out[i] = w[i] ^ (seed & 0xFFFF)
        seed = (seed * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
    return out.astype(np.uint16)


def ncgr(path, palette, mode=None):  # RGBA rows; colour 0 transparent
    d = open(path, 'rb').read()
    i = d.index(b'RAHC')
    th, tw, depth = struct.unpack_from('<HHI', d, i + 8)
    _, linear, size, off = struct.unpack_from('<IIII', d, i + 16)
    raw = d[i + 8 + off: i + 8 + off + size]
    if mode:
        raw = decrypt(np.frombuffer(raw, '<u2'), mode).tobytes()
    data = np.frombuffer(raw, np.uint8)
    px = np.stack([data & 15, data >> 4], -1).reshape(-1) if depth == 3 else data
    if linear == 1:  # scanned rows
        w = tw * 8
        img = px[: w * (len(px) // w)].reshape(-1, w)
    else:  # 8x8 tiles, row by row
        img = px[: th * tw * 64].reshape(th, tw, 8, 8).transpose(0, 2, 1, 3).reshape(th * 8, tw * 8)
    rgba = np.zeros(img.shape + (4,), np.uint8)
    rgba[..., :3] = palette[np.clip(img, 0, len(palette) - 1)]
    rgba[..., 3] = np.where(img == 0, 0, 255)
    return rgba


def save(rgba, out, zoom):
    im = Image.fromarray(rgba)
    im.resize((im.width * zoom, im.height * zoom), Image.NEAREST).save(out)


def main(a):
    zoom = int(a[a.index('--zoom') + 1]) if '--zoom' in a else 1
    if len(a) >= 4 and a[0] == 'ncgr':
        mode = a[a.index('--decrypt') + 1] if '--decrypt' in a else None
        save(ncgr(a[1], nclr(a[2]), mode), a[3], zoom)
    elif len(a) >= 4 and a[0] == 'trainer':
        k = 5 * int(a[2])
        save(ncgr(f'{a[1]}/{k + 4}.ncgr', nclr(f'{a[1]}/{k + 1}.nclr'), 'pt'), a[3], zoom)
    else:
        sys.exit(open(__file__).read().split('\nimport ')[0])


if __name__ == '__main__':
    main(sys.argv[1:])
