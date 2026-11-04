# Terrain generation for a Pokemon open world

Owner's request (3-4 November 2026): generate new zones, starting with a realistic mountain range in
Pokemon's style, using the games' existing assets or real-world logic; then whether this could scale to a
world map of every region. The tools are in `tools/terrain/` (how to run them: its README) and
`tools/switch/scarlet_assets/` (reading Scarlet's assets).

## 1. What the owner judged, step by step

Each approach below was rendered and shown to the owner; their verdicts drove the next step.

| Step | Approach | Owner's verdict / what was seen |
|---|---|---|
| 1 | Ridged noise, droplet erosion, flat colours, my own geometric trees | Shape "60% good", render "looks like an AI mock-up" |
| 2 | Same terrain, Scarlet's ground textures | Better; erosion patterns not realistic |
| 3 | Millions of small drops (3 M) on a 1024 grid | Parallel brush strokes, not a branching network; erased the network below (dropped) |
| 4 | Stream-power erosion (E = K A^m S, Braun & Willett 2013) with uplift | Branching drainage appears; still "clearly generated: everything is vertical subtraction, no geology, no volume" |
| 5 | Geology: tectonic uplift (folded belt, finite faults, plateau), 3D strata with hardness, erosion by the exposed rock | Valley-and-ridge pattern seen from above; in 3D, monoliths and comb-like gullies: "does it look like a mountain to you?" No (dropped) |
| 6 | Example-based synthesis from real terrain (owner's choice: patterns of real mountains, not a 1:1 copy) | "A lot better"; still smoothed look, flat snow, no grass, my trees |
| 7 | Scarlet's firs and grass, physical snow, a ground-level view | Ground too smooth up close (30 m data), grass edge, too much rock |
| 8 | swissALTI3D 2 m detail window, grass falloff, LOD forests, rock above ~40 degrees | "Flat" at ground level |
| 9 | Baked shadows and occlusion, x2 vertical exaggeration, low sun | "Far much better", still poor in horizontality and composition |

What made the difference, in order: real terrain as the source of patterns (step 6), the game's own
assets (steps 2, 7) and baked shadows (step 9). Simulation from noise (steps 1-5) never looked real.

## 2. The pipeline (step 9)

1. **Range, 30 km at 30 m** (`synth.py`). A layout of crests drawn by hand (large shapes only) is
   filled with 192-cell patches of Copernicus DEM detail (terrain minus its 48-cell blur) from Valais,
   the Bernese Oberland and the Dolomites, rotated or mirrored; each patch is chosen among 900 for its
   fit with the placed neighbours and for slopes running the layout's way, and joined along a
   minimum-error cut (image quilting, Efros and Freeman 2001). Lake patches (flat water in the DEM) are
   never used. Mistakes along the way: a layout noise 20 times too strong (the crest disappeared),
   blocks shifted as wholes (visible squares).
2. **Scene data** (`scene_data.py`): rivers from drainage area, a material slope smoothed over 3 cells
   (grass covers small gullies), forest and meadow masks, and snow: an altitude line moved by sun
   exposure, curvature (crests scoured, couloirs filled) and drift, sliding off faces steeper than
   ~48 degrees.
3. **2 m window** (`fine.py`): around the ground view, 2 x 2 km of the range upsampled plus swissALTI3D
   detail (Grindelwald) under ~60 m, in 96-cell patches turned so their gullies run downhill; the
   detail fades to zero at the border, where the coarse terrain (sunk under the window) meets it.
   Valley floors are excluded from the sources (village, roads); hiking trails remain in the data.
4. **Baked light** (`bake.py`): sun shadows by marching toward a sun 25 degrees high, ambient occlusion
   from horizon angles in 16 directions.
5. **Render** (`render/`): Scarlet's ground layers (grass 0, meadow 5, forest floor 12, sand 16, snow 29,
   rock 40 and 42 of its 60-layer array), triplanar on rock, Scarlet's normal maps; Scarlet's firs
   (`plants_me_tree_fir02-04`, LOD0 within ~450 m, LOD2 beyond, snow-covered near the snow) and grass
   tufts (`plants_me_main_grass01/11`, `level_grass01`) with density falling over ~300 m.

Known limits now: the horizon and composition (owner), the background mountains are the 30 m terrain,
the sky is flat, forests read as scattered trees from the ground, SwiftShader renders take ~10 minutes.

## 3. Scale: the owner's world map (fan-made map of every region, 4 November)

On that map Paldea is ~165 x 155 px of 1646 x 1280: the map is about 10 Paldeas wide, 8 tall, about 30
to 35 Paldeas of land. Scarlet's data is about 7 GB (measured in the owner's XCI: arc/data.trpfs
5.4 GB, audio ~1 GB, movies). Estimates, not builds:

| World | Estimate |
|---|---|
| Generated terrain and rule-placed vegetation at 1-2 m | ~10 GB |
| + towns and key places made by hand | ~30-60 GB |
| As dense as Paldea everywhere (Scarlet x 35) | ~150-250 GB |

Paldea's real size was not measured (its terrain tiles `field_graphic/terrain` would give it).
Detail must be generated per region or per tile on demand, as streamed open worlds (Scarlet included) do.

## 4. Towns from the DS games (HeartGold)

`tools/remake`'s `world` command reads HeartGold's world since commit 850a02c8 (map pieces with
HGSS's extra header section, cell size from the pieces' median width): matrix 0 (`a/0/4/1`), map pieces
`a/0/6/5`, building models `a/0/4/0`, map texture sets `a/0/4/4`. Ecruteak City (Rosalia) is header 78,
cells x 11-12, y 4-5, map textures set 6 (found by matching texture names); rendered as the reference
for a remake in Scarlet's style. The Bell Tower is a separate map. Next step to decide with the owner:
modernise HeartGold's buildings or replace them by Scarlet's closest models.
