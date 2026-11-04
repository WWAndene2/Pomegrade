# Terrain generator

New mountain terrain for a Pokemon open world, built from the patterns of real terrain (never a copy of
a real place) and painted with Pokemon Scarlet's own ground materials, firs and grass. The design,
what was tried before it and why it was dropped are in `docs/Terrain_Generation.md`.

| File | What it does |
|---|---|
| `fetch_dem.py` | Downloads the real elevation data: Copernicus DEM tiles (30 m) and swissALTI3D tiles (2 m, joined into `mosaic.npy`) |
| `synth.py` | A new 30 km range: a hand-drawn layout (crests, spur) filled with ~6 km patches of the Copernicus terrain, chosen to fit the layout's slopes and their neighbours, joined along minimum-error seams |
| `scene_data.py` | The scene's per-cell data (`data.js`): height, rivers, material slope, forest and meadow masks, snow from altitude, sun, wind and slope, the ground-level view |
| `fine.py` | A 2 x 2 km window of 2 m relief around the ground view (`fine.js`): the range upsampled, plus swissALTI3D walker-scale detail turned so its gullies run downhill |
| `bake.py` | Baked light: sun shadows and ambient occlusion of the range and of the 2 m window |
| `render/scene.html`, `render/trmodel.js` | The three.js renderer: Scarlet's ground layers blended by slope, altitude and masks, Scarlet's firs (LOD0 near, LOD2 far) and grass placed by rule |
| `render/textures.py` | Copies the Scarlet ground layers the scene uses into `<scene>/tex/` |
| `render/capture.js` | Renders the scene's views in headless Chromium and saves them as `<scene>/view<N>.png` |

## Running it

Python 3 with numpy, scipy, numba, pillow, tifffile (+ imagecodecs), pycryptodome; Node with Playwright
and Chromium (preinstalled in the cloud sessions). Scarlet's assets come from the owner's dump through
`tools/switch/scarlet_assets/` (README there); the dump, keys and every file extracted from them stay
outside the repository (CLAUDE.md §7), as does every scene folder made below.

```sh
T=tools/terrain; A=tools/switch/scarlet_assets; W=/path/to/work        # W: outside the repository
python3 $T/fetch_dem.py copernicus $W/dem
python3 $T/fetch_dem.py swiss $W/swiss                                   # Grindelwald, 10 x 10 km
python3 $T/synth.py $W/dem $W/synth
python3 $T/scene_data.py $W/synth/synth_h.npy $W/scene
python3 $T/fine.py $W/swiss/mosaic.npy $W/scene
python3 $T/bake.py $W/scene
export SCARLET_DUMP=/path/to/dump OOZ_LIB=$W/ooz/libooz.so              # scarlet.xci + prod.keys
sh tools/switch/ooz_linux/build_libooz.sh $W/ooz
python3 $A/ground_layers.py $W/layers
python3 $T/render/textures.py $W/layers $W/scene
python3 $A/model.py $W/scene/models plants_me_tree_fir02 plants_me_tree_fir03 plants_me_tree_fir04 \
  plants_me_tree_fir03_snow plants_me_tree_fir02:plants_me_tree_fir02_lod2 \
  plants_me_tree_fir03:plants_me_tree_fir03_lod2 plants_me_tree_fir04:plants_me_tree_fir04_lod2 \
  plants_me_main_grass01 plants_me_main_grass11 plants_me_level_grass01
node $T/render/capture.js $W/scene 0 3                                   # overview, ground level
```

Checked on 4 November: run from these files into empty folders, the steps reproduce the session's
figures (snow cover 0.133, the same ground view, shadow and occlusion means) and view 0 is identical
to the session's render pixel for pixel. A view takes about 10 minutes with SwiftShader (no GPU).

Credits for images made from this data: "produced using Copernicus WorldDEM-30 (c) DLR e.V.
2010-2014 and (c) Airbus Defence and Space GmbH 2014-2018, provided under COPERNICUS by the European
Union and ESA" and "swissALTI3D (c) swisstopo".
