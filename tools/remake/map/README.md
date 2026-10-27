# Hoenn map (ORAS)

`hoenn_map.png`: a Sword / Shield style map of Hoenn drawn from Omega Ruby's own data (owner's request, 27 October): the
roads as the tiles lay them, the places where the overworld enters them, the land outline of the game's town map.
No game data is in this folder; the scripts read it from a work directory made from your own dump.

## What is data and what is not

- **Read from the game:** each map matrix's place (from the edge crossings, warp kinds 0x500-0x503), the roads (a
  path traced over the terrain each piece's model shows: paths first, grass, never cliffs, forest or water), the
  places' positions (their doors and warps), 22 landmarks (where an outdoor zone warps into them).
- **Chosen:** ORAS's world does not close into one grid (some loops miss by up to 85 tiles); each gap is put
  between two sea routes, in open water, with Slateport - Route 109 and Pacifidlog - Route 131 kept exact. Which
  place each line joins follows Hoenn's route order (routes in one matrix have no warp between them).
- **From the town map (DexNav screen, `ref/dexnav.jpg`, not committed):** the land outline, bent (thin-plate
  spline) so each city's square lands on the city; approximate: Safari Zone, Shoal Cave, Seafloor Cavern, Cave of
  Origin; Sootopolis, Pokémon League, Southern Island, Battle Resort placed by hand. Victory Road's stop is halfway
  between Ever Grande and the League.
- **Missing:** Route 125's number, Weather Institute, Trick House, Sealed Chamber, Nameless Cavern, the Mirage spots.

## Run

From a work directory holding `map/names.txt` (a/0/7/3 member 90, one `line<TAB>text` per name), `wat/oz/` (ORAS
zones), `map/a_0_4_0/` (matrices), `map/a_0_3_9/` (pieces) and `ref/dexnav.jpg`, with `REMAKE_TOOL` naming
`remake_tool`, in order:

```
python3 hoenn_terrain.py    # map/kinds.npy: the terrain kind of each tile of the assembled world
python3 hoenn_roads.py      # map/paths.pkl: the traced roads and the places' positions
python3 hoenn_landmarks.py  # map/landmarks*.pkl: where the overworld enters each landmark
python3 hoenn_draw.py       # hoenn_map.png
```

`hoenn_place.py` (matrix placement), `hoenn_assemble.py` (the tile render, `hoenn_assembled.png`) and
`hoenn_lines.py` (the lines' place order) are read by the others.

## Hoenn in X/Y's town map style (28 October)

`hoenn_xy_map.png`, the owner's approved version: X/Y's town map look (flat relief terraces with outlines, white routes
with a dark edge, grey stops, orange places, Mauville's Poké Ball as the capital, sea routes as dots with one shared dot
at each junction), drawn whole (the DexNav screen's cut edges widened and rounded off).

- **From the game:** the relief (the pieces' 3D terrain, `hoenn_height.py` with `piece_height.cpp`), the roads' course
  (`map/paths.pkl`, warped onto the map through the towns and places; the warp misses by up to 34 px, so each route was
  then redrawn by hand along it, straight and 45° legs).
- **From the DexNav screen:** the land outline and the towns' and places' squares.
- **Chosen, approximate:** the hand-drawn bends, the sea routes of the 124-128 block, the 10 places the DexNav omits
  (Weather Institute, Desert Ruins, Scorched Slab, Ancient Tomb, Trick House, Island Cave, Sealed Chamber, Secret
  Islet, Meadow, Shore), the land past the screen's edges. Not drawn: the Mirage spots.

Run, after the four steps above: build `piece_height` (`g++ -std=c++17 -O2 -I../src piece_height.cpp
<build>/libremake.a -lz`), then `REMAKE_PIECE_HEIGHT=<it> python3 hoenn_height.py` and `python3 hoenn_xy_draw.py`
(with `ref/dexnav.jpg` and X's decoded `xtm/png/` in the work directory: `darc_png.cpp`, the same build, decodes a DARC's
BCLIM images to PNG: `darc_png <file> <offset of "darc"> <out dir>`).

The two games' own town maps, read 28 October (static): Omega Ruby's `a/0/6/7` member 0 (DARC at 0x5900: `map_lower.bclyt`,
`poke_navi_map.bclim` 380 × 270 ETC1, cursor, player icons, city icons; module DllTownmap.cro, classes NaviLayout,
NaviUtil, SecretBaseView, NutsView...), X's `a/0/6/9` member 0 (DARC at 0x3B00: `townmap_upper/lower.bclyt`, weather
animations, `townmap_bg1.bclim` 320 × 210 the Kalos map, a 160 × 100 photo per place `image_c01`...; its DllTownmap.cro
holds TownmapData). Different code and files: X's is to be ported beside Omega Ruby's (both maps in the menu); not done.
Not read yet: the zone -> map position / Fly point table of each, how the menu opens the module.

## Kalos in the same style (28 October)

`kalos_xy_map.png` (`kalos_xy_draw.py`), drawn to match `hoenn_xy_map.png` (same palette, terraces, icons, route
strokes, Lumiose's Poké Ball as the capital). From X's own town map image (`townmap_bg1`, a/0/6/9 member 0): the land,
the relief (its colours, greener low, browner high, in the six terraces), the 16 stops, 14 places and 27 routes, read off
the image square by square (their positions X's own, each route joining the icons X joins). Approximate or invented:
the coast past the image's cut top, right and bottom edges (X's image shows only part of Kalos; drawn irregular, from
nothing in the game), one size for every stop (X draws cities and towns alike), the routes made straight and 45° legs.
Tried and set aside (28 October): Kalos assembled from X's tiles; its routes join towns through gate rooms (no world
position), Lumiose's two rings (matrices 128, 129) fit nothing, X's material names are not TopView's; several hours.
