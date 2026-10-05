# Omega Ruby's Littleroot Town, and how ORAS holds a town

Reference for the Platinum-in-ORAS remake: where everything that makes up a town lives in Omega Ruby (Europe, decrypted `.3ds`), what each file means, and how it was established. Every statement is tagged:

- **checked**: measured on the real files, and the code that reads it is tested (`tests/remake/`);
- **seen**: observed on the real files, not tested or only on one example;
- **inferred**: deduced, not proved;
- **unknown**: not understood.

Inspect any zone, piece or area pack with `remake_tool oras-inspect <oras.3ds> zone|piece|area <index>`. The owner's dumps are never committed.

## 0. Start here (handover for the next agent)

**Goal (owner's words)**: recreate Pokemon Platinum in Omega Ruby with ORAS's own assets, as a LayeredFS mod played in Azahar; tools good enough to rebuild any map right the first time, whatever the setup. Any ORAS asset pack may be used, not Littleroot's alone. The owner tests on a phone and sends Azahar logs; nothing in the container runs the game, so every in-game claim is "untested" until the owner reports.

**Inputs** (the owner's dumps, never committed; the Google Drive folder has them): `platinum.nds` (French Platinum CPUF) and `oras.3ds` (Omega Ruby Europe, decrypted).

**Build and test** (about 3 minutes cold):

```
cmake -S tests/remake -B build-remake-tests -G Ninja && ninja -C build-remake-tests   # libremake and the 13 suites (run each remake_*_test; all print ok)
cmake -S tools/remake -B build-remake -G Ninja && ninja -C build-remake remake_tool     # the command line tool
```

**Commands, by what you want to know or make**:

| Want | Command |
|---|---|
| Understand a zone, a map piece or an area pack | `oras-inspect <oras.3ds> zone\|piece\|area <n>` (piece lists each mesh's layer, blended or opaque, material and textures) |
| Check the readers still match the game | `oras-verify <oras.3ds>` (439/439 texture files, 536/536 zones, 857/857 pieces) |
| Search every piece and pack | `oras-catalog <oras.3ds> <dir>`: `pieces.tsv` (meshes and materials), `packs.tsv` (textures per pack), `tiles.tsv` (every tile value, pieces and tiles using it; the source of `TownCheck`'s established set) |
| Read Platinum's world | `oras-world <platinum.nds> <matrix> <dir>` |
| Build a Platinum town as an ORAS mod | `oras-town <platinum.nds> <oras.3ds> <out dir> [options]` (usage in `prototype/README.md`); prints its design-rule results (section 8) and writes the mod, `town_preview.gltf` and `town_layout.txt` |
| Measure the whole game's zone, outline and tile rules | `oras-measure <oras.3ds> <dir>` (`OrasMeasure.h`; 23 s; the findings are in section 9e) |
| See a piece from above, simplified (zones, rim, blades, structures; optional tile grid, tile values, doors) | `oras-topview <oras.3ds> <piece> <out.png> [--grid] [--tiles] [--doors] [--points] [--px N]`, or `topview <GR piece file> <out.png> ...` for a mod's piece (`TopView.h`; the legend is printed). Colours: green ground, light green lighter grass, tan path, yellow blades, red rim, grey structure, dark shadow, blue water. `--points` (with `--grid`) is the view the zone and outline rules were measured with: zone borders (dark green lighter grass, dark brown path), the blade strip's triangles (orange), tips (red) and roots (blue); make it for the game's piece and the mod's to compare them |
| Preview a piece or glTF | `bch`, the editor `tools/remake/editor/world_editor.html`, `prototype/render_compare.js`, `prototype/render_closeups.js [--top]` (a material blends in the preview only when its mesh draws in a layer above 0, as in the game: 9c) |
| See a zone's border and blade points on the tile grid | `oras-topview ... --grid --points` (the game's piece) or `topview town_piece.bin ... --grid --points` (`oras-town`'s piece): borders dark green / brown, strip orange, tips red, roots blue |
| Measure how a piece lays a zone and its strip | `mesh-json <GR piece> mesh.json`, then `python3 prototype/measure_zone.py mesh.json [zone texture] [strip texture]` (9d) |
| Look at a texture | `oras-texture <oras.3ds> <area pack> <name> <out.png>` (find the pack with `oras-catalog`'s `packs.tsv`) |

**Where the code is** (`tools/remake/src/`): `Bch` (read) and `BchWriter` / `BchTextureFile` (write), `Garc` and `BinLinker` (containers), `OrasZone` and `Amx` (zones, scripts), `PlatinumWorld` and `TerrainScan` (Platinum side), `TownLayout` (roles, collision, doors), `TownBuilder` and `TownShapes` (the ORAS piece), `OrasTown` (zone, area pack, patches, preview, design-rule block), `TownCheck` (the rules), `OrasInspect` (inspect, verify, catalog), `TopView` (the simplified top view), `OrasMeasure` (the whole-game measures). Tests mirror them in `tests/remake/`.

**Rules the work follows** (they exist because each one broke a phone run or the owner's patience):
1. Never write an archive member except through `ReplaceMember` (a member compressed twice crashed the field's start).
2. Never flip a PICA texture's v; textures are stored bottom row first.
3. Look at how ORAS does it in the game's own data before inventing (outline blades on the zone border, tile-stepped zones: the owner insisted, and a first version with blurred, rounded zones was wrong).
4. Show results: render and send images, deliver mods as a zip; say what was not tested.
5. A diagnostic is deleted once it has served; a finding goes in this file, a code comment or a commit message.

**State** (latest mod: v24, `oras-town` default options, built by the `Remake mod` workflow): Twinleaf (Platinum) rebuilt as Littleroot's piece 6, with Littleroot's grass and its zone rules (tile-stepped zones with their own corners pulled in, half-tile paths, forest-tip arcs on the rim, outlines on soil paths placed as measured, 9d), square crossroads corners, ORAS's white picket fence, the pond's walls in earth, snow from the ice cave at half-tile precision with snow clumps (9f), tile values by surface (9e), trees within 2 tiles of open ground (996,992 bytes), the zone on Littleroot's own area pack 8 with the donor's textures added to it, and Littleroot's own door models moved onto the new doors. **Phone results (checked, Littleroot save, Azahar on Android): v24 shows the town** (section 11 has every run). Not yet reported: walking, collision, doors and warps in v24. Known deviations: 3 warps for 4 doors (a new warp needs the zone's entity words, unknown); Littleroot's door models (types 2, 7) on Petalburg's houses, whose own door (type 4) hides the map (11); a Route 201 piece not rebuilt by this tool; **the houses are Petalburg's red-roofed ones, not Littleroot's wooden tiled ones, and there are no house cast shadows, flower sprites or grass tufts**, so the owner's reference screenshot (Littleroot in game) is matched on the ground only.

**Littleroot's houses, measured but not built** (tile coordinates of its piece 6, `a/0/3/9`): two small houses, 6 tiles wide by 5 deep (solid cols 11-16 and 23-28, rows 7-11), door tile (14, 11) and (26, 11) (door type 2, index 3 of 6 from the left), made of mesh 5 (`t01_01`, texture `t101_01`; boxes x 10.5-17.5, z 6.5-12.8) plus a cast shadow in mesh 11 (`shadow1`, box x 11-20, z 5.5-12.5); one large house, 7 wide by 6 deep (cols 13-19, rows 18-23), door tile (16, 23) (door type 7), made of mesh 7 (`t01_02`, `t101_02_fix`; box x 12.4-20.5, z 18.5-24.8) plus mesh 5's trim and chimney and its shadow (box x 12.5-22.5, z 16.5-24.5). Mesh 2 (`chip_mado`) holds the windows, meshes 4 and 6 (`chip_wood_*`) the trims, mesh 12 (`t01_a01`, blended) a large cover over the ground. The Petalburg donor already has same-texture slots for the windows (18, `chip_mado`) and the shadow (26, `shadow1`), a `touka_house01` slot (16) and a `touka_waku01` slot (17) that `BchSetTextureName` can re-point to `t101_01` and `t101_02_fix`; those two and the door texture `t101_door` (pack 8's file 1) must first be added to the donor pack by `ImportTextures` (today it adds the three grass textures). Whether door type 2 and 7 models find their textures in the donor pack is unknown and untested.

**Next steps, in the order that pays most**: (0) Littleroot's houses, shadows and sprites as above, to match the owner's screenshot; (1) rebuild towns from several ORAS pieces and packs, choosing a donor by its layers and mesh slots (section 9c: blend is a layer flag, not per-material registers); (2) read the engine's per-layer state and how it draws a projected texture (`.code` is BLZ-compressed; capstone ARM works); (3) the Pawn scripts (needed for NPCs and events), the `coll` geometry, how matrices (2b) connect, adding meshes or materials to a terrain model: section 10.

## 1. Littleroot's identity (checked)

| What | Value | How known |
|---|---|---|
| Zone | `a/0/1/3` member **6** | its 3 warps, 11 characters, 4 furniture and 3 triggers all lie in Littleroot's matrix cell (21 of 21) |
| Map matrix, cell | matrix 1, cell (x 2, y 4) = tiles x 80-119, z 160-199 | the terrain model's name `world01_02_04` is `world<matrix>_<x>_<y>` |
| Map piece | `a/0/3/9` member **6** (GR) | same name |
| Area pack | `a/0/1/4` member **8** (AD) | zone header word 1 |
| Spawn / fly position | tile (100.5, 172.5) | zone header words 22-24 |
| Houses (zones entered by its 3 warps) | zones 223, 225, 227 | warps' destination words |

Zone 6's name: the English place names are in text bank 90 (`a/0/7/3`), "Littleroot Town" at line 170 (checked: the only bank holding that exact line). The link from a zone to its name line is **unknown** (the header's words 12-15 are not it: they step by 2-6 from zone to zone).

## 2. The RomFS, by archive (all are GARC; counts and sizes read)

| Archive | Members | Role |
|---|---|---|
| `a/0/1/3` | 538 | zones (ZO), checked (section 3) |
| `a/0/1/4` | 229 | area packs (AD): textures and small tables of a zone's area (section 5) |
| `a/0/3/9` | 857 | map pieces (GR), the 40x40-tile terrain and collision (section 4) |
| `a/0/4/0` | 431 | map matrices (MM), section 2b |
| `a/0/2/3` | 380 | building and prop models (BM), seen (not used by Littleroot's houses: they are in its terrain model) |
| `a/0/2/1` | 544 | characters, seen |
| `a/0/3/2` | 1030 | effects, seen |
| `a/2/6/7` | - | tall grass models, seen |
| `a/0/7/1` ... `a/0/7/8` | 175 each | game text, one archive per language (3 English, 4 French), checked decoded (section 6) |
| `a/0/7/9`, `a/0/8/0` ... `a/0/8/6` | 637 each | text too (20-byte first members), role **unknown** |
| `a/0/8/8`, `a/0/0/8`, `a/1/5/2`, `a/1/6/0` ... | large | not looked at |

All 300 RomFS archives are listed with their member counts by a throwaway scan (not kept): `a/0/9/1` (974 LZ members), `a/0/9/2` (631), `a/0/2/2` (511), `a/1/2/0`, `a/1/2/1` (500 each) are per-id tables whose use is **unknown**.

## 2b. Map matrix (MM) - seen on matrices 0, 1 and 2 (`oras-inspect matrix N`, `oras-inspect piece-names`)

A matrix is a `MM` container of two files.
- **File 0** (2312 bytes in matrices 1 and 2, 12 in matrix 0): `u16` 1, `u16` 0, then `u16` width and height (8 and 8 in matrices 1 and 2; 1 and 1 in matrix 0), then width x height `u16` **map piece numbers** (`a/0/3/9` members), row by row, `0xFFFF` for no piece; the rest of the file is `0xFFFF`. Seen: matrix 1 holds pieces 1-6 at the cells their models name (`world01_02_01` = piece 1 at x 2, y 1; ... `world01_02_04` = piece 6, Littleroot, at x 2, y 4), matrix 2 pieces 7-10 at `world02_*`'s cells. Why the file has room for 1,152 words is **unknown** (a bound on a matrix's size is inferred, not checked).
- **File 1** (324 bytes): a `u32` count, then groups that start `0001 0000` followed by pairs of words that read as floats (0x44B9A000 = 1485, 0x45067000 = 2151 in matrix 1): **unknown**, perhaps the camera or areas in world units.
- **The overworld is not one matrix**: the 165 pieces named `world<NN>_<x>_<y>` belong to 14 matrices (`world01` to `world14`, 3 to 23 pieces each), each a section of Hoenn.
- **Sections are joined by warps at their edges** (seen, `oras-inspect zones`, header word 2 = matrix): zones of one matrix have no warp between them (Littleroot, zone 6, and zone 23, both on matrix 1, cell rows 4 and 3), while zone 7 (matrix 1, cell 2,2: Oldale, its 5 warps) has a warp to zone 24 at tile (80.5, 100.5), on its section's west edge (x 80 = cell 2's first column), and zone 24 (matrix 2) one back to zone 7 at (199.5, 140.5), the east edge of its cell 4. Zones 8 and 30 (matrices 3 and 5) are joined the same way, by two warps each at one tile's distance. A warp's words (seen on zones 6, 7, 8, 24, 30, `OrasZone.h`): 0 destination zone, 1 the destination's warp index the player arrives at, 2 the kind (low byte 1 a door, 2 an edge on the section's east side, 3 its west side; high byte 3 doors, 5 edges), 4 and 6 x and z in pixels, 5 the height (signed pixels), 7 always 1, 8 the **span in tiles** along the edge (1 for doors). Zones 8 and 30 share an edge 19 tiles long as two warps of 15 and 4 tiles, contiguous (z 285.5 + 15 = 300.5), each pointing at the other side's warp of the same index; zones 7 and 24 one of 3 tiles. North and south edges, and whether 15 is a span's maximum, are not seen.
- So a region rebuilt from Platinum can be laid out as matrices of pieces, walked across seamlessly inside a matrix, and joined to the next matrix by edge warps, a kind of entry the tools already write (inferred, untested). The other pieces (`c101...`, `battle01...`) are towns' interiors, gyms and battle maps, one piece each.

## 3. Zone (ZO) - checked on all 536 zones that are ZO containers

Reader: `OrasZone` (`OrasZone.h` holds the layout), test `remake_zone_test`. Of the 538 members, 536 are ZO containers and all 536 read; the other two are not ZO.

- file 0, 56 bytes: 28 `u16` words. Word 1 area pack, word 2 matrix, word 13 the zone's own number, words 22-24 and 25-27 a position in pixels (1/18 tile). Others kept raw.
- file 1: `u32` size (counts what follows the field), four `u8` counts (furniture, characters, warps, triggers), `u32` count of a fifth kind, then the arrays from offset 12: furniture 0x14 bytes, characters 0x30, warps 0x18, triggers 0x18, fifth kind 0x18. Rule `size + 4 == 12 + 0x14 nf + 0x30 nn + 0x18 (nw + nt + n5)` holds for all 536. Then, immediately, a Pawn script (the initialisation script), padded to 4.
- file 2: a Pawn script (the zone's own). file 3 empty, file 4 12 bytes of zeros in Littleroot's.
- Field meaning (seen on Littleroot, positions land in its cell): furniture tile x, z at words 4, 5; characters: index word 0, model word 1 (289, 281, 4153, ...), tile x, z at words 20, 21; warps: destination zone word 0, position in pixels x word 4, z word 6 (a tile's centre is at +9); triggers tile x, z at words 6, 7. The other words (movement, facing, script ids, flags) are **unknown**.
- Totals over all zones: 2904 characters, 911 warps, 832 furniture, 361 triggers.

### Scripts - header checked, content unknown

All 1072 scripts (536 x 2) have a valid Pawn/AMX header (`AmxInfo`): magic 0xF1E0, versions 10/10, flags 0x1C (packed code). Littleroot's zone script: 6438 bytes holding 10432 bytes of code, 684 of data, 1 public, 58 native calls; its initialisation script: 1267 bytes, 13 natives. The packed code, the native table (what each native call does) and the link from a script to text lines are **unknown**: a decompiler is a separate project (`pk3DS` has an opcode table for it).

## 4. Map piece (GR) - Littleroot's, seen on the real file

`remake_tool oras-inspect <oras.3ds> piece 6`. Seven parts:

| Part | Size | What |
|---|---|---|
| 0 | 6528 | `u16` 40, `u16` 40, then 40x40 `u32` tile values (row-major), then 124 bytes (zeros in Littleroot's) |
| 1 | 382976 | the terrain BCH: model `world01_02_04`, 13 meshes, 9757 vertices, 9001 triangles |
| 2 | 2432 | collision geometry, magic `coll`, then `u32` 0x910, `u32` 0x5D8, small counts, then floats (-363.8, ...): polygons in world units; meaning of the fields **unknown** |
| 3 | 256 | door models: `u32` count (5), then 44 bytes each: type (2), scale x y z, rotation, position x y z in pixels; Littleroot's first positions repeat its warps' (1701, ., 3087) |
| 4 | 128 | starts 04 02 00 00, then 18.0f, **unknown** |
| 5 | 128 | zeros, **unknown** (other pieces: tall-grass models `enc_grass_NNNN` per the earlier notes) |
| 6 | 128 | `KAGE` ("shadow"), then 0x13122500, zeros, **unknown** |

- **Tile grid (checked on this piece)**: only two values: `0x00000020` free (446 tiles) and `0x01000021` blocked (1154). Other values and their bits are in `DEVELOPMENT_NOTE.md` (phone tests on Route 101).
- **Houses are in the terrain model** (meshes `t01_01` with texture `t101_01`, 4641 vertices; `t01_02` with `t101_02_fix`; windows `chip_mado`, `t01_a01`), not separate models.
- **Ground**: `chip_kusa_a` (the main grass, 975 vertices, vertex colours painted dark under trees: the open ground's are about 0.7, 1.0, 0.8) over which `chip_kusa_b` (lighter, 148 vertices) draws the path.
- **Soft edges are two meshes**, both measured on this piece and on all 857:
  - `chip_grass_decolate` (here with texture `chip_alpha`): strips 8.7-10 units wide, 0.3 above the ground, along the border of the lighter grass (inner vertices white, v 0.302; outer vertices the grass's colour, v 0.496; u advances 0.0245 per unit), plus square decals (bare earth u 0.02-0.48 v 0.52-0.98; flowers u 0.51-0.99 v 0.51-0.99; 32-66 units). `chip_alpha` is an atlas: grass blades at rows 38-60 of 128, leaves, a bare-earth patch, flowers. In Petalburg 72% of the equivalent mesh's vertices lie within 12 units of the sand path's border (median 5.4). 51 of the 857 pieces have it, with a different alpha texture per town.
  - `chip_edge_tex` (`projection_dummy` + `chip_grass_edge`, the texture projected by the material): a rim 9 wide rising from y 1 to 3.5, along the outline of the playable ground. 77 of 857 pieces have it, mostly with `chip_kusa_edge`; in Petalburg none of its vertices is near the sand paths.
- **Texture v**: a PICA texture's rows are stored bottom first and decoded in that order, so the game's v indexes them directly (checked: Littleroot's blade strips land on the blades only without a flip). The old preview flipped v, which mirrored every texture in it.
- Size context: the largest of the 857 pieces is 1,368,064 bytes and 35,691 vertices; Littleroot's is 392,704 and 9,757.

## 5. Area pack (AD) - Littleroot's pack 8, seen

`remake_tool oras-inspect <oras.3ds> area 8`: 12 files. File 1: a BCH of 14 textures (doors and windows: `door_light`, `fs_door_01/02`, `t101_door`, `t102_door`, `pokecen_door`, `in`, `kage`, `track`, `chip_mado`, `chip_wood_a/b`, `chip_wood_shadow`). File 11: a BCH of 45 textures (grass `chip_kusa`, `chip_kusa_a/b`, `chip_kusa_edge`, `chip_grass_edge`, `chip_alpha`; houses `t101_01`, `t101_02`, `t101_02_fix`, `t102_01/02`, `101to_kotoki_*`; cliffs `chip_gake*`; sea `chip_sea_a/b`; `pokecen_01`, `pc_mado`, `pokecen00`; `enc_grass`; `r103_*`). Files 2 and 4 (2944 bytes) are BCH containers with no model or texture (animations, **inferred**); files 0, 3 and 6 (640, 1024, 1152 bytes) are small tables, **unknown**; the rest are empty.

Pack 9 (Petalburg's) holds what pack 8 lacks for Petalburg's houses, river and flowers and lacks `chip_kusa_a`, `chip_grass_edge` and Littleroot's houses. **A town built only from Littleroot's own meshes and pack 8 needs no texture added to a pack and no shared pack changed**: every texture it uses is in pack 8 (houses, grass, edges, alpha, trees, sea, Pokemon Center). Missing from pack 8: Petalburg's river and flower textures.

## 6. Text - checked

Format (Gen 6 text, as in 3DS Pokemon games): `u16` sections (1), `u16` lines, `u32` total, `u32` 0, `u32` section offset; at the section: `u32` length, then per line `u32` offset, `u16` length, `u16` unused; each line's `u16` characters are XOR-ed with a key that starts at 0x7C89 for line 0 and grows by 0x2983 per line, and rotates left by 3 bits after each character. Control codes (0x10 variable, 0xE07F, ...) are kept as raw characters. Decoded 175 banks of English and French; bank 33 holds place descriptions, bank 90 place names, bank 83 radio news.

## 7. What the code shows (checked on the decompressed `.code`)

The `.code` in ExeFS is packed with BLZ (backward LZ, flag 0x01 of the extended header). Unpacked: 0x530000 bytes, ARM code from 0x100000. Seen:
- `0x4D83FC(this, slot)` returns a pointer to a zone header from `this + 0x94C0 + slot*4`; `0x4D8978` reads its word 1 (the area pack), `0x4D83EC` word 2 (the matrix). A garbage pointer there crashed the first phone run of `oras-town` (a zone member compressed twice, now refused by `ReplaceMember`).
- Blocks are validated by a magic then a version word (`KAGE` at `0x3CA7C0`); `coll` is validated elsewhere (not found by a literal load).

## 8. Design rules, enforced

`oras-town` checks these before writing a mod (`TownCheck.h`, `ReplaceMember`), because each broke or could break a phone run:

| Rule | Why | Kind |
|---|---|---|
| Every texture a material names is in the area pack the piece uses (`projection_dummy` and meshes that draw nothing excepted) | the pack holds the only textures the game loads for the area | error: no mod written (`--allow-errors` overrides) |
| A mesh has at most 65536 vertices | 16-bit indices | error |
| The piece stays within the game's largest piece (1,368,064 bytes, 35,691 vertices) | memory use beyond what any original piece needs is untested | warning |
| A replaced archive member is the container it must be (GR, ZO, AD), compressed once, and reads back identical | a zone compressed twice crashed the field's start | error (`ReplaceMember`) |
| A texture's v is used as stored, never flipped (previews included) | PICA textures are stored bottom row first, decoded in that order | rule of the BCH preview |
| Textures are added to an area pack under their own names, never written over an existing one (`--grass -1` adds none) | a pack is shared by every piece of its area | by construction |
| The piece is at most 996,992 bytes (`PieceBytesShown`) | the largest seen to show in Littleroot's place; 1,074,944 showed nothing (11) | error |
| The zone keeps its own area pack; what the piece needs is added to it | a zone moved to another town's pack hung the game (11) | by construction (`--area` 8) |
| Door models are the target's own entries, only moved | Petalburg's door type with zeroed words hid the map (11) | by construction |

Layout rules (`CheckLayout`, all warnings except the first two): the tile block is 40 x 40 (error); the door block fits its count (error); tile values are among the 94 the game uses in at least 5 pieces; door models are at scale 1, turned by a multiple of 90 degrees, and centred on a tile (section 9b). The builder places door models at scale 1, so Twinleaf raises none of them.

Not yet checked by code: a door model per placed door (the block holds 5), warps inside the piece's cell, the zone's area pack matching the piece's.

## 9. The whole game, not only Littleroot (checked: `oras-verify`, `oras-catalog`)

- **Area packs**: 228 packs hold textures (the 229th member is not one). Every pack is an `AD` container of exactly **12 files, always the same slots**: 0 and 3, 4, 6 small tables (unknown), 1 a BCH of textures (doors, windows, props: 14 in Littleroot's), 2 a BCH with neither model nor texture (animations, inferred), 11 the main BCH of textures (45 in Littleroot's), 8 filled in 7 packs only, 5, 7, 9 and 10 empty. Pack sizes: 4 to 123 textures, median 30; the richest are 11 (123), 196 and 15 (99), 14 (95).
- **Map pieces**: 857, all with a readable terrain model. They are outdoor maps **and interiors** (materials `table01`, `shelf01`, `chair01`, `wall01`, `floor01` appear in 50-80 pieces each). 684 pieces are compatible with exactly one pack (the textures they name are all in it and in no other), the rest with 2-14: a piece's pack is determined by its textures.
- **Materials are town-specific**: 2884 distinct names for 857 pieces, because each town names its own objects (`c108_rune_*`, `t101_*`, `touka_*`). Shared across the game: `shadow_a` (285 pieces), `chip_kusa` (177), `chip_rock_b` (158), `chip_wood_b` (132), `gake_basic` (128, cliffs), `chip_sea_b` (126), `shadow1` (123), `platan_bk` (114), `chip_edge_tex` (77), `chip_grass_decolate` (54). Buildings are baked into each piece's terrain with the town's own materials.
- **Texture files**: `BchWriteTextureFile` (`BchTextureFile.h`) writes a texture BCH from a list of decoded textures and reproduces **all 439 texture files that hold data, byte for byte** up to the relocation table, whose entries (9 * count + 16) match as a set. So **a texture of any pack can be added to any other pack's main texture file** (done by `oras-town` for the grass). The 17 other files are empty placeholders (one texture, no data).
- **What stays limited**: a piece's terrain keeps its donor's meshes and materials: only their geometry (`BchReplaceGeometry`) and texture names (`BchSetTextureName`) can change, not add or remove a mesh or a material. A material's render state is now read (section 9c) but only partly understood, and culling and the depth/blend registers are still not decoded. Composing assets from several pieces therefore means choosing a donor piece with the slots wanted, then re-texturing them from any pack.

## 9b. Construction rules (measured on all 857 pieces)

- **Frame**: a piece is 40 x 40 tiles; one tile is 18 model units (Platinum: 16 DS units, scale 1.125). Geometry, props and doors are aligned to this grid.
- **Heights**: ground levels are multiples of 18 (one tile of height).
- **Planar UVs**: ground textures repeat every 72 units horizontally and 54 vertically (4 and 3 tiles).
- **Doors**: door models sit at a tile's centre (x, z = 9 mod 18), scale 1 (all 368 door models in the game), rotation a multiple of 90 degrees; the door block is a u32 count followed by 44-byte entries.
- **Tile block**: 40 x 40 u32 values after a 40, 40 header; 94 values occur in at least 5 pieces (`TileValueEstablished`), and each maps to a surface kind.
- **Seams**: neighbouring pieces continue each other's ground at the shared edge.
- **Collision**: a `coll` block with its own header, geometry fields undecoded.

## 9c. Material render state (inferred, not engine-confirmed)

`BchMaterial` now carries `Flags` (parameter block word 1), `TextureUnits` (command register 0x80, low 3 bits) and `Mappers[3]`. Read on all 12,481 materials of the 857 pieces; the meaning below is **inferred from correlations, not tested in game**:

| Field | Values seen | Reading |
|---|---|---|
| `Flags` top byte | `0x3A` (9,188 materials), `0x3E` (3,225) | bit 2 set = **blended**: all of `chip_grass_decolate`, `shadow1`, `shadow_a`, `chip_wind`, `chip_edge_tex`, light and sea meshes; none of `chip_kusa`, `chip_wood_b`, `window01_gr`. In Littleroot, layer 0 meshes are all opaque and layers 1-3 all blended. |
| `Flags` low byte | `01` (12,413), `11`, `02`, `13` | bit 4 (`0x10`) goes with projected or shared textures (`platan_bk`, `chip_edge_tex`, cliffs). Meaning unconfirmed. |
| `TextureUnits` | 1 (most), 3, 7, 0 | the number of texture units a material samples: the edge ribbon uses 3 (unit 0 projected, unit 1 `chip_grass_edge`); 0 = untextured (68 materials). |
| `Mappers[0]` | `00020200` (most), `00000200`, `00020300`, `00030300`, `01020200`, `00020203` | wrap and filter words; `...03` on unit 0 pairs with the projected edge material. Individual bits not separated. |

The glTF preview follows the same rule: a material blends only when a mesh using it draws in a layer above 0 (a layer-0 texture with holes, such as a tree canopy or a picket, is cut out, solid). Before, it blended any texture with a clear texel, so the canopies hid nothing behind them and the rim and bands showed through the trees.

`oras-inspect piece` prints each mesh's layer and blended/opaque. **Checked**: between an opaque material (`chip_kusa_`) and a blended one (`chip_grass_decolate`, `shadow1`) the whole 0xC0-byte parameter block differs in that one bit and nothing else, and across the corpus the top byte of `Flags` only ever takes `0x3A`, `0x3E` and `0x00`. So the BCH holds **no per-material blend factors, depth or alpha-test registers**: the engine derives them from the layer and this flag. Culling and depth are therefore not material data to decode; what a layer does (draw order, which state it sets) is engine code, still unread.

## 9d. How ORAS cuts its ground zones (checked on Littleroot's piece 6, `chip_kusa_b`, `chip_grass_decolate`, `chip_edge_tex`)

Seen on the real meshes (welded vertices, tile units; no other piece measured yet):
- **A zone is made of whole tiles** (the whole game agrees: 9e). The light-grass patches (`chip_kusa_b`) have every boundary vertex on a lattice point of the 18-unit tile grid, one vertex per lattice point along the edge (edges of exactly 1 tile), shifted by under 0.11 tile in each axis (a fixed jitter, not a curve). All their corners are square, convex and concave alike (94 boundary edges: angles 0 and 90 degrees only, plus the jitter). So the game's zones are **not rounded**: what reads as soft in game is the blade strip.
- **The blade outline** (`chip_grass_decolate`, chip_alpha) is a ribbon centred on that stair border (about 0.45 tile wide: 0.225 each side), with a vertex at every lattice point, also with square corners (its nearest vertex to a patch corner is 0.15 tile away on average, convex and concave).
- **The rim** (`chip_edge_tex`, where open ground meets the forest) follows the lattice too, but cuts the **forest's convex tips**: where three of the four tiles around a lattice point are open, the lattice point is replaced by one vertex 0.36 tile towards the forest tile on each axis, so the border passes 1 tile before and after the corner (a radius of one tile, three points). A tip of the open ground (one open tile of four) stays square. Checked: of 7 such arcs in Littleroot's rim, 5 are reproduced by `StairZone` to 0.01 tile; the 2 others sit beside sign tiles, which are blocking tiles that are not forest. The strip's far side is 0.5 tile into the forest (9 units), rising from 1 to 3.5.
- `TownShapes` (`StairZone`) builds exactly this: fill from whole cells, chains with a point per lattice point, normals mitred at corners, `roundTips` for the rim. `oras-town` makes a tile a zone's when at least two of its four half-tiles are (a tie goes to the path). Its old blurred, rounded zones were wrong and are removed.
- **Measured closer, by corner kind** (`prototype/measure_zone.py` on piece 6; it reproduces these numbers): the shift from the lattice is not a random jitter. The zone's **own corners** (one of the four tiles around the lattice point in the zone) are pulled **0.122 tile into the zone** on the diagonal (median; 10%-90% 0.04-0.16); its **inner corners** sit on the lattice (0.000) and its straight runs nearly so (0.006). The strip is not one width either: its tips lie **0.23 tile in on a straight run, 0.32 at the zone's own corner, 0.13 at an inner corner**, its roots **0.25, 0.22 and 0.36 out** (along the normal, or the bisector at a corner); the 0.225-each-side ribbon above is their average. The game's border tiles with the same 3x3 neighbourhood have the same border to 0.02 tile (so it is a rule on the lattice, not hand-drawn), but the strip's points scatter 0.14 tile between such tiles (ORAS's own scatter, not predictable point by point). `StairZone(..., jitter 0, outerPull 0.12)` and `OutlineStrip` build this; run on Littleroot's own tile mask, the strip lands a median 0.07 tile from Littleroot's real tips and roots (90% within 0.15; 0.18 with the jitter-and-centred version).

## 9e. The whole game's rules for zones, outlines and tiles (checked: `oras-measure`, all 857 pieces, 12,481 materials)

`remake_tool oras-measure <oras.3ds> <dir>` measures every mesh's boundary against the tile lattice and writes `zone_shapes.tsv`, `tile_surfaces.tsv` and `outline_offsets.tsv`. What it shows (boundary vertices welded to 0.05 tile; "kind" by `TopView`'s classification of the material):

| Rule | Measured |
|---|---|
| Zones lie on the tile lattice | lighter grass: 96.8% of boundary vertices on a tile lattice point, 98.9% of edges axis-aligned, 93% of edges exactly 1 tile long, **62 of 73 meshes stair-cut on the tile lattice, 6 on the half-tile lattice**. Paths: 65% on the tile lattice, 81% on the half-tile one (**103 meshes tile lattice, 94 half-tile lattice, 65 other axis-aligned offsets, 16 free-form, 8 diagonal-rich**), 93% axis-aligned edges. Water: 69% / 86% (132 tile, 89 half-tile, 56 other, 40 free-form). |
| Half-tile steps exist | a path or a pond may step by half tiles, not only light grass (which almost never does). Platinum's own paths are half-tile, so `oras-town` keeps them at half-tile precision. |
| Jitter is Littleroot's style, not the rule | mean shift from the lattice: 0.007 tile on light grass (59 of 73 meshes under 0.01), 0.011 on paths, 0.002 on water. Littleroot's (0.056 on its light grass) is among the 3 meshes of 73 above 0.05. `StairZone` takes the jitter as a parameter; the town uses Littleroot's because that is the look asked for; 0 is the game-wide norm. |
| Zone corners | square (a point per lattice point, diag-edges under 3% for zones), never rounded. Only the forest rim cuts tips (9d). |
| Blade outlines | a ribbon centred on the zone border, **half-width 0.256 tile** (median over 143 blade meshes; 10th to 90th percentile 0.248-0.297; Littleroot 0.252 = 4.5 units, as built). |
| What gets an outline | lighter grass: 66 of its 73 meshes (85% of its border vertices have a blade vertex within 0.45 tile); **paths in `chip_soil_a`: 49 of 63 meshes; sand and road paths (`sand`, `batres_sand`, `r124_sand`, `r110_road`): 0 of 29**; water: 2 of 323; the forest rim: none. So soil paths and light grass are outlined, water, sand and roads are not. |
| The rim | bimodal offsets: inner edge on the lattice, outer edge 0.5 tile into the forest (9 units), 93% of its vertices on the half-tile lattice. |
| Heights | ground meshes: 54% of vertices at a multiple of 18; 143 of 520 ground meshes have one height level, 377 several (terraces); light grass is flat (66 of 78 meshes one level); water 265 of 330. |
| Tile value to surface | see below. |

**Tile values** (`tile_surfaces.tsv`: for each value, the surface found under its tiles' centres; 208 values, 1.37 million tiles). The value is a bit field; what the data says about its parts, for the surface mesh under the tile:
- low byte `0x21` solid (842 pieces, 1.05 million tiles: the forest, houses and the outside); `0x20` walkable, nothing special; `0x06` water (93% of its 89,635 tiles lie under water meshes; with byte 2 `0x1A` 85%, `0x18` 80%); `0x04` an encounter tile (tall grass: `20004004`, `27005004`, `20084004`, 95-99% under plain ground meshes).
- a **path tile** has its own value: `020A8020` (81 pieces, 7,944 tiles, **95% under path meshes**), also `025A8020` (100%); byte 1 `0x80` with byte 3 `0x02` is 83% path. A puddle you can walk through is `070AA020` (98% under water meshes). The sea is `3D1A0006` (173 pieces, 95% water) or `411A0006` (88%).
- So the surface decides the value: `oras-town` writes `0x3D1A0006` under the pond (it wrote `0x3D180006`, the shore variant, before), `0x020A8020` under a path tile (it wrote plain `0x20`), and keeps `0x01000021` solid and `0x20004004` tall grass.
- Not yet known: what each bit means exactly (the surface under a tile is the evidence, not the engine), and values used by fewer than 5 pieces.

## 9f. Snow, fence, pond and kits: what Twinleaf takes from where (seen on the real files; the look in game untested)

- **Snow**: ORAS has no snowy town. Its only snow ground is the ice cave (piece 386, `d113r0107`, area pack 72): `chip_icedoukutsu02` on a blended layer (mesh 7, layer 1) over an ice floor, planar mapping u = x/72, v = -z/72, vertex colour 0.95 1 1, every vertex on the tile lattice (217 of 270, median 0.001 tile off); alpha 0.57 inside and on its open border, 1.0 against the walls. `d06_yuki01` and `c109_konayuki`, the other "yuki" textures, are falling-snow sprites, not ground; `c109_pur_yaneyuki` is a roof's snow. Twinleaf's snow uses the cave's texture, **opaque** (on green grass 0.57 shows pale green; the owner's choice), at **half-tile precision** read from Platinum's white (`TownLayout::Snow2`: white half tiles, grown into grey tree shadows, specks and pinholes cleaned, then grown a tile under the trees so it ends under the canopies), edged with **snow clumps**: chip_alpha's stone cluster (its earth-patch decal, u 0.02-0.48, v 0.52-0.98) in a texture made of that cluster's alpha and the snow's colour (`snow_clump`, `PicaTextureEncodeRgba8`), one every 12 units of border, 20-28 units, never where the ground half a tile out is a tree, the forest or a house. Tried and dropped: whole-tile squares; a fade on the border (the reverse of the cave's alpha); a band blending the snow through Route 113's mask `t105_a01` (the ash/grass transition mask of `hai_kusa`/`hai_najimi`: its luminance is a ragged edge, its alpha a straight ramp); Route 113's `chip_alpha_haji` soil bands as an outline.
- **Fence**: `c103_saku` (area pack 21; piece 153's mesh 25, opaque layer 0): vertical panels 14 units high, the 32x32 texture (two pickets) once a panel of about 24 units (23.1 and 24.6), two sheets 0.25 apart, vertex colour 1.0 front and 0.78 back. `chip_saku02` is a black iron railing; `chip_saku02b` an iron post.
- **Pond walls**: `chip_gake_b` (Littleroot's earth cliff, area pack 8): a grass lip at row 140 of 256 over brown earth and stones; the walls show rows 140-204. The donor's own band (`gake_01_touka`, rows 216-239) is rock with a blue water line.
- **Kits cut out of a piece** carry whatever the rectangle catches: Route 101's tree cut caught one triangle of the next tree's canopy (two tiles off, 27-38 units up), copied beside every tree; `KeepNear` drops a kit's pieces more than a tile from its anchor. The house and flower kits hold only their own pieces.
- **The forest rim over snow** (`chip_edge_tex`, 9d) drew a grey line on the snow against the trees; it is left out where the ground on its inner side is snow.

## 10. What is NOT known (the work left)

0. How to add a mesh or a material to a terrain model, and the rest of a material's render state (section 9c: not stored in the BCH; the engine's per-layer state is code, unread).
1. The scripts' semantics (packed Pawn code, 58+ natives, the text links): needed for any NPC, sign or story event.
2. The remaining words of zones' entries, and the zone-to-name-line link.
3. The `coll` geometry fields, GR parts 4-6, the 124-byte tail of part 0, the area pack's small files.
4. How the engine draws a mesh with projected texture (the material's render state is not decoded by the BCH reader).
5. How a map piece is placed by the matrix (`a/0/4/0`), and why a piece in Littleroot's place shows only up to between 996,992 and 1,074,944 bytes (11) when the game's own pieces reach 1,368,064 elsewhere.
8. What a door model type needs from its area (Petalburg's type 4 hides the map in Littleroot's, 11), and why a zone on another town's area pack hangs the game.
6. Interior zones (houses), encounter tables, music, weather, camera: not looked at.
7. The Platinum side of Twinleaf (zone events, scripts, text): read for the world, collision and warps only; scripts and text not decoded.

## 11. Phone runs (checked: the owner's phone, Xiaomi 2306EPN60G, Azahar in Pomegrade, a save in Littleroot)

Each mod built by the `Remake mod` workflow (`.github/workflows/remake-mod.yml`) from these options; "picture" is the map shown, "music" the field running with a black screen, "hang" neither.

| Mod | What differs from the game | Result |
|---|---|---|
| v21 (and the zone alone) | zone on area pack 9 | hang |
| z1 | piece and pack 9, zone on pack 8 | music |
| v22 | zone on pack 8, the donor's textures added to pack 8 | music |
| t1 | pack 8 enlarged only (69 textures) | picture |
| t6 | Littleroot's piece recompressed and rewritten | picture |
| t7, t8, t10, t11 | Littleroot's piece, its model padded to about 450, 540, 700, 860 KB | picture |
| t9 | the same, about 1,126 KB | music |
| t5 | Petalburg's piece as it is in the game | music |
| t13, t15, t16 | the built model, alone or with the built tiles or collision | picture |
| t14, t17 | the built door block (Petalburg's type 4, other words zeroed) | music |
| v23 preview | the full model with trees within 1 tile (887 KB), Littleroot's other files | picture |
| v24, v24 trees 2 | the whole town, Littleroot's door entries moved (886,528 and 996,992 bytes) | picture |
| v24 trees 3 | the same, 1,074,944 bytes | music |
| v24 door 4 | v24 with door type 4, the other words Littleroot's | music |

What follows (inferred from these runs, the engine not read): a zone's area pack is tied to more than its textures (a zone on another town's pack hangs the game); a piece in Littleroot's place shows up to at least 996,992 bytes and not from 1,074,944, below the game's largest piece elsewhere (1,368,064); a door model type the area does not use (4 in Littleroot's) hides the map, so Petalburg's door needs something of pack 9 that pack 8 lacks, which one is unknown (its pack's slot 1 holds door and prop textures: the first suspect). The archive rewrite itself (GARC version 4, all 857 pieces LZ11, members 4-byte aligned, the header's largest member 514,942 bytes) is sound.

