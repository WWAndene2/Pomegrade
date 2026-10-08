# Building Sinnoh in Omega Ruby — the progressive plan

The plan agreed with the owner (8 October) to rebuild all of Sinnoh in ORAS, stage by stage, once the tools of section 2 exist. Each building stage ends with a mod
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

## 2. Readiness: what must exist before building

No stage of section 3 starts until every tool below exists and is checked. Building earlier means borrowing Hoenn's
zones, scripts or warps, and every borrowed piece is a constraint to undo later (tried on 8 October with
`oras-region --new-zones`, which renumbered copies of Hoenn zones; dropped for that reason).

| # | Tool | Why it blocks | State |
|---|---|---|---|
| R1 | ORAS script writer (AMX assembler), checked by reading its output back with `oras-script` | Every zone runs a script when it is entered; signs, people, story need them | Scripts read and disassembled, 799 natives named; not written |
| R2 | Zone writer from nothing: header words, events file, scripts, all from Platinum data, no template zone | A copied zone carries Hoenn's scripts, warps and texts | Header words partly known (pack, matrix, number, name, spawn); the others to read |
| R3 | One zone-number allocator shared by every tool (region, sandbox) | Two tools appending from 538 clash in one mod | Each tool appends on its own |
| R4 | Matrices for Platinum's interiors and caves (its 288 other matrices), built as the main map is | Hoenn's houses lead to Hoenn | Main map only |
| R5 | Warps and events written only from Platinum's events | Retargeted Hoenn warps keep Hoenn destinations | Warps written on Hoenn zones only |
| R6 | Relief: ledges in four directions, slopes, cliffs, bridges from Platinum's heights | Sinnoh cannot be walked as designed | South ledges only |
| R7 | Writers for encounters, trainers, texts from Platinum's data; correspondence tables (Pokemon, sprites, items, classes) | Population, battles and dialogue | ORAS formats read; not converted |
| R8 | Headless regression check (save loads, field shows, a warp works) | Each phone mod must be known to start | Pieces exist (`run_local.sh`); no fixed check |

R1 to R3 come first: without them no zone of Sinnoh is free of Hoenn.

## 3. Building stages (after section 2)

1. **Main map**: the 372 pieces, collision, water, trees, paths, relief, on Sinnoh's own zones. Test: walk all of Sinnoh.
2. **Zones and connections**: one zone per header, names, warps between zones and matrices (interiors, caves, Mt.
   Coronet's floors), multi-storey buildings.
3. **Buildings**: Platinum's distinctive buildings (Hearthome, Veilstone, the League...).
4. **Population**: people, signs, items on the ground.
5. **Wild encounters**: the 183 tables converted.
6. **Trainers and battles**: teams, classes, gym leaders, rival.
7. **Dialogue**: bulk text import (French first).
8. **Story**: scripts translated from Platinum's or authored; flags, progression, cutscenes.
9. **Atmosphere**: light and camera per zone (tools ready), weather, music.
