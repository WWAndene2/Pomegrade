#!/usr/bin/env python3
"""A run's shots on one contact sheet, in the order taken, each labelled with its name (23 October: every test rebuilt
this by hand). Read the sheet instead of each shot.

  shot_sheet.py <run dir> <out.png> [--columns N] [--width PIXELS] [--only PREFIX]

Shots are the run's .png files except the sheet itself, ordered by the frame the log names ("shot runs/x/s3.ppm at
frame 812"), else by name. --width is each tile's width (default 160; the two screens keep their proportions).
"""
import argparse
import os
import re

from PIL import Image, ImageDraw


def ordered(run, only):
    frames = {}
    log = os.path.join(run, "log.txt")
    if os.path.exists(log):
        for line in open(log, errors="replace"):
            m = re.match(r"shot \S*?([^/\s]+)\.ppm at frame (\d+)", line)
            if m:
                frames[m.group(1)] = int(m.group(2))
    names = [f[:-4] for f in os.listdir(run) if f.endswith(".png") and (not only or f.startswith(only))]
    return sorted(names, key=lambda n: (frames.get(n, 1 << 40), n)), frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("out")
    ap.add_argument("--columns", type=int, default=10)
    ap.add_argument("--width", type=int, default=160)
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    names, frames = ordered(a.run, a.only)
    names = [n for n in names if os.path.abspath(os.path.join(a.run, n + ".png")) != os.path.abspath(a.out)]
    if not names:
        raise SystemExit("no shots in " + a.run)
    first = Image.open(os.path.join(a.run, names[0] + ".png"))
    w, h = a.width, a.width * first.height // first.width
    cols = min(a.columns, len(names))
    sheet = Image.new("RGB", (w * cols, (h + 12) * ((len(names) + cols - 1) // cols)), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    for k, n in enumerate(names):
        x, y = (k % cols) * w, (k // cols) * (h + 12)
        sheet.paste(Image.open(os.path.join(a.run, n + ".png")).convert("RGB").resize((w, h)), (x, y + 12))
        draw.text((x + 2, y), n + (f" f{frames[n]}" if n in frames else ""), fill=(255, 255, 255))
    sheet.save(a.out)
    print(f"{a.out}: {len(names)} shots")


if __name__ == "__main__":
    main()
