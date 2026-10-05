# Omega Ruby's Littleroot Town, and how ORAS holds a town

Reference for the Platinum-in-ORAS remake: where everything that makes up a town lives in Omega Ruby (Europe, decrypted `.3ds`), what each file means, and how it was established. Every statement is tagged:

- **checked**: measured on the real files, and the code that reads it is tested (`tests/remake/`);
- **seen**: observed on the real files, not tested or only on one example;
- **inferred**: deduced, not proved;
- **unknown**: not understood.

Inspect any zone, piece or area pack with `remake_tool oras-inspect <oras.3ds> zone|piece|area <index>`. The owner's dumps are never committed.

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
| `a/0/4/0` | 431 | map matrices (MM); member 1 is a 24x24 grid of packed `u32` (two `u16` each: e.g. 0x00190019, 0x00060006), meaning **unknown** |
| `a/0/2/3` | 380 | building and prop models (BM), seen (not used by Littleroot's houses: they are in its terrain model) |
| `a/0/2/1` | 544 | characters, seen |
| `a/0/3/2` | 1030 | effects, seen |
| `a/2/6/7` | - | tall grass models, seen |
| `a/0/7/1` ... `a/0/7/8` | 175 each | game text, one archive per language (3 English, 4 French), checked decoded (section 6) |
| `a/0/7/9`, `a/0/8/0` ... `a/0/8/6` | 637 each | text too (20-byte first members), role **unknown** |
| `a/0/8/8`, `a/0/0/8`, `a/1/5/2`, `a/1/6/0` ... | large | not looked at |

All 300 RomFS archives are listed with their member counts by a throwaway scan (not kept): `a/0/9/1` (974 LZ members), `a/0/9/2` (631), `a/0/2/2` (511), `a/1/2/0`, `a/1/2/1` (500 each) are per-id tables whose use is **unknown**.

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

Not yet checked by code (by hand until it is): tile values only from the known set, a door model per placed door (the block holds 5), warps inside the piece's cell, the zone's area pack matching the piece's.

## 9. The whole game, not only Littleroot (checked: `oras-verify`, `oras-catalog`)

- **Area packs**: 228 packs hold textures (the 229th member is not one). Every pack is an `AD` container of exactly **12 files, always the same slots**: 0 and 3, 4, 6 small tables (unknown), 1 a BCH of textures (doors, windows, props: 14 in Littleroot's), 2 a BCH with neither model nor texture (animations, inferred), 11 the main BCH of textures (45 in Littleroot's), 8 filled in 7 packs only, 5, 7, 9 and 10 empty. Pack sizes: 4 to 123 textures, median 30; the richest are 11 (123), 196 and 15 (99), 14 (95).
- **Map pieces**: 857, all with a readable terrain model. They are outdoor maps **and interiors** (materials `table01`, `shelf01`, `chair01`, `wall01`, `floor01` appear in 50-80 pieces each). 684 pieces are compatible with exactly one pack (the textures they name are all in it and in no other), the rest with 2-14: a piece's pack is determined by its textures.
- **Materials are town-specific**: 2884 distinct names for 857 pieces, because each town names its own objects (`c108_rune_*`, `t101_*`, `touka_*`). Shared across the game: `shadow_a` (285 pieces), `chip_kusa` (177), `chip_rock_b` (158), `chip_wood_b` (132), `gake_basic` (128, cliffs), `chip_sea_b` (126), `shadow1` (123), `platan_bk` (114), `chip_edge_tex` (77), `chip_grass_decolate` (54). Buildings are baked into each piece's terrain with the town's own materials.
- **Texture files**: `BchWriteTextureFile` (`BchTextureFile.h`) writes a texture BCH from a list of decoded textures and reproduces **all 439 texture files that hold data, byte for byte** up to the relocation table, whose entries (9 * count + 16) match as a set. So **a texture of any pack can be added to any other pack's main texture file** (done by `oras-town` for the grass). The 17 other files are empty placeholders (one texture, no data).
- **What stays limited**: a piece's terrain keeps its donor's meshes and materials: only their geometry (`BchReplaceGeometry`) and texture names (`BchSetTextureName`) can change, not add or remove a mesh or a material. A material's render state (blending, culling, layer, texture mappers) is not decoded, so a mesh slot cannot yet be chosen by what it does. Composing assets from several pieces therefore means choosing a donor piece with the slots wanted, then re-texturing them from any pack.

## 10. What is NOT known (the work left)

0. How to add a mesh or a material to a terrain model, and what a material's render state means (section 9).
1. The scripts' semantics (packed Pawn code, 58+ natives, the text links): needed for any NPC, sign or story event.
2. The remaining words of zones' entries, and the zone-to-name-line link.
3. The `coll` geometry fields, GR parts 4-6, the 124-byte tail of part 0, the area pack's small files.
4. How the engine draws a mesh with projected texture (the material's render state is not decoded by the BCH reader).
5. How a map piece is placed by the matrix (`a/0/4/0`) and how much memory the engine gives the pieces around the player (not tested since the zone fix: Twinleaf's 39,863 vertices exceed every original piece's maximum of 35,691).
6. Interior zones (houses), encounter tables, music, weather, camera: not looked at.
7. The Platinum side of Twinleaf (zone events, scripts, text): read for the world, collision and warps only; scripts and text not decoded.
