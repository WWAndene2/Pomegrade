# Building Sinnoh in Omega Ruby — the progressive plan

**On hold (owner's decision, 9 October)**: only this remake is paused; the tooling of `tools/remake/` goes on being used and
extended for Delta Emerald (`Delta_Emerald_Development.md`). R4 stopped as section 4 says.

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
| R1 | ORAS script writer (AMX assembler) | Every zone runs a script when it is entered; signs, people, story need them | **Done (8 October)**: `amx-asm`, `oras-script ... source`, `oras-zone-script`, sandbox `script` / `init-script` (`Amx.h`, `OrasSandbox.h`). All 1,072 game scripts come back byte for byte; natives checked by name and by the zone's mask. **Checked in the game** (headless runs, below) |
| R2 | Zone writer from nothing: header words, events file, scripts, all from data, no template zone | A copied zone carries Hoenn's scripts, warps and texts | **Done (8 October)**: `OrasNewZone.h` (every header word documented from the game's accessors and a census of the 536 zones; the unread ones take Littleroot's and its house's values, written as numbers), used by `oras-sandbox` (`pack`, `music`, `line`; `template` removed). **Checked in the game** (run `r2house`: the demo's zone 539 shows its name and characters, the player walks into its house, zone 540, and back out) |
| R3 | One zone-number allocator shared by every tool (region, sandbox) | Two tools appending from 538 clash in one mod | **Done (9 October)**: `OrasWorkspace.h`, `oras-build` (workflow `build <steps file>`). Every tool edits the same archives, each number the next free one as the build stands; one mod. A merge could never join a region and a sandbox (both patch `a/0/1/3`), whatever the numbers. **Checked in the game** (runs `r3sand`, `r3twin`, `sandbox/region_and_sandbox.txt`): the sandbox zone 538 and the region's Twinleaf both load from one mod. The region still places Sinnoh on Hoenn's zones: writing them from nothing as R2 does is stage 1's work |
| R2b | The region's zones written from nothing (R2 applied to `oras-region`) | The region placed Sinnoh on Hoenn's zones (Twinleaf on Littleroot's zone 6) and added its textures to Hoenn's pack 8 | **Built (9 October)**: `oras-region --new-zones [--name H:TEXT]`: each header a new zone (538 up, shared numbering), its doors leading to new interiors on the named game interiors' maps (R4 replaces those maps), the pieces' textures in new area packs. Platinum's own place names wait for its text reader (R7): `--name` gives them meanwhile. **Checked in the game** (run `r2btwin`, the save moved to 538): Twinleaf loads as zone 538, named "Bonaugure", the field up; after 8 tiles east and 20 north the name shown is still Twinleaf's, so reaching Route 201 (539) and entering a house are not shown yet |
| R4 | Matrices for Platinum's interiors and caves, built as the main map is | Hoenn's houses lead to Hoenn | **Built, checked headless (9 October)**: `oras-region --matrix M --header H --new-zones`, the interior matrix written as the game's (no zone grid, first words 0 0), the room the tiles Platinum gives terrain (every other tile solid), every warp linked by header across the build (`OrasWorkspace::LinkWarps`: doors, mats, stairs); steps in `sandbox/twinleaf_interiors.txt`. Stairs as ORAS writes them (pieces 506, 507, 509): on a solid tile walked into, kind 0x0500 + the way walked (1 north, 2 east, 3 west), height +9 up and -9 down. Each interior piece named after its `a/0/3/9` member (seven rooms named alike drew a room's floor over Twinleaf). Rooms use Littleroot's area pack (8) with the room's textures added: a house's own pack (112) froze the piece loader. Shell: `RoomBuilder`, computed in ORAS's measures (t101r0101: walls 54 high with skirting, wallpaper and moulding, the dark cap one tile deep, the wood floor darkened along the walls, the south lip), the way out (floor one tile further out, mat, light) on Platinum's exit mat (building `d_mat01`), ORAS's window (opening, recess, sill, frame, glass) where Platinum has `window01`; both floors in the ground floor's materials; the stairs computed on Platinum's stairs tiles, running away from the warp (up: 6-high steps to the wall's top, as ORAS's go into the ceiling; down: an opening with the steps, dark sides and bottom, a railing); **no furniture** (the owner, 9 October: walls first). Platinum's objects are read with their model name and exact place (`TownLayout::Objects`), for the furniture to come. **Checked in the game**: into 417 and out (r4pack6, r4exit); 412 up to 413 and back, 414 up to 415 and back (rm4-rm7, wl3); 416 and 417 in and out (rm8, rm9); 412 and 413's shell (wl1-wl3), the stairs (st5-st7). **Not checked**: the phone. **Open**: Platinum's furniture tiles are solid in an empty room. Caves not built yet |
| R5 | Warps and events written only from Platinum's events | Retargeted Hoenn warps keep Hoenn destinations | Warps written on Hoenn zones only |
| R6 | Relief: ledges in four directions, slopes, cliffs, bridges from Platinum's heights | Sinnoh cannot be walked as designed | South ledges only |
| R7 | Writers for encounters, trainers, texts from Platinum's data; correspondence tables (Pokemon, sprites, items, classes) | Population, battles and dialogue | ORAS formats read; not converted |
| R8 | Headless regression check (save loads, field shows, a warp works) | Each phone mod must be known to start | Pieces exist (`run_local.sh`); no fixed check |
| R9 | Seamless joins where the place allows it (owner's request, 9 October): a cave with an open mouth, a gorge, a tunnel built on the same matrix as the outside, joined by ground instead of a warp | Walking between zones of one matrix already has no transition (Twinleaf to Route 201); only a warp to another matrix fades | Not started. First a headless test: whether the camera and light of each zone (its area pack's files 6 and 4) change when crossing zones without a warp |
| R10 | Faster transitions through the warps that stay (houses): a shorter fade, or none | Every house keeps its warp | Not started: where the warp's fade is set (its kind, or the game's code) is not read. Houses at real scale with no warp are left out: their rooms are 2-3 times larger than the houses outside, and ORAS cannot hide a roof around the player (owner's decision, not the house) |

R1 to R3 (and R2b) come first: without them no zone of Sinnoh is free of Hoenn.

**How a zone script is called (read and checked 8 October):** a script's `main` (the header's `cip`) runs with a command
in its public variable `g_mode` (`#D7477C97`; the other two every zone script declares are `g_interactive_flag` and
`g_ai_flag`, named by the game's own lookups, `Script_FindPublicVariable`). The initialisation script runs on entering
the zone with `g_mode` 2 (its scripts handle -1, 0, 1, 2); the zone script runs for an event with the event's script
number (zone 6's 2-19: its signs 16-19, triggers 10-12, people 9, 13-15; 2000 is none) and handles -1 and 1 as well. The
public function `#63F02D54` (118 zone scripts) is optional: `Zone_LoadZoneScript` skips it when absent. **Checked in the
game** with scripts of ours that record their calls (`sandbox/own_scripts.txt`): in Littleroot (runs `r1own`,
`r1sign`), the initialisation script ran once with 2, and reading the sign ran the zone script 3 times, last with 18; in
the new zone 538 (run `r1zone538`, the save moved there), the initialisation script ran once with 2 and the zone script
was loaded under mask 0x243 (no event in that zone to call it). The field stayed up and the main thread idle in every run.
What -1 and 1 mean is not read; empty handlers are enough (zone 80's are).

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


## 4. Next agent: finishing R4 (quick start)

**What is left of R4**: the furniture, placed from Platinum's objects (`TownLayout::Objects`: name, place) on the grid; the
caves (not tried); the phone test.

**Setup (one command, about 40 minutes the first time, all outside the repository):**

    export POMEGRADE_ORAS_DRIVE_ID=... POMEGRADE_PLATINUM_DRIVE_ID=... POMEGRADE_SAVE_DRIVE_ID=...   # ask the owner
    tools/remake/headless/session_setup.sh $WORK dumps tool core     # core-gl only for the Remaster's OpenGL path

**Build the R4 mod and walk into a house** (`headless/run_local.sh`, its header lists every script command):

    $WORK/build-remake/remake_tool oras-build $WORK/dumps/platinum.nds $WORK/dumps/oras.3ds $WORK/r4mod \
        tools/remake/sandbox/twinleaf_interiors.txt                      # prints the zone rows for oras-engine
    $WORK/build-remake/remake_tool oras-engine $WORK/dumps/oras.3ds $WORK/r4mod --zone-rows 546
    tools/remake/headless/fast_run.sh $WORK r4test $WORK/r4mod "538 105.5 877.5" \
        "hold up 40;wait 250;hold left 120;hold up 150;hold left 20;hold up 20;hold right 20;wait 400;shot up" 700

The save is moved two tiles south of house 412's door; the walk goes up its stairs (walked into eastward) to 413; `shot NAME`
writes runs/r4test/NAME.ppm. A run takes 3-10 minutes; read the log's `RESULT:` line and the shots. A run started from a
title state does not write the game's save file (the in-game save shows, the file stays as moved): tell rooms apart by their
look. Run only to check a finished change.

**Where things are**: the region and interior builder `src/OrasRegion.cpp` (`inside` = an interior matrix), warps
`src/OrasWorkspace.cpp` (`LinkWarps`) and `src/OrasNewZone.cpp` (door, exit and stairs kinds), the rooms' look
`src/RoomBuilder.cpp`, the steps
`sandbox/twinleaf_interiors.txt`, the rules and facts `ORAS_LITTLEROOT.md` (section 0: the phone test protocol) and
`ORAS_ENGINE.md` (zone rows, heaps). The graphics work (Remaster V1, path tracer) is separate: `render/ORAS_RENDER.md`.