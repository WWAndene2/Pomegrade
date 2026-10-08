# Building Sinnoh in Omega Ruby — the progressive plan

The plan agreed with the owner (8 October) to rebuild all of Sinnoh in ORAS, stage by stage. Each stage ends with a mod
tested on the phone, one question per mod (protocol: `ORAS_LITTLEROOT.md` section 0). The formats are in `ORAS_ENGINE.md`
and `ORAS_LITTLEROOT.md`; this file keeps the plan and where each stage stands. Figures are measured on the owner's
Platinum dump unless marked otherwise.

## 1. How Sinnoh is built, against ORAS

| Domain | In Platinum | ORAS equivalent | State of the tools |
|---|---|---|---|
| Zones | 593 map headers (area, matrix, scripts, texts, music, encounters, events, weather, camera) | zone `a/0/1/3` (header + 5 files) | Read on both sides. 476 headers reached through warps, 67 on the main map; ORAS zones can be appended (538 and up) |
| Map | matrix 0 + 288 others (interiors, caves), 666 pieces of 32 x 32 tiles | matrix `a/0/4/0`, pieces `a/0/3/9` (40 x 40) | Sinnoh = 24 x 24 ORAS pieces, 372 used; built whole (all6, works on the phone) |
| Assets | 75 areas, 590 building models, textures per area | area packs `a/0/1/4`, ORAS pieces | Rebuilt from ORAS assets (Twinleaf rules); Platinum's own buildings have no equivalent yet |
| Floors and levels | terrain heights per piece (BDHC), multi-storey buildings in separate matrices, bridges | relief in the piece model + collision | Weak point: south ledges only. No hills, cliffs, bridges or slopes |
| Scale | 1 DS tile = 1 ORAS tile | the same | Measured: 93 % of buildings land on their tiles, at scale 1 |
| Events | 534 files: people, signs, warps, triggers | furniture, characters, warps, triggers | Platinum read whole. ORAS: warps and characters written; signs and triggers only kept |
| Wild encounters | 183 tables (`pl_enc_data`) | file 3 of each zone (9 kinds) | ORAS format read and written (copy of a table). Conversion not written |
| Trainers | trainer files | `a/0/3/6` (record) + `a/0/3/8` (team) | ORAS format read. Writing not done |
| Dialogue | 724 text banks (`pl_msg`) | `a/0/7/x` | ORAS read and written (place names). Bulk import not done |
| Story | 1124 scripts (DS bytecode), flags, variables | AMX (Pawn) scripts, 799 natives named | Biggest gap: ORAS scripts read and disassembled, not written |
| Light, camera, weather | weather and camera type per header | area pack files 4 and 6 | Light and camera: `oras-sandbox` (8 October). Weather not studied |
| Music | SDAT | BCSTM (inferred) | Not started |

## 2. Stages

0. **Foundations.** Sinnoh in new zones only, no Hoenn zone replaced, so it can share a mod with the sandbox; a fixed
   table "Platinum header -> ORAS zone" reused from stage to stage. **Started (8 October)**: `oras-region --new-zones`
   (below). Not yet run on the phone.
1. **Main map, flat**: the 372 pieces, collision, water, trees, paths, on new zones. Test: walk through all of Sinnoh.
2. **Relief and levels**: ledges in all four directions, slopes and cliffs from Platinum's heights, bridges.
3. **Zones and connections**: one zone per header, names, warps between zones and between matrices (interiors, caves,
   Mt. Coronet's floors), multi-storey buildings.
4. **Buildings**: Platinum's distinctive buildings (Hearthome, Veilstone, the League...), modelled from ORAS pieces or
   converted.
5. **Population**: people at their places (DS sprite -> ORAS model table), signs, items on the ground.
6. **Wild encounters**: the 183 tables converted (grass, water, fishing, time of day), Pokemon available in ORAS.
7. **Trainers and battles**: teams, classes, gym leaders, rival.
8. **Dialogue**: bulk text import (French first), names.
9. **Story**: ORAS scripts, translated from Platinum's or authored; flags, progression, cutscenes.
10. **Atmosphere**: light and camera per zone (tools ready), weather, then music.

## 3. Tools still to write

- An ORAS script writer (AMX assembler): blocks the story, signs and events (stages 5, 9).
- Relief from Platinum's heights (stage 2).
- Warps between matrices: interiors and caves (stage 3).
- Writers for encounter tables, trainers and texts from Platinum's data (stages 6-8).
- Correspondence tables: Pokemon (national dex), people's sprites -> ORAS models, items, trainer classes.
- A headless regression check per stage (the save loads, the field shows, a warp works), run before each phone mod.

## 4. Stage 0: `oras-region --new-zones`

Every zone the region uses (given by `--zone H:Z`, or a door's interior) is appended as a new zone, numbered from the
archive's count (538 on the game's), in the order of the zones it copies. The copy keeps the header, the warps and the area
pack as the region sets them, and drops Hoenn's characters, triggers, other entries and encounters (file 3 empty), and the
furniture too except in interiors. The matrix and the doors name the new numbers; the copied Hoenn zones are left as
they are. The zone tables grow to the new count as `oras-sandbox` grows them (member 536 one 56-byte row per zone,
member 537 one encounter file per zone). The log prints the mapping (`map header -> new zone: H:N ...`) and the
`oras-engine --zone-rows N` to pass to the engine mod.

Checked on the all6 recipe (s6's options + `--new-zones`, built locally 8 October): Twinleaf 411 -> 538, Route 201
342 -> 539, the four interiors 412/414/416/417 -> 540-543, 544 zone rows; the 19 remake tests pass. Known limits:

- An interior's other warps (a house's upstairs) still lead to their Hoenn zone.
- `oras-sandbox` also appends from 538: a mod cannot hold both yet (next step of stage 0: one appending order for both).
- Hoenn's scripts are kept in the copies (the zone's script reference is in its header).
