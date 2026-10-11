#!/usr/bin/env python3
"""The ride viewer (point 39 of Delta_Emerald_Development.md): Giratina flown by the player, one self-contained HTML
file built from ride_viewer.html and the game's data. The output holds the game's models, textures and animations: it
stays in scratch space, never committed or published (CLAUDE.md section 8).

  build_ride_viewer.py <data dir> <out.html>

<data dir> holds, all made with tools/remake/spica (`spica`) from the game's files extracted with remake_tool
(`oras-extract` a/0/0/8 and a/2/5/4, then `garc`):
  gbind.dae, gbind.pose.json          Giratina Origin Forme: `spica dae <a/0/0/8 5435> gbind.dae` and `spica pose <5435>`
  gb0..9.pose.json                    its 10 battle motions: `spica pose <5440> gbK.pose.json --anim K --skeleton <5435>`
  gk0..6.pose.json                    its 7 Poke Amie motions: the same on <5441>
  pbind.dae, pbind.pose.json          the player riding (a/2/5/4 member 3, TM file 15): `spica dae` / `spica pose`
  p00..14.pose.json                   the player's 15 ride motions (member 3, TM files 0-14), `--anim 0 --skeleton <file 15>`
  *.png                               textures: `spica png` on 5437's file 0 (a BCH) and on the player's file 15
(5435 is the Origin Forme's PC pack, 5437 its PT pack, 5440 its PB pack, 5441 its PK pack: members of a/0/0/8 found by
the names pm0487_12 inside them, 11 October.) three.js is bundled from ride_viewer_three.js with esbuild (npx, the
render/ node_modules), so the page needs no network.
"""
import base64
import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLAYER = ["recul (pmback01)", "freinage (pmbrake01)", "descente (pmdown01)", "looping (pmflip01)",
          "virage int. (pminside01)", "refus (pmiyaiya01)", "virage ext. (pmoutside01)", "virage gauche (pmturnL01)",
          "virage gauche 2 (pmturnL02)", "virage droite (pmturnR01)", "montée (pmup01)", "attente (pmwait01)",
          "attente 2 (pmwait02)", "marche (pmwalk01)", "accélération (pmspeedup01)"]


def poses(path):
    return [{k: [round(x, 4) for x in v] for k, v in frame.items()} for frame in json.load(open(path))]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data, out = sys.argv[1:]
    bundle = os.path.join(data, "three_bundle.js")
    subprocess.run(["npx", "esbuild", os.path.join(HERE, "ride_viewer_three.js"), "--bundle", "--minify", "--format=iife",
                    "--outfile=" + bundle], cwd=HERE, check=True)
    p = lambda name: os.path.join(data, name)
    # the order of the lists is the page's: its defaults name Combat 0 (index 1) and the player's wait (index 11)
    mount = [["Pose de repos", poses(p("gbind.pose.json"))]]
    mount += [[f"Combat {k}", poses(p(f"gb{k}.pose.json"))] for k in range(10)]
    mount += [[f"Poké Récré {k}", poses(p(f"gk{k}.pose.json"))] for k in range(7)]
    player = [[PLAYER[k], poses(p(f"p{k:02d}.pose.json"))] for k in range(15)] + [["Pose de repos", poses(p("pbind.pose.json"))]]
    textures = {os.path.basename(f): "data:image/png;base64," + base64.b64encode(open(f, "rb").read()).decode()
                for f in glob.glob(p("*.png"))}
    blob = json.dumps({"gdae": open(p("gbind.dae")).read(), "pdae": open(p("pbind.dae")).read(), "tex": textures,
                       "mount": mount, "player": player}, separators=(",", ":"))
    page = open(os.path.join(HERE, "ride_viewer.html")).read()
    page = page.replace("/*DATA*/null", "JSON.parse(document.getElementById('data').textContent)")
    page = page.replace("<!--DATAJSON-->", '<script id="data" type="application/json">' + blob.replace("</", "<\\/") + "</script>")
    page = page.replace("<!--THREE-->", "<script>" + open(bundle).read().replace("</script", "<\\/script") + "</script>")
    open(out, "w").write(page)
    print(out, os.path.getsize(out) // 1024, "KB")


if __name__ == "__main__":
    main()
