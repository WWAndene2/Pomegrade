# BNTX (Switch texture container): texture info (BRTI at its pointer table, fields read on Scarlet's terrain arrays),
# Tegra X1 block-linear deswizzle, and mip 0 of a layer as an image (BC1-BC7 through Pillow's DDS reader, RGBA kept).
import struct
class Tex: pass
def parse(b):
    assert b[:4] == b'BNTX'
    nx = 0x20
    count = struct.unpack_from('<I', b, nx + 4)[0]
    info_arr, data_blk = struct.unpack_from('<QQ', b, nx + 8)
    texs = []
    for i in range(count):
        o = struct.unpack_from('<Q', b, info_arr + 8 * i)[0]
        assert b[o:o + 4] == b'BRTI', b[o:o + 4]
        t = Tex()
        t.flags, t.dim, t.tile, t.swizzle, t.mips = struct.unpack_from('<BBHHH', b, o + 0x10)
        t.fmt = struct.unpack_from('<I', b, o + 0x1C)[0]
        t.w, t.h, t.d, t.layers, t.bh_log2 = struct.unpack_from('<iiiII', b, o + 0x24)
        t.size, t.align = struct.unpack_from('<II', b, o + 0x50)
        name_ptr, parent, mip_ptrs = struct.unpack_from("<QQQ", b, o + 0x60)
        l = struct.unpack_from('<H', b, name_ptr)[0]; t.name = b[name_ptr + 2:name_ptr + 2 + l].decode()
        t.mip_off = list(struct.unpack_from('<%dQ' % t.mips, b, mip_ptrs))
        t.data = b
        texs.append(t)
    return texs
BLOCK = {0x1a: (4, 4, 8), 0x1b: (4, 4, 16), 0x1c: (4, 4, 16), 0x1d: (4, 4, 8), 0x1e: (4, 4, 16), 0x1f: (4, 4, 16),
         0x20: (4, 4, 16), 0x2d: (4, 4, 16), 0x2e: (5, 4, 16), 0x2f: (5, 5, 16), 0x30: (6, 5, 16), 0x31: (6, 6, 16),
         0x32: (8, 5, 16), 0x33: (8, 6, 16), 0x34: (8, 8, 16), 0x0b: (1, 1, 4), 0x02: (1, 1, 1), 0x07: (1, 1, 2), 0x09: (1, 1, 2)}
def div(a, b): return (a + b - 1) // b
def deswizzle(src, width_blocks, height_blocks, bpp, block_height):
    # Tegra block-linear: GOBs of 64 bytes x 8 rows, stacked block_height GOBs tall
    import numpy as np
    pitch = width_blocks * bpp
    gobs_x = div(pitch, 64); gobs_y = div(height_blocks, 8 * block_height)
    out = bytearray(pitch * height_blocks)
    sv = memoryview(src)
    for y in range(height_blocks):
        for x in range(0, pitch, 16):
            gob_addr = ((y // (8 * block_height)) * gobs_x * block_height * 512 + (x // 64) * block_height * 512
                        + ((y % (8 * block_height)) // 8) * 512)
            xi, yi = x % 64, y % 8
            off = gob_addr + ((xi % 64) // 32) * 256 + ((yi % 8) // 2) * 64 + ((xi % 32) // 16) * 32 + (yi % 2) * 16 + (xi % 16)
            n = min(16, pitch - x)
            if off + n <= len(src):
                out[y * pitch + x:y * pitch + x + n] = sv[off:off + n]
    return bytes(out)

def to_image(t, layer=0):
    """Mip 0 of one layer as a PIL image (BC1-BC7 via Pillow's DDS reader)."""
    import io
    from PIL import Image
    bw, bh, bpp = BLOCK[t.fmt >> 8]
    wb, hb = div(t.w, bw), div(t.h, bh)
    base = t.mip_off[0] + layer * (t.size // max(t.layers, 1))
    gob_h = 1 << t.bh_log2
    while gob_h > 1 and hb <= (gob_h // 2) * 8: gob_h //= 2
    raw = deswizzle(t.data[base:base + t.size], wb, hb, bpp, gob_h)
    dxgi = {0x1a: 71, 0x1b: 74, 0x1c: 77, 0x1d: 80, 0x1e: 83, 0x20: 98}[t.fmt >> 8] + (1 if (t.fmt & 0xff) == 6 else 0)
    hdr = struct.pack('<4sIIIIIII44sIIIIIIIIIIIII', b'DDS ', 124, 0x1007 | 0x80000, t.h, t.w, len(raw), 0, 1, b'\0' * 44,
                      32, 4, 0x30315844, 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0) + struct.pack('<IIIII', dxgi, 3, 0, 1, 0)
    return Image.open(io.BytesIO(hdr + raw)).convert('RGBA')
