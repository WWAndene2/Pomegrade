# Scarlet's terrain ground materials: the 60 layers of field_graphic/terrain/splat_tex/global_color_layer_array
# (BC7, 256x256) and global_normal_layer_array (RGBA8), each written as <out>/<index>.png and <index>_n.png, plus a
# contact sheet. tools/terrain's renderer names the ones it uses (tools/terrain/README.md).
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import pack, bntx
from PIL import Image

def main(out):
    os.makedirs(out, exist_ok=True)
    p = pack.Packs()
    color = bntx.parse(p.files('arc/field_graphicterrain_splat_texglobal_color_layer_arrayglobal_color_layer_array.bntx.trpak')[0][1])[0]
    normal = bntx.parse(p.files('arc/field_graphicterrain_splat_texglobal_normal_layer_arrayglobal_normal_layer_array.bntx.trpak')[0][1])[0]
    gob = 1 << normal.bh_log2
    while gob > 1 and normal.h <= (gob // 2) * 8: gob //= 2
    stride = normal.size // normal.layers
    sheet = Image.new('RGB', (10 * 128, 6 * 128))
    for i in range(color.layers):
        im = bntx.to_image(color, i); im.save(f'{out}/{i}.png')
        base = normal.mip_off[0] + i * stride
        raw = bntx.deswizzle(normal.data[base:base + stride], normal.w, normal.h, 4, gob)
        Image.frombytes('RGBA', (normal.w, normal.h), raw).convert('RGB').save(f'{out}/{i}_n.png')
        sheet.paste(im.convert('RGB').resize((128, 128)), ((i % 10) * 128, (i // 10) * 128))
    sheet.save(f'{out}/sheet.png')

if __name__ == '__main__':
    main(sys.argv[1])
