#!/usr/bin/env python3
"""A town map drawing made the fly screen's background (owner, 28 October: the maps in the X/Y style, hoenn_xy_map.png and
kalos_xy_map.png, as the fly screen's backgrounds; test maps, the simplest way): the drawing scaled into the screen's map
area and laid on a 380 x 270 image filled with its sea, as raw RGBA rows (oras-engine --fly-map-hoenn reads it).

  fly_background.py <drawing.png> <out.rgba>

Omega Ruby's fly screen (a/0/6/7 member 0, its layout map_lower.bclyt, read 28 October): the background picture "0" is
380 x 270 (poke_navi_map.bclim) at (0, 15), and the places' pane "s", 320 x 210, at the same centre; the drawing is scaled
to fit those 320 x 210, its proportions kept, and centred. The sea is the drawing's top-left pixel.
"""
import sys

from PIL import Image

WIDTH, HEIGHT, AREA_W, AREA_H = 380, 270, 320, 210


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    drawing = Image.open(sys.argv[1]).convert('RGBA')
    sea = drawing.getpixel((0, 0))
    scale = min(AREA_W / drawing.width, AREA_H / drawing.height)
    size = (round(drawing.width * scale), round(drawing.height * scale))
    canvas = Image.new('RGBA', (WIDTH, HEIGHT), sea)
    canvas.paste(drawing.resize(size, Image.LANCZOS), ((WIDTH - size[0]) // 2, (HEIGHT - size[1]) // 2))
    open(sys.argv[2], 'wb').write(canvas.tobytes())
    print(f'{sys.argv[2]}: {WIDTH} x {HEIGHT} RGBA, the drawing at {size[0]} x {size[1]}')


if __name__ == '__main__':
    main()
