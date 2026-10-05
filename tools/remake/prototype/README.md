# Remake prototypes: Sinnoh test mod (Twinleaf over Littleroot)

These are working prototypes, not finished tools. They built the Twinleaf test mods (tests 1-4). None of them contains game data. You supply it from your own dumps.

## Build

```
cmake -S tools/remake -B build-remake -G Ninja -DREMAKE_PROTOTYPES=ON && ninja -C build-remake
```

Python needs numpy and Pillow. The renders need Node with Playwright (`PLAYWRIGHT=<path to playwright module>` if it is not installed globally).

## Work directory

Every tool runs from a work directory (`REMAKE_WORK` for the Python and JS tools, the current directory for the C++ ones) laid out like this:

| Path | What it is | How to get it |
|---|---|---|
| `oras/a039.garc`, `oras/a013.garc` | ORAS map pieces (GR) and zones | `remake_tool oras-extract <oras.3ds> a/0/3/9 oras/a039.garc` (same for a/0/1/3) |
| `t0/gr5.bin`, `t0/gr6.bin`, `t0/gr8.bin` | Route 101, Littleroot and Petalburg pieces, decompressed | `remake_tool garc oras/a039.garc t0/`, then rename |
| `pref/ref.gltf` | Platinum reference terrain (Twinleaf and Route 201 cells) | `remake_tool oras-world <platinum.nds> <matrix index> pref/` (Sinnoh overworld matrix; Twinleaf is cells rows 25-27, cols 2-4) |
| `slice/` | Grids, colour maps and outputs | Written by the tools below |

## Pipeline, in order

1. `python3 tile_materials.py [R] [gltf] [out] [ox oz]` lists the Platinum textures covering each tile (or each 1/R tile) → `slice/cover*.json`.
2. `python3 texture_sample.py <gltf> <R> <out> <ox> <oz> <gc0> <gr0>` records the colour each half tile really shows from above → `slice/<out>_colours.npy/.png`. Example for Platinum's Twinleaf: `ref.gltf 2 plat_tw 0 0 92 856`.
3. `python3 classify_terrain.py` turns each tile into a role (house, water, fence, flower, path, forest, tree...) → `slice/twinleaf_vis.txt`, `slice/*_path2.txt` and a control map. It stops on an unknown texture: give that texture a role in `ROLE`.
4. `./TwinleafBuild` builds the Twinleaf piece from the Petalburg donor, the Route 101 trees, the scaled houses, door models and tree levels of detail → `slice/gr6_twinleaf3.bin`.
5. `./ZoneWarps` moves zone 6's warps onto Twinleaf's doors and sets area pack 9 → `slice/a013_twinleaf.garc`.
6. `./SliceGarc` puts pieces 5 and 6 into a/0/3/9 → `slice/a039_slice.garc`.
7. `./BpsMake <original> <modified> <out.bps>` makes the patch. Install the patches as `load/mods/000400000011C400/romfs_ext/a/0/3/9.bps` and `.../a/0/1/3.bps`.
8. Checks:
   - `./PiecePreview` exports pieces to glTF, which you open in `../editor/world_editor.html`.
   - `node render_compare.js <gltf> <prefix> <ox> <oz>` and `node render_closeups.js <gltf> <prefix> <ox> <oz> name:col:row...` render Platinum and the rebuild side by side.
   - `python3 scene_diff.py <name> <gc0> <gr0>` maps where the paths and water differ.

## Check against Azahar's own code (`azahar_check/`)

These link Azahar's real `layered_fs.cpp` and `patch.cpp` so a mod can be checked without the phone:

```
A=azahar; g++ -std=c++20 -O1 -Itools/remake/prototype/azahar_check/stub -I$A/src -I$A/externals/fmt/include \
  -I$A/externals/boost -I$A/externals/cryptopp/include -Itools/remake/src -DFMT_HEADER_ONLY \
  tools/remake/prototype/azahar_check/layered_fs_check.cpp $A/src/core/file_sys/layered_fs.cpp \
  $A/src/core/file_sys/patch.cpp $A/src/common/file_util.cpp $A/src/common/string_util.cpp \
  build-remake/libremake.a -Wl,--unresolved-symbols=ignore-all -o lfscheck
```

`bps_check.cpp` builds the same way, with `patch.cpp` only. Usage: `bps_check <original> <patch.bps> <expected>`.

`layered_fs_check` builds against the repository sources. Its run on the test 4 mod has not been rechecked since it moved here: its earlier version, with a copy of `file_util.cpp`, passed.

## Limits

- Paths and the Twinleaf/Petalburg choices are hard-coded in the C++ prototypes.
- No texture can be added to an area pack yet: that needs BCH dictionary editing, which also blocks snow (`c109_pur_yaneyuki`) and the white fence.
