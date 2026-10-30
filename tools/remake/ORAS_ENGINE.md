# Omega Ruby's engine, decomposed

The atlas of the game's code and data, built from the code itself (Ghidra's decompilation and the emulator's GDB stub on the
running game), not from guesses: what each part does, the tables it reads, the limits it enforces and how to lift them. The
goal (owner, 7 October): a sandbox in which Sinnoh and a story are built on a blank map, nothing tied to Hoenn. Each fact is
marked **checked** (seen in memory or under the debugger), **read** (from the decompiled code) or **inferred**.
`ORAS_LITTLEROOT.md` keeps the history of the Littleroot and Route 201 work; this file keeps the engine.


## 0. Start here (the state on 7 October, for whoever continues)

### 0.00 Handover of 28 October (read first; then 0.0)

The owner now writes French or English: answer in the language of the message. Short, simple answers; ask before a run
over 15 minutes; one run at a time, drawing off until the shot (`draw off` ... `draw; wait 8; shot`), 15 fps or more.

**Done 27-28 October** (all on `main` after this merge):
- **The whole of Kalos builds** into Omega Ruby: `oras-xy-world ... --matrices 0,1,...,399 --story off` takes X's 360
  zones (mod zones 538-897) and 241 matrices, no error; then `oras-engine` with the options it prints. 9 minutes.
- **Battle backgrounds unbounded** (`OrasXyBackgrounds.h`): X/Y's values have a bank of their own (n 1-127, records 256 +
  n of 384); an X/Y zone's header carries bit 14 of halfword 15 (unused by both games' zones), FUN_004D86B4 and
  FUN_0048AA44 patched (OrasEngine.cpp, cave 0x579FB0-0x579FF3). Whole world: 57 values in the bank. Checked headless:
  a wild battle in Santalune Forest (mod zone 824, tile 23.5 54.5) draws a forest background. Not covered:
  FUN_003DA1E8's background byte in an X/Y zone. Not checked on the phone.
- **Neutral Kalos** (`--story off`): besides the story's characters, triggers and init scripts, the 123 field-move
  obstacles (characters of X's hiden.amx, script ids 10000-10089, models 0x200C/0x200B/0x2009/0x2034) are left out.
  Which model is which move not checked; not checked in game.
- **Neutral Hoenn** (`oras-xy-world ... --hoenn-story off`, OrasHoennNeutral.h): Omega Ruby's own zones lose their
  story characters (talk or script 2000 with a vanish flag), field-move obstacles (scripts 10050/10052/10054), triggers
  and the scenes started on entering; init scripts kept. Built 29 October: 304 zones changed, 697 characters, 190
  obstacles, 361 triggers, 118 scenes. The script rewrite reassembles all 536 Hoenn scripts to the same bytes (over the
  header's size; the rest is padding). Not run in game yet; not tested on the phone.
- **Hoenn map** (`tools/remake/map/`, its README): the X/Y-style town map `hoenn_xy_map.png`, approved by the owner, and Kalos in the same style, `kalos_xy_map.png` (its east, north and south coasts invented).

**Phone test of the reference mod "kalosfull"** (28 October: the whole of Kalos, story off, music, gyms, dust boxes and
clouds; "things seem to work"), three issues the owner reported, to fix:
- **Tall grass drawn as flowers** (fixed 28 October, `oras-engine --xy-flower-grass <x.3ds>`, OrasXyEffectPack.h): the
  flowers are X/Y's own flower beds (tiles 0x21-0x23; X itself draws the same lavender on Route 5, run xr5b); walked on,
  they turned to plain grass because Omega Ruby's grass tables name grass00 for those kinds where X's name its grass01-03.
  Checked headless (runs r5s, r5fs, mod zone 539): with the option the lavender stays under the player, as in X. Not
  checked on the phone.
- **The "big block" behind the foe** (owner's shot, Route 5, mod zone 539; not a bug, owner, 28 October): X's scenery set
  model `btl_B_kusa` (X a/1/4/6 member 16, a grassy ledge), named by X/Y's background value 1 (Route 5's: ground X 3,
  backdrop X 26, scenery set 1). DllBackGround loads and places it as X does (read 28 October, both modules decompiled:
  Omega Ruby 0x100043E4 / X 0x10004340, the node at +0x60 given the transform at +0xBC, the same code): it sits at its
  own coordinates, and X shows it too (owner's X shots). Not compared: the foe's platform (X's shots a flat disc, the
  mod's shot the tall-grass ring), which may follow the tile rather than the zone (not read).
- **Story and event scripts still run** in the neutral build (owner's example: Route 5's champion and her two Lucario
  on every entry, from either side). Fixed 28 October: the scenes a zone starts by itself (its script's
  GetSceneChangeData table, read by Zone_LoadZoneScript; 55 of X's zones) are left out with `--story off`
  (OrasXyScript.cpp; checked at build: Route 5 and 7 note it; not seen in game, the scene needing an entry on foot).
  `--story off` also kept out the items on the ground (207) and 26 trainers, by their flags: now kept (run r5ib: Route
  5's ball drawn). The rest is kept (owner, 28 October: a map with no story keeps its champions, items, people,
  trainers, gifts, services and wild battles): `XY_NEUTRAL_SCRIPTS.md` lists what the zone scripts can still run, by a
  static reading (`prototype/XyZoneDump.cpp`, `prototype/xy_zone_cases.py`): 106 scenes, of which the only event-camera
  ones not a gym battle are gameplay (Shalour's gym, Prism Tower's lift, the Battle Chateau, the Elite Four's rooms), and
  20 step events (Parfum Palace's maze, Flare's doors...). Path blockers open without the story (owner): Vaniville's
  gate opens and lets the player through in a neutral build (run vg1, mod zone 538 from 460.5 605.5, walking north);
  the owner's blocked double door not found yet (which one, where, to ask).

**Session notes of 28 October** (session setup: `session_setup.sh <work> dumps tool core ghidra ghidra-x`, 35 minutes;
the environment's `PkmXY` and `PkmXYsave` hold share links: give the setup and the download the id inside them):
- **Rebuilding kalosfull** with the fix: the same `oras-engine` command plus `--xy-flower-grass dumps/x.3ds`
  (`headless/test_all_mod.sh` passes it).
- **One matrix is enough to test a Kalos place**: `oras-xy-world ... --matrices 2 --story off` (30 s) builds X's Route 5
  (X zone 262, mod zone 539) and Route 7 (X 264, mod 540); the zone numbers are in the log (`zone N (X/Y's Z)`). A tile to
  stand on: a piece's cell (its file 4: y, x) times 40, plus 20, plus its tuft or vertex position / 18.
- **X itself as the reference, run headless**: `prototype/xy_save_move.py dumps/xsave/main <out> ZONE X Z 0`, then
  `POMEGRADE_GAME=dumps/x.3ds POMEGRADE_SAVE=<out> run_local.sh <work> <name> - "" "wait 120;draw every 15;mash a 8;wait
  40;draw;wait 8;shot a"` (runs xr5b, xr5s: Route 5 in 4 minutes). The owner's X save shows a Player Search System
  message on its first steps: a `press a` between moves clears it (run xbt3).
- **No wild battle came headless** on Route 5, in the mod or in X (runs bp1-bp4, xbt-xbt3: 60-80 steps in the lavender,
  walking `hold left 16;hold right 16` with drawing off or every 15). Not explained (the save's repel, the lead's ability
  or the walk itself not checked); a battle to look at needs a forced one (the horde check of 23 October raised the
  zone's chance to 100 in a diagnostic variant).
- **Reading a whole module of both games**: `ghidra/decompile.sh` with every function start of the module's range from
  `<work>/functions.tsv` (or `<work>/x/functions.tsv`, `POMEGRADE_GHIDRA_GAME=x`), the module's linked base from
  `linked/modules.tsv` / `xlinked/modules.tsv` (DllBackGround: 140 and 135 functions, a minute each). A table's readers:
  its linked address searched as a word in `linked/modules.bin` (the literal pools; the file starts at 0x10000000).
- **DllField's read-only segment** (file 0xF1000 on, its tables) moves in the file by `--field-room`: a word patched
  there is at its linked address + the room, and is added after every cave (`PatchCro` takes it; OrasXyEffectPack.h).

**Fly between the regions** (owner, 28 October: one fly screen, L/R switching Hoenn and Kalos, the maps
`map/hoenn_xy_map.png` and `map/kalos_xy_map.png` as its backgrounds). Read (static) and checked:
- A fly is a zone number: the town map hands it to the party menu (0x3EF8CC `ldrh r5, [r5, #2]`, kept at the fly event's
  +0x40), the event (vtable 0x5DEE48) warps with Event_StartCharacterMove kind 2 and FUN_00403F40, which lands in the zone
  of the header's word 21 bits 5-14 at its spawn (words 22-24). New zones named zone 6 there: fixed (`NewZoneHeader`);
  checked by run fly3 (a diagnostic copy of the mod whose 0x3EF8CC was `mov r5, #540`: Fly from the menu, Littleroot
  picked on the map, the player landed on X's Route 7). The save: `prototype/oras_save.py move <save> <out> 0 3 19`
  (Fly) then `badges ... 0xFF`.
- The places' table (`.code` 0x5862E0, 78 entries of 10 bytes: visited flag, attributes (bit 1 a fly point), the
  Pokémon Center's zone and tile, the town's zone; bounded by 0x4E in 6 functions) also gives the last healing place
  (FUN_003EBE7C, Teleport and blackout). X's (0x544E4C, 19 entries of 20 bytes, its first 10 as Omega Ruby's) holds
  Kalos's 19 places, now given after Omega Ruby's 78 (`oras-engine --xy-places`, OrasXyPlaces.h; whole Kalos: 98
  entries, the table at 0x6AFCD8). Checked: Fort-Vanitas's flag 0x17AA set on leaving its Pokémon Center (run fv4, a
  full boot: run_local.sh; a save made through fast_run.sh's title state is not written, runs fv1-fv3). Left: the fly
  screen: read 28 October (a RomFS read trace of the screen opening, run fm1): its files are a/0/6/7 member 0 (a header,
  then a DARC at 0x5900: the layout map_lower.bclyt, the image poke_navi_map.bclim) and its places a/0/6/6 (168 records
  of 60 bytes: zone, text lines, the layout's panes, the place's flag 0xAC6..., its fly place). Hoenn itself is the
  image (ETC1, 512 x 512 stored, the map in the bottom-left 380 x 270, upside down as PicaTextureDecode gives it); the
  layout's pane "s" holds one bounding pane per place (where the cursor names it) with three pictures (the square or
  road shown, lit, dimmed). Done (owner: test maps, the simplest way): `map/fly_background.py <drawing> <out.rgba>` then
  `oras-engine --fly-map-hoenn <out.rgba> [--fly-map-kalos <out.rgba>]` (OrasFlyMap.h) makes the drawings the
  background and the places' pictures transparent; checked, run fm6: `hoenn_xy_map.png` drawn the right way up. Not
  matched: the bounding panes stay where Omega Ruby's map had its places, so the cursor names a place there, not on the
  drawing's towns. **L/R (28 October)**: both maps in the one texture (Hoenn's 256 rows, then Kalos's, the image
  declared 380 x 256); DllTownmap's FUN_107F5434 (the screen's update, every frame) branches to a cave (the module's
  code grown by a page) that, on L or R (FUN_004C84F0 on the input manager's +0x24 button device; the game's masks L
  0x100, R 0x200), moves the picture "0"'s texture address by 0x10001 (pane id 0 of the layout at this + 0xA4,
  material +0x13C, texture slot +0x34, address +4). Read in the game's memory (runs lr1-lr7): the texture slot's address
  is read every frame (a poke changes the picture at once), the picture's texture coordinates and its material's
  texture SRT are not; the manager's +0xC device is a touch one. Checked headless, run lr7: L shows Kalos, R Hoenn, R
  Kalos. **Hoenn's places on the drawing (28 October)**: `--fly-places-hoenn map/hoenn_xy_places.txt`
  (OrasFlyMap.h). The cursor names the place whose box it is on: a bnd1 under "s" (named by its id in base 36), named
  by a/0/6/6 records (60 bytes: u16 0 the zone, u16 2 the box, u16 4 a town's own name, u16 8 the flight, u16 10 the
  flag that makes it shown and flown to, u16 16 the description). Without a town name the name is the zone's place
  name (header halfword 14 bits 0-9, a line of a/0/7/4 member 90; the mod's Kalos names are its appended lines 356 on).
  Each point of the drawing, read from `hoenn_xy_draw.py` (17 towns, 17 DexNav places, 10 added from the tiles), has
  its box moved onto it (12 x 12); the routes and Cave of Origin, without a point, lose theirs. The 7 places Omega
  Ruby's screen lacks are new records (the game takes their count from the file). Checked headless (run pl1):
  "Lavandia" on Mauville's point, "Ruines Désert" on the desert's. **Kalos's places (28 October)**:
  `--fly-places-kalos map/kalos_xy_places.txt`. Its points are X's own town map icons as `kalos_xy_draw.py` draws them
  (scale 6, exact once turned upside down, the owner's choice): the 17 towns named by X's a/0/6/8 records (64 bytes,
  u32 2 the pane in X's townmap_lower, on the icon within 2 px), the 14 places by the roads to them (inferred, 8 of
  them). Each is a new record (a copy of a route record: a copied town record took Littleroot's name, run pk3) with its
  zone, its flight (Kalos's entry n of the places table, flagged by X's flag + 0xD00, is 79 + n) and that flag. L/R also
  moves the boxes and switches the records (the cave, FlySwitch): Kalos takes the 28 boxes the routes left, then 3 of
  Hoenn's places'. A record's zone is read when the screen opens (zones swapped at L/R showed zone 0's name, run pk4),
  so it is kept; the record's box is what switches (Hidden, 0 x 0, off its map). The screen opens on the player's
  place: the mark kept at the cursor's object (this + 0xA4) +0x90; on a box only Kalos shows, the cave shows Kalos at
  once. Read: the screen took the L/R press for itself (the cursor went to the player's place), so the cave clears it.
  Checked headless (runs pk6-pk9): the screen opens on Kalos at Camphrier with the player there, on Hoenn at
  Littleroot after flying there; L/R switch; "Grotte Étincelante", "Fort-Vanitas", "Illumis" named on their points;
  flown Camphrier to Littleroot and Camphrier to Lumiose (its flag set in the save: an unvisited place is named but not
  flown to, as Omega Ruby's). Known: Kalos's places have no description (line 0); the player's mark stays where it was
  put when L/R changes the map; Scorched Slab named by zones
  164-168 ("Grotte Zénith", inferred); the "you are here" mark on a route goes to the hidden box. **The field's
  Navi-Map (29 October)**: the same texture, in a layout DllField's FUN_102B5164 updates (the player's mark and the
  map's scroll: its object +0xC +0xF0 the player's zone, +0x10 the view, whose +0xA8 is the layout; found from the
  texture's address in memory, run nv1). Its entry branches to a cave in DllField's room that shows Kalos's half when
  the zone is the first Kalos zone of the places file (538) or above (`--fly-places-kalos`). Checked headless (run
  nv4): Kalos at Camphrier, Hoenn after flying to Littleroot. A Hoenn zone added past 538 (a sandbox) would show Kalos.

**Next, in the owner's order:**
1. **Both maps in the menu** (owner: Hoenn and Kalos in the town map, then Fly between them). Static reading started:
   the two games' DllTownmap modules differ (X's 24 KB with TownmapData, Omega Ruby's 80 KB PokéNav Plus); X's art is
   `a/0/6/9` member 0, Omega Ruby's `a/0/6/7` member 0 (map README). Left to read: each module's zone -> map position /
   Fly point table, how the menu opens the module (to offer Hoenn or Kalos), then port X's module beside Omega Ruby's.
   Fly between the regions is not ported.
2. **Load test of every Kalos zone**, in batches, each asked first (step 5 of DEVELOPMENT_NOTE 3e).
3. Neutral Hoenn (its story and field-move obstacles out); the save that reloads at the zone's start point (X's
   player house); X/Y's projection maps (owner's decision pending); footsteps with the sounds (step 2).
4. Not explained: black stripes on the battle menu's lower screen in the Santalune shot (likely the first frames after
   `draw`; not checked).

### 0.0 Handover of 23 October (read this first, then 0.1 for the rules and the history)

**The project**: Pomegrade's remake tooling merges Pokemon X/Y into Omega Ruby ("Equilibrium"): one game with both maps,
built as a mod (LayeredFS: romfs files, BPS patches, a `.code` IPS) by `remake_tool` (C++, `tools/remake/src`). The
owner's rules for the merge are in `CLAUDE.md` section 8 (**100% identical = merged, not identical = ported; X/Y is
ported whole**) and, since 23 October, **everything of X/Y must work in Omega Ruby and everything of Omega Ruby in X/Y's
zones** (features X/Y's own content never calls included). The owner writes French: answer in French.

**How to work** (the owner's demands, 23 October, after a day lost to blind runs): read and compare both games' code
statically first (Ghidra decompilation of both, below); a run only confirms, once, with a control; one change per run;
report after about 30 minutes without a result; work only when asked, one point at a time; commit and push each verified
step; ask before a run over 15 minutes and before any behaviour or scope change; say what is not verified. Never commit or
publish the owner's game dumps. Merge into `main` only when asked (fast-forward from the branch).

**Start a session** (dumps, tools, Ghidra programs of both games: about 45 minutes the first time, one step at a time):
`tools/remake/headless/session_setup.sh <work dir>` (its steps: section "Setting up a session" below). The work dir of
23 October was `/tmp/claude-0/work` (not kept by a new container). **25 October** (35 minutes, `session_setup.sh <work>
dumps tool core ghidra ghidra-x`): the environment's `PkmORAS` / `PkmYX` hold share links, not ids: give the setup
the id inside (`.../file/d/<id>/view`) as POMEGRADE_ORAS_DRIVE_ID / POMEGRADE_X_DRIVE_ID, else Drive answers a 1.6 KB
404 page and the tool step stops ("not a 3DS game image"). The owner's X save is now the Drive file "Pkm XY save -
main" (id 1cTLQR0Gu4EQBnj1pKjm7d4rcUpTNUPK4, 25 October), into `<work>/dumps/xsave/main`. Checked that day: the 26
remake tests and `cro_dis_test.py` pass, the phone test mod builds (3 min) and boots to its hub (run boot3: the
title, Continue, the field). Speed: drawing every frame gives about 2.5 frames/s on the field, `draw every 15` after
the Continue menu 20.6 (retro_host.cpp's measures); the title and the menu need drawing (`mash a` there first).

**The plan and its state**: `docs/DEVELOPMENT_NOTE.md` 3e, "State of the port, 23 October": what is left, in working
order (steps 1-9). **Step 1 (wild battles) is in progress.** Done in step 1: X/Y's scripted wild battles
(`CallWildBattle` 59-71); Sweet Scent's battle (needs nothing). Deferred: Sweet Scent's refusal by weather (goes with
X/Y's weather, step 6). **Read (23 October, owner-approved phase A; static, nothing run), the plan B1 then B2 approved**: X/Y's dust box
(trash cans) and dust cloud, everything they touch, X's address -> Omega Ruby's (`prototype/xy_import_match.py`, read
where it found only a copy):
- **dust box**: no zone script calls its natives; one shared script does, X's `a/0/3/1` member 39 (the three natives'
  hashes found in X's image by bytes, placed by `gamefs.py where`): it must be ported too (`oras-xy-world` translates zone
  scripts only). The table: X's game data + 0x18C, 0x13C bytes (allocated by 0x109208, initialised by 0x112A48: 10 entries
  of 0x1C, byte +0 the state 2, then flags +0x118-0x11B, 8 words from +0x11C at -1); Omega Ruby's game data has no such
  member: the table goes into the room. Filled at each zone change: X's 0x3B843C (Omega Ruby's `Zone_ChangeLoadStep`
  0x3D5698, 98%) calls the reset 0x3B513C (0xCC bytes, reads the date 0x479B84 = Omega Ruby's 0x4AA6E0) then the filler
  0x3B4C14 (0x3EC bytes, its helper 0x3B5000; save flags 0x140170 = 0x12FD48, 0x3E3250 = 0x406B30, 0x45A908 two
  candidates 0x48AD84 95% / 93%). The flags: X's 0x3CEB48 (Omega Ruby's 0x3F0AD0, 86%: without those lines) sets +0x11A,
  +0x11B and +0x118 (when 0x479B84 gives 4). Searched cans hidden: X's 0x1027D3BC (Omega Ruby's
  `Map_PlacePropsFromContainer` 0x102607A4, 95%) calls 0x3B4BC0 for props of ids 0xC50-0xCE5 (a save flag, the prop's
  model hidden by 0x19AB0C: no match, to read). Natives (X DllField): `CallDustBoxEncount` 0x1028CFD4 (entry n: +4
  species, +8 level, +0xC; the battle by X's 0x1029A650, 72%: to read; then 0x448E08 = 0x478964 and 0x3DEA54 =
  0x3FFF30), `CheckTypeOfDustBox` 0x1028D7EC (entry n's byte: 0, 1, else 2), `GetItemIdOfDustBox` 0x1028D8F0 (entry n's
  +0x14). Not saved (inferred: game data, not the save; the searched flags are the save's);
  **B1's reading (23 October, static)**: the filler draws per zone change, from X's tables (X .code 0x5432A4: 10 spots
  of 12 bytes, the entry, its save flag 0xC50-0xC59, its group), 40% an item (0x544B6C: 4 groups of 7 items with
  cumulative percents), else a Pokemon (0x5449EC: 4 groups of 6, species, form, level, cumulative percent: 0 Garbodor
  46-50, 1 Banette 46-50, 2 Trubbish/Garbodor 34-38, 3 Rotom's 6 forms 38). The entry is +4 species, **+8 form, +0xC
  level** (the phase A line above said +8 level: wrong). The random 0x45A908 is Omega Ruby's 0x48AD84 (read side by
  side; the 93% one is the next function); 0x11F034 = 0x11FA10, 0x1765F0 = 0x177300 (same). The battle: X's
  0x1029A650 is X's scripted wild battle 0x1029A7DC (native 0x1028FEA4) with situation byte 0x15 instead of 0x11;
  Omega Ruby's `CallWildBattle` takes a table entry (0x102CC01C), not a species: X's setter 0x1026DE30 has no copy,
  so it is ported. X's encounter context is Omega Ruby's with one word more at +0x78 (X's InitContext 0x1026E434 and
  Omega Ruby's 0x102BFDD4 read side by side: X's +0x7C-0xA4 are Omega Ruby's +0x78-0xA0): the setter's +0x8E becomes
  +0x8A, its +0x98/+0xA4 tests +0x94/+0xA0; the start 0x3BCB44 is Omega Ruby's `Field_StartEncounterEvent` 0x3DADE8
  (one argument more, 0). Not read: what situation 0x15 does in Omega Ruby's battle setup; the model hide: 0x19AB0C is
  a `|static|` import stub (its word 0x19AB10 written by the loader), to resolve through static.crs; X's flags
  0xC50-0xC59 against Omega Ruby's use of them.
  **B1, the rest read (23 October, static)**: the situation byte is the **battle type**: X's context +0x8E, Omega
  Ruby's +0x8A (X's 0x102DD9C4 and Omega Ruby's 0x103279DC, the same function, read it), so 0x15 is the mod's X/Y type
  73 + 0x15 (`--xy-battle-types`). The battle's first word: X's `[field+0x48]+0xF4` is Omega Ruby's `+0x104` (both Rock
  Smash natives, X 0x102909DC and Omega Ruby 0x1029DB40); X's getter 0x4A4000 is Omega Ruby's 0x4DBE1C (`ldr r0,
  [r0, #0x148]`); InitContext's last argument `[field+0x2C]+0x34` is Omega Ruby's `+0x1C` (Omega Ruby's 0x102CC01C).
  The native's diary line (X 0x448E08 = Omega Ruby 0x478964, `PokeDiarySet`'s, with X's diary event 0x204 and the zone
  at game data + 0x124): Omega Ruby's event 0x204 is not read, so not ported yet (the owner's call). The `|static|`
  stubs: 0x19AB08 is X's DllField 0x102D030C = Omega Ruby's 0x1031BEAC (the prop of an id in a prop list), 0x19AB10
  X's 0x102141AC = Omega Ruby's 0x10257E34 (plays the prop's animation slot n + 3: the searched can's). The prop hook:
  X's call sits after each prop's placement in X's 0x1027D3BC (`id - 0xC50 < 0x96`); Omega Ruby's loop (0x102607A4)
  keeps the prop list in r8 (X's map + 0x168, Omega Ruby's + 0x170) and X's map + 0x50 is Omega Ruby's + 0x60 (X's
  0x1029E1F8 and Omega Ruby's 0x102DF4E0, the same function), the game data at its + 0x1C. Event work: game data's
  first word + 0x13140 (X's + 0x13148), the date + 0xEB0 (X's + 0xEA8; both from X's 0x3CEB48 and Omega Ruby's
  0x3F0AD0); X's date constructor 0x13A158 is Omega Ruby's 0x139404 (the call before every date read). Hooks: the zone
  change at 0x3D5858 (`ldrh r0, [sb, #4]`, after 0x3F0FBC = X's 0x3CF184, where X calls the reset and the filler),
  the new day at 0x3F0C34 (`mov r0, r0` inside 0x3F0AD0's new-day block, where X sets +0x118-+0x11B). X's filler, helper
  and reset are one block, X .code 0x3B4C14-0x3B5208 (7 calls to map, 8 literal words to X's tables, the offsets
  +0x13148 and +0xEA8). X's flags 0xC50-0xC59 become 0x1950-0x1959 (`XyNumbersFor`'s rule; `--story large`).
  **The shared script**: Omega Ruby runs a script id through 86 ranges (.code 0x587678, 16 bytes each: first id, last,
  the member of a/0/2/9 (archive 0x1D), the text kind, the text file; read by `Msg_ResolveTextSource` 0x3FB230, bound
  0x56), X through its own (X .code 0x5457A4, read by X's 0x3D9C64): member 39 is X's ids **10480-10529** (text file
  142), which in Omega Ruby are its own (rows 25-29: members 27, 56, 52, 83). The port gives it a free range (none past
  12109 but 20000-20010) and moves the ids X/Y's zones name.
  **B1 written (23 October; built and checked offline, not run)**: `oras-engine --xy-dust-box <x.3ds>` (OrasXyDustBox.h:
  the table, X's block and the hooks in the room, the prop routine in DllField's cave, the 3 natives) and X/Y's shared
  scripts (OrasXySharedScripts.h): X/Y's ids + 30000 (32000-50010; 20011-29999 and 50011-59999 free for new scripts),
  the script appended to a/0/2/9 (objectencount.amx: member 85, ids 40480-40529, text file 175), the ids in furniture
  word 0, character word 5 and trigger word 0 (the words holding ids of 2000 and up in X's 361 zones; only zones 318 and
  349 name 10480-10489, the cans), `--xy-shared-scripts` copying the 86 rows, 96 masks and the names into the room.
  objectencount.amx dispatches on its ids (`casetbl`, moved) and computes a can's flag as spot + 0xC50, also the can
  prop's id: only FlagGet/FlagReset's argument moves. **Then (23 October)**: the diary line ported as is (Omega Ruby's
  list of events 0x10B-0x242 is read only by the save, 0x444FF4 and DllGift: Game Sync's upload, shown by nothing in the
  game; the zone at game data + 0x124 in both games); the ids given GlobalCall, ReserveScript, WeakReserveScript and the
  AI natives moved (`XyMoveScriptIds`, unpacked past 16 bits); every X/Y shared script but trainer.amx ported (64 rows,
  55 members appended, 3 merged: OrasXySharedScripts.h; a row's text kind 1 is the story text, kind 0 the system text;
  X's member 41 is a plain Pawn image whose first byte is LZ10's mark). Not moved: computed flags, works and zones (75,
  reported). **29 October**: fld_item.amx and hide_item.amx read their item at (id - 7000) x 3 and (id - 8000) x 3 of
  their data; their ids moved by 30000, picking up an item read past the table and hung the game (owner's phone,
  Shalour; runs it9-it13): the first a script takes off its id now moves with it (`add.c -37000`). A moved save shows a
  zone's items only with `oras-save ... --characters-from` (on Continue the game rebuilds people from the save). **Run (23 October, runs db1-db2, sandbox/xy_dust_box.txt: X's zone 318, Pokemon Village, as mod zone
  538, warped to from Hoenn sandbox zone 539)**: the room's code runs in Azahar (the table filled at the save's load,
  in zone 539: the new-day stub then the filler, entries 0 and 5 drawn); spot 0's can gives X's item path ("Une
  silhouette vient de disparaître sous vos yeux ! ... tombé au fond de la poubelle", the Pretty Wing obtained, then
  "Une poubelle !" once empty) and, its entry poked to Garbodor level 46, X's battle ("Oh ! Un Miasmax sauvage surgit de
  la poubelle", the command menu). Fixed by the run: the script id rows are searched in order for the first whose first
  id is at most the id (run db1: the fatal error 0x11EF4C from 0x3FB290 with the rows after the game's): X/Y's go
  first. **Checked 26 October (headless)**: X itself run in its Pokemon Village (run xv3, the owner's X save moved by
  `prototype/xy_save_move.py`, `POMEGRADE_GAME=dumps/x.3ds` relative, as `run_local.sh` names X's save folder by it): its
  can of spot 0 stands in the flower field on tile (37, 59), behind the player seen from (37, 60), as in the mod; a search
  opens its lid, closed again after, the same in the mod (runs sc2, xv3). The prop routine: X gives the animation call
  s0 = 1.0 (`vldr` at X's 0x3B4C04), the cave did not (fixed; no difference seen, runs sc9, sc10); at the zone load X's
  reset clears the spots' flags, the filler sets the drawn spots' (spots 0 and 5: run sc8, `readers` under the
  interpreter), then the routine reads them. The weekday list: the new-day stub is X's 0x3CEB48's lines (0x11A, 0x11B,
  weekday 4 then 2), its three calls the same code in both games, the date at + 0xEB0 as Omega Ruby's 0x3F0AD0 reads it
  (static). The diary: a search writes one record through `PokeDiarySet` (0x478BA8-0x478BCC, save + 0x1C2B8 + 0x1E4:
  zone 555, species 569, the date; its event field 0xDB 0x04 not decoded, so not compared with X's 0x204; run dy1,
  `writers`). A can in a Hoenn zone: `dust-box X Z spot N` (OrasSandbox.h), the hub's can of spot 9 on (19, 17) drawn, its
  script answering "Une poubelle !" (runs hc2, hc3); with spot 9's flag set after the zone's load (the reset clears it:
  0x1959, byte 0x928CAAB bit 1 in that run) and its entries poked to Garbodor level 46, a search opens the can's lid,
  then the battle (Littleroot's background) plays to its end, the loss taking the player home (runs hc5, hc6). Not
  checked: the phone;
- **dust cloud**: no script imports `CallTsuchikemuriEncountEvent` (X 0x102914A0; ported anyway, owner's rule): the step
  does, X's 0x102265DC (0x5AC bytes, reached from 0x1022711C, 0x10227324; Omega Ruby's 0x10263014, 97%: the dust-cloud
  branch to add) creates `field::EventTsuchikemuriEncount` (0x34 bytes, vtable X 0x102FEF4C: 6 methods, about 350
  instructions, the update 0x10239704 257) as the native does. Its update calls about 25 functions: 15 found, 3 copies
  at 100% to confirm, 7 X DllField functions with no counterpart (0x10250CC4-0x10250D68, carried with it), and X's Route
  13 module `DllUSR13` (0x107AF818: marks a dust-cloud spot used in the module's list, 6-byte entries at +0x18): the
  module is ported with the gym modules' tooling (27 imports: 8 same, 4 near, 2 several, 11 none, 2 data). A zone has
  dust clouds when it is given that module's area kind (Hoenn's too). Not read: whether `DllUSR13` loads its spots from
  an archive (B2's first 15 minutes).
  **B2 read further (23 October, static)**: the event's vtable X 0x102FEF4C holds 0x10239B54 (destructor), 0x10239B0C,
  0x10239668, 0x1023969C (start: allocates the 0x84-byte cloud effect, X's 0x10250D68, on the field's heap at
  [field+0x48]+0x10, starts it with 0x10250CC4, stops the player [field+0x48]+0x158's method 0xC, keeps
  [field+0x48]+0x198, `DllUSR13`'s object), 0x10239704 (update, 12 states: the effect runs (0x10250D1C/D00/D54/CE0/D34),
  `DllUSR13`'s 0x107AF818 marks the spot used (its list at +0x18, 6-byte entries: +2 the spot, +4 cleared; count +0x6C),
  then the battle X 0x1029B1E4: Rock Smash's shape with table kind 0x16 and X's start 0x3BC140(..., 3), the player
  object's checks (0x4A4734 bits 8 and 0x1000000), a fade, an object of 0x4C bytes (vtable X 0x10239AE4) on the
  field's heap with 0x1CE040(0x16, ...)), 0x10239670 (frees the effect). The effect (0x10250D68) builds a 0x3C-byte
  model object (X 0x1027EDAC(heap, ..., 4, 0, 8)) from a resource named by X's data 0x10250DEC: X's effect files,
  to port with it. So B2 is the module, the event (6 methods), the effect class (7 functions) and its files, the
  encounter of table kind 0x16 (X/Y's encounter file: its table for the cloud, OrasXyEncounters.h), and about 12
  X field offsets to match to Omega Ruby's ([field+0x48] +0x10, +0xF4 (= +0x104), +0x158, +0x160, +0x198;
  field +0x18, +0x1C (+0x188, +0x20), +0x2C (+0x34 = +0x1C), +0x54): more than phase A's 4-5 hours, about two
  sessions. The [field+0x48] object's offsets, X -> Omega Ruby (23 October: the counts of loads after `ldr rX, [rY,
  #0x48]` in both DllFields, X's / Omega Ruby's: 0xF0 54 -> 0x100 64, 0xFC 27 -> 0x10C 31, 0x158 33 -> 0x164 33,
  0x168 12 -> 0x170 12, 0x194 14 -> 0x19C 14, 0x198 18 -> 0x1A0 13; 0x158 -> 0x164 checked on the same instruction of
  the step check, X 0x10226600 / Omega Ruby 0x1026303C): 0x160 (2 loads) is 0x168 or 0x16C, not settled (X's
  0x1027F5AC, which takes it, has no copy found in Omega Ruby's DllField); 0x10 not read. X's battle 0x1029B1E4 is
  Omega Ruby's `Field_WildEncounter_StartRockSmash` 0x102D9F04 (95%);
  **B2 read further (23 October, static, second part)**: X's start 0x3BC140(heap, field, context, 3) is Omega Ruby's
  `Field_StartEncounterEvent` 0x3DADE8(heap, field, context, 3, 0): both build the encounter event (X 0x26C bytes, Omega
  Ruby 0x290) and keep their fourth argument at the event's +0x1C as flags, read alike (bit 0 the event's +0x29, both
  games' 0x3BC520 / 0x3DA1E8; bit 1 at X 0x3BCC70 / 0x3BCD98 = Omega Ruby 0x3DAFC0 / 0x3DB104, the same tests); Omega
  Ruby's added bits (8, 0x40, 0x80, 0x100: the context's +0x7C) are not in 3. So the battle is Rock Smash's with X's
  table 4 (the mod's kind 12, OrasXyEncounters.h), battle type 95 and flags 3. The code the event reaches (its call
  graph, X's edges): 109 functions, 30 of X's DllField (the event, the effect, the effect's update 0x10277324 called by
  it alone, and the helpers it shares with X's other field effects: 0x1027E618, 0x1027E964, 0x1027ED9C, 0x1027EDAC,
  0x10277D18, 0x10277E64, which Omega Ruby keeps in its .code near 0x452A00, 0x4525BC, 0x452E7C, 0x4521E4: leads by
  shape, not read) and 79 of the .code (44 the same function in Omega Ruby; X's 0x3E018C / 0x3E0194 (a byte at +0x70)
  and 0x4AA5C8 (a half at +0xC) getters with none; 0x14F220, 0x1D4744 empty; 0x11E864 the fatal error). To port:
  X's DllField part copied whole (its own calls inside it, its .code calls mapped), not matched to Omega Ruby's helpers.
  **B2's map, X -> Omega Ruby (23 October, static; "same": prototype/xy_import_match.py; "read": side by side)**:
  - the effect's resource class (X DllField 0x1027EDAC, 0x3C bytes) is Omega Ruby's .code class of 0x40 bytes (its
    constructor 0x452E7C, one word added at +0x2C, the fields past it 4 further: read): X's 0x1027E618, 0x1027E964,
    0x10277D18, 0x1027EF88 = 0x452A00, 0x4525BC, 0x4521E4, 0x452EE8 (the same code but those offsets, read with
    offdiff), X's 0x10277E64 = 0x4CE280 (Omega Ruby's effects call it in its place, 0x10272094: read); X's 0x1027ED9C
    (sets +4, +8, which do not move) has no copy (inlined): copied;
  - the 0x4C-byte object of the event's state 6 is Omega Ruby's 0x48-byte one (vtable 0x10349664, constructor
    0x1026BCD0, the same six methods with X's +0x18 byte left out and the fields past it 4 nearer: read; X's 0x3BF770 =
    Omega Ruby's 0x3DDCDC build it alike), its six words from the battle type table (X 0x1CE040 = 0x1DBCD8, X/Y's type
    0x16 the mod's 95 in the longer table, --xy-battle-types: X's entry 7, 6, 0, 9, music 0x10018, 0x1001B);
  - the field object [field+0x48]: +0x10 and +0x24 kept, +0x2C -> +0x38 (X 0x10241038 = 0x1027A6E4), +0x160 -> +0x168
    and +0x15C -> +0x16C (the two 0x48-byte objects built in turn, X 0x10245D00 = 0x1027F7CC); X's 0x1027F5CC (its +0x50
    chain) = 0x102CDFD0 (called at the same place, X 0x1021B580 = 0x1025E270);
  - the game system [field+0x1C]: +0x188 -> +0x184 (the weather: X's InitContext 0x1026E554 = 0x102BFF3C), the records
    [+0] + 0x1B814 -> + 0x1C8D4, record 0x77 (119, Sweet Scent encounters, X/Y and Omega Ruby's one list: PKHeX
    Records.cs RecordList_6) kept;
  - .code: 0x11E46C = 0x11EB78, 0x14F220 = 0x14EA64 (both empty), 0x1D4744 = 0x1E8294, 0x2FF520 = 0x2FF8C0, 0x359404 =
    0x371C04, 0x36FD78 = 0x3878F8, 0x377E38 = 0x38F934, 0x37E398 = 0x395C80 (read side by side, or the thunk Omega
    Ruby calls at the same place: 0x3D4564 = 0x3F57B8);
  - the encounter: X's 0x1029B1E4 written as Omega Ruby's Rock Smash (InitContext kind 12, SelectTable 95,
    Field_StartEncounterEvent flags 3) with X's record increment;
  - not ported now: X's weather byte (+0x70 set to 3 when set, X's 0x3E018C / 0x3E0194): X/Y's weather is step 6, and
    Omega Ruby's weather object has no such byte read (no such getter, inlined or not found);
  - field +0x54 kept (Omega Ruby's Script_TalkSequence_Update 0x3DC120 gives it to 0x4E435C as X's 0x3BDD00 to
    0x4AB4E0); X's 0x4AA5C8 (a character's +0xC, its id) = 0x4E3234 (the step check's calls side by side, X 0x102265DC /
    Omega Ruby 0x10263014: 55 calls, each pair mapped);
  - **how a dust cloud is found (read)**: a dust cloud is a character of the zone, of model 0x38, 0x39 or 0x3A (X/Y's
    numbers). DllUSR13 (X's Route 13 module, 12 KB) lists them from the zone's character records ([[[[map+0x50]+0x1C]
    +0x1C], count +4, 0x30-byte records from +0x10, the model at +4; at most 12: 0x107AF3E8), shows 9 at random the first
    time (a byte the save keeps, event work + 0x12E4 kind 0x11: 0x107AF898), creates their characters (X 0x3D41D8 ~
    0x3F54E8) and moves one away when it is used (0x107AF818 then 0x107AF664: a random spot 3 tiles or more from the
    player). The step check (X 0x102265DC) creates the event (0x34 bytes, its +0x1C the character) when the character the
    player walks into is of those models: X's branch at 0x10226878, which Omega Ruby's 0x10263014 lacks between
    0x102632FC and 0x10263310 (its `ldr r0, [fp, #0x1c]` at 0x10263304 the place of the hook). X/Y's characters keep
    their models through the zone port (OrasXyEvents.cpp, modelOf: X/Y's a/0/2/1 member appended), so the module's and
    the hook's models are the mod's numbers of X/Y's 0x38-0x3A, given at build time;
  - **ported (24 October, OrasXyDustCloud.h, oras-engine --xy-dust-cloud on)** and **run headless** (runs dc1-dcj, X's
    zone 272 alone, `--matrices 31`, as mod zone 538): DllUSR13 lists the zone's 11 spots and shows 8-9; the player
    walking into one starts the event; the cloud's character goes, X's swirl plays (X's a/1/7/5 as a/3/3/6), the
    field is torn down and the battle starts (a wild Dugtrio). Fixed by the runs: the room's stubs written in another
    order than placed (the array constructor called the destructor, dc2-dc3); the character manager's map + 0x14 -> +
    0x10 (read live: its words 4 nearer from + 0x10, the module read garbage, dcd); Omega Ruby's encounter event pushed
    at once tore the field down under the effect (X's 0x3BC140 does not push it; X's update does at its state 9: dce ->
    dcg). Found by the runs, not fixed (the owner's call, below): **the battle's command menu takes no input** with X's
    type 0x16 whole: its word 3 is 9 (X's special wild types 17-22 hold 2, 8-12 there; Omega Ruby's types 0, 1, 5-8),
    with 1 the battle plays to its end (run dcj; word 0, X's 7, kept), so X's dust box battle (type 0x15, word 3 0xC)
    is likely stuck the same (it reached the command menu only, run db2); and **a cloud shows as a standing woman** (X's
    model 0: X hides the character and plays a small swirl from time to time through its move controller of codes
    0x38-0x3A, which Omega Ruby's code 0x3A is not; where the controllers are made not read). Not ported: X's weather
    byte (step 6), the module's lamps (an X object Omega Ruby lacks: off), the script native CallTsuchikemuriEncountEvent.
  **Word 3 read live (24 October, runs rd1-rd4, rw1-rw2, interpreter `readers`)**: in a trainer battle no code reads it
  (the settings' copy at 0x3DB0B8 into the encounter transition object, 0x48 bytes: word 0 at +0x18, words 1-5 at
  +0x1C, where X's 0x3BCD60 puts words 0-5 at +0x1C); in a wild battle one reader, DllBattle 0x1006C024 (X's
  0x10064C68, the same code): a switch on 0-12 giving the appearance message of the battle text (a/0/7/4 member 147
  in French) 0x105-0x109 for 8-12, 9 "... sauvage surgit du sol !" and 0xC "... de la poubelle !", lines Omega Ruby
  has. So Omega Ruby handles words 3 of 8-12 as X does: nothing to port there, and the stalled menu has another cause
  (not found). **Tested** (24 October, runs catA/catB: an Omega Ruby wild battle on Route 102, its type record poked
  from a state saved just before it): word 3 = 9 alone, the battle plays (menu, move chosen, turn played); word 0 = 7
  alone, about 3 s into the battle the core loops on an unimplemented local-wireless call (nwm::UDS), under 1 frame/s, no
  menu by the run's end (whether stuck or only slow not checked). The nwm::UDS errors are not the cause: they come in
  the working battle below too. Word 0 indexes the transition's table of effects (Omega Ruby's DllField 0x1026BB58, X's
  0x1022FF28, the same code and the same table 3, 8, 0, 1, 2, 4, 5, 6, 7, 9 for 0-9). **The dust cloud's battle plays**
  (run dcx, 24 October: the mod of 08:08, X's type 0x16 whole, 7 and 9; the player walked into a cloud on Route 13, a
  wild Kraknoix (Trapinch), "Attaque", a move chosen, the turn played: "Le Kraknoix sauvage se cache dans le sol !"). The earlier
  stall (dcj) is not reproduced; its cause is not known (an older build, or the keys pressed during the intro: not
  checked).
  **In the phone test mod** (`headless/test_all_mod.sh`, 24 October): X's matrix 31 added (Route 13, X zone 272, mod
  zone 554; the zones after it and the hub one further: 538-556, hub 557), the hub's trigger at x 32 warps to it (tile
  126, 81), `--xy-dust-cloud on`. Run tay: the trigger loads Route 13 and its module, the player walks into a cloud, the
  battle (a wild Dugtrio) plays its turn. Not checked: the phone. Still open: a cloud is drawn as X's model 0 (a
  woman); X's controller of move codes 0x38-0x3A that hides it is not found.
  **Searched for it (24 October, static), ruled out**: X's switches on 0x40 and 0x45 values (.code 0x38C530 = Omega
  Ruby's 0x3A3DB8, the same cases; DllField 0x1026A820, a position by move code: 0x38-0x3A share the default); X's
  0x17A218 (a graphics command queue); X's 0x4A64CC (= Omega Ruby's 0x4DE514, which lacks its branch: for a character
  with a flag at + 0x4C and move code 0x38-0x3A it answers 1 without testing the flag; the flag is a trainer's (0x6CC +
  n): the trainers' sight, not the drawing; not ported, its effect not known). The only other X test of these codes is
  the step check's. The cloud character is X's a/0/2/1 member 0 in both games (a woman): what X draws instead is not
  read. **Tried, not kept (24 October, runs taz, tb)**: X's branch put into Omega Ruby's 0x4DE514 (a hook at 0x4DE5A0):
  the cloud's woman then shows and vanishes in turns (about 400 frames each, run tb), the battle still starts; no run
  without the branch was filmed the same way, so what it changes is not known. Not kept: Omega Ruby's zone 47 has two
  trainers (106, 108) of move code 58, whose beaten flag the branch would skip. The clouds in X's zone 272 (mod 554):
  kind 2, a flag (4135-4145), move codes 56-58, an area (e.g. 17 x 22 tiles), X's model 0.
  **X itself run on Route 13 (24 October, handover: start here)**: the owner's X save (Drive file id
  1IJLf3c0zxuN3A88gkYAY_wNe5mdexzTy, into `<work>/dumps/xsave/main`, never committed) moved by
  `prototype/xy_save_move.py dumps/xsave/main dumps/xsave/main_r13 272 126.5 81.5 2.0`, run with
  `POMEGRADE_GAME=dumps/x.3ds POMEGRADE_SAVE=<abs>/dumps/xsave/main_r13 headless/run_local.sh ...` (`run_local.sh` puts it in
  X's folder; the core's dump/report/writers accept X's program since this day: `pomegrade_report.inc`, the session's core
  rebuilt). X shows no woman there, only a small dust swirl (run xr2). **Found**: the 9 shown clouds are objects of
  0xAA8 bytes in X (0xAB0 in the mod), u16 +0xC the zone record's id, +0x10 model, +0x12 move code, +0x14 kind; their
  word +4 is 0x01010007 in X, 0x00010003 in the mod. **Bit 0x4 of that word hides the character**: poked into the mod's
  9 clouds, the woman goes at once and stays gone (run pk4); bit 0x01000000 alone does nothing (pk6). **Next**: find
  which X code sets bit 4 for a cloud (a `writers` run on X's cloud objects under the interpreter, from the state
  `runs/xr2/field.state`: the module recreates clouds every few hundred frames; run xw1 was stopped before its result),
  then port that code (or its branch) into Omega Ruby; check Omega Ruby's move code 58 trainers (zone 47) are untouched.
  **What hides a cloud in X (25 October, runs xr3, xw2, xr4, xw5: X on Route 13 from the owner's new X save, the
  interpreter's writers / readers from the state `runs/xr3/field.state`)**: the 9 clouds at 0x08D043CC + n * 0xAA8 (move
  code 58, one 56). When the module recreates a cloud, its word +4 goes 0, 1, 0x10001, 0x10003 (as in the mod), then
  **0x10007 and 0x01010007** by X's .code 0x3D037C (`SetFlag(chara, bits)`: word +4 |= bits) called from **X's DllField
  0x102485C0** (40 bytes: `SetFlag(chara, 4); SetFlag(chara, 0x1000000); return 3`). That function is one step of a
  character command: X's 0x102446F0 (11 KB, Omega Ruby's twin 0x1027E180, 93%) builds 70 command controllers (0x48 bytes:
  +0 the class, +4.. its steps, +0x44 the step count) into a table, X's at slot + 0xB0; X's dispatcher 0x10247AA8 runs
  command number `u16 [state + 8]` (under 0xD4) from that table (read live: slot 0xB0 read by run-time 0x6F3ABC = linked
  0x10247ABC, from .code 0x3D72F8, the character update 0x3D7200, 23 times in 3,000 frames). **Omega Ruby has no such
  step**: no function there sets 4 then 0x1000000 (DllField swept); its slot 0xB0 is another command (0x1027D1D0 sets
  flag 8; its slot 0xAC sets 4 alone), and the two tables differ past 0xE4 (X's 0xE8-0xFC and 0x32C-0x348 only in X,
  Omega Ruby's 0x3C0-0x3E0 only there). **Not found yet**: which code gives a cloud that command (the number is not an
  immediate 0x2B / 0x2C in DllUSR13 or DllField; the character's command states at +0x8B8 / +0x8D4 are written only at
  its creation in run xw5): next, a writers run on the command state the dispatcher receives (its address read live).
  **Who gives it (25 October, runs xr6, xw10, xw13)**: the state is the character's + 0x8F4 (+0 the character, +4 a
  table, **+8 the command, u16**, +0xA, +0xC the step); the dispatcher reads command **0x2C** there for every cloud (run
  xr6). The writers missed it until `patch_core.py` hooked 8- and 16-bit writes too (25 October: it hooked 32/64 only).
  Set once while the zone loads (run xw10, from a state at frame 120: three times 0, then 0x2C, by DllField's
  0x102425D4 called from .code 0x3D9874 `SetCommand(chara, command, arg)`); a recreated cloud only gets its step reset
  to 0 (+0x900 written), so the command runs again from the hiding step. The caller (the setter's `push` writes its
  return address, run xw13: 16 calls): **X's DllField 0x1025B30C** (124 bytes: `SetCommand([this], 0x2C, 9)`, then reads
  [chara + 0x8AC] and a table), called from 0x102DB160, 0x102DCCE8 and 0x102DCD84 (28-44 bytes each); its nearest Omega
  Ruby function (0x102A83A4, 72%) is another one. So X's dust-cloud character gets X-only command 0x2C, whose first step
  hides it; Omega Ruby's command 0x2C is another (flag 8). (Corrected the same day: X's 0x2C is ActionCmdVanishON, the
  same class as Omega Ruby's 0x30, the tables numbered apart: "Action commands" below; the port gives it 0x30.) Lesson: capstone's `disasm`
  stops at the first word it cannot decode: scans over a module must go word by word (the immediate search of 0x2C
  missed 0x1025B334 that way).
  **Move codes, X against Omega Ruby (25 October, static; the owner's choice (B): the whole list before porting)**: each
  game's builder (X DllField 0x1024C138, Omega Ruby 0x10286208, the same code) fills a table of one 4-byte object per
  move code (its class's vtable), X's at 0x103376CC (codes 0-0x40), Omega Ruby's at 0x10385720 (0-0x3E; the same builder
  fills a second table, 24 trainer event types, 0x1038581C). Compared by the classes' RTTI names (vtable word -1 ->
  typeinfo -> name; an instruction comparison did not tell the classes apart): **0x00-0x37 the same names in both**
  (None, Player, DirRnd*, Up/Down/Left/Right, RndH/VLim, RouteTwoPoint 0x13-0x2B, TrainerPair, AlongWall*, Rand,
  AlongBit*, Kakuremino, Sit, Route, RouteEasy); then:
  | X | X's class | X's zones | Omega Ruby's code (its zones) |
  | 0x38-0x3A | MoveCodeTsutikemuri AlongBitLeftHand / AlongBitRightHand / Rand (dust clouds) | 272 | none: X-only |
  | 0x3B-0x3D | MoveCodeYagi Stay / Slow / Quick (Skiddo) | 270 | none: X-only |
  | 0x3E | MoveCodeTravelingPoke | 351 | none: X-only |
  | 0x3F, 0x40 | MoveCodeSpinRight, SpinLeft | 146 | 0x38, 0x39 (no zone uses them) |
  Omega Ruby's own 0x3A MimicryType1Move (zone 47), 0x3B MimicryType2Move (zone 47), 0x3C Seiza (9 zones), 0x3D
  SecretBaseTrainer (51), 0x3E Fishing2 (14) are not in X. The zone port copies a character's move code (record word 2)
  unchanged (OrasXyEvents.cpp), so today X's 0x38-0x40 run Omega Ruby's classes of those numbers. Omega Ruby's table
  is read in three places only (its pool words at 0x10286E4C, 0x10286F84, 0x10286FE8: the builder, the teardown loop
  0x10286F18 and the getter 0x10286FBC, `cmp r0, #0x3f`): moved into the room with 7 slots more, X's seven classes get
  0x3F-0x45, X's 0x3F/0x40 become 0x38/0x39, and the zone port translates word 2. Same name is not proved the same
  code: each pair is to be read side by side before it is called merged (CLAUDE.md 8).
  **Action commands, X against Omega Ruby (25 October, read live from both games' heaps: X's table at 0x08183624, 0xD4
  entries, run xr3; Omega Ruby's at 0x081884D0, more than 0x100, run or1; each entry's object -> its class's RTTI
  name)**: the number a character's command state holds (+8 of the state at its + 0x8F4 in X) and the number zone
  scripts give `MdlAcmdSet` (Omega Ruby's native 0x10293B44 passes it unchanged into the command list, 0x10244918) index
  these tables. **The numbering differs**: Omega Ruby has commands X lacks (SlowWalk_12F 0x18, StealthyWalk_16F 0x19,
  Turn2F, StayWalk*...), so from X's 0x09 on most same-named commands sit at other numbers: of X's 212, 9 at the same
  number, 99 at another (ActionCmdVanishON: X 0x2C, Omega Ruby 0x30; Walk_7F: X 0x12, Omega Ruby 0x13), 104 with no
  same name: the ActionCmdAnimation<Acmd, DrawStatus, mode> instances (their names hold their own number, so they match
  by the animation they play, DrawStatus, whose numbering is to be compared too) and X's own (AnalogJump/Move, Cycle*,
  Climb*, PokeRide3*, Tear). So today **X/Y's ported zone scripts and ported code give Omega Ruby's commands of X's
  numbers**: a command past X's 0x08 is another action (not seen in a run yet). To port: the map by name (the basic
  commands), then by DrawStatus (the animations), X's own commands as ported classes, and the translation in the
  script port (MdlAcmdSet's code) and in each ported block.
  **The clouds hidden and their swirl drawn (25 October, runs dc2a-dc4c, headless)**: X's move-code classes 0x38-0x3A
  (MoveCodeTsutikemuri*, the mod's 0x3F-0x41) are ported into the room (OrasXyDustCloud.cpp): their start gives the cloud
  X's VanishON (Omega Ruby's command 0x30) and makes its swirl, a record of X's effect kind 0x1F
  (FieldMoveModelEffectTsuchikemuriEncount, read live: X's 9 records of that kind are Route 13's clouds), also ported:
  its own methods copied, its base's Omega Ruby's (field::mmodel::FieldMoveModelEffect, the same code in both), its
  three files (X's a/0/6/6 TM entries 0xBD-0xBF: a model and two animations) appended to Omega Ruby's a/0/6/4 TM (189
  files) at the same numbers, the field's effect pack (built by 0x1026D628: 0xA5 entries of 0x4C bytes, ids 8-0xAC, the
  TM's entry numbers) grown to 0xB8 entries, its builder skipping Omega Ruby's own entries 0xAD-0xBC (7 models and 8
  animations the game never loads as effects: loaded, they left two of the scene's animations without a resource, read
  near address 0 about 5,000 times a run on X/Y's zones, PCs 0x38A28C and 0x4CCDF4; bisected 26 October: none at
  6372c2f8, from cc0d85de, gone without the pack grown, runs sc1x-sc3x; with the skip none, the swirl drawn and the
  battle started, runs sfx, sfb). Offsets read side by side or live: the effect manager at the character
  manager + 0x36C0 (X's + 0x14B4), records at its + 0xD8 (X's + 0xE0); the player's position + 0xC64 -> + 0x6F8 (the
  PlayerGrid read in both heaps); the camera + 0xAB8 -> + 0xAA4; the model (xy_system::H3dShaderModel in both) virtual
  + 0xB8 -> + 0xD0, position + 0x138 -> + 0x130. **The last cause found**: the clouds are characters of **model 0**,
  both games' "no model" (its per-area list entry, X's a/1/4/9 and Omega Ruby's a/1/3/7, the same 24 bytes in every pack
  of both: no member, no parameters); the zone port had given them X's a/0/2/1 member 0 (the woman) with an entry
  copied from model 284's, so they had a model (the character manager's table copies the entry into the character's +
  0x884, X's + 0x87C, found by `writers`, run dc3f: 0x3F62D8), and the effect base (both games' 0x10325BE8 / 0x102DC6EC)
  makes no effect for a character with a model (byte + 0x887 non zero) and the flag 0x800000: their swirls were made
  and dropped (run dc3c: the start called 5 times, never the update). Model 0 kept: the 9 swirls live (run dc4b), drawn
  where X draws them, and walking into a cloud starts the event and a wild battle (run dc4c, a Trapinch). Not checked:
  the swirl's sound when the player is near (ported, X's 0xA00C7 translated; no run listened), the phone. Omega Ruby's
  own codes keep their classes in the moved table (run dc4b's memory: 0x38-0x39 Spin, 0x3A MoveCodeMimicryType1Move,
  the class of the game's own 0x3A, 0x3E Fishing2), so its zone 47 trainers of move code 58 keep theirs (a walk in zone
  47 not run).
  **A dust cloud's battle played to its end (26 October, run bt1, headless)**: walking into the cloud, then A pressed
  through the battle: the command menu takes the input, the turns play, the owner's level 5 Torchic loses to the wild
  level 28 Trapinch and the game takes the player home as after any lost battle (Littleroot, "... se hâte vers sa
  maison"). So X's battle type 0x16 (the mod's 95) whole works; the stuck menu of run dcj (24 October) does not come
  back. **X's dust box battle (type 0x15, the mod's 94) played to its end too (26 October, runs db5, db6, headless)**: in
  the Pokemon Village (mod zone 555, X's 318), the table drawn by the new day (spots 1, 0, 0, 0, 2, ...), its entries
  poked to Garbodor level 46, A at the can of spot 0 (tile 37, 59, as in X's zone file; script 10480) gives "Oh ! Un
  Miasmax sauvage surgit de la poubelle !", X's background 1 of that zone, the command menu, the turns, the loss and
  home with Mom. Not compared with X itself on screen (the X save in the session is at the game's start).
  The tools of that search: `prototype/heap_classes.py` (a heap dump's tables and objects named by their RTTI classes,
  who points at them) and `prototype/archive_find.py` (a game's archives searched for bytes, members decompressed, or
  listed by member count); the runs' `dump`, `readers` and `writers` (headless/run_local.sh).
- `CallHideItemEncount` is empty in X: nothing to port.
**Porting X/Y code: the method (24 October, for the next agent; how B2 was done)**:
1. *Map.* `prototype/xy_import_match.py` gives X's functions Omega Ruby has; `prototype/xy_code_compare.py` (`pairs`,
   `calls`, `offsets`, `closure`, `sites`) gives the rest: a copied block's twins, what each call became, which fields
   moved (the map object, the character manager, the character, the records: OrasXyDustCloud.cpp's words list them,
   each with its X and Omega Ruby offset), what a port takes.
2. *Addresses.* .CRO files are mapped linked = base + file offset (X's DllField 0x101F9000, Omega Ruby's 0x10242000);
   DllField itself is loaded where the heap has room (0x6F3000 in the runs), so ported code never holds a DllField
   address: it calls DllField through the getter word the loader writes (.code 0x19C8D4, OrasXyNatives.h) and lives in
   the .code's room past the .bss (0x6AF000+, written by code.ips). In a mod grown by `--field-room 0x20000`, a DllField
   read-only address seen live is linked 0x10242000 + (runtime - load base) - 0x20000.
3. *Copy, then patch.* Copy X's functions whole (PortArmBlock), point their calls at Omega Ruby's functions, rewrite
   every field offset that moved, and assert each part's place and each patched word's old value (`placed()`, intended
   == changed): a wrong order of written stubs was the costliest bug (dc2-dc3).
4. *Run headless* (`headless/session.py start/do/stop`, about 8 minutes to boot; retro_host's `hold`, `press`,
   `mash a N`, `shot`, `mem`, `watch`). To see what a ported function does, a temporary thunk can store registers to an
   unmapped address (0xF0000000+): Azahar logs each such write with its value and PC. Remove every such thunk before
   committing. Truncate the run logs: a crash loop wrote 1.6 million lines and filled the disk.
5. *Never substitute.* When X's value has no Omega Ruby meaning (the battle type's category 9), port X's handling of it,
   or ask the owner (CLAUDE.md 8).
Code to port goes into **DllField's room** (below). Step 2 (Sky and Inverse Battles, Battle Chateau, 79 sounds) follows.

**Step 5, the substitutes' inventory (26 October, owner: the map before the rest; read statically and headless).** X/Y's
zones are ported (pieces' ground, collision and props, textures, events, scripts, light and battle backgrounds X/Y's own),
but each X/Y area pack is built on a copy of an Omega Ruby pack (Littleroot's 8 outdoors, a house's indoors:
OrasXyWorld.cpp). Both games' packs are AD containers of 12 files, the same layout:
- file 4 (light): X/Y's (converted); files 0 (props list) and 1 (textures): placeholder pack 0's (the pieces carry
  their own textures);
- files 2 (material animations: pack 40's bm_w_river, bm_w_taki* waterfalls, lights) and 11 (textures, holding the frames
  those animations swap, bm_w_river.1/.2...): **X/Y's** (BchUpgradeTo21, 26 October). With pack 0's empty file 11 the
  animations read near-null entries (4,335 reads, run wt1); with both, the Village's river and waterfall drawn and moving,
  0 reads (run wtf). X/Y's file 1 added as well: the area stops loading, both screens black (run wtc); pack 40's files 1
  + 11 are 1.56 MB, Omega Ruby's largest 1.47 MB (a size limit inferred, not read). Other packs than 40 not checked.
  Runs: the first shot after `draw on` is always black (cm3: 0 then 113.6), shoot after a few frames;
- files 3 (the **camera areas**: a count, then 0x64-byte records, a map area and its mode byte +0x17) and 6 (the
  **camera presets**: 0x44-byte records, a u16 flag, a u16 number, then height, pitch, field of view 32, far 2000,
  distance...; X/Y's 60 in 4,096 bytes, Omega Ruby's 16 in 1,152): **X/Y's** (27 October). The code reading them is the
  same in both games (the area search Omega Ruby 0x102CE4B8 / X 0x1027FBFC, the preset copy 0x102C9C50 / 0x102782B4, the
  zone-load setup 0x1026FF20 / 0x1023679C: `call_tree_compare.py`). The field starts on the preset that **a/0/1/4's
  per-pack table** names (its last member; FUN_102E22D0 reads the entry's +0xA and copies file 6's record; +8, 0xFF for
  none, read beside it). X/Y's table (its a/0/1/3's last member) has 36-byte entries, Omega Ruby's 44: the same first 36
  bytes, then 8 bytes more (-1 in most). Our packs kept the Omega Ruby slot's entry, so the Village started on X/Y's
  preset 4 (run mm1: the camera low and close, cm3, cm4) where X starts on 36 (run xm1: memory searched for the records,
  pitch 15.9, distance 305.8). With X/Y's +0xA: the view matches X's at the same tile (run cma against xv3).
  `OrasWorkspace::SetAreaPackTableWord`. The entry's other fields are not compared yet. On the way: zone header words 16,
  17, 20 and 21 (builder's reference values, not X/Y's) change nothing on screen (runs cm6, cm7, cm9); X/Y's word 15 stops
  the area loading (6 reads at 0x48B39C, run cm8): its meaning not read;
- **the player's scale against X/Y's world** (27 October, owner's decision): X's player stands 35.8 units from its
  feet to the top of its hair (its parts, a/0/8/8, no hat), May 33.0 (a/0/2/1 member 171, ribbon left out): 8 % more.
  The two worlds share a scale (18-unit tiles; the props both games have, the same size), so the player is drawn x1.08
  in X/Y's zones (`--xy-player-scale FIRST:LAST`, OrasXyPlayerScale.h: a per-frame cave writing the model's scale,
  1.0 put back outside them). Checked headless: 1.08 in the Village (runs ps3, ps5), 1.0 in the Hoenn hub (ps4, ps6).
  The colours "darker than X's" were the hour: the game's clock is the real one, X's reference shot taken in the
  evening and ours after midnight; at the same hour X is as dark (runs xv4, cma; ours still ~10 % darker on the flowers,
  not explained). The pack table entry's other fields (0, 4, 6, 7) changed nothing seen (run tb1): left as they are;
- file 5, X/Y's **projection maps** (27 October, read, not ported): its textures (`cloud` in 50 packs, `treelight`,
  `pokecen`, `plr0201`/`0701`/`0702`) are projected on the ground by X's `field::projectionmap` classes (Base, Cloud,
  TreeLight, Pokecen, ProjectionMapPlr0201/0701/0702: X DllField 0x102218D8-0x10223720), which Omega Ruby has none of.
  X's area load (0x1029E728) calls their factory (0x102224D0) with the area: its kind is the per-pack table entry's
  +0xE (field 7: Pokemon Village's pack 40 reads 1, the clouds, shown from 3:00 to 20:00 and in weathers up to 12), its
  texture pack file 5 (getters 0x4AB77C / 0x4AB74C). Of the 26 functions they use, 13 have a copy in Omega Ruby, five
  none (.code 0x35EE88, 0x35F0BC, 0x35F2E8, at most 59 % alike, and DllField 0x1027CA58, 0x1027CC10): the port reaches
  into the renderer. Not done (the owner to decide when);
- doors, street lamps and stairs (27 October, checked headless against X, Vaniville in the test mod): X's door models are
  drawn now (runs vd1, xvd1) and the lamps too (vl1, xvl1); X's stairs in the player's house are not warps but
  triggers (event 5, 6) running `MapChangeFake(5, 5, x, y, z, dir)`, a move inside the same zone (both floors are one
  piece, 163, side by side), which Omega Ruby registers (bit 2): walked up headless (session stS). Trigger kinds (word 3:
  0, 5, 6, 7, 8) are the same set in both games. **Seen wrong**: a save in that house (zone 539, X's 2), moved by
  oras-save or made with the game's own menu (its Situation block holding the right zone and position), loads at the
  zone's start point by the bed, with or without X's story (`--story off`, run sto1): the game's load ignored the
  position there, not known why (the header matches Omega Ruby's house 223, its source). **No longer seen (29
  October)**: two saves moved into zone 539 at (5.5, 8.5) and (12.5, 12.5) load at their own tiles (runs s39a, s39b,
  the whole-Kalos mod); Omega Ruby's own house 223 keeps a moved save's tile too (runs sv1, sv2). Likely fixed by the
  tile fields' repacking (27 October); a save made with the menu there not rechecked (the full boot's run hung at
  start, run ms1);
- Tiles (27 October): **the two games split a tile word differently** (their readers): X flags bits 0-10 (0x3E11A0),
  footstep ground type 11-15 (0x3E0A34), battle background 16-23 (0x3E0AA8, 0x3DFB94); Omega Ruby flags 0-11
  (Map_TileAttr_Flags), ground type 12-16 (0x402FCC), background 17-23 (FUN_00403190); kind 24-31 in both. X's values had
  been kept as they are, so Omega Ruby read X's fields one bit off (its background halved: 22,253 tiles of X's
  background 1 read as none). `XyTileValue` now moves each field (0x31000020 still made 0x20); the "29 X-only
  behaviours" were mostly that shift: 14,263 tiles keep values Omega Ruby's pieces lack, new combinations of known flag
  bits, and X's ground types 13, 15, 16 (Omega Ruby has 13: its footstep code, 0x102E5DC8, skips the rest). Checked
  headless: a Route 13 battle draws X's badlands backdrop as X does at the same hour (runs tbg2, xbg2). **Footsteps**:
  X's code (0x1026DAF8: 17 ground types x 23 movement kinds, its own sound ids) differs from Omega Ruby's (13 x 9):
  ported 29 October (OrasXyFootsteps.h, oras-engine --xy-footsteps, written by oras-xy-world): in X/Y's zones Omega
  Ruby's code reads a 17 x 9 table of X's sounds. Built and patched (run fs1). **Not working yet**: the owner hears steps
  now and then in Hoenn, none in Kalos (phone, 29 October). Headless no step ever sounds, Hoenn's own included: with
  the table forced to Omega Ruby's SEQ_SE_FT_02 everywhere (zone 0 up), a walk in Littleroot, on Route 2's grass (zone
  797, X's ground 4) and in Vaniville records the same samples as with no table (runs dP/dN, g1/gN/gP;
  headless/audio_diff.py), drawing on or off, so the step function 0x102E5DC8 is not reached on those walks (its
  caller FUN_102E93EC plays a step only on frames of the player's motions it lists, 0x102E962C, not traced). To hear
  them on the phone: cheats below (no music, no wild Pokemon).
  **Ground 0 has no sound in either game** (checked on the phone by the owner, 30 October): 96% of Omega Ruby's tiles
  (grass, paths, floors indoors) are at ground 0. Grounds from textures (owner's request, 30 October,
  `OrasFootstepGround.h`, run by oras-xy-world): each ground-0 tile takes the ground of the texture drawn over it (the
  highest upward face over its centre, the material's name when no texture is named), by a table of name words read
  from the games' own tagging (`remake_tool piece-grounds [--x] <pieces>` prints it: 50% of the tiles the games tag
  get the same ground back, the rest wet sand 10 and tall grass 5 read as sand 8 and grass 4). Built (local, matrix 0):
  422,575 of Omega Ruby's tiles in 726 pieces given a ground; its pieces recompressed, the a/0/3/9 patch 98 MB. Not
  covered: X/Y's outdoor terrain, drawn by projection (projection_dummy, 404,507 tiles), and the 84 pieces whose model
  has no face over the tiles; they stay silent. The new tile values (00004020...) are not among the game's own;
  not tested on the phone.
- *Sounds.* X/Y's 1,554 sound ids: 146 streams (music, ported apart), 1,329 of a name Omega Ruby has, 79 of none. By
  their data (the sound's sequence file, its banks, their wave archives; each file's version word +8 set aside, 0x0100 in
  X/Y and 0x0101 in Omega Ruby, and a bank's wave archive ids 0x05xxxxxx compared by the archive's contents, the two
  games numbering them apart): **1,005 the same, 324 different** (212 `SEQ_SE_FLD`, 40 `SEQ_SE_DEMO`, 23 `SEQ_SE_SYS`,
  11 `SEQ_SE_BT`, the rest menus and others). Today all 1,329 take Omega Ruby's sound of the name (OrasXySound.h): the
  324 should be X/Y's own (the owner's rule), which needs X/Y's sequences, banks and wave archives appended to
  Omega Ruby's archive (BcsarAppendStreams adds streams only: not written). The 79 of no name: 40 roller skates
  (`SEQ_SE_RS_*`), 30 the PR Video studio (`SEQ_SE_PVD_*`), the moves 546-550 (`SEQ_SE_W546`-`W550`), 4 `FT`/`PFT2`.
  Not checked: that the 324 sound different to the ear. The way chosen (owner, 26 October): X/Y's sounds **added
  beside** Omega Ruby's, with their own sequences, banks and wave archives, rather than written over Omega Ruby's
  same-named data (those files are shared by several sounds: Hoenn's would change). A sound plays only once its data
  is loaded, which the game does by groups (CGRP: 28 in X/Y, 2 of them the same as Omega Ruby's), scene by scene: the
  added files must go into the groups loaded where X/Y's sounds play. Not read yet: which groups Omega Ruby loads in the
  field and in battle. Tools: `prototype/sound_compare.py` (this comparison, `--list` the 324).
  Instrument level (29 October, `--why`): of the 324, 238 differ in their sequence and their banks, 78 in their banks
  only, 8 in their sequence only; 190 name a wave (sample) found in none of Omega Ruby's wave archives (by its bytes),
  **174 different waves** in all: the rest of the 324 replay waves Omega Ruby already holds, with other instruments or
  notes. Footsteps (`SEQ_SE_FT_*`, `SEQ_SE_RU_*`, 12): all differ, 10 with a wave of their own. Checked: over the 1,005
  same sounds, all 1,179 waves their banks name are found in Omega Ruby's archives.
  Ported 29 October: oras-xy-world appends the 324 to the archive (Bcsar.h BcsarAppendSequences: 179 banks and 164
  wave archives added, 98 and 113 shared; all 324 checked by sound_compare.py's reading to play X/Y's data, Omega
  Ruby's 1,709 unchanged), ids of category 0 after the streams' (oras-engine --xy-sequences; category 0 now computed
  by a routine in FUN_00115984, the list no longer fitting). Not yet heard in game. Each growth (streams, then these)
  left the old copies of the tables it grew in INFO, which the game holds in memory: with both, INFO 0xA0D00 bytes and
  the game stopped at boot, an allocation of its sound setup failing (owner's phone, 29 October; run mu1). The replaced
  tables are now removed (Bcsar.cpp WithoutInfoRanges): INFO 0x82100 with the streams, the title reached. Tools:
  `sound_compare.py --appended` checks a grown archive, `remake_tool sound-id` names an id in both games.
- *Testing cheats* (Azahar's Cheats menu, Gateway codes, 29 October; Azahar invalidates the code a cheat writes; not
  tried): no wild Pokemon, Field_WildEncounter_StepRate (0x4E4778) returning 0: `004E4778 E3A00000`, `004E477C
  E12FFF1E`; no music, Snd_BgmStack_PlayCurrent (0x44DB28) doing nothing: `0044DB28 E3A00000`, `0044DB2C E12FFF1E` (the
  music playing goes on until the next zone change). Unticking leaves the code patched until the game restarts.
- *Inverse and Sky Battles.* Omega Ruby keeps X/Y's three natives, called by none of its scripts: `PokePartyCheckSkyBattle`
  (the same code as X's, its callees too: merged), `SkyBattleResultGet` (the same, the battle's result byte read at the
  game data + 0x1D0, X's + 0x1CC, where both games' `_BattleGetResult` read it: merged) and `CallSakasaBattle` (the same
  but one constant: the battle's two background values, Omega Ruby's 7 and X's 8, FUN_003EAB6C's arguments taking the
  zone's and the tile's place; its setup FUN_003DA5CC is X's 0x3BC720 built on Omega Ruby's structures). X/Y's scripts
  for them are shared scripts 50 (Inverse, ids 10425-10429; X zone 280's character 0, model 88) and 52 (Sky, ids
  10350-10379, `_CallTrainerBattleCore` between the party check and the result), ported with the others (23 October).
  `CallSakasaBattle(0, 65, 25)` (X's arguments; 55 and 15 with flag 2640) run from the test hub (trigger x 34, run iv2):
  a battle starts, against Omega Ruby's "Jeremiah le Patron" (trainer name 681, a Magneton) on an indoor background, not
  X's opponent; where the 65 and 25 pick him is not read (both setups' FUN_003676B4 / X 0x34F110 differ in one branch,
  0x1F019C / 0x1DC0D0). Not read: X's Inverse opponent and background on screen (run xi1: the house drawn without its
  character, a save moved into a zone), the type chart's inversion in DllBattle, how a Sky trainer's battle reaches
  script 52 (no X character holds ids 10350-10379).
- *Battle Chateau.* No native of its own in X; its zones' scripts and 51 noble classes: not read yet.
  Tools of this inventory: `prototype/call_tree_compare.py` (a function and its callees in both games),
  `prototype/bch_sections.py` (what a BCH holds), `prototype/bps_apply.py` (a mod's patched file rebuilt),
  `prototype/oras_save.py flag` (a moved flag set in a save).

**What changed on 23 October** (all headless; **nothing of 22-23 October is tested on the phone**):
- wild battles in X/Y's zones: X/Y's encounter effect entries and battle types, its 9 intros, hordes on its tiles with
  its horde camera, fishing and Rock Smash types (0.1 below, "Hordes" and "All 9");
- X/Y's scripted wild battles (`--xy-static-wild`, `XyMoveWildBattles`; 0.1 below);
- **DllField grown by 128 KB** (`oras-engine --field-room 0x20000`, `GrowCroCode`): caves from linked 0x10333000 to
  0x10353000, almost all free. In a grown mod DllField's read-only and data segments sit 0x20000 past the linked image
  (Ghidra, `cro_link.py`): add it to an address read there. The `.code`'s own free room (0x579604-0x57A000) is nearly
  full: 0x579DC4-0x579DFF and 0x579EFC-0x579F7F only.

**Tools** (each documented in its file's header; the list with examples: section "Setting up a session" below):
- `remake_tool` (build: `cmake -G Ninja` of `tools/remake`, then `ninja remake_tool`): `oras-xy-world` (X/Y's zones into
  Omega Ruby; prints the `oras-engine options` line to use), `oras-sandbox` (zones from a text description,
  `tools/remake/sandbox/*.txt`; `--onto` a mod, `--xy` X/Y's data), `oras-engine` (code patches: every option in
  `remake_tool` usage and `OrasEngine.h`), `oras-save` (a save moved to a zone and tile), `mod-file` (a file as a mod
  has it), `garc`, `garc-set`, `romfs-ls`, `amx-asm`, `xy-script-export`, `bch-upgrade`;
- `headless/xy_test_mod.sh <work> <name> <description>`: X/Y's world + a sandbox + the engine + a save, in one command
  (checked 23 October: it rebuilds the mod of run sg byte for byte); `sandbox/xy_static_wild.txt` is the scripted wild
  battle test with its control;
- `headless/session.py start|do|swap|stop|list`: one game kept running (boots once, about 9 minutes), command batches
  (`retro_host` commands: wait, hold KEY N, press, until COND MAX, burst, shot, save/load NAME, mem, poke, dump), shots put
  on one sheet to read; `swap` does not work for battle sequences (its header says why);
- `headless/mod_variant.sh`: a copy of a mod with files or archive members swapped, each read back;
- `ghidra/decompile.sh <work> <out> ADDRESS... [--callers A] [--callees A]` (Omega Ruby; `POMEGRADE_GHIDRA_GAME=x` for X):
  decompiles into `<out>/decomp.c` in about 15 s; `ghidra/function_names.tsv` names Omega Ruby's functions (read|guess);
- `prototype/gamefs.py` (ls, members, get, find, diff, where: files of the game or of a mod), `prototype/romfs_member.py
  --log` (which files a run read), `prototype/shot_sheet.py`, `prototype/cgfx.py`, `prototype/cro_dis.py`,
  `prototype/code_find.py`; the linked images of all modules: `<work>/linked/modules.bin` (Omega Ruby, from 0x10000000;
  DllField at 0x10242000) and `<work>/xlinked/modules.bin` (X; DllField at 0x101F9000), `.code` at `<work>/dumps/code.bin`
  and `xcode.bin` (both from 0x100000): capstone reads them in a few lines of Python.

**Pitfalls met** (each cost hours): a state load rebuilds LayeredFS but replays the same random numbers; a module runs at
another address than its linked one (DllField at 0x6F3000): a cave reaches its own data from the pc, the `.code` by an
absolute literal (it is loaded where it is linked), never by a branch; every a/0/3/2 member must be LZ11 (the loader
decompresses all); a hand-rebuilt GARC with a stale size field gives false results (use `garc-set`); `pkill -f` kills the
agent's own shell; never two heavy jobs (emulator, Ghidra, a build) at once.

### 0.1 Taking over (21 October, A's handover; read after 0.0, the 7 October text below is history)

**How the owner works with an agent** (their decisions, 20-21 October): one agent at a time (`docs/AGENT_PROCESS.md`, the
three-agent period stopped); it works **only when the owner asks**, **one point at a time**, a point closed only when
**entirely** done (every element of its step, in both regions where the step says so: point 2 was closed too early once
and reopened); commit and push at every verified step, log the progress in the plan as it goes, merge into `main` when a
point closes. Ask before a run over 15 minutes, before anything changing behaviour or scope, and say plainly what was not
checked. Answer in French when the owner writes French.

**Where things are**: the plan and each step's state with the runs that show it, `docs/DEVELOPMENT_NOTE.md` 3e (3d item
9: X/Y's features Omega Ruby lacks). Run recipes: `docs/AGENT_PROCESS.md` 3.1. Setting up a session (dumps from the
owner's Drive, remake_tool, the headless core, Ghidra's program): `tools/remake/headless/session_setup.sh`.

**State on 21 October**: points 1 (Grant's gym, B1) and 2 (scenes, C2) closed headless; next is **A2** (the League and
badges editable). Open, written in 3e: Grant's ported module writes into X's field at +0xFC on the way back from a won
battle (null in Omega Ruby; ignored by the emulator, a console would fault): the same kind of access is to be translated
or neutralised for every ported module; the badge is given by X's number (`BadgeSetFlag(1)`), A2's to map; wall climbing
is a feature of its own (3d item 9); X's cutscenes are translated but skipped (owner's decision), while
`CallFieldTradeDemo`, `CallApplicationDemo` and `_MapChangeAndDemoCall` are not skipped and may fail the same way. Nothing
is tested on the phone.

**State at the second handover (21 October, evening), point 3 = A2 in progress** (3e's A2 row has the runs):
- Done and checked headless: 16 badges (Hoenn 0-7, Kalos 8-15, `--badges 16`, X's scripts' badges + 8; owner: "the rank
  mixed, the badge's place stays its origin"); the badge case lighting each badge by its bit (DllTrainerCase patch; run
  hG27); the League's statements (`oras-sandbox` `restyle`, `class C battle-type T`, `edit` of a mod's zone, with
  `team`: runs hL1/hL0). X's 8 badge models found (a/1/6/5 342-350, identified: 3e) and converted (`bch-upgrade`).
- **Done (22 October): Kalos's leaders' own names in their battle intro**, checked headless on Grant's battle (run gk1:
  "Lino" in X's drawn name, the rest of the intro as before; run gkh1, his class given Roxanne's type 44 by
  `class 39 battle-type 44`: "Roxanne" in Omega Ruby's style, her rocks, so Hoenn's intros still go through the moved table
  and the rewritten a/2/1/4; no loader error in either log). How: `oras-xy-world` (AddXyTrainers) gives X/Y's leader types
  (X/Y's intros 0x11-0x18) the intros 162-169, each an a/0/3/4 sequence 976-983 copied from the one it showed with only
  its name changed (gym_name09-16), and adds X/Y's eight drawn names (X's a/2/4/1) to a/2/1/4 under those names in its 7
  languages; `oras-engine --xy-intros` (given by the options line) copies DllBattle's intro table longer into the free
  bytes past its code, points the literal's relocation at the copy and raises the sequence bound (0x3D0 at 0x10081410).
  `WriteDarc` lays files out as the game's DARCs (data at 8, BCLIM at 0x80, other files at 8; a/2/1/4 writes back byte for
  byte). Since (22 October) Kalos's Elite Four and Champion the same way (X's intros 0x19-0x1C -> 170-173, sequences
  984-987 copied from Omega Ruby's Elite Four's 851-854, big_four_name05-08 Thyméo, Malva, Dracéna, Narcisse; 0x1D -> 174,
  988 a copy of 799, the Champion's sequence no intro id names, champion_name02: X's name image cut to its name, 256 x 64,
  its title "Maitre de la Ligue" being drawn apart in Omega Ruby). Checked headless on Grant's battle given their types:
  run e79 (type 79: "Thyméo") and c89b (type 89: "Dianthéa", the title once). Omega Ruby's Elite Four intros are 0x2F-0x32
  (not 0x38). The Elite Four intro shows the four members' drawings (a/1/6/0 15, 21, 18, 24 in run t79's read trace: classes
  194-197, a class's picture p its member 3p), the one fought lit: the sequence itself names them, commands 0xA8 (u16
  0xA8, u32 3, u32 a class, the pane chara_01-04, in big_four_name01-04's order); the copies give Kalos's four classes
  (run e80: Wikstrom, Malva, Drasna, Siebold drawn, Malva lit, "Malva"). The effects stay the copied Hoenn member's, as the
  leaders'. Not tested on the phone.
- **In progress (22 October): the badge case's Kalos drawer** (owner's choices: a touch button and L/R, the case's own lid
  closing and reopening on the other region). Read so far (DllTrainerCase, 0x1081E000; runs bc1-bc6 on the gk mod, the
  badge word poked): the case is a mode of the trainer card (Menu, the card, A "Voir les Badges"; B back), one layout
  for both, a/1/0/9 member 0 (`tranercase_lower/upper`: badge_01-08 lit and `_base` outlines, tr0013-0020_full the
  leaders, tcase_up/_g/_n/_s and tcaselid_low/_g/_n/_s the case's colours, the lid's animation
  `tranercase_lower_tcaselid_low`), members 1-2 its 3D scenes. X's a/1/1/3 member 0 is the same layout with Kalos's
  icons, outlines and leaders under the same names. The 8 badges' table at 0x10822048 (9 words each: pane and animation
  ids, +0x20 the leader's drawing in a/1/6/0: 27, 33, 36, 39, 30, 42, 45, 51), read through several literal words; the
  badge models a/1/5/2 974-981 and 985-999 (odd) by the table at 0x10822168 (X's: a/1/6/5 342-349, 354-369). The
  badge word in the headless runs: 0x926E094 (the status at 0x926E080 given to FUN_004E9E64; poke it to light
  badges). The update (FUN_10820A44) reads the keys by the game's masks (FUN_004C7CFC held, FUN_004C7E34 pressed):
  1, 2, 3, 4, 8; L and R showed nothing with badges lit (run bc5). retro_host has no touch input yet.
  **Set aside by the owner (22 October), after a check**: one Hoenn slot made a Kalos badge (session demo, not committed:
  slot 2 Brawly's -> Grant's): the leader's drawing (the table's +0x20, 33 -> 378), the texts (a/0/7/4 member 26 lines 30
  and 38 <- X's a/0/7/5 member 25 lines 27 and 35: "Badge Mur", "Champion : Lino") and the bottom icon (a/1/0/9's
  badge_02 and badge_02_base <- X's a/1/1/3) all show (run bdj); X's model for it (a/1/6/5 343, badge_c02, bch-upgrade) in
  a/1/5/2 975 stopped the game when the case built the badges (unmapped reads at PC 0x170A78, then the fatal-error loop;
  runs bdi, bdk). **Found and fixed (22 October)**: the reads are the heap allocator's (FUN_00178260, a free block's
  pointer worth 1: the heap overflowed) and the stop the allocator's failure loop (FUN_0012B9A0); `BchUpgrade` gave the
  header's uninitialised data (+0x38) 4 bytes a raw-data pointer only, while the game's files also count each shader's
  runtime data: 4 an operand descriptor, 328, 244 a DVLE, 72 more a geometry one (exact on 847 of Omega Ruby's BCH
  files); with it X's Cliff badge (a/1/6/5 344, badge_c03: X's c0N are not the gyms' order, 343 is Clemont's Voltage)
  shows and turns in the case (run bds; bdr with the size set by hand confirmed the cause).
- **Done (22 October), headless: Kalos's battle backgrounds, X/Y's own** (owner: a background follows the place and the
  context). Read: a zone's background value (header halfword 15, bits 7-13, FUN_004D86B4; 7 bits, 128 values) gives the
  battle's background record in FUN_0048AA44 (X's 0x45A5C8): a table (Omega Ruby's 0x595CD0, 93 values; X's 0x551C04, 65)
  of 2 bytes a value (record bytes 0-1), then at +2N 5 bytes a value (bytes 2, 3, 4, 8, 9); byte 5 drawn at random from the
  pairs of a 12-byte entry (by byte 4) of the table at 0x595F5C (X's 0x551DCC), bytes 6-7 from a 0x18-byte entry (by
  byte 5) of the table at 0x595F68 (X's 0x551E68): random scenery, one entry in Omega Ruby (never drawn), 10 in X (trees,
  rocks, snow, sand: X's btl_V_*). The record goes to DllBackGround (FUN_100043E4: byte 3 the backdrop through the table
  at 0x10005000 and its two light tables of 4 times of day, 0x30 bytes each, at 0x10005174 and 0x100062E4 (X's one
  literal, the second +0xC30), byte 0 the ground through 0x10007454, bytes 4 and 5 through 0x100075C8 (12 bytes) and
  0x100075D4 (0x18), the same entries as the static tables) and to DllBattle (the platform: byte 2 through 0x10106074).
  A battle may take bytes 2-9 from a second value (FUN_003EAB6C): its script's (`_CallTrainerBattleCore` arguments 4 and
  5: Grant's X 49, his arena btl_G_gym1_copy2_c03, which Omega Ruby's 49 made a cave, run bgk1) or the tile under the
  player's bits 17-23 (FUN_00403190). Both games' gyms are so: the zone's value an indoor backdrop with the gym's
  platform, the leader's battle the arena. Ported (`OrasXyBackgrounds.h`): each X/Y value met takes a number with X's
  record, 93-127 for headers and tiles, 128-255 for scripts (35 free below 128 are fewer than X/Y's values), its members
  converted (BCH 0x07 -> 0x21, in the "BG" containers too; none the same as Omega Ruby's) and appended to a/1/3/6, the
  tables copied longer into the room (`oras-engine --xy-backgrounds`: the .code's readers through their literals, the
  record table's `add #0xBA` made #0x200; the modules' literals through their relocations, each made a copy of its
  neighbour so the loader leaves the room's address: `PointLiteralAt`). All of X/Y: 49 numbers, 89 members. Checked
  headless: Grant's arena (runs bgk3, bgk4: its floor and side lights; the rest of the model is dark, as the converted
  model shows off the game too), a route's (bgr1, Grant's script given X's 1 in a session copy: X's forest, grass and
  platforms). Not checked: against X running (no X save), the scenery drawn (random), the bytes 1, 8, 9 (carried, the
  same ranges in both games: inferred the same meaning), other scripts' battles naming a background (only
  `_CallTrainerBattleCore` moved); X's tiles naming 106, 107, 110 (no X value; 12 tiles of pieces 346-348) cleared.
  Found after (owner, 22 October: the forest's platform looked wrong): X's converted ground drew a raised block over the
  backdrop (runs bgf1-bgf3; with Omega Ruby's own ground, the same model, it draws right, run bgf_ix). The difference was
  not found in the file (geometry, textures, materials, metadata compared); a lone model the same as Omega Ruby's in every
  part the reader sees is now Omega Ruby's file (owner: shared only when 100% the same; run bgf4: the forest right). 2 of
  X's grounds are so. The block's cause, found after: oras-engine left DllBackGround's grounds (and second light, sceneries)
  on the game's tables (a relocation not neutralised, fixed in PointLiteralAt), so X's grounds from 93 read past them; the
  8 converted grounds now draw right (runs bgv1fix, bgw5-bgw62: beach, snow, cliffs, city, town, meadow, autumn, route).
- **Done headless (22 October): Kalos's context intros, X/Y's own** (leaders, Elite Four, Champion). X's sequence played as
  is in Omega Ruby (Grant's, X/Y a/0/3/6 832, its name's texture renamed) stopped the game (null read at 0x0078086C,
  run xi1). Read, step 1 (the format): a sequence (Omega Ruby's a/0/3/4, X's a/0/3/6) is "SESD", u32 2, u32 0, u32 the
  last frame, then records {u32 start frame, u32 end frame, u16, u16 (version 2), u16 command, arguments} ending with
  u32 0xFFFFFFFF; the static player (FUN_003C513C loads, FUN_003C4F98 steps, FUN_003C4DF0 reads a header) hands each
  record to DllBattle's object and moves on by the argument size of a per-command table, or further where the command
  read more (some commands' arguments vary). DllBattle's tables: Omega Ruby's 213 commands (handlers 0x1012E5B0, sizes
  0x10108800), X's 186 (0x1011EEF8, 0x100FB1F8). Omega Ruby added 27 commands, inserted (0x11, 0x1C, 0x24, 0x25, 0x29,
  0x2C, 0x38, 0x4D, 0x5A, 0xA5, 0xAB-0xBB): X's numbers are shifted, hence the crash. X's commands mapped to Omega Ruby's
  by an ordered alignment of their handlers' code (masked branches and relocated words); confirmed by the variable-size
  commands, which pair the same (X 0x6C, 0x19, 0xB8, 0x67, 0x6E, 0x2E -> Omega Ruby 0x75, 0x1A, 0xD3, 0x70, 0x77, 0x34).
  X's 13 Kalos intros (a/0/3/6 831-843) parse whole and use 25 commands; 23 take the same argument size in both games,
  2 one u32 more in Omega Ruby, always 0 there, after X's same bytes (X 0x14 -> 0x15: 32 -> 36; X 0x97 -> 0xA0: 16 ->
  20). Handlers paired by a weaker likeness: X 0x14, 0x5B, 0x97, 0xA5 (-> 0x15, 0x64, 0xA0, 0xC0), checked in use below.
  Step 2, **done headless (22 October)**: the resources. Command 0xA0's first argument is a screen layout of a/0/3/3
  (X's a/0/3/5: gym_01-08, big_four_01-04, champion, the same names as Omega Ruby's but other files: X's appended); 0xA9
  names a pane and a texture of a/2/1/4 (the drawn name, X's under its new name; gym_vs, big_four_vs, champion_vs the same
  file in both games; champ_font another, X's added as champ_font02); the effects (0x5D, 0x64) name the same numbers with
  the same arguments as Omega Ruby's own intros (a/0/3/4 844). OrasXySequence.h converts; AddXyTrainers plays X's own
  sequence for each Kalos intro (the Omega Ruby copies and the Champion name's crop removed). Runs xi5 (Grant: his
  silhouette, X's gym_02 layout, his portrait and name, the battle goes on), xi5_170 and xi5_174 (his battle given the
  Elite Four's and the Champion's intro: X's stained-glass and rose-window screens, "Thymeo", "Diantha / Maitre de la
  Ligue"). Not checked: against X running, the Elite Four and Champion in their own battles, on the phone.
  **Then (22 October): X/Y's battle types, intros and trainer classes ported whole.** X/Y's 64 battle types appended
  (73-136) and its 32 intros as its own sequences, with the layouts, models, motions and sounds they load; X/Y's 178
  trainer classes appended (280-457, the game made to load 460: OrasEngine.cpp), each with its own drawings (a/1/6/0,
  a/1/2/1). X/Y's trainers use intros 0xD (571 of them), 0xE (163), 0x11-0x1D (the League) and 0x21 (1); 0x1-0xB are
  not a class's (wild battles: their type is the encounter effect entry's byte +4, +5 for a horde, read 22-23 October:
  OrasXyEncounters.h; all nine of X/Y's wild intro sequences, 819-827, load X/Y's intro camera 0x201, a/0/3/4 513).
  **Fixed 23 October (run f1): X/Y's wild intros play.** The cause was storage, not the camera: Omega Ruby's battle
  loader reads every a/0/3/2 member through its decompressor (all 1017 of the game's are LZ11); the port appended X/Y's
  members as X/Y stores them, and X/Y's intro camera (a/0/3/4 513) is plain there, so the loader's thread read it for
  ever (run k1: `report` twice a minute apart, thread 216 at the same place under Res_AsyncLoadJob_Run, thread 217 in
  Res_ReadCompressedEntry) and every X/Y wild intro stayed black. Appended members now follow the archive's own storage
  (OrasXyTrainers.cpp); f1: X/Y's grass intro (sequence 819) whole with X/Y's camera (Pansear, the battle on). Read in both
  games' code on the way, all the same in X/Y and Omega Ruby: the camera command (Omega Ruby 0x1F FUN_100B1794, X/Y 0x1D
  FUN_100A796C, from the two command tables 0x1012E5B0 and 0x1011EEF8), the sequence preload (FUN_10038748), the load
  request (FUN_10098EEC, no bound on the number), the per-frame camera update (FUN_1005C5B0 / FUN_100585B0, the same 10
  element names). One difference: Omega Ruby fills the up vector only for a LookAt view (FUN_1005C418), so the port also
  writes X/Y's Aim cameras as LookAt (an unanimated twist of 0: the same view); whether this is still needed with the
  storage fixed is not tested. Wrong turns, so they cost nothing again: the camera's absent X curves, its view type and
  its number were each blamed before the storage was looked at; `prototype/gamefs.py members` now flags a member stored
  unlike the rest of its archive. **Hordes, checked 23 October (session hx2)**: in X/Y's zone 264 (its horde table, the chance
  raised to 100 in a diagnostic variant), a step in plain grass (tile 0x20) starts a horde of five Hoppip with X/Y's
  intro and its horde camera. Two ports made it: X/Y's horde camera (X a/0/3/4 0x202, named by no sequence) carried
  beside its intro camera and swapped in by FUN_1009905C's horde test (oras-engine --xy-band-camera); X/Y's horde
  tiles (X/Y rolls hordes on its tables 0-3, plain grass included; Omega Ruby on tile kind 0x27 only) in X/Y's zones.
  Wrong turns: the 'horde' of the controls wb4 and s3 was Omega Ruby's horde battle type forced on one Pokemon; and
  four horde tests ran at the zone's own chance, because mod_variant.sh rebuilt each change to one archive from the base
  mod, the last change dropping the earlier ones (fixed; found by reading the loaded file in the game's memory: `dump`
  of the heap, the encounter file found by its slots). **Scripted wild battles, checked 23 October (session sw)**: both games' `CallWildBattle(n)` start entry n of a
  static table of 12-byte entries read alike (Omega Ruby's 59 at DllField 0xF1B20, X/Y's 13 at its 0xEE46C): X/Y's 13
  appended at the start of DllField's room and named 59-71 by X/Y's scripts (oras-engine --xy-static-wild, XyMoveWildBattles),
  their types +73 and backgrounds made the mod's; a sandbox trigger calling 59 fought X's Snorlax, the control calling
  0 Omega Ruby's Poochyena. Not established: what Omega Ruby's mode 3 extra (flag 0xBCE cleared) means for X/Y's
  legendaries. Sweet Scent and X/Y's field encounters: DEVELOPMENT_NOTE.md 3e step 1. **DllField grown, checked 23 October
  (session sg)**: its cave was full, so `oras-engine --field-room BYTES` grows its code segment (GrowCroCode,
  OrasEngine.cpp): zeros inserted at the end of the code's last page (file 0xF1000), everything after moved (header
  offsets, segment table, the file offsets inside the export and import tables; segment tags unchanged), the code segment
  and the header's code region longer, so the loader maps the room executable. oras-xy-world asks for 0x20000 (128 KB,
  the owner's margin): caves from linked 0x10333000 to 0x10353000. Checked: every table, name and the data segment read
  the same as the original's (a script reading both files); the game loads it (`DllField loaded at 0x006F3000,
  fixed_end=0x00835000`) and the scripted wild battle code placed in the room runs (X's Snorlax; control Poochyena).
  Since then DllField's read-only segment and its data run 0x20000 further than the linked image (cro_link.py, Ghidra)
  says: an address read there is the linked one + 0x20000 in a grown mod. **All 9 of X/Y's wild intros checked 23 October (session iv)**: one game, the field state loaded
  before each battle and X/Y's effect entry 0's battle types written in memory (`poke`: DllField at 0x6F3000, the entries
  at 0x7E353C, types +4/+5) to X/Y's types 0, 5, 6, 7, 10, 19, 23, 25, 26 (intros 1, 3-8, A, B; 2 checked in f1): each
  plays whole (speed lines, water, cave, snow, sand, falling leaves, shadow, hexagons) with X/Y's own lines ("Votre
  monture a failli écraser...", "... fond sur vous !"). Not checked: the phone. Checked
  headless on the sandbox (demo.txt with X/Y's trainers, oras-sandbox --onto): X/Y's trainers 1 (Youngster, 0xD), 107
  and 130 (0xE) and 188 (Korrina, 0x21) play their X/Y intros with their own drawings and names (runs xf_17-xf_27).
  Not checked: against X running, on the phone.
- **Before, the owner's item: Grant's own battle intro.** X's leaders show Brawly's portrait and name
  ("Bastien") in their intro. Found: the intro is chosen by the battle type record's word +4 alone (0x27-0x2E Omega
  Ruby's 8 leaders, 0x38-0x3E the Elite Four; run hI2: Grant's type given 0x27 shows Roxanne's portrait, name and rocks);
  the record is copied into the battle's settings at +0x2C (so +0x30 holds it). **Found by the owner's search (22
  October, every BCLIM and CGFX texture of the 299 archives listed by size, session scan; not yet linked by the code):
  the drawn name** is a/2/1/4 member 0, one per language (0, 0_2-0_8; French 0_3), a DARC "timg" of 18 BCLIMs:
  gym_name01-08 (RGBA4 256 x 64: Roxanne, Bastien, Voltère, Adriane, Norman, Alizée, Lévy & Tatia, Marc, decoded and
  read), big_four_name01-04 (Damien, Spectra, Glacia, Aragon), champion_name (Pierre), gym_vs, big_four(_vs),
  champion_vs, champ_font. **The portrait, likely**: a/1/6/0, 549 RGBA8 512 x 256 BCLIMs, every trainer class's
  drawing in threes (small, large, silhouette): Brawly 33-35, Steven 12-14, Flannery 39-41, and Kalos's leaders
  among them (Grant 378-380, Viola 375-377, Clemont 387-389, Olympia 393-395, Wulfric 396-398; read by eye). Next:
  the code reading a/2/1/4's gym_nameNN and a/1/6/0's member from the intro type. **The chain, read 22 October** (Grant's
  battle, runs gi3/gi4 on the A2 recipe; RomFS read trace, Ghidra): the portrait already is the trainer's own (gi3: Grant's
  drawing, a/1/6/0 378-379 read by his class), only the name is Brawly's. The intro id (the battle type record's word +4)
  indexes a u32 table in DllBattle at 0x10105CDC (162 entries, ids 0-161; read by FUN_10068370 through its literal word
  0x10069278) giving an a/0/3/4 member, the intro's sequence: 843-850 the 8 leaders (0x27-0x2E), 851-854 the Elite Four
  (0x38-0x3B), 855- the Champion's and others. Each leader's sequence loads the shared layout gym_01 (a/0/3/3 member 2,
  read for every leader) and names its pane `gym_name01_00`'s texture: "gym_name0N.bclim" (844: gym_name02, Brawly);
  FUN_100af3e4 reads the two 0x18-byte names from the sequence and finds that texture in a/2/1/4's DARC (the player's
  language, 0_3 in French), loaded at the battle's start by FUN_100d07c4 (object +0x9BC). So a Kalos leader's own name
  needs its own sequence (a copy of 844 with gym_name09-16), X's names added to a/2/1/4's DARC under those names, and
  intro ids whose table entries give the new sequences (the table to extend or unused ids to take). The read trace's
  offsets + **0x344000** (not 0x563C6C: checked on DllBattle.cro's read, 1,089,536 bytes at 0x2DAD0, the file at
  0x371AD0) are image offsets. **X has Kalos's own** (same
  search on X): a/2/4/1 member 0, the same DARC per language (0, 0_2-0_8, French 0_3) with the same 18 file names:
  gym_name01-08 Violette, Lino, Cornélia, Amaro, Lem, Valériane, Astera, Urup (RGBA4 256 x 64, decoded and read),
  big_four_name01-04 Thyméo, Malva, Dracéna, Narcisse, champion_name Dianthéa (512 x 128); X's trainer drawings
  are a/1/7/8 (273 RGBA8 512 x 256). Before (21 October): where the portrait and the
  drawn name come from. A RomFS read trace (below) of the same battle with 0x28 and with 0x27 differs only in a/0/3/1
  646 (a CGFX particle effect: Roxanne's rocks) and a/0/3/2 576/598/604 (move effects): the portrait is read by both,
  or from a file the mod patches (LayeredFS: not traced), or held in memory from earlier. Not it: a/0/3/3 (the intros'
  effect layouts gym_0N/big_four/champion: sparks, "VS"), a/1/6/0 440-443 (512x256 drawings of X's Tierno), a/1/5/2
  cut-ins, the switch at DllBattle 0x1003F400 (battle commands). Next leads, first the owner's (21 October): a search of the whole game by file type, then place, then name: every
  image (BCLIM, packed ones included, and every BCH and CGFX texture) in every archive, those that may be a portrait or a
  drawn name (256 px and up, or wide and short) listed by archive and member, grouped (8 leaders + 4 Elite Four + the
  Champion should stand out), their names read, a contact sheet to find Roxanne's and Brawly's by eye; then the code
  that loads that member. Else: trace the reads from the battle's start
  (frame of the "veut se battre" line) with the mod's patched archives logged too; or read DllBattle's use of the
  settings' +0x30. X's own intro: a layout (a/1/1/3) with Kalos's 8 badges in 2D, the leader likely his 3D battle model.
- Then, the owner's order: the badge drawer (Kalos's models, names and outlines in the case, with the animated
  Hoenn/Kalos switch and the case's look and colour: 3d item 9); later the League's order and each region's Elite Four
  and Champion (with C3).
- RomFS read trace (a session diagnostic, never committed): in `<work>/core-src/azahar/src/core/file_sys/romfs_reader.cpp`,
  `DirectRomFSReader::ReadFile` logs `offset` and `length` when `POMEGRADE_ROMFS_TRACE` is set; `ninja -C build-azahar
  citra_libretro`; the log's offsets + 0x563C6C are the image offsets of `N3dsRom::Files()` (Omega Ruby), a GARC member
  then read from its FATB. Files the mod patches are read by LayeredFS from memory, not logged.

**A file-swap test, the short way** (23 October, after a day lost to hand-made copies): build the base mod once; `headless/mod_variant.sh <work> <base> <work>/out/var
PATH=FILE PATH:N=FILE...` makes the variant (whole files, or archive members through `remake_tool mod-file` and
`garc-set`, each read back); run it; `prototype/shot_sheet.py <run dir> <out.png>` puts the shots on one labelled sheet;
with `POMEGRADE_ROMFS_TRACE=1`, `prototype/romfs_member.py <game.3ds> --log <run>/log.txt [--after FRAME]` lists what was
read, grouped by archive (files the variant gives are not traced). `prototype/cgfx.py` lists a CGFX file's cameras and
animations (view type, curves absent, constant or animated). **Method**: run the control first (the same variant with the
game's own file), then change one thing per run.
**The game's files** (23 October): `prototype/gamefs.py <work> ls|members|get|find|diff|where ... [--mod MOD] [--game
x]` lists the RomFS (what the mod does to each file), an archive's members (size, type, first name in each), gets a
file or member as the game or the mod has it, finds the members holding a text or bytes, lists the members a mod
changes, adds or drops, and names the file and member at an image offset; archives are extracted once into
`<work>/fscache`. First use: the mod's a/0/3/2 member 1031 is a BCH named `tile_test`, not X/Y's band camera
(a/0/3/4 514) that the ported sequences' 0x407 names (not looked into further).
**A game kept running** (23 October): `headless/session.py start <work> <name> --mod <mod> [--save S] [--hot PATH]`
boots once (about 7.5 min for a mod not run before: its title state is made first), then `session.py do <work> <name>
"<commands>"` runs retro_host commands in the same game and prints only what they report, with a sheet for two shots
or more; `stop` ends it. retro_host's `until COND MAX [KEYS:N,...]` runs until a condition (`log:DllBattle`, `change`,
`still:N`, `bright>X`), walking if a key cycle is given: a wild battle in about 100 frames (session s3), where whole
runs were lost to steps without one; `burst NAME EVERY COUNT` takes a series of shots. `save NAME` / `load NAME` go
back to a moment: s3 fought the same battle again after `load field` (Pidgey's colours looked off after the load: not
looked into). Not working: `session.py swap` of a/0/3/4 (see its help). A forced-encounter patch tried on 23 October
(StepCheck's rate exits skipped) left every battle black (sessions s1, s2 against s3) and was removed.

**Pitfalls met, so they cost the next agent nothing**:
- A GARC rewritten by hand needs its data section's size (FIMB, the word before the data offset) and its file size
  changed with its members: a test on 22-23 October whose GARC kept the old size failed for that alone, and its result
  (a "resource number bound") was wrong. Use `remake_tool garc-set`.
- Two runs of different mods do not draw the same random numbers (their title states differ): encounters, hordes and
  wild Pokemon change; compare within one session (`session.py`, `save`/`load`), or walk with `until`.
- `pkill -f <word>` kills the calling shell when the word is in its own command line (exit 144), here too for
  `code_find.py`.
- A run's log can grow to gigabytes after a fault and fill the disk: watch it (`stat` in a loop) and stop the host with
  `pkill -x retro_host`. Never `pkill -f` with a word that is in the watcher's own command line: it kills the watcher's
  shell (exit 144). `ulimit` is refused in these containers.
- `fast_run.sh` makes one title state per mod, save and move (the first run of a new mod boots once more, minutes). A run
  killed while making it can leave a bad state: the next runs show a black screen at 30 s; delete `<work>/states/<key>`.
  Its "field not up" is often wrong: look at the shot.
- A shot taken 30 frames after a press can miss a whole line of text: shoot right after each press to see dialogue. The
  yes/no box opens on the press that ends the question; a second A answers "yes" before a "down" reaches it.
- Flags read at 0x8C7DF78 (0xCE1 bit 1, 0xCE2 bit 2) only in a mod without `--story large`; with it their address is not
  known: check by what the scene shows.
- The owner's saves (`headless/saves`, `dumps/save`) are early in the game: a gym leader's battle is lost even against a
  level-1 team when mashing A picks Growl. A real win needs a weakened team (`oras-sandbox ... --onto`, below); a "won" result forced in a script is not a win:
  the engine took the loss path (point 1).
- gdb through the headless stub did not connect reliably here. What worked: save a state with the JIT just before the
  moment (`save NAME`), then an interpreter run (`POMEGRADE_INTERPRETER=1`) that loads it (`load NAME`: the state keeps its
  file name, `load title` finds nothing) and traces only that moment (`trace on` / `trace off FILE`, a minute or two).
  Compare two traces (one that works, one that does not) by function: DllBattle and DllField load at the same address
  (0x6F3000), so a trace over a battle mixes their code; and `ghidra/function_names.tsv` can give a block the name of the
  function before it.
- Test changes to an X/Y zone's script go through `oras-xy-world --scripts DIR` with a copy of `xy-script-export`'s
  source kept outside the repository; `xy-script-export` of all 360 zones assembles all 720 scripts, a quick check of
  any translator change.
- The tools those runs needed are `remake_tool` commands (owner's request, 21 October): a zone as a mod leaves it, with its
  characters' and triggers' raw words, `oras-inspect <oras.3ds> zone N --mod <mod dir>`; a trainer's team changed in a mod
  already built (a weak team for a won battle; X's trainers are 950 and up), `oras-sandbox <oras.3ds> <mod dir> <team
  description> --onto <mod dir>` with only a `team ID` block; a zone copied as a new one, `oras-zone-copy <oras.3ds> <out
  dir> Z [--onto <mod dir>]`. `--onto` rewrites the mod's archives (`OrasWorkspace::LoadMod`) and leaves its other files.

**The owner's objectives, in order** (their words, 7 October): (1) take the game apart to the binary and learn every part;
(2) reference every table and asset; (3) a limitless sandbox; (4) remove the memory, cache, zone and asset limits; (5)
patch the game where it is in the way; (6) Sinnoh rebuilt on a blank map, nothing tied to Hoenn; (7) a story mod.
**(6) dropped by the owner (17 October)**: Sinnoh is no longer in the project, new assets cannot be made from nothing.
The project is Equilibrium (`docs/Equilibrium_Scenario.md`), Omega Ruby joined with X: X's characters and maps brought
into Omega Ruby. The tools stay in use (sandbox, regions, engine patches, map pieces work on any map, X's included);
only the Sinnoh-specific goals and notes below are history. Step
1-5 are under way here; 6 and 7 wait on them. The owner tests mods on their phone (Pomegrade, `ORAS_LITTLEROOT.md` 0).

**Where each stands**:

| Objective | State |
|---|---|
| 1 Decomposition | the whole code (`.code` + 145 modules) in one Ghidra program, checked against the game's own load; Ghidra's wrong no-return marks cleared (`FixNoReturn.java`: code was missing from 1,753 functions); **3,831 functions named** (`ghidra/function_names.tsv`, read or guess), from coverage traces (section 6: entering a zone, a door warp, a sign, the start menu) and from the game's own tables: **every script native** (799, all 16,321 native calls of the 1,072 zone scripts resolved), the script machine (load, run, wait, natives by mask), the message command `TalkMdlMsg_Seq` **checked live** with its first argument (section 6); the names **reviewed** on 7 October (99 changed: section 6), 1,384 still marked guess, to be read before they are relied on. talking to a character **traced and checked** (section 6: 307 functions beyond a sign, all but 10 named); the wild encounter selection **read and checked live** (section 6: the step check, the zone's encounter file, its tables, the slot and level pick; a wild Wurmple at level 2 in run `enc2`); the trainers **read and checked live** (section 6: a battle against trainer 7 in run `trainer3`); saving **read and checked live** (section 6: the blocks' classes, the save thread, the menu's save; run `sv2`); the region map (needs touch input in retro_host). Then objectives 3 and 6 |
| 2 Tables and assets | zones (section 2), map pieces (3), the boot memory map (4.2), the 299 archives tied to their code where opened by a constant (5); not yet: the 210 archives opened by computed numbers, the asset formats beyond `tools/remake/src`'s readers |
| 4 Limits | **done for building a world** (section 4.6): all 927 fatal checks listed, the field's and the `.code`'s classified; lifted and checked headless: zones 536 -> 1024 (2), application memory 64 -> 124 MB (New 3DS mode) and the linear heap 43.3 -> 88 MB, the normal heap and heap 4, heap 0xC 2 -> 8 MB, heap 0x17 28.4 -> 64 MB (4.4), characters past 26 (4.5); refused at build time where they cannot be raised: a zone's events file under 0xC84 bytes (4.5), a piece model's 51 textures (4.6), the 178 MB mode (4.4); the zone scripts' native masks for zones 536-1023 (section 6, `--script-mask`); left with their reason: the 8-deep load queue per object, collision objects per cell, the 174-entry Secret Base table (4.6) |
| 5 Patches | `remake_tool oras-engine` writes them all (`exheader.bin`, `exefs/code.ips`); **checked on the phone (owner, 7 October): mod `all6`** (the whole of Sinnoh as r12, the title, the save in Twinleaf, and `engine --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000 --characters 64`, Remake mod run 147) with the APK of `main` at PR #33: "everything works fine" |
| 3, 6, 7 | **3 started (8 October)**: `remake_tool oras-sandbox` builds new zones 538+ from a text description (a hand-drawn grid of tile roles, names, characters, trainers; no Hoenn events), **checked live** (runs `sand1`-`sand8`, section 2). 6 and 7 not started on this basis: Sinnoh's region tools (`oras-region`, `ORAS_LITTLEROOT.md`) still borrow Hoenn's zones; next is building Sinnoh's zones from 538 up with the tools above |
| After 6: Platinum's music | added by the owner (7 October), after building Sinnoh's map: a tool that moves Platinum's music into Omega Ruby. Not started; nothing in `tools/remake` reads either game's sound yet. Platinum keeps sequences played by the DS sound hardware (SDAT: SSEQ with SBNK/SWAR instruments), Omega Ruby recorded streams in its sound archive (BCSTM, inferred from the format's common use, not checked on this game): the tool must extract, render, encode and replace. First step: how Omega Ruby stores and picks its songs (`Snd_ChangeZoneBgm`, section 6) |

**What remains, and how** (handover, 7 October; in this order, each with rule 4's budget):
1. **Trainers: done (8 October).** Every function of the battle's trace is named (section 6, **Trainers**; 81 of the
   last 127 read, 46 guess). The battle is **checked live** (run `trainer3`, section 6, **Trainers**). To trace it again
   in about 25 minutes: the owner's save in front of trainer 7 (zone 24, tile 174.5 146.5, made in the game:
   `tools/remake/headless/saves/main_Trainer_Battle`, put in the repository by the owner on 8 October) with
   `POMEGRADE_SAVE=$PWD/tools/remake/headless/saves/main_Trainer_Battle` (an absolute path: `run_local.sh` copies it
   from inside the work folder) and `POMEGRADE_INTERPRETER=1 run_local.sh <work> trainer3 - "" "mash a 15;wait 500;shot
   start;trace on;press a;wait 150;shot talk;mash a 10;wait 400;shot battle;trace off trainer.txt;wait 200;shot battle2"
   150`, launched with a Bash `timeout` over 30 minutes. What it ran: `coverage_map.py <work> trainer3 trainer.txt
   --minus enc1/enc_grass.txt --minus enc1/enc_idle.txt --minus enc2/enc_battle.txt` (`decompile.sh`, then names). A
   save moved by `oras-save` into another matrix does not load (`trainer1`): walk through an edge warp instead
   (`trainer2`, Oldale 83.5 101.5 to Route 102), or use a save made in the game.
2. **Encounters: done (8 October).** Section 6, **Wild encounter selection**. Open, not blocking: what kind 2 is (no
   encounter starter found; the species list reads it) and which screen shows member 537's list.
3. **Saving: done (8 October).** Section 6, **Saving**: the 58 blocks named by the game's own classes, the save
   thread, the menu's save (DllReport), **checked live** (run `sv2`).
4. **Region map.** Needs touch input: add a `touch X Y FRAMES` command to `headless/retro_host.cpp` (the libretro
   pointer device), then trace it.
5. **Names to read.** 1,384 names are still guesses (`function_names.tsv`, last column `guess`) and 10 talk functions
   unnamed (section 6, **Talking to a character**): read before relying on them; `prototype/name_review.py suspects`
   flags the likeliest wrong ones and `summary` reads a decompilation fast.
6. **The sandbox (8 October): built, checked live, phone test next.** `remake_tool oras-sandbox` (section 2) builds zones
   538+ from a text description; the demo is `tools/remake/sandbox/demo.txt` (two zones, a pond, tall grass, a ledge, a
   house and its inside, a fence, a person, trainer 10). For the phone, the Remake mod workflow's variants (its header
   lists them): `sb: sandbox tools/remake/sandbox/demo.txt`, `eng: engine --zone-rows 541 --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000 --characters 64` (the memory options are needed on the phone: `--zone-rows` alone froze the owner's phone at boot on 17 October, mod `test2` with them started; headless runs start either way, so a headless start does not prove a phone start), `sv: save 538 14.5 20.5`,
   `demo: merge sb eng sv`. Headless reruns: `headless/fast_run.sh` (about 3 minutes). Not built yet: east and west
   ledges, relief (hills, cliffs), warps between matrices, writing scripts, trainers and encounter tables of one's own.
   Camera and light done (8 October, section 2): `camera ...` and `light ...`, read and checked headless; this closes
   the sandbox's "reused from the game" column.
7. **Zones past 536, the rest.** A new zone needs its own file 3 (encounters) beside its member 537 entry (section
   2), and a script mask word (`oras-engine --script-mask`, 0x243 by default; lifted 8 October, section 6).
   Then objectives 3 and 6: Sinnoh's zones from 538 up.

The tools made for this on 7 October are in **Setting up a session** below (`decompile.sh`, `peek.sh`, `oras_tables.py`,
`coverage_map.py --minus <run>/<trace>`, the host's `speed:` line). Every run's work folder stays outside the repository
(rule 8); merge to `main` the same day (rule 7).

**How to work** (the owner's rules, 7 October, after a session lost hours to slow and failed runs; read before any run,
they hold for every run):
1. **Copy before you invent.** Before any run, open the last run that did the same kind of thing (`runs/<name>/script.txt`,
   `log.txt`, `ORAS_ENGINE.md` 6) and reuse it exactly; change one thing at a time.
2. **One heavy job at a time.** Never run the emulator, Ghidra or a core build together, and never several in one shell
   command. Check `nproc` / `top` first.
3. **Measure in the first 5 minutes.** Read the frame counter after 3-5 min. Under 4 frames/s, stop and find out why;
   never wait 30-60 min hoping it gets better.
   (Read from the logs of 7 October's runs, alone on 4 cores: at 5 minutes the frame counter stood between 896 and 1,103,
   about 3.0 to 3.7 frames/s, in every run, the failed ones too; the map loaded between 230 and 348 s, frame 886 to
   1,361. Averaged over a whole run it reads lower, 1.7 to 3.9 (why is not established): judge a run at 5 minutes, not by
   that average. The previous session reported 5.6 frames/s for its sign trace (not checked here: its logs are not kept).
   The two runs that failed, `sign1` and `sign2`, were not slow: the map loaded, but the script's `field` command never
   saw it and the script waited until the run was stopped; use fixed waits, as the sign recipe in section 6 does.)
4. **Budget per attempt: 10 minutes, at most 2 failed attempts** on the same thing. After that, stop and write to the owner
   what failed, the evidence, and one proposal. Don't keep running.
5. **Static first, live to confirm.** Read the code to find the answer, then run the game once to confirm it with a trace.
6. **Short reports**: what is checked (with the evidence), what is a guess, the next step. No logs, no history.
7. **Every result goes to its home the same day**: names in `ghidra/function_names.tsv` (read|guess), the finding in
   `ORAS_ENGINE.md`, a tooling fix in the script that failed (e.g. add `gdb-multiarch` to `session_setup.sh` if you need
   it). Commit, push and merge it; never leave a result only in the scratch folder.
8. **Nothing in the repository from a run**: work folders and dumps stay in the scratchpad; check `git status` before
   each commit.

**Setting up a session** (`tools/remake/headless/session_setup.sh <work dir>`): fetches the dumps from Drive into a work
folder outside the repository (ids in `POMEGRADE_ORAS_DRIVE_ID`, `POMEGRADE_PLATINUM_DRIVE_ID`, `POMEGRADE_SAVE_DRIVE_ID`:
the files of the owner's Drive folder "Pokemon Project - Radiant Platinum"; the Drive connector may not list recent uploads,
so ask the owner for share links), builds `remake_tool` and the code image, builds Azahar's libretro core with the headless
patches (about 30 min on 4 cores, once) and the Ghidra program (15 min on 7 October: the analysis, then `FixNoReturn` and
`ApplyNames` in a pass of their own; checked on a fresh folder on 7 October: 47,750 functions, the analysis's 47,557 and 193
more, 190 made by `ApplyNames` where a named address had none; it also writes `functions.tsv`, `edges.tsv` and
`noreturn.tsv`). Run the steps one at a time (`session_setup.sh <work> core`, then `ghidra`): together they slow each other
(rule 2). Then:

- **Run the game** (`tools/remake/headless/run_local.sh <work> <name> <mod|-> "<zone x z>" "<script>" [seconds]`): the game
  runs at about 3.0 to 3.7 frames/s in its first 5 minutes (read from the logs of 7 October, alone on 4 cores; `msgdump`
  with the JIT at frame 1,086 after 5 minutes, `actE2` under the interpreter at 963), and a run to the map and one action
  took 6 to 13 minutes (`actE1` 380 s, `msgdump` 513 s, `actE2` 772 s). The field comes up between frame 886 and 1,361 (it
  varies between runs: wait 500 frames after `mash a 15` before acting, as the sign recipe in section 6 does; the host's
  `field` command, which waits for the map, never saw it in two runs, `sign1` and `sign2`, which waited until stopped). The
  owner's save stands in Littleroot (zone 6, tile 104.5, 170.5); move it with the zone and tile. A zone's characters are
  placed when the player **enters** it: to test them, start next door (zone 23, 100.5 150.5) and walk south. `shot NAME`
  saves the screen (with drawing off, never `draw on` just before it: that frame comes out black, `shot` alone draws its
  own frames; runs `aqua93`, `dew8` black against `dew8s` drawn, 11 October) (the host's own "field up" detection is wrong under some mods: look at the picture). `report` lists every
  thread; one in the fatal-error loop shows pc 0x11EF50 / 0x11ABxx / 0x110Axx, `0011EF60` in its stack code addresses, and
  its registers and raw stack, from which the frames' saved registers give the object that failed. The host prints `speed:`
  once a minute of real time (frames/s over the last minute and since the start; the other `[frame N]` lines come only with
  an event, so the last of them is not where the game is: 7 October). `headless/peek.sh <work> <name>` looks at a run that
  goes on: its speed, the last commands it reached and its screenshots so far as PNG. Saves made by the owner in the game
  are in `headless/saves/` (put there by the owner, 8 October): `main_Trainer_Battle` in front of trainer 7 on Route 102
  (zone 24, tile 174.5 146.5; run `trainer3`), `main_Startpoint` (zone 6 at tile 104.5 890.5, the position of the 6 October
  Sinnoh tests, not the owner's Littleroot save at 104.5 170.5) `main_Wild_Encounter` (made by the owner for wild
  encounters, 8 October: zone 24 at tile (183.47, 134.85)) and the save's `00000001.metadata`; a run starts from one with
  `POMEGRADE_SAVE=<absolute path>`. **Moving the save into another zone** (8 October): `oras-save <main> <out> <zone> <x>
  <z>` writes the zone (block 4, +2 and +0xF4), the position (both copies) and, on a zone change, block 10: the characters
  of the zone the save was made in (records of 0x108 bytes: number, zone, model, tile; `prototype/oras_save.py characters`
  decodes them), which the game restores on loading. The player's record stays in its place with the new zone, the others
  are emptied, and the game places the destination's characters. **Checked**: the owner's Littleroot save so moved loads in
  zone 8 (matrix 3, Dewford) and in Petalburg (matrix 2: one run of three, the two others stuck before the field, a fault of
  the test, not of the save: `save_test.sh`); kept, the old characters leave the screen black once DllField loads (runs
  `matrix2`-`matrix8`). `--template <save>` takes block 10 from a save the game made in the target matrix instead. Seen, not
  read in code: the record layout. Test saves: `headless/save_test.sh <work> <save>...` (LOADED, BLACK, or UNSURE when the
  run sticks). A JIT run boots at a varying speed: `wait 700` before `mash a 15` (`matrix1` stayed on the title). **Fast runs** (8 October): the software renderer
  waited on its 4 workers after every triangle (60% of the cores idle, 21% system time); `patch_core.py` lets the host skip
  drawing (`draw off`; a shot draws 3 frames first) and set the worker count (`POMEGRADE_SW_THREADS`), and lets a state made
  by the rebuilt core load (its revision reads "UNKNOWN"). With `POMEGRADE_STATE=<work>/title.state` and `wait 120;load
  title;mash a 8;draw off;wait 900;...` a run reaches the field and walks in 2.5 minutes (run `fast6`) instead of 10-12. The
  title needs drawing (with it off from the start, A was never taken: `fast1`); a title state made by an older core build
  must be made again (`wait 700;save title`). A title state holds the save read at boot and the mod booted with (`sand5`: a state made without
  the mod showed Littleroot with a save moved to zone 538): `headless/fast_run.sh` keeps one per mod, save and move and
  reuses it (first run about 10 min, each rerun under 3: `fr3`).
- **Show that a function runs**: `trace on` / `trace off FILE` around the action (needs `POMEGRADE_INTERPRETER=1`), then
  look for the function's running address in the file (a DllField function at its linked address - 0x10242000 + 0x6F3000) or
  map the whole trace with `prototype/coverage_map.py` (`--minus <run>/<trace>` subtracts a trace of another run). Not GDB:
  `gdb-multiarch` is not installed by `session_setup.sh` and the stub dropped the connection in both tries of 7 October; on
  22 October (runs bt1-bt6, with it installed) no breakpoint fired under the JIT on code the run did execute, the stub dropped
  the connection under the interpreter, and attached before a dialogue it kept the game from taking keys.
- **Read a value while it runs**: script commands `mem`, `watch` (a word's changes frame by frame), `dump ADDRESS LENGTH
  FILE`. A script's state: `dump 0x08000000 0x6000000 heap.bin` while the thing is on screen, then
  `prototype/amx_dump.py heap.bin --script <size>` lists the loaded scripts, their contexts (native mask, wait) and the call
  frames left in their stack (how `TalkMdlMsg_Seq`'s first argument, 16, was read).
- **Find where a value comes from** (8 October, the light; section 2): `lights on` / `lights off FILE` (the GPU's light
  registers and shader uniforms at each draw, with counts: what the screen really gets); search both dumps, the heap
  `dump 0x08000000 0x6000000` and the linear heap `dump 0x14000000 0x2B48000`, for every form of the value (float, byte,
  GPU register) and eliminate the known copies; `poke ADDRESS VALUE` (a word written while the game runs; at the title,
  after `load title`, to change what a zone load reads); `writers LO HI` / `writers off FILE` with
  `POMEGRADE_INTERPRETER=1` (each write into the range with its instruction and return address), then read that code.
  **Who reads a field**: `readers LO HI` / `readers off FILE`, the same for reads (22 October, the battle type's words: the
  settings found by `writers`, then their readers, and those of the copies they were made into). The lines come in time
  order; memory freed and given to another object is read by its new owner too, so stop at the owner's last use. The
  interpreter is about 10 times slower: save a state just before the moment in a JIT run (`...;save pre`), then run the
  interpreter from it (`POMEGRADE_STATE=<work>/runs/<run>/pre.state`, `wait 120;load pre;readers ...`).
  The full decompilation for grepping: `Export.java` with every address of `functions.tsv` (15 min, 47,750 functions).
- **Area packs read so far** (section 2): file 4 the light's colours, file 6 the camera presets, file 3 read by
  `Field_AreaTriggers_Init`, file 7 by `FUN_1025EF50`; `a/0/5/9` holds tables of file 4's layout for battles.
- **Read code**: `ghidra/decompile.sh <work> <out> ADDRESS... [--callers ADDRESS] [--callees ADDRESS]` decompiles the
  functions holding those addresses, or calling or called by one (the call graph `edges.tsv`, which follows the import
  stubs: the script natives that reach a function show up), with their names, into `<out>/decomp.c` (about 15 s; it runs
  `Export.java` read-only) (a module's address is its linked one: `linked/modules.tsv` gives each base, DllField 0x10242000;
  in the running game DllField sits at 0x6F3000). `prototype/code_find.py <work>/dumps/code.bin --at|--imm|--word|--calls
  ...` finds an instruction, a constant or every call to an address (`--base 0x10000000` on `linked/modules.bin` for the
  modules); `prototype/cro_dis.py <module.cro> dis <offset> <n>` reads a module with its imports resolved (`symbols` on
  `static.crs` and a module lists which module function each `.code` import stub reaches). `code_find.py --field OFFSET
  --within LOW HIGH` lists the loads and stores at that offset from a register in that code (a structure's field: narrow
  it, the whole code has too many structures sharing an offset). X's code: `session_setup.sh <work> ghidra-x` builds its
  own program (no names; about 10 min, 22 October: 106,082 calls in its graph), read with
  `POMEGRADE_GHIDRA_GAME=x ghidra/decompile.sh ...` (checked: X's transition object, FUN_003bcbe4); `cro_dis.py` and `code_find.py`
  read X's files as they read Omega Ruby's. **Battle sequences**: `prototype/sesd.py <DllBattle.cro> oras|x <member>...`
  lists a SESD sequence's records with the argument sizes of that game's DllBattle. **Which file a RomFS read loads**:
  `prototype/romfs_member.py <game.3ds> <trace offset>...` (POMEGRADE_ROMFS_TRACE's offsets; Omega Ruby's RomFS base by
  default).
- **Write a script**: `remake_tool amx-asm <source.txt> <out.amx> tools/remake/ghidra/function_names.tsv` assembles Pawn
  source (format in `Amx.h`, above `AmxSource`: labels for jumps, calls and `casetbl`, natives by name, checked against the
  game's tables); `oras-script <oras.3ds> <zone> [init] source` gives a game script in that format. **Checked** (8 October):
  all 1,072 scripts come back byte for byte, read and written (`AmxScript`) and assembled from their source; packing as
  pawncc writes it (a 5-group cell's top group holds 4 bits); 7,962 of 7,962 `casetbl` addresses, relative to the cell
  before each, land on an instruction. **Run in the game** (runs `r1own`, `r1sign`, `r1zone538`: `SINNOH_BUILD.md` R1):
  scripts of ours in Littleroot and in a new zone 538 run on entering the zone and for a sign. A zone's scripts are replaced
  by `oras-zone-script` (game zones) or a sandbox description's `script` / `init-script` blocks (new zones).
  **A person of ours saying a line of ours, checked live** (run `sceneB2`, 16 October): a sandbox of two zones (538, 539),
  in 539 `line Archie: La mer se souvient de tout. Moi aussi.` and `character 284 24 19 face 2 script 9`, its `script`
  Littleroot's (`oras-script ... 6 source`) with talking (g_mode 9, `L001A28`) sent straight to its talk routine
  (`L000348`, arguments line, 1, -1: -1 the person talked to, from `WorkGet(32785)`) with line 0. Walking from 538 into
  539 and pressing A shows the line in the person's balloon, accents included; A closes it and the field goes on. The
  routine chain under it (`L000348` -> `L000130` -> `L000AC8`, the 19 arguments of `TalkMdlMsg_Seq`, then
  `CheckWinAllSuspend` until the window closes) is the game's; a script written from nothing needs it rewritten. The
  sandbox finds the native names at `<description's folder>/../ghidra/function_names.tsv`: a description kept outside
  `tools/remake/sandbox` needs a `ghidra` link beside its folder. The run needs the save's `00000001.metadata` in
  `<work>/dumps/save` (without it the emulator stopped on loading the title state).
- **Write a scene** (16 October): a sandbox description's `scene` block (`SceneScript.h`: `on N`, `say L`, `wait F`,
  `flag N on|off`, `if [not] flag N` ... `else` ... `endif`) compiles to a zone script on Littleroot's, taken from the
  game when the mod is built: the scene's own `main` (the header's cip moved to it) and the library's talking and waiting
  routines, found by their code (`say` is the routine 243 zone scripts have, arguments line, 1, -1). **Checked live**
  (run `sceneC1`, `sandbox/scene_demo.txt`): talking to the person shows lines 0 then 1 and sets flag 0xCE0; talking
  again shows line 2, the flag read back. **Story flags, read** (`FlagGet`/`FlagSet` 0x10293330/0x10293344, then
  0x12FD48 and `Save_ClearEventFlag_0011F9E8`): 1-0xCFF are bits of the save (save + 0x13434 for writing), 0xD00-0x2FFF
  are fatal (`FUN_0014ea64`), 0x3000-0x303F a separate area of 64. The 1,072 zone scripts name 699 flags as constants
  (2,056 uses, 10 to 3,080) and compute 76 more, 35 of them `1740 + n`; none names 0xC09-0xCFF, the range a mod's story
  takes from the top. Not read: a use of that range by the game's own code. **More room** (19 October, read; checked headless for flags, run `hStory` against `hStory0`): `oras-engine --story large` sends flags 0xD00-0x2C3F and event works 0x4178-0x436B into save block 47 (`ReservedSaveData`, 2000 bytes, no user found, zero in the owner's saves), at EventWork + 0xA550 (EventWork is block 19, SaveData + 0x13140; measured in memory): X/Y's story names 880 flags and 176 works. The works' range check in FUN_0011fa90 is empty in the build (a work past 0x4177 lands on the flags). Checked across a save and a reload (20 October, `headless/story_save_test.sh`, runs svC-svE): flags 0xCF0, 0xD05, 0x28F0 and works 0x4170, 0x4180, 0x4300 set by a zone script, saved from the menu, are in the file at their places (block 19, EventWork's data at 0x14A00: works u16 from +0, flags from +0x2F0; block 47 at 0x20600: works 0x4178 up from +0, flags 0xD00 up from +1000), and a boot with that file holds them in memory at the title, before any zone script runs (the owner's save: 0 there).
  **Movement** (16 October): `act WHO CODE DIR [N]` sends the game's movement commands (`MdlAcmdInit`, `MdlAcmdSet`
  CODE DIR 1 0 per step, `MdlAcmdSetEnd`, then `MdlAcmdUpdate` each frame until done, as the library's command-list
  routine does: a list is triples, code, direction, 1). Run `sceneD1` (18 commands, a line between each): directions 0
  north, 1 south, 2 west, 3 east; codes 19, 13, 12, 17 move one tile (19 seen in all four, on the person and the player;
  speeds not compared), 6 turns (the player, north: its back to the camera); 3, 5, 232, 233 moved nothing, 7 not
  understood; a `say` turns the person back to the player, so turning was not seen on them. WHO -1 is the person
  talked to, 255 the player. Named: `walk WHO DIR [N]` (19), `face WHO DIR` (6), WHO `player`/`person`. **Checked
  live** (run `sceneC2`, `sandbox/scene_demo.txt`): the person walks two tiles north and back, the player turns south,
  then the lines and flag as before.
  **Fades** (16 October): `fade out|in both|top [F]`. Out is `FadeRequestOut(0, kind, F)`, called by the scripts
  themselves (284 calls: kinds 2 and 0, 15 to 45 frames); in is the library's routine that waits for the map's blocks (at
  most 60 frames) then calls `_FadeRequestIn(kind, F)`; both end with the routine waiting for `FadeIsEnd`. Run `sceneE1`
  (a scene saying a line, fading out and in with kind 2, then 0): the top screen's mean brightness 138 -> 31 faded out and
  back; the bottom screen's 77 -> 18 with kind 2, unchanged (77) with kind 0. Fading in took longer than the 30 frames
  given (the map wait). **Camera** (16 October): `camera X Z H TX TZ TH [FOV [F]]` and `camera back [F]`.
  `SetInitParam` copies the field camera as the move's start; `SetDstParam` takes an int (0 in the scripts) and 8 floats
  (`ghidra/Disassembly.java`: `vldmia s16-s23` stored where SetInitParam keeps the field camera's position, target, fov
  and a last value), read on Littleroot's calls as position x, y, z, target x, y, z, fov (30-35) and 0, in world units
  of 18 a tile (its camera tables stand near x 1,880, z 3,100: the save's tiles 104, 170); `SetFrameParam(0, F, 1, 0)`
  the length; `Begin` starts it; `IsEndFrame` is polled each frame; `End(F)` gives the field camera back. The scene
  takes tiles (x 18) and heights in tiles. **Checked live** (run `sceneF1`: `camera 27.5 25.5 4 24.5 19.5 1 30 60`
  then a line, `camera back 30`): the camera swings to a low view from the south-east framed on the person and the
  player, then back; the sandbox's map stands at the world's origin and its ground at height 0.
  **Battles and items** (16 October): `battle T` is `_CallTrainerBattleCore(0, T, 0, 0, 0, 0, 0)` as the 44 scripts
  that start a trainer's battle call it (through a 6-argument routine), a frame's wait, then `_BattleGetResult`'s first
  cell, 0 a loss handed to `CallTrainerLose` (32 scripts; 26 others use `_CallWildLose`). `give item I [N]` is
  `ItemAdd(I, N)` with no message; `if [not] item I` reads `ItemGetNum(I)`. **Checked live** (runs `sceneG1`, `sceneG2`:
  a scene of camera shots, two lines, then `battle 10`): the battle starts from the scene ("Dixie la Fillette veut se
  battre !", trainer 10 is a Lass with a Zigzagoon, not the Youngster the sandbox's notes guessed), the turns run, and
  a loss (A pressed blindly, the save's Torchic beaten) is the game's own: the player sent home to their mother. **A win
  goes on with the scene** (run `sceneG4`: "Vous avez battu Dixie", then flag 0xCE1 set after `battle`; talking again
  says the after-battle line and "Master Ball presente dans le sac", so `give item 1` and `if item 1` work).
  **`give pokemon` checked live** (run `sceneI1`, with `if [not] party N`, `PokePartyGetCount(0)`): the scene says "one
  Pokemon" before `give pokemon 24` and "at least two" after it. `heal` ran (run `sceneG3`) but its effect was not
  measured.
  **Triggers** (16 October): a sandbox's `trigger X Z [W H] script N` writes a zone trigger as the game's 361 are (word
  0 the event, 6-7 the tile, 8-9 the size; words 1-2 a work variable condition, left 0 as 134 of the game's), and a
  scene's `say L by N` makes character N speak when no one was talked to. **Checked live** (run `sceneH1`): walking east
  over a 1 x 3 trigger, the camera moves, the person walks two tiles south to the player, turns west and says two lines,
  then the camera comes back and the player walks on.
  **Teams of our own** (16 October): a sandbox's `team ID` block rewrites trainer ID's team (`a/0/3/6` record +0 the
  format, +7 the count; `a/0/3/8` entries: u8 difficulty, u8, u16 level, u16 species, u16 form, then u16 item for
  formats 2-3, then 4 u16 moves for formats 1 and 3; read on the game's 949 teams). **Checked live** (run `sceneJ1`,
  trainer 10 given Wurmple 2 with Tackle and String Shot, Magikarp 3 with Splash): "Dixie la Fillette envoie un
  Chenipotte" at level 2, "Le Chenipotte ennemi utilise Charge !", then "envoie un Magicarpe" at level 3, knocked out;
  the scene went on after the battle.
  **A trainer's class and name** (16 October): `team ID class C name TEXT`. The names are a/0/7/1-8 member 22 (950
  lines, a trainer each; written with `ReplaceGameTextLine`, the other names' bytes kept), the classes member 21 (280:
  178 Team Magma's leader, 174 Team Aqua's; the record's u16 +2). **Checked live** (run `sceneJ3`, trainer 10 as
  class 178 "Max le Jeune" with a Numel): the battle opens with Team Magma's own transition and Maxie's portrait, "Max
  le Jeune le Leader Team Magma veut se battre !", then "envoie un Chamallot !".
  **Scenes in the game's own zones** (16 October): a sandbox's `edit Z` block (OrasSandbox.h). **Checked live** (run
  `sceneK1`, `edit 6`: 2 lines after Littleroot's 39, character 11, a 5 x 1 trigger at row 167; the save moved to Route
  101, zone 23, 100.5 150.5, walking south): crossing the trigger, the camera moves, character 11 speaks, steps north
  and speaks again, the camera returns and the player walks on in Littleroot. **The chained main checked** (run
  `sceneK2`, the same mod, the save at 103.5 172.5 facing the sign): Littleroot's own sign still shows "Maison d'Andene",
  its event handled by the zone's own main through the scene's. A camera's framing is the scene's to get right:
  the person faces the player (west of them), so a shot from the south-east shows the back of their head (`sceneG1`).
  The battle draws its Pokemon yellow under the headless software renderer; the field draws normally.
  **Any model** (17 October): a model must be in its zone's area pack character list (`a/1/3/7`, `OrasWorkspace::CharacterList`),
  else entering the zone hangs the game (runs `h5`, `h64`). The sandbox adds each model it places: to its own packs' lists,
  and in place to a game pack's list for `edit Z`. **Checked live**: models 174 (the hero, presumably) and 64 beside X's
  gym leader in zone 539 (run `hfix`, and on the owner's phone, mod test6, blinking included); 299, 284, 174 and 64 in
  Littleroot, entered from Route 101 (run `hea`). A save made inside the edited zone showed none of them, the game's own
  299 and 284 included (runs `hed`, `hec`): enter the zone to see them.
- **Read a script**: `remake_tool oras-script <oras.3ds> <zone> [init]` disassembles it; `prototype/amx_natives.py tables
  <code.bin> <linked/modules.bin> natives.tsv`, then `... | amx_natives.py names natives.tsv` puts each native's name beside
  its hash, and `amx_natives.py check` proves every zone script's natives are registered (section 6). A zone's text:
  `remake_tool oras-text <oras.3ds> a/0/8/2 <member>` (French; `a/0/7/9`-`a/0/8/6` one archive per language).
  `prototype/oras_tables.py native-zones <work> NAME` lists the zones whose files import a native.
- **Read game tables**: `prototype/oras_tables.py files <file>` (a two-letter container's files), `encounters <work> <zone>`
  (a zone's wild tables: species, form, levels by kind), `trainer <work> <id>...` (record and team), each decoded the way
  the code reads it (its docstring says what is read and what only seen); the archives are extracted once into
  `<work>/tables/`.
- **Name functions**: read the code (Export.java), then write `address, name, role, read|guess` in
  `ghidra/function_names.tsv`; a guess is a lead, read it before relying on it. `prototype/name_review.py` lists the names to
  read again (`grew` after a change to the program, `suspects` from the call graph, `placeholders`) and `summary` makes a
  decompilation quick to read; the method and the 7 October review are in section 6.

**How a limit is lifted, the way every one above was**: find the fatal call (the census, or the stack of a frozen run),
read its function, find where the bound and the matching allocation live (an immediate, a literal-pool word, a caller's
argument), predict what a patch changes, build a mod that crosses the limit **and an unpatched control** with the same
data, run both: the control must stop, the patched one must not, and a screenshot or a memory read must show the new
thing is really used. Add the words to `OrasEngine.cpp` (each with the game's own value: `CodePatchIps` refuses a word
that is not), a test where the code is new (`tests/remake/remake_codepatch_test.cpp`), and a subsection here. Pitfalls
met: a value can appear in the code for unrelated reasons (0x5ED000 is both heap 4's size and a data address: patching
both looped the game); an ARM immediate holds only an 8-bit value rotated by an even amount (pick sizes like 0x800000,
0x1800000); a continued save restores state from the save rather than from the files (the characters); a limit can hide
another (51 characters passed the count check and stopped on the events buffer).

## 1. How the engine is read

- **The dumps** (Drive folder "Pokemon Project - Radiant Platinum": the cartridge, Platinum, the owner's save `main.zip`) are
  fetched into the session's scratch space, never into the repository (`CLAUDE.md` §8).
- **Ghidra** (11.4.2, headless): `remake_tool oras-code` writes the ExeFS `.code` decompressed (5,439,488 bytes, loaded at
  0x100000); imported raw as `ARM:LE:32:v6` at 0x100000 and auto-analysed (256 s on 4 cores): **16,402 functions, 3.9 MB of
  code**. `ghidra/Export.java` lists them (entry, size, callers, callees) and decompiles given addresses:
  `analyzeHeadless <project> oras -process code.bin -noanalysis -scriptPath tools/remake/ghidra -postScript Export.java <out dir> <address>...`
- **The running game**: the headless core (`tools/remake/headless`): code coverage (`trace`), memory (`mem`, `watch`, `dump`,
  `prototype/amx_dump.py` for scripts) and screenshots, section 0. It also has a GDB stub (script command `gdb 24689`, then
  `gdb-multiarch` with `set architecture arm`, `set osabi none`, `target remote :24689`), but `gdb-multiarch` is not
  installed by `session_setup.sh` and the stub dropped the connection in both tries of 7 October: use the trace and dumps.
- **Ghidra's wrong no-return marks**: the analysis marks some functions that return as no-return, and every call to them ends
  its caller (8 such functions on 7 October, code missing from 1,753 functions). `ghidra/FixNoReturn.java` clears them in the
  setup; `ghidra/CallGraph.java` lists the marks left (`noreturn.tsv`: 26, none returning) and every call (`edges.tsv`).
- **All the code in one program**: `prototype/cro_link.py <out> static.crs <all .cro>` links the 145 modules at once from 0x10000000 (8.9 MB, to 0x108D9000), their imports resolved to the `.code` through `static.crs`'s absolute segments (code 0x100000, rodata 0x57A000, data 0x5EC000) and to each other; 0 unresolved. **Checked** against the game's own load of DllField (memory dump on the field, script command `dump`): every code word matches but the 340 that call other modules (placed elsewhere by design); 101 data words differ (written at run time). `ghidra/AddModules.java` adds the image to the `.code`'s program, names the exports and disassembles from them before auto-analysis.
- **The whole program analysed** (7 October, 417 s): the `.code` and the 145 linked modules, **47,602 functions** (18,373 in the `.code`, 4.05 MB; 29,229 in the modules, 3.87 MB). **Checked**: DllField's piece handler (offset 0x1E8CC, linked at 0x102608CC) decompiles with its calls into the `.code` resolved (the file count `< 7` test, the fatal call 0x11EF4C), as read by hand in `ORAS_LITTLEROOT.md` 0.
- **Code modules**: 145 CRO files in the RomFS (655 files in all), loaded above the `.code` (DllField at 0x6F3000 on the field);
  `prototype/cro_dis.py` reads one with its relocations and imports. They include Game Freak's debug modules `DllFieldDebug`,
  `DllDebugPokeMake` and `DllDebugProcLoop`: **checked**, empty in the retail game (8 KB each, 220 bytes of code, no text): no
  developer menu to switch back on.

## 2. Zones

- A zone is three records in `a/0/1/3`: its own member (0-535, a `ZO` container: header copy, events, scripts), a 56-byte
  entry in the **zone header table** (member 536, 30,016 bytes = 536 x 56) and an entry in the **encounter container**
  (member 537, `EN`, 536 files). **Read.** Each `EN` file is byte for byte the zone's own file 3 (checked on all 536 zones,
  150 not empty); the field reads the zone's copy, member 537 is loaded whole at boot by `GameData_LoadBootTables` (its use
  there not read).
- The header loader (`FUN_003d9740`): `if (zone > 0x217) fatal; header = table + zone * 0x38`, the table kept in memory whole;
  its other branch reads a zone file whose decompressed size must stay under a constant (`DAT_003d9884`). **Read; the
  `cmp r5, #536` at 0x3D9774 checked under the debugger** (zone 6 on the field).
- **The zone header table** is loaded once by `FUN_00112b50`: archive 13 (`a/0/1/3`) opened, **0x7540 bytes** (30,016 =
  536 x 56) allocated, member 536 read into it, **fatal unless the read size is exactly 0x7540** (the word at 0x112C0C); the
  pointer kept at 0x5F45BC, which the header loaders index with `zone * 0x38` (`DAT_003d9880` = 0x5F45BC). **Read.**
- **Two header loaders bound the zone number**: `FUN_003d9740` and `FUN_003d99b8`, `if (0x217 < zone) fatal`. Their other
  branch reads the zone's own member (`a/0/1/3` member = zone number) into a buffer of **0x4A58 bytes** (19,032; the word at
  0x3D9884): a zone file decompressed larger is fatal. **Read.**
- **536 (0x218) is also the "no zone" value**: lists of zones end with it (`FUN_004e4c08`), `FUN_003cbdec` returns it for an
  invalid id, and DllField initialises its zone fields to it (offsets 0x1B8EC, 0x35FE4, 0x9D998). Members 536 and 537 being the
  tables, a zone numbered 536 or 537 cannot exist; new zones are members **538 and up**, and the sentinel needs no change.
  **Read.**
- Not zone limits (other structures of size 0x218): 0x1D6C7C (an allocation), 0x31D60C (records of 0x218 bytes); 0x444964,
  0x444B84 not yet read.
- **Zone count raised, checked headless (7 October)**: `remake_tool oras-append-test <oras.3ds> <out> zone-raised` appends zone
  538 (a copy of zone 6 naming itself 538) over Littleroot's 16 blocks of matrix 1, grows member 536 to 539 rows and member 537
  to 539 files, and writes `exefs/code.ips` (`CodePatch.h`, each word checked against the game's own): the two bounds `cmp rN,
  #0x218` -> `#0x400` (zones below 1024) and the table size word 0x7540 -> 539 x 56. With the owner's save moved onto zone 538
  (`oras-save ... 538 100.5 172.5`): **without the code patch** (`zone`, the old a4) the game stops in the fatal-error loop
  (black screen); **with it** the field loads, the player walks, thread 1 idle. Under the debugger (breakpoint on the bound at 0x3D9774,
  gdb attached after boot) the header loader receives **zone 538**. So the 536-zone limit is lifted by data plus three code
  words. Not yet checked: the phone; save data kept per zone (flags, visited places); the region map; zones far
  above 538.
- **What raising the zone count takes** (derived first, now built as above): the table member 536 grown by 56 bytes a zone
  and the size word at 0x112C0C patched to match; the bound 0x217 in the two loaders raised (the immediate `cmp r5, #0x218`
  at 0x3D9774 and the one in `FUN_003d99b8`); the encounter container (member 537) grown with them (the field reads the
  zone's own file 3 instead, section 2); the new zones' members appended from 538. Other per-zone tables (flags, names, the
  region map) still to find.

- **A zone from a text description (the sandbox, 8 October)**: `remake_tool oras-sandbox <oras.3ds> <out> <description.txt>`
  (`OrasSandbox.h` holds the format: `zone`, `template`, `spawn`, `encounters`, `pieces W H`, then `map` and 40 rows of 40
  letters a piece: `.` grass, `g` tall grass, `:` path, `s` pale grass, `*` flowers, `~` water, `t` tree, `T` forest).
  The grid is built into pieces as `oras-town` builds Platinum's (`BuildTownPiece`), on a new matrix whose every block is
  the zone; the zone is appended with the template's header words and scripts and no furniture, characters, warps or
  triggers; members 536 and 537 grow to match; then `oras-engine --zone-rows <rows it prints>` in the same out dir.
  **Checked live** (run `sand1`: zone 538, one piece, forest border, a path cross, a pond, tall grass; template 6,
  encounters of zone 24; the owner's save moved to 538 20.5 20.5): the field loads on the drawn ground and the player
  walks the path. **Then (8 October)**: several zones on one matrix (`blocks`, one digit a 10 x 10 block), place names
  (`name`: a new line of text file 90 in the eight languages; header word 14 & 0x3FF, read in `Zone_GetLocationNameId`),
  characters and trainers (`character`, `trainer`: a game character's 24 words with the known ones set). **Checked live**
  (fast runs `sand6`-`sand8`, `fr3`): walking from zone 538 into 539 shows "Bac à Sable Est" on the sign and the Navi-Map,
  the zone's characters appear, and trainer 10 (face 2, sight 5, east of the player) walks up and says his line before
  the battle, with zone 6's scripts as with Route 102's (a trainer's script is not the zone's: inferred). Face 2 looks
  west (seen). A save moved into a zone shows none of its characters: a continued save restores them from its block 10,
  which `oras-save` empties (`sand3`); walking in from another zone loads them. **Houses, doors, fences** (8 October): map
  letters `H` (a house's wall), `D` (its door), `F` (a fence), and `door X Z house Z2` in a zone: Petalburg's house is put on
  the door as wide as the walls beside it, the door gets a warp (a copy of Littleroot's) into a new zone after the
  described ones, a copy of game interior Z2 with no characters or triggers whose warp 0 leads back out. A door tile is
  solid, as Littleroot's (0x01000021 in piece 6): with it free the player stood on it and nothing happened (`house2`).
  **Checked live** (`house3`): the player walks into the door, enters the inside (zone 540, a copy of 223) and walks back
  out in front of the door; the fence shows (`house2`). **Ledges** (`L`, jumped down southward): the tile value of Route
  101's south ledges (0x75000021, piece 5: its rows 15, 16 and 28; seen, its east and west ledges 0x72020021 and 0x73020021
  are vertical runs, which way each faces not checked) and a one-tile slice of Route 101's ledge mesh (`chip_jump_gake9`,
  texture `chip_jump_gake` in area pack 8) on each tile. **Checked live** (`ledge2`, `ledge3`): the ledge shows, the player
  passes it going south and is stopped by it going north. Its foot showed a black band (the owner): the foot is translucent and the builder laid no
  ground under `L` tiles; with grass under them (`ledge5`) it shows the soft green shadow of Route 101's own ledge (reference
  shot `ref101`: the owner's save moved to zone 23 at 99.5, 146.5, above the ledge at row 148). Turning the texture over was
  wrong (`ledge4`). Not built yet: east and west ledges, relief (hills, cliffs),
  warps between matrices; not yet tested on the phone.

- **What the sandbox can do** (8 October; "checked" means seen in a headless run or on the owner's phone):

  | Part | Made new | Reused from the game | State |
  |---|---|---|---|
  | Zone | yes | yes | checked: new zones 538+, names, several zones a matrix, phone test of the demo |
  | Floor (insides, storeys) | no | a game interior copied behind a door | checked: in and out of a house (`house3`); stairs and storeys not tried |
  | Relief | no | south ledges | checked: ledge jumped south, blocks north (`ledge3`, `ledge5`); east and west ledges, hills, cliffs not built |
  | Battle | no | the game's trainers | checked: trainer 10 walks up and speaks (`sand7`); a trainer's own team not written (format known, section 6) |
  | Wild encounters | no | a zone's table copied | the format is read and checked (section 6); a table of one's own not written |
  | Dialogue box | no | the template zone's lines | place names are written (`name`); a dialogue needs a script |
  | Event, script | no | the template zone's scripts | scripts are read (disassembler), not written |
  | Cutscene | no | no | not studied |
  | Camera | yes | | `camera height H pitch P yaw Y distance D fov F near N far F` (`OrasSandbox.h`): preset 0 of the zone's own pack. **Read** (`Field_CameraApplyParams` 0x102CB460, `Field_CopyCameraParams` 0x102CB128, `Field_CameraComputePos` 0x1031C0AC) and **checked live** on 8 October: pitch -70 looks straight down (`w1`), distance 600 shows the whole sandbox (`w7`), fov 60 widens the view (`w6`), `height 14 pitch -8 distance 40 near 4` a view over the shoulder (`pov`); near and far read from their values only |
  | Lighting | yes | `lighting Z`: a pack's file 4 | `light ambient R G B diffuse R G B` (`OrasSandbox.h`). **Read** and **checked live**: the terrain's light (light set 0, one directional light from above) takes its colours from the area pack's file 4, copied into a buffer at field setup (`Res_CopyToCachedBuffer`): planes of 12 floats (3 kinds x 4 times of day; `FUN_0013D908` reads them, `FUN_0012DDF4` blends two entries and sets the light, `FUN_00139124` ambient, `FUN_001391D4` diffuse): ambient red/green/blue at +0x00/+0x30/+0x60, diffuse at +0x90/+0xC0/+0xF0, the specular colours at +0x120 and +0x1B0 (`FUN_001392F4`, `FUN_0013937C`; not tested), the direction x/y/z at +0x240/+0x270/+0x2A0 (`FUN_0013D540`, normalised; (0, -1, 0) outdoors). Runs `lt_below`, `lt_sunset`, `lt_cave` (`light direction ...`): the GPU's light reversed from below, a grazing orange sunset, a dim blue cave light. The characters' light (the player's too) is a second set with the same layout 0x2D0 further (`FUN_0013DC4C`; ambient (0.7, 0.8, 1.0) in use in Littleroot, its 4 entries look like times of day: (1, 0.8, 0.5), (0.5, 0.5, 0.5), (0.7, 0.8, 1.0), (1, 0.7, 0.5), inferred), set every frame; `character-light ...` (run `lt_char`: red diffuse, blue ambient on the characters only). Found with `writers LO HI PCLO PCHI` (only the light setters' writes, run `lit8`). Run `lit_red2`: diffuse (1, 0, 0) and ambient (0, 0, 1) give the GPU (254, 0, 0) and (0, 0, 254) and a red and blue field; the player keeps its own light. Not read: how the kind and the time slots are chosen; the earlier `light5` and `lit_red` changed the wrong entries |
  | Texture | not checked | the area packs' textures | the piece builder adds a pack's textures to the zone's pack |
  | Animation | no | no | not studied |
  | 3D model | no | pieces cut from the game's (house, trees, fence, ledge) | a model of one's own not made |
  | Music | yes | the header's id (words 4-5) | X/Y's 225 streams added (`oras-xy-world --music on`, `OrasXyMusic.h`, `OrasXySound.h`): a zone's id (category << 16 \| n) is resolved through the code's table of 0x17 categories (0x592728, `FUN_00115984`) to an archive item, **checked in memory** (Littleroot 0x10005 plays STRM_BGM_T_01, Route 101 0x1000B STRM_BGM_R_A); X/Y's tracks through the empty category 0 (`oras-engine --xy-sounds`), **checked by a recording** (run hMusA: Vaniville's sound correlates 1.00 with X's converted theme) |
  | Area pack | yes | | the sandbox fills the game's 9 placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195) and changes no Hoenn pack (`light5`), then appends past the game's (`OrasWorkspace::AddAreaPack`). **Checked live** (run `apkA`, 16 October): zone 538 on appended pack 230 loads as on a free slot (same screen as the control `apkok`). a/0/1/4 holds 228 packs, then a table of 44 bytes a pack (member 228, 228 entries, not read further); an appended pack needs an entry there (extended, the table kept at member 228) and an a/1/3/7 member (one a pack). Fatal-error loop without the entry (`apk5`) and with the table moved last (`apk8`) |
  | Ground | yes | the game's grass, paths, water | checked: grass, tall grass (its ground since `fix1`), paths, pond (a bed in the water's colour, `fix2`: the surface still faint), trees, forest |

- **Camera and lighting** (8 October):
  - Camera, **read**: an area pack's file 6 is 16 presets of 0x44 bytes from offset 0 (the earlier reading from +8 was off by
    two words). `Zone_ApplyZonePackData` (0x102E22D0) copies preset N, N the low byte of a u16 at +0xE of the area pack's
    loader (0 past the file's end); `Map_RendererSetupSteps` hands it to `Field_CameraApplyParams`. Offsets: +0x08 the
    height of the point aimed at above the player (15.85), +0x0C pitch in degrees (-40.74), +0x10 yaw (0), +0x18 near (32),
    +0x1C far (2000), +0x20 field of view in degrees (30), +0x24 distance (254.4); +0x28 is clamped to 1 and kept at +0xA8,
    +0x2C at +0xA4 (roles not read); +0x14 is read by neither. **Seen** (run `cam10`, a heap dump in Littleroot): N = 0, and
    the preset 0 copy at the camera. Where N comes from is not found; the sandbox writes preset 0, which its zones use.
  - Lighting, how it was found (8 October): the GPU's light state captured at each draw (`lights on/off`, runs `lit1`,
    `lit2`), the pieces' materials found white (so the GPU colours are the light's), the light's floats found in memory by
    elimination (heap and linear heap, `lit3`), the runtime copies shown empty before the field loads (`poke` at the title,
    `lit5`), their writer caught with `writers LO HI` under the interpreter (`lit6`: `FUN_00139124`, `FUN_001391D4`), then
    the code read back to file 4's layout.

## 3. Map pieces

- The piece slot table: 4 slots, filed under the word opening each piece's file 4 (`row | column << 8`); full table = fatal
  (`FUN_003c8a24`), a slot freed for the cell leaving the player's window (`0x3C8C84`). **Checked** (the Route 201 freeze,
  `ORAS_LITTLEROOT.md` 0).
- The piece buffer: 1 MiB (`0x100000`), checked in memory with piece 857.

## 4. The engine's hard limits: the fatal-error census

Every hard limit ends in the fatal-error routine (code 0x11EF3C, entered at 0x11EF4C by most callers: a loop calling the
handler at `[0x6173F0 + 0x6C]` forever, the freeze seen on the phone). `ghidra/FatalCensus.java` lists every call to it,
through the modules' import stubs too, with the decompiled lines before each call: **927 checks in 624 functions** (read).
By where they are: the `.code` 548 checks (382 functions), DllBattle 103, **DllField 96** (69 functions), DllSparring 54,
DllFieldEventSecretBaseMyBasePc 53, DllUSSecretBase 19, DllStatus 9, DllTownmap 7, the rest 1-5 each. Most compare against a
value loaded from a data word (the zone bound `0x217` is `DAT_003d9884`-style, not an instruction immediate), so each check is
classified by reading its function, not by its constant. Classified so far:

| Check | Where | Limit | How it was found |
|---|---|---|---|
| zone number > 0x217 | `FUN_003d9740` (0x3D9774) | 536 zones | read; checked under the debugger |
| piece slot table full | `FUN_003c8a24` | 4 pieces loaded | read; checked at the Route 201 freeze |
| piece file count < 7 | DllField 0x1E8CC (0x102608CC linked) | a piece is a `GR` of 7+ files | read |

### 4.1 DllField's 96 checks, classified (7 October)

Read from the decompiled functions (the 69 that hold a check, `ghidra/Export.java`), split in three and read in parallel by
sub-agents; their meanings marked **read** where the code shows them, **guess** otherwise. Spot-checked by hand: the 4 / 2 / 8
object counts of `FUN_102e0a4c` and the 320-byte bound of `FUN_102c0260` match the code. Offsets are DllField's (linked
address - 0x10242000). The ones that bound a larger region:

| DllField offset | Check | Limit | |
|---|---|---|---|
| 0x1E8CC | piece file count `< 7`, extra files in pairs, `(n-7)/2+1 < 5` | a piece: 7 files plus at most 3 extra pairs (4 animated parts, slots of 0x44 at +0xB4) | read |
| 0x9EA4C | counts at +0x84 `> 3`, +0x88 `> 1`, +0x8C `> 7` | 4 / 2 / 8 objects of three kinds per load (0x94, 0xA4, 0x24 bytes); the 4 matches the 2 x 2 piece window | read (the kinds: guess) |
| 0x84C98 | piece load direction not 0..7; window count asserted `== 4` | the streaming window is 4 pieces, 8 load directions | read |
| 0x271A8 | no free entry in the table at +0x8C (0xE4-byte entries, count at +0x90) | a runtime-sized object table, fatal when full | read (spawned objects or characters: guess) |
| 0x271A8 | id not found in the 0x50-byte records at +0x14 (16 sub-slots each) | requested ids must exist in that table | read |
| 0x42058 | id not in the two lookup tables (+0x3C, +0x38); 33 slots of 0xA0 (full: warning, slot 0 reused) | new models or objects must be added to those tables | read |
| 0x42CA8 | loads into 34 slots of 0xB8 | 34 loaded entries of some kind | read (kind: guess) |
| 0x2B628 | 13 objects of 0x40 and 165 of 0x4C; container files 8..172 read | a fixed-layout container of at least 173 files | read |
| 0x65228 | grid index outside width x height of the grid at +0xC4; direction byte not 0..3 | the field's grid bounds | read |
| 0x7E260 | a slot handler's size `> 0x13F` | 320-byte work buffer per slot (0x15C-byte slots at +0xD8) | read (script or event tasks: guess) |
| 0x993C8 | a value `> 0xFFF`, read with a 0x1000-byte request | 4 KB cap on some streamed data | read (what data: guess) |
| 0x87BD8 (`FUN_102c8bd8`) | container file count `!= byte count + 1` | N sub-files plus one shared file, the count stored apart | read |
| 0x73574 | 8 registry entries, all used | an 8-entry registry | read (content: unknown) |
| 0x15718, 0x3B2CC, 0xA0B50, 0xE05B0, 0xE16A8, 0x803D4 | scene registration (`FUN_0038b374`, `FUN_0038b69c`) failed | a scene capacity in the `.code`, not yet read | read (that it is a capacity: guess) |
| 0x27414, 0x29B14, 0x20B44, 0x2B3D8 | async load request refused (`FUN_0036ea84`, `FUN_0036d9dc`) | the loader's queue or heap, in the `.code`, not yet read | guess |
| many | resource binding failed (`FUN_003749f4`) | any malformed or oversized model or animation | read |

The others are invariants (idle state byte 0x14 before a new request, objects initialised twice, required globals present,
a character still stuck after a corrected move): rules on order, not capacities. Not in DllField: the zone bound (section 2),
the 4-slot piece table (section 3).

### 4.2 The memory map: every heap, created at boot by `FUN_00107c0c`

**Read, and the sizes held in data words read from the code** (7 October). `FUN_00145d44(0x8000000)` takes the application
memory, `0xE88000` of it goes to a system region (heap 1, `FUN_0010abd8`), the rest is split by `FUN_001120b4(parent, id, size)`
into sub-heaps. These sizes are the memory budgets: a larger world means larger heaps here (a patch to these words), within the
console's memory (ORAS asks the 3DS for its standard application memory; whether more can be had is not yet checked).

| Heap id | Size | | Heap id | Size |
|---|---|---|---|---|
| 8 | 0x20000 (128 KB; event work buffers) | | 0x13 | 0xA800 |
| 9 | 0x392000 (3.6 MB) | | 0x14 | 0x14100 |
| 10 | 0x10000 | | 0x196 | 0x2900 |
| 0xB | 0x1400 | | 0x197 | 0x8000 |
| **0xC** | **0x200000 (2 MB; whole archive members loaded asynchronously, zones and areas)** | | 0x19 | 0x10000 |
| 0xD | 0x142420 (1.3 MB) | | 0x1A | 0x16D300 (1.4 MB) |
| 0xE | 0x3A000 | | 0xF8 | 0x80000 (512 KB) |
| 0xF | 0x115000 (1.1 MB) | | 0x18 | 0x1E000 |
| 0x10, 0x11 | 0x2000 each | | 0xF1 | 0x1B8000 (1.7 MB) |
| 0x12 | 0x4110 | | 0x1DD | 0x5000 |
| 0x112 / 0x113 | 0x3000 / 0x18000 | | **0x16** | **0x504000 (5 MB)** |
| 0x195 | 0x4000 | | **0x17** | **0x1C6D000 (28.4 MB, the largest)** |
| 0x1DE / 0x1DF | 0x3C00 / 0x10000 | | 0x1D9 / 0x1DA | 0x600 / 0x15C500 |

What lives in each heap is known only where a check names it (0xC: async member loads; 8: event work; 0xD, 0xF8: the
allocations in sections 4.3); the rest is to read.

### 4.3 The `.code`'s 548 checks, classified

Read in six parts by sub-agents from the decompiled functions, as DllField's; the memory map above checked by hand. Most are
invariants (singletons created or freed twice, null pointers, save-block magic checks, objects destroyed while loading). The
capacities that bound a larger world:

| Function | Check | Limit | |
|---|---|---|---|
| `FUN_003d9740`, `FUN_003d99b8`, `FUN_00112b50` | zone bound, zone file size, header table size | 536 zones, 0x4A58-byte zone files (section 2) | read; lifted (section 2) |
| `FUN_003c8a24` | no free slot | 4 map pieces loaded (section 3) | read, checked |
| `FUN_003f7ff4` | `0x1A < count` (zones 0x10, 0x30, 0x1C3 exempt), spawned `> 0x19` | **26 field entities per zone**, 0xAB0 bytes each | read (that they are characters: guess) |
| `FUN_003f54e8` | no free entry among the count at +0x36A0 | the field object pool (0xAB0-byte entries) | read |
| `FUN_003f2ed8` | `4 <` objects in a cell; 48-entry result buffer | 4 collision objects per cell | read (collision: guess) |
| `FUN_00471520` | pending count `>= 8` | async load queue of 8 | read |
| `FUN_004074c4`, `FUN_00404a5c`, `FUN_0049d830` and others | an async load could not start | whole members loaded into heap 0xC (2 MB) | read |
| `FUN_003d7dd0`, `FUN_0043abb0` | allocation returned null | the event heap and other heaps out of memory (section 4.2) | read |
| `FUN_0048883c` | `0x33 <` entries | 51 texture-slot entries per model (arrays inline in the model object) | read (textures: guess) |
| `FUN_00361604` | index `>=` capacity at +0x20 | model and animation slots of a resource set | read |
| `FUN_003623ec` | index `>=` capacity at +0x24 | resource slots, each an archive | read |
| `FUN_003cc4fc` and 7 others | `0xAD <` id | a table of 174 entries of 0x28 bytes (DAT_003cc530 ...) | read (what it holds: unknown) |
| `FUN_003fb230` | id over a range's max | 86 id ranges of 0x10 bytes (DAT_003fb388) | read (message or script ids: guess) |
| `FUN_003db6e4` | size `>=` DAT_003db770 | a multi-section event block copied into a fixed buffer | read (script data: guess) |
| `FUN_0010a4b0` (from the boot) | 0x20 | 32 code module slots | read |
| `FUN_00368600` and 10 others | module list `< 2` or `< 3` | each process loads 2-3 named CRO modules together | read |
| `FUN_004a0108` | `999 <` nodes | resource cache bucket walk | read |
| `FUN_00459018` and 5 others | `0x1E <` box, `0x1D <` slot, 0xE8 bytes | 31 x 30 Pokemon storage (PC boxes: guess from 232-byte records) | read |
| `FUN_004d343c` and others | `5 <` index | 6-entry arrays (party: guess) | read |
| `FUN_004ebde4` and others | magic checks, 0x2D1 | save blocks; 721 species bitfields | read |

### 4.4 The application memory, raised (checked headless, 7 October)

- **What the game gets** (**checked** in memory on the field): a linear heap of **0x2B48000 (43.3 MB)** at 0x14000000 and a
  normal heap of **0xDF0000 (14.3 MB)** at 0x08000000, plus the code (to 0x6AF000): the 3DS's standard 64 MB application
  mode, which the game declares in its extended header (`system_mode` 0, flags0 bits 4-7 of the ARM11 local caps at 0x20E).
- **Where it asks for them** (**read**): `FUN_00106448` at boot checks that 0x3938000 bytes are free, asks for the normal heap
  `mov r6, #0xDF0000` and the linear heap at the word **0x106508 = 0x2B48000** (`FUN_00107090`); `FUN_00107c0c` then makes
  heap 1 of the linear heap less 0xE88000, from which the sub-heaps of section 4.2 are carved. A larger linear heap grows
  heap 1 with it; the sub-heaps keep their sizes until patched.
- **A larger system mode alone changes nothing** (checked): with `exheader.bin` set to 96 MB (Azahar applies it: the game
  is reported "tainted") the game still takes 43.3 MB. **With the linear heap word raised too** (`code.ips`), it takes what
  it is given: 0x4748000 (71.3 MB, +28 MB, mapped 0x14000000-0x18748000 and read back in the game's own variable at
  0x61726C); the field loads and the player walks, thread 1 idle.
- **The parents** (**read**, `FUN_00107c0c`): heap 1 = the linear heap less 0xE88000 (it holds heap 0x17 only); **heap 4**
  (`[0x5F5014 + 0x18]`) = the top **0x5ED000** bytes of the normal heap (heaps 8, 0xB, 0xC, 0xD, 0xF8, 0x18, 0x10, 0x196 ...);
  heap 5 (`+0x1C`) = the last 0xE88000 bytes of the linear heap (9, 0xA, 0xF, 0x12, 0x14, 0x1A ...); heaps 0xE, 0xF1, 0x16 come
  from the system heap and 0x17 from heap 1. Heap 4's size is read by `FUN_00106448` (two `sub`s, 0x500000 + 0xED000) and from
  the word 0x108564; the same value in the words 0x110254 and 0x11AB74 is an address in the game's data, not this size
  (patching them sent the game reading 0xFFD000 in a loop: checked).
- **`remake_tool oras-engine <oras.3ds> <out> [--memory 64|72|80|96] [--linear-heap BYTES] [--normal-heap BYTES]
  [--zone-rows N] [--heap ID:BYTES]...`** (`OrasEngine.h`) writes every engine patch into the one `exheader.bin` and
  `exefs/code.ips` a mod carries, each word checked against the game's own; a heap whose size is an instruction's immediate
  takes only ARM-encodable sizes. **Checked headless (7 October)**:
  - `--memory 96 --linear-heap 0x4748000`: the same bytes as the run above;
  - `--heap 0x17:0x346D000` with it: heap 0x17 28.4 -> 52.4 MB, field and walk;
  - `--heap 0xC:0x800000` alone: **the boot stops in heap 0xC's creation**, heap 4 has no room;
  - `--memory 96 --linear-heap 0x4100000 --normal-heap 0x1800000 --heap 0xC:0x800000`: normal heap 14.3 -> 24 MB (mapped
    0x08000000-0x09800000), heap 4 6.2 -> 16 MB, **heap 0xC (async member loads) 2 -> 8 MB**, linear heap 65 MB: the field
    loads (screenshot, `shot` command) and the player walks, thread 1 idle.
- **The New 3DS mode, 124 MB** (`--memory 124`: the extended header's `n3ds_mode` 1 at 0x20D instead of the system mode;
  Azahar grants it with its New 3DS setting on, its default, on the phone too unless changed). **Checked headless**: with
  `--linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000` the game takes an 88 MB linear
  heap (read back at 0x61726C), a 24 MB normal heap, heap 0xC 8 MB and heap 0x17 64 MB (the game's 28.4), loads the field
  (screenshot) and walks. **178 MB (`n3ds_mode` 2) cannot be used**: the system's shared font is mapped at 0x14000000 plus its
  place after the application's memory, beyond the game's 128 MB linear window, and the game panics at boot
  (`svcBreak`, "cannot map APT:SharedFont") even with its own heaps unchanged (checked); the tool refuses it. In 124 MB the
  linear heap's ceiling is the total (code, normal heap and linear heap within 124 MB), which the tool checks.
  Not yet: the phone.

### 4.5 Characters per zone, raised past 26, bounded by the events buffer (checked headless, 7 October)

- **The limit** (**read**): `FUN_003f7ff4` places a zone's characters (its 0x30-byte records) when the player **enters** the
  zone; it stops the game when the zone lists more than 26 (`cmpne r6, #0x1A` at 0x3F8038; zones 0x10, 0x30 and 0x1C3 are
  exempt) and before a 27th appears (`cmp r0, #0x1A` at 0x3F808C, the count at +0x36AE). Each character takes an 0xAB0-byte
  entry of the field manager's pool, which `FUN_00112d4c` makes with the count its caller passes: **32** (`mov r3, #0x20` at
  0x109048), beside 32 inline records of 0x1B4 bytes in the manager. A continued save restores its characters from the save,
  not from the zone file: the check runs on entering a zone (checked: a crowded zone 6 resumed into does not stop).
- **`oras-engine --characters N`** (26 to 32) raises both bounds. **Checked**: zone 6 given 30 characters (`oras-append-test
  ... crowd`, clones of its own 11 on a grid; written with `OrasZone::Write`), the save on Route 101 (zone 23) north of it,
  walking south into Littleroot: **without the patch the game stops in the fatal-error loop; with `--characters 32` it enters,
  the clones stand in the town** (screenshot) and thread 1 is idle.
- **Above 32** (Platinum needs it: `platinum-zones` counts 28 headers listing more than 26 characters, 8 more than 32, at most
  51 in header 466): `--characters` up to 255 also grows the pool (`mov r3, #0x20` at 0x109048 -> the bound; the pool is
  allocated apart from the manager). With it, 51 clones still stop the game, elsewhere: `FUN_003db6e4`, called on entering
  a zone (DllField `FUN_102ddd8c`), copies the zone's **events file** (file 1: the arrays and the initialisation script) into
  a buffer of **0xC84 bytes** and stops when it does not fit (`DAT_003db770`); the buffer's size is also returned by a
  virtual getter (0x4605BC) of a save-data block, so it is part of the save's layout and is **not raised**. Zone 6's events
  file is 2,032 bytes, 1,268 of them its script: predicted and **checked** 35 clones (3,184 bytes) enter and show, 36
  (3,232 bytes) stop at the same place. So a zone's characters are bounded by that byte budget (each 0x30 bytes), not by 26:
  `OrasZone::Write` refuses an events file of 0xC84 bytes or more (`OrasZone::EventsBudget`), so a zone that would stop the
  game is refused when the mod is built. A Platinum map of 51 characters fits only with a short script, or by splitting
  its characters across zones.
- **`OrasZone::Write`** writes a zone back over its container (header, events, the rest kept): **checked identical on all 536
  zones** (`remake_tool oras-zone-check`).


### 4.5b Character models and shown characters past 31 / 32 (17 October)

Read in the code (Ghidra, headless) and checked headless; the owner checked on the phone where said.
- **A pack's model list (a/1/3/7)**: a model a zone shows must be in its area pack's list, else entering the zone hangs
  (runs h5, h64). The sandbox adds the models it places (its own packs list only their zones' models; `edit Z` grows the
  game pack's list in place: the zones of one matrix are crossed without a load and share one list, so a list cut to one
  zone's characters hung Littleroot, run hE). Checked: models 174 and 64 beside X's gym leader (run hfix, phone test6),
  four new models in Littleroot (run hea).
- **31 models a list**: the field character manager caches the list after the player in 33 slots of 0x18 (+0x36CC) and
  stops past 32 (cmp #0x21 at 0x3F7300); DllField's model resource manager holds 33 slots of 0xA0 inline (+0x40).
  `oras-engine --character-models N` (up to 127) moves both to new tails (40 .code words; 104 DllField words, caves in the
  padding at DllField file 0xF03F0; Azahar only, a console's static.crr check would refuse the module). Checked: 40 models
  in one zone (run hG; phone, crowd mod).
- **32 shown characters**: DllField's field object array (one 0x2F0 object a shown character; `mov r1,#0x20` at
  0x3F525C) shares its last object past 32 (its assert is an empty `bx lr`): the phone showed an invisible but solid
  row, people sunk in the ground and one stretched across the screen. `--characters N` now grows it to N + 6, its heap
  0x62 (`mov r2,#0x10000` at DllField 0x10283954) and the two model instance heaps 0x159 / 0x15A (sizes loaded at
  0x10283970 / 0x10283994, through caves) by objects / 32. The array alone, or with heap 0x62 only, hung in the allocator's
  out-of-memory loop (0x12BAEC, FUN_0012b9a0) while a skeleton was built (runs hK, hL, hP); all three grown, the 59-people
  crowd of 40 models enters and draws (run hQ). **Checked on the phone** (owner, 17 October, mod crowd2): every
  row drawn, no invisible wall, no stretched character; the people half in the ground are swimmer models, whose idle
  motion swims (not a bug).
- **X characters beside the game's** (17 October): `oras-copy a/0/2/1 544=xy:<x.3ds>:<member>` appends the converted
  character as member 544 (the game has 544), model 544. **Checked headless** (run hX): X's character 31 as 544 stands
  beside ORAS's own 284, which keeps its model. Checked on the phone (owner, 18 October, mod grass).
- **Events file past 0xC84** (17 October): the buffer is not in the save (the save block of 0xC84 bytes is
  MyPhotoIconSaveData; the sentence above saying so was wrong): `Event_LoadZoneEventsFile` copies a zone's file into slot
  0 of the events object (0x1958 bytes; slot 1 unused), and Continue rereads the zone's file. `oras-engine --events
  large` and `oras-sandbox --events large` let slot 0 take both slots: 0x1910 bytes. **Checked headless** (run hEv2): a
  64-character zone (3,224 bytes) entered from the next zone shows all its people. The zone's characters need a pool above
  their number plus the player: with `--characters 64` the same zone stopped in `Event_PlaceZoneCharacters`'s overflow
  (0x3F80B8, run hEv); `--characters 72` passes.
  **Slot 1 unused, checked** (run slot1, the unmodified game, save on Route 101): the events object found in memory at
  0x08D193C4 (its +0x194C and +0x1950 pointing at +0x34 and +0xCC0); before and after walking into Littleroot and back,
  slot 1's 803 words are all zero while slot 0 changed (the two zones' files written in it). `oras-sandbox --characters N`
  refuses a zone placing more than N - 1 people (26 without it).
- **Swimmers** (17 October, read): models 259, 273, 274, 278, 279, 345-348 are placed only on water tiles; their idle
  motion (slot 1) is a 30-45 frame swim, a walker's is 1 frame, so none can stand. 345-348 share their skeleton byte for
  byte with 230, 229, 227, 228, which stand (slot 1 of 1 frame, a walk of 14): their land twins. The other five have no
  twin. The sandbox warns when it places a swimmer (`OrasWorkspace::Swims`: their a/1/3/7 entries alone read 01 06 at
  bytes 2-3 and 00 at byte 6; that the game reads those bytes is inferred). Next: `docs/DEVELOPMENT_NOTE.md` 3d.
- **Method** (bisection, one headless run at a time): the same mod with one change each (models 299 vs 174; 31 vs 33
  entries; 34 vs 40 models; 34 vs 70 objects; demo zone vs crowd), the frozen threads' pc and stack code addresses
  (`report`) named against the Ghidra program; agents read the code without running the emulator.
- **Saved characters past 32** (17 October, read, not yet run): MoveModelSave (block 10) holds 32 records of 0x108 inline
  in the SaveData object (`operator new(0x723B8)`, literal 0x1091F0 in FUN_00108dd4; vtable 0x5E064C at +0x1314, records
  +0x1318, Misc right after at +0x3418), so it cannot grow in place. The save file has no layout table: each block's offset
  is the sum of the 0x200-aligned sizes before it, taken from the size getters (Save_ReadBlocks / Save_WriteBlocks), the
  footer (ids, FEEB, the table) its last 0x200 bytes, the file's size that sum (FUN_00115a84 at boot; no 0x76000 in the
  code). `--characters N` (N > 31) moves the records to a tail at SaveData + 0x73314, N + 1 of them (the block rounded up
  to an ARM immediate: 0x4C00 for 73), 15 words (OrasEngine.cpp); the blocks after 10 then move and the file grows (0x78A00
  for 73). A save of the old layout reads short (Save_ReadBlocks returns 4): `oras-save <main> <out> --save-records N + 1`
  converts one (`OrasSave::ResizeMoveModels`; back to 32 gives the game's bytes again). Not read: the heap the SaveData comes
  from (r7 of FUN_00108dd4; up to 0x10800 more bytes), and the archive under Save_Commit.
- **Shared characters across a border** (`share Z`, 17 October, read and checked headless): a 0x30-byte record holds
  two zones at +0x1C/+0x1E, a selector at +0x20 and an id at +0x22; `Field_InitCharacterFromEventEntry` (0x3F9464)
  copies them to the loaded character's +0x52/+0x54/+0x56. `Event_ReleaseCharactersNotInZone` (0x3F6DF0) keeps a
  character whose +0x52 or +0x54 is the zone entered. `Event_CharacterPool_Acquire` (0x3F54E8) rebinds a loaded
  character instead of taking a new entry when its id matches and, if the record's +0x1C equals its +0x20, its +0x52
  is the zone entered, else its +0x54. So +0x20 must name the zone whose file lists the record: left 0, entering zone
  538 rebound, but going back into 539 loaded the 61 shared people a second time and Acquire's fatal stopped the game
  (pool of 72 full; run hFar, black screen, stack 0x3F80B8 under the fatal). With +0x20 set, the same walk 539, 538,
  539 shows the crowd (run hFar3).
- **The parent of the instance heaps, and the largest pool** (17 October, read and checked headless): heaps 0x159 and
  0x15A (Field_SubsystemInit) are carved from the field's **heap 0x5D**, which FUN_00404600 makes from heap 0x17 with the
  size word at 0x404768 (0x1350000, 19.3 MB; called by Field_ReloadZoneResources with heaps 0x16 and 0x17); the character
  models are decompressed into it as they load (FUN_003f4d8c). With `--characters 127` the boot ran it out (run h127c: the
  allocator's out-of-memory loop at 0x12BAEC, r5 = heap 0x5D, r7 = 0x17800; the headless report now prints registers in
  that loop too). `--characters N` grows 0x5D by what 0x159 and 0x15A grow (0x1B0000 / 32 x (N + 6) - 0x1B0000): 127
  enters the field (run h127e), 72 still walks 539, 538, 539 (run h72h). Doubling heap 0x16 instead stopped the boot in
  0x16's own creation (run h127b). **The scene's model groups** (18 October, read and checked headless): a zone of 120
  characters (scratch description, run hMax) stopped in `Field_RegisterObjectModelsInScene` (DllField 0x102C23D4):
  `Gfx_SceneGroupAddModel` (0x38B374) found no free entry. The field's scene (FUN_00404c80, through FUN_00392ab4) has 8
  model groups, each one's capacity in word 0x18 of a 100-byte entry: 256, 32, 32, 32, 32, 64, 64, 32 (read in memory,
  run hMax3: groups 5 and 6 at 64 of 64). `--characters N` (N + 6 > 64) makes the two stores of 0x40 (0x404E30,
  0x404E3C) take r9, 0x80 there: 128 each, and the 120 characters show (runs hMax4, hMax6). A zone's characters are
  then bound by the events file (`--events large`: 0x1910 bytes) and the pool (`--characters`, up to 127 checked).
- **Still bound**: a zone's events file (0xC84 bytes, about 60 people with a short script, 4.5); the extra heaps come out
  of the field's heap 0x5D (grown with them, above).

### 4.6 What is left of the limits, and why (7 October)

- **Texture slots per model, 51** (`FUN_0048883c`, read; that it governs map pieces is inferred): Hoenn's 857 pieces use at
  most 45 distinct textures (member 552), a built Sinnoh piece 29 (measured). Not raised (the arrays are inline in the
  model object); `TownBuilder` refuses a piece whose model names more than 51, so it cannot reach the phone.
- **8 pending loads per object** (`FUN_00471520`, read): the pending list is inline (+0x3FC .. +0x41B) with other fields
  after it, so raising it means moving the array; it bounds one object's simultaneous asynchronous loads, not a map's
  size, and no run has reached it. Left as is.
- **Collision objects per cell, 4** (`FUN_003f2ed8`, read: gathers the collisions in the grid around a moving entity, a
  48-entry result buffer): what fills a cell is not read yet, and no run has reached it, the 35-character crowd included.
  If a build ever stops there, the frozen thread's stack holds `003F2ED8`-area return addresses: read it then.
- **The 174-entry table** (`FUN_003cc4fc` and seven others, entries of 0x28 bytes at 0x5837D8): used only by the Secret
  Base modules (`DllUSSecretBase`, `DllFieldEventSecretBaseMyBasePc`, one call from DllField): the Secret Base goods
  (inferred). Not a world-building limit.
- **The 4 loaded pieces**: the 2 x 2 window around the player, by design; what had to be right was each piece's cell word
  (section 3).

So for building a region the limits are lifted (zones, memory, heaps, characters) or checked when the mod is built (a
zone's events size, a model's textures); the rest bound features Sinnoh does not need or have not been reached.


## 6. What each activity runs: the subsystems that matter, named (7 October)

**Method** (the owner's choice: decompose the parts the remake needs, not all 47,600 functions). A headless run under
the interpreter (`POMEGRADE_INTERPRETER=1`) records every block of code the game enters between `trace on` and `trace
off FILE`; `prototype/coverage_map.py` maps the blocks to the Ghidra program's functions (modules moved from where the
run's log says they were loaded to their linked address) and subtracts other traces: an activity minus the idle field
(and minus walking) is what that activity alone runs, also against a trace of another run (`--minus <run>/<trace>`). A
trace does not say when a block ran: where the game put another module at the same address during the trace (DllField
and DllBattle both at 0x6F3000 when a battle starts, `enc2`; the start menu before the field, `actE1`), the block is
given to each, marked `DllField?`, and subtracting a trace of the other activity drops the wrong ones (7 October; until
then the module loaded last took them all). Each set is then decompiled and read (sub-agents for the large ones), and
the names go to **`ghidra/function_names.tsv`** (address, name, role, read|guess), which `ghidra/ApplyNames.java` gives
the program (`session_setup.sh` does it): the decompilation then reads `Zone_LoadHeader` with its role as comment.

**Traces recorded** (vanilla game, the owner's save; each checked on screen):

| Activity | Run (script) | Functions beyond the idle field | Specific (not run by walking) | Named |
|---|---|---|---|---|
| walking a few steps | Littleroot | 521 (398 `.code`, 111 DllField, 12 DllFieldEventPlayer) | | |
| entering a zone (Route 101 -> Littleroot, walking south) | save on zone 23 at (100.5, 150.5) | 1,261 | **782** (546 `.code`, 236 DllField) | **all 782** |
| opening the menu (X) | Littleroot | 381 | 127 | yes (see below) |
| entering a house (door at 106.5, 171.5) | save at (106.5, 172.5), walking up | 2,481 | 1,522 beyond walking and zone change (842 `.code`, 652 DllField, 28 DllFieldEventEntranceIn) | yes (see below) |
| reading a sign ("Maison d'Andene", furniture at 103, 171) | save at (103.5, 172.5), facing up, A | traced (`actE1`) | 199 beyond walking | yes (see below) |
| talking to a character (the mother in the player's house, zone 225, character 6 at tile 12, 16: "Ça va, Andene ? Tu as l'air fatiguée...") | run `house1`: save at (106.5, 172.5), walk up through the door, then one tile up, five right, up to face her, A | 874 | **307** beyond the sign (232 `.code`, 75 DllField; 169 named before) | 128 more named (10 left: role not readable) |
| a wild encounter (Route 101's tall grass, x 89-95, z 147-151) | save at (92.5, 149.5), walking left and right | 3,882 | 3,425 (2,043 `.code`, 1,325 DllBattle, 57 DllBackGround) | not yet |
| walking in tall grass, no battle (run `enc1`, 7 October) | zone 23 at (92.5, 149.5), left and right 34 frames x 8 | 844 beyond idle | 844 (595 `.code`, 237 DllField, 12 DllFieldEventPlayer) | the encounter path: 15 named |
| a wild battle (run `enc2`, 7 October; Wurmple at level 2) | the same, 32 walks | 5,114 beyond `enc1`'s walk and idle, many counted twice (`DllField?` / `DllBattle?`: same addresses, see `coverage_map.py`) | 2,032 `.code`, 1,463 DllBattle?, 1,421 DllField? | the slot pick |

Not recorded yet: a trainer battle, saving, the region map (needs the touch screen, which retro_host does not drive), a
script started on entering a zone. The battle itself under the interpreter is very slow (more than 10 minutes for a few
turns).

**How a zone is entered** (from the 782 named functions; **read** where the code shows it, otherwise **guess**):

- `Zone_LoadHeader` (0x3D99B8, read) and `Zone_HeaderLoad_Poll` (0x3D9918, guess): a zone's 0x38-byte header, from the resident
  table or by an async read of its member of `a/0/1/3` (`Zone_CreateZoneArchiveHandle` 0x112C10, read: archive 13).
- Music: `Zone_ChangeBgmOnEnter` (0x3C7D84, read) compares the new header's word at +0x1C with the playing one and queues a
  fade (0x3C frames); `Snd_ChangeZoneBgm` (0x44E858, guess); `Zone_ResolveFlagVariant` (0x3C79F8, read) picks a per-zone value
  from event flags at save +0x13140 against a 0x54-entry table (music or layout variants: guess).
- The place name: `Zone_ShowLocationName` (DllField 0x102B5450, guess) and `Zone_GetLocationNameId` (0x4D86EC, guess: the
  header's halfword +0x1C & 0x3FF, passed with text file 0x5A).
- Map pieces: `Map_LoadPieceModelFromContainer` (DllField 0x102608CC, read: files 1, 2, 6, 7+ the model, animations and
  textures; file 4 the cell word to `FUN_003c8a24`; files 3 and 5), `Map_PlacePropsFromContainer` (DllField 0x102607A4, read:
  file 3's 11-word rows: id, position, rotation, scale), `Map_FreeMapPieces` (0x3C8C84, read), `Map_AreaObject_Destroy`
  (DllField 0x1027B990, read) on leaving.
- Characters: `Event_PlaceZoneCharacters` (0x3F7FF4, read), `Event_CharacterPool_Acquire` (0x3F54E8, read),
  `Field_InitCharacterFromEventEntry` (0x3F9464, read: a 0x30-byte record into a 0xAB0-byte object),
  `Field_GetCharacterModelInfo` (0x3F62D8, read: a model's 0x18-byte info from a cache, the pool or the archive).
- Other zone objects: `Zone_PlaceZoneObjects` (DllField 0x102BC9C0, read: up to 6 entries of a 0x5A-row table belonging to
  the zone, at grid positions cell * 18 + 9).
- Scripts: `Script_ResetZoneLocalState` (0x3FF288, guess: on a zone change, stops scripts and clears the work values from
  0x4000 and 12 slots), `Script_ResolveWorkValue` (0x3FE118, read: ids 0x4000-0x7FFF are event work in the save,
  0x8000-0xBFFF script temporaries, others literals), `Script_RunFieldScriptMode` (0x3FEC30, guess), a script VM call
  (0x3AADAC, guess).
- Resources: `Res_Decompress` (0x36AF08, read: by header type, 0x10 LZ, 0x20 Huffman, 0x30 RLE, 0x40/0x50 extended LZ),
  `Res_AsyncLoad_Create` (0x36E694, read: a 0x54-byte job with a completion callback).

Most of the 782 are support code (221 `Gfx_`, 178 `Util_`, 49 `Sys_`): the zone-specific ones above are about 140.

**Talking to a character** (7 October, run `house1`; **checked**: the mother's words on screen, `TalkMdlMsg_Seq`
(running 0x747260) and `Msg_Balloon_Show` (0x772B74) in the talk trace and not in the idle one). Beyond what reading a
sign runs, talking loads and runs the character's own script (`Script_Load`, `Script_LoadFieldScript`,
`Script_RegisterNativeTablesByMask`, the core, float and console natives), turns the two to face each other
(`_TalkMdlStartInit`, `TalkMdlSetEyeToEye`, `TalkMdlSetTalkMotion`, `MdlAcmd*`), reads the story state (`FlagGet`,
`WorkGet`, `GetMonth`, `BadgeGetFlag`) and plays a jingle (`MEPlay` and the sound stream functions). **How to reach a
character** (two failed runs before this one): a zone's characters are placed when the player enters it, so a save moved
inside the zone has none (run `talk1`: the tile in front of Littleroot's character 6 was empty); walking in from the
next zone places them, but a walk by held frames ends where it ends (`talk2` stopped beside the lab). A house is the
short way: the door recipe enters the zone, and its characters stand a few tiles from the arrival. `remake_tool
oras-inspect <oras.3ds> zone <n>` lists each character's tile, movement, kind, script, facing, sight and range (movement
0 stands still) and the trainer it is (script 3000 + trainer id: inferred, `OrasZone.h`), the way the mother was chosen.
The walk counted about 17 frames a tile (`hold right 90` for five tiles, checked on the screenshot). The talk trace's
functions: `prototype/coverage_map.py <work> house1 house_talk.txt --minus house_idle.txt`. Its 138 unnamed functions
were read on 7 October and 128 named: the speech balloons and their windows (`Msg_Balloon_*`, `Msg_Window_*`: six slots,
the tail pointed at the speaker), the look-at controller that turns the two characters' heads (`Gfx_LookAtController_*`,
`Field_LookAt_*`, the joint "Spine2"), the music saved around the jingle (`Snd_BgmStack_*`), the script's resource slots
and work values (`Script_*`), matrix helpers (`Util_Mtx34_*`) and 27 DllField import stubs; the meaning of the look-at
and music names is a guess, their mechanics read.

**Wild encounter selection** (7 October, run `enc1`, grass walk minus idle, no battle in eight walks; run `enc2` with a
battle). **Read** in the decompilation: on each step `Field_WildEncounter_StepCheck` (0x102CD2B4, DllField) takes the
tile's kind (`Map_TileAttr_Kind`, bits 24-31: 0x1F, 0x20, 0x24, 0x25, 0x2B, 0x2D and 0x42 table 0, 0x27 table 1, any
other kind no encounter; flag bit 1 set, table 3 whatever the kind: water, a guess), the zone's encounter file
(`Zone_DataContainerFile`, file 3 of the zone loader's slot 0), the tile's rate (`Field_WildEncounter_StepRate`), rolls
a horde on the file's byte +8 (when the horde table at +0xC6 is not empty), scales the rate
(`Field_WildEncounter_ApplyRateModifiers`) and rolls it against `random(100)`, picks the table
(`Field_WildEncounter_SelectTable`: kinds 0-8 at +0xE, +0x3E, +0x6E, +0x7A, +0x8E, +0xA2, +0xAE, +0xBA, +0xC6) and
starts the encounter event. The container accessor `BinLinker_GetFile` never ran in the walk: the file is fetched
through the zone loader. **Checked on the data**: the zone's file 3 equals its member 537 entry for all 536 zones, and
zone 23's table 0 reads 12 slots of species 265, 263 and 261 (Wurmple, Zigzagoon, Poochyena) at level 2. The slot pick
**read** (`Field_WildEncounter_PickSlotAndLevel`, 0x102DCA88): a slot is 4 bytes, a halfword (species bits 0-10, form
bits 11-15, form 31 an empty slot), the minimum level, the maximum level; the level is the minimum + random(max - min +
1); two context flags limit the pick to slots of type 8 or 12 (an ability's effect: a guess), otherwise the table kind's
own pick function chooses (not read); a horde rolls 20% for one different member. **Checked live** (run `enc2`, 7
October: 32 walks of two tiles, a battle within the first 8): a wild Wurmple at level 2, one of the table's slots; the
pick function ran in `enc2` and not in `enc1`. Not read: the per-kind pick functions, the meaning of tables 1-7 beyond
their offsets, the battle side. **The slot picks, read on 8 October**: `PickSlotAndLevel` calls one of 7 functions by
the mode `SelectTable` sets (`Field_WildEncounter_PickSlotMode0`-`6`, method table 0x103357F0), each a draw of
`random(100)` against a weight table: kinds 0 and 1, 12 slots, 10 x 9, 5, 4, 1; kind 2, 3 slots, 60, 35, 5; kinds 3 and
4, 5 slots, 50, 30, 15, 4, 1; kinds 5, 6, 7, 3 slots each, 60, 35, 5; kind 8, one of 3 groups of 5 (hordes), 60, 35, 5.
The slot counts fit the tables' sizes in the file exactly. What the kinds are is inferred from these counts and weights
(grass, a second grass, surfing for kind 3, which the step check picks on the water flag, rock smash, three rods,
hordes), except kind 2 (3 slots; zone 23 holds species 506, 570, 540 there): not established. `oras_tables.py
encounters` prints each kind's weights. **Member 537's use, read 8 October**: its only reader found is
`Field_ListZoneEncounterSpecies` (0x102B4FEC, DllField), which lists a zone's species by category through
`Encounter_ListSpeciesByCategory` (0x401040): 0 kinds 0, 1 and 8, 1 kind 3, 2 kinds 5-7 together, 3 and 4 kind 2 by its
mode. So the file's header bytes switch each kind on: byte k for kind k, byte 2 kind 2's mode (1 or 2; zone 23 has 1),
byte 8 the hordes (also the horde chance in the step check). The rods' grouping and the water kind back the inferred
names; which screen shows this list: not read (DexNav or the Pokedex's area page, a guess). **The kinds, read on 8
October**: kind 4 is Rock Smash (`Field_WildEncounter_StartRockSmash`, called by the script native
`CallWildBattleRockCrash`, no rate roll); kinds 5-7 are the fishing rods (`Fishing_TryEncounter` in the module
DllFieldEventFishing picks 5, 6 or 7 by its rod field, then `Field_WildEncounter_TryKind`, whose rate is the header byte
[kind] - 1); kind 3 is the water flag's (surfing: inferred, no Surf code read); kind 2 has no encounter starter among
the three found (the step check, TryKind, Rock Smash): only the species list reads it. Zone 24's tables match the game's
Route 102 (Zigzagoon, Wurmple, Poochyena, Seedot; Magikarp, Goldeen, Corphish by rod; Marill, Azumarill, Surskit on
water). Run `wild1` (the owner's `main_Wild_Encounter`, 24 walks in Route 102's grass): no battle (chance; `enc2`
already showed one).

**Trainers** (7 October; **checked on the data and live**: run `trainer3`, a battle against trainer 7 on Route 102, his
Zigzagoon at level 4 on screen as `a/0/3/8` gives it). The natives `_CallTrainerBattleCore` (44 zone scripts) and
`_TrainerEyeBattleStart` read a trainer through `Trainer_ReadData` (0x453E08): member <id> of `a/0/3/6` (archive 0x24,
950 members), id 0 or past 949 read as 1. `Trainer_BuildTeams` (0x454434) builds each trainer of a battle from it: the
record is 24 bytes, byte +0 the team's format, u16 +2 the class (under 280: `a/0/3/7`, read by `Trainer_ReadClass` and
loaded whole at boot by `GameData_LoadBootTables`), byte +7 the number of Pokemon; the team is member <id> of `a/0/3/8`
(archive 0x26), entries of 8, 16, 10 or 18 bytes for formats 0 to 3 (read: the four loops), species u16 at +4, form at
+6. **Checked**: all 949 trainers' team sizes equal count x entry size (trainer 0 is a 16-byte placeholder); trainers
1-3 (class 127) hold one Pokemon each, species 252, 255, 258 (Treecko, Torchic, Mudkip), u16 +2 of the entry 5 (the
level: seen, fits the rival's first fight); trainer 10, class 131, a Zigzagoon at 5. Not read: what formats 1-3 add
(likely moves and items: a guess), the record's other bytes, where a field character names its trainer (the character
records `oras-inspect` prints show no trainer id). **Live** (run `trainer3`, from a save the owner made in the game in
front of the trainer, `POMEGRADE_SAVE`): the battle ran `Trainer_ReadData`, `Trainer_ReadClass`, the native
`TrainerGetInfo`, `Trainer_BuildOneTeam` (0x454EA4, one trainer's record, class and team) and `Trainer_FillInfo`
(0x455450); `Trainer_BuildTeams` did not run (used elsewhere: not read). The trace minus `enc1`'s walk and idle and
`enc2`'s wild battle: 592 functions (209 `.code`, 177 `DllBattle?`, 193 `DllField?`, 13 DllUSAreaFriendShip); 220 of its
347 unnamed functions named on 8 October (42 read, 178 guess: the battle's action and argument blocks, type chart and
effectiveness, sequence commands, its UI panels; in the field a line-of-sight test, `Field_TrainerSight_CheckLine`
0x10263530, guess), and the last 127 the same day: 73 import stubs named by the target word after their `ldr
pc,[pc,#-4]` (`<Module>_Import_<target>`, read), 54 functions read with up to three callers each (and, for the 7 with no
direct caller, the pointer table holding them), 33 of them guess; **every function of the battle's trace is named** (0
left). Which character is which trainer: `oras-inspect zone` (script 3000 + id, inferred). (The "+0xAE" zone word noted
here on 7 October was a misread: moving a save into another matrix: section 0, **Run the game**.)

**Saving** (8 October, **read, then checked live**: run `sv2`, below). The game keeps its C++ class names (RTTI strings
`N8savedata<n><Class>E`): 60 classes in `savedata::`. `Save_SaveData_Construct` (0x115DE0) builds the 58 block objects
in block order; each class's vtable +0x10 returns its size, and the order and sizes match the save file's block table
one for one (all 58 but 19, which has no named class, and 35, `AccessPointSaveData`, whose getter is not a constant:
placed by the order). So block 4 is `Situation` (the player's zone and position), 10 `MoveModelSave` (the zone's
characters), 1 `MYITEM`, 2 `BAG`, 12 `BOX`, 18 `PokePartySave`, 20 `ZukanData`, 56 `BoxPokemon`; the full list is
`prototype/oras_save.py`'s `CLASSES` (`oras_save.py blocks` prints it). Writing: `Save_SaveData_WriteBlockTable`
(0x4652D4, SaveData's vtable +0x1C) fills the footer's table, for each block its size, its index (the "id" of the file's
table: checked equal to the index in the owner's saves) and its CRC16 (`Util_Crc16Ccitt` 0x173F84, seed 0xFFFF: the one
`OrasSave.cpp` writes), then `FEEB`. The thread `savedata::SaveLoadControlThread` (`Save_Thread_Start` 0x45D4C0,
`Save_Thread_DoMode` 0x124BEC) runs mode 3 load (`Save_ReadBlocks`), 4 full save (`Save_WriteBlocks` then
`Save_Commit`), 5 the writes alone and 6 the commit alone (the two-part save). The menu's "Sauvegarder" is the module
**DllReport** ("report", the Japanese games' word for saving): `Report_SaveFull` (0x1069FD2C) counts the save, shows
the message, refreshes the header data and calls `GameData_SaveFull` (0x1E7BA4: a new random save id in the footer,
then mode 4). The other callers of the full save: DllGameClearSave (the save after the credits), DllBattleMatch, DllGts,
DllLiveCup, DllRandomTrade. **Live** (run `sv2`, from `saves/main_Startpoint`: X, right, down held 10 frames (a
6-frame press did not move the cursor down), A on "Sauvegarder", A twice; `run_local.sh` leaves the file the game wrote
in the run's `Azahar/sdmc/.../00000001/main`): the file was rewritten with all 58 checksums right; blocks 0, 3
(`GameTime`), 4, 5, 6 (`PlayTime`), 10, 11, 17, 31 and 44 changed; block 44 (`Record`) u32 +4 went 6 -> 7 (the save
count `Report_SaveFull` adds, record 1); the footer (from 0x75DF8, `FEEB` at its +0x18) holds the new 64-bit save id at
0x75E00 and the previous one, the old current, at 0x75E08, as `Save_Footer_NewSaveId` reads. Not read: block 19's class and contents, what the header refresh (0x46511C) writes, the
archive calls under `Save_Commit` (guess).

**Door, sign and menu** (the three traces above, 1,728 more functions named on 7 October by six sub-agents over the
decompilation, two per part where a part was re-run; names that only said "role not determined" were left out; most of
these are graphics, layout and import stubs, **read** only where an agent read the code, the rest **guess**):

- **A door warp**, in order: `Warp_BuildDestinationFromWarpRecord` (0x4D8F3C, read: warp N, a 0x18-byte record of the
  zone's events, to a location of zone, warp index and kind 3/4); `Warp_CreateZoneChangeEvent` (0x3D60D4, read: a 0x7C-byte
  event, the destination header loaded); `Warp_GetArrivalPosition` (0x3CD634, read: the record's grid shorts to floats, 18
  units per tile, offset along its width/height); the entrance module `DllFieldEventEntranceIn` picks the door animation by
  kind (`Warp_EntranceIn_SelectHandler` 0x10393750, read: 0x1C-byte records) and walks the player in
  (`Warp_EntranceInStep` 0x103949A4, read: door sound, move command 0xEF); `Warp_ZoneChangeStep` (0x3E9F64, read) calls
  `Script_ResetZoneLocalState`, then `Zone_ChangeLoadStep` (0x3D5698, read: the new header, map and matrix reloaded only
  when the **area** id changes (0x4D896C), then the zone's events and scripts), then places the player by arrival kind
  (2, 4, 5; `Warp_PlacePlayerAtArrival` 0x3D5A0C, read). DllField frees the old field (`Warp_FieldTeardownStep`
  0x102DD418, read), reloads per-zone resources (`Field_ReloadZoneResources` 0x40405C, read: 3 handles), loads in up to 3
  steps per time budget (`Warp_FieldLoad_RunSteps` 0x102DEC18, read), rebuilds the field (`Field_ZoneSetupStep`
  0x102DF70C, read), loads the area's own module (`Field_LoadAreaSpecificModule` 0x10273D30, read: DllUSPokecen,
  DllUSGym*...) and shows the place name when it differs (`Field_ShowZoneNamePopup` 0x10261660, read).
- **A script showing a message**: `Event_FindTalkTargetInFront` (0x102665A8, read) finds the sign or character ahead;
  `Script_StartEventScript` (0x3DC070, read) and `Event_CreateScriptManager` (0x3FE45C, read: a 0x168-byte context);
  `Script_RunContextStack` (0x3FFA90, guess); `Msg_ResolveTextSource` (0x3FB230, guess: system text or the zone's script
  text). The window: `Msg_Window_Open` (0x10258B50, guess), `Msg_WindowLayout_SetStyle` (0x34E2D8, guess: 14 styles),
  `Msg_Window_CreateTextPrinter` (0x3482A8, guess: 0xF0 bytes). The printer, read: `Msg_TextPrinter_Construct` (0x3A7070,
  0x68-byte line slots), `Msg_SetText` (0x3A6F10), `Msg_TextPrinter_Update` (0x3A5F18: a character per tick, control codes
  0xBE00/0xBE01, likely wait-for-button and scroll), `Msg_ParseNextLineTag` (0x3A6D48), `Text_CountLines` (0x3A5C14),
  `Text_SeekToLine` (0x3A60A4). Text is UTF-16, 0x0A a line break, 0x10 the start of a tag. The script command that asks
  for the message: below.
- **Script commands are Pawn (AMX) natives, named by the game** (7 October). A zone script calls the game through
  `sysreq.n`, by a native's name hash (`h = h * 0x83 ^ c`, `Script_LinkNativeImports` 0x506118, read): the game ships the
  names, in tables of `{name, function}` pairs that the linker (`Sys_LinkImportsByNameHash` 0x506190) hashes and matches,
  first match wins. **Read**: `Script_RegisterNativeTablesByMask` (0x3FBC80) links a script's natives one table per bit of a
  mask, each DllField table through a `.code` import stub (0x19C8C8-0x19C910) to a two-instruction getter: bit 1 0x1033A234,
  2 0x10339594 (332 natives: messages, talk, sound, camera, models...), 4 0x1033A99C (`IECreate`...), 8 0x1033AFBC
  (`AILoad`...), 0x10 0x1033A9FC (`PokerusCheckTemoti`...), 0x20 0x1033958C (one `GetKeyCont` row before 2's rows), 0x40
  0x1033AA9C (`HideItemInit`), 0x80 0x1033AAAC, 0x100 0x1033AB44, 0x200 `.code` 0x5885CC, 0x400 0x1033ACDC. With the
  `.code`'s 0x57A860 (`_FadeRequestIn`...) and 0x5A630C (`floatround`...), registered at load (below), 799 functions, all
  named `Script_Native_<name>` in `function_names.tsv`. **Which tables a script gets** (read, and checked against every
  script): `Script_Load` (0x3AAE3C) registers for every script Pawn's core (0x5A6394: `numargs`... `random`), console
  (0x6179E4: `printf`) and float (0x5A630C) natives and `_Suspend` (0x5A6758); `Script_LoadFieldScript` (0x3BD498) adds the
  `.code`'s field table 0x57A860 (`_FadeRequestIn`...); then the mask. A zone script's mask comes from a table of **536
  words, one per zone**, at 0x587D58 (read by 0x3FF5AC): 0x243 (bits 1, 2, 0x40, 0x200) for 507 zones, 0x2C3 (+ 0x80) for
  19, 0x643 (+ 0x400) for 7, 0x343 (+ 0x100) for 3. **Checked**: all 16,321 native calls of the 1,072 zone scripts come from
  a table their zone's mask or the load registers; the running Littleroot script's context (0x8D734D0, run `msgdump`) holds
  mask 0x243 at +0x98. Other scripts take their mask from a record (+0x10, `Script_RunContextStack`) or from +0x34 of their
  owner (0x3FCA2C, 0x400D20: the `g_ai_flag` scripts). **Lifted (8 October)**: `oras-engine --zone-rows` replaces the load
at 0x3FF5AC by a call to a 7-instruction routine in the zeros closing the code segment (0x579608; the exheader's text
ends at 0x579604, its pages at 0x57A000): the game's table below 536, else a new table of 488 words for zones 536-1023,
0x243 unless `--script-mask ZONE:MASK` gives one. **Checked live** (run `mask2`: the zone-raised mod with this code.ips,
the save moved to zone 538, a breakpoint after the load): zone 538 read mask 0x343, the value given. Before: the mask
table has 536 entries, so a
  zone numbered 536 or more (allowed since the zone count was raised to 1,024, section 2) reads the words after it
  (0x05050502...: bits 2, 0x100, 0x400 and not bit 1, the flags and work values). Sinnoh's zones from 538 need the table
  moved and grown first. The functions around the script machine had been guessed as sound or graphics code
  (`Snd_SequenceReset`, `Snd_SequenceStep`, `Snd_SequenceStart`, `Gfx_AnimState_Step`, `Res_Dictionary_Lookup`,
  `Util_HashMapFind`): renamed `Script_Load`, `Script_RunUntilYield`, `Script_LoadFieldScript`, `Script_StepOrWait`,
  `Script_FindPublicVariable`, `Script_FindPublicFunction`. **Checked**: the 1,072 zone scripts call 384 distinct natives
  and all 384 resolve (`_Suspend`, 341 scripts, in a one-entry table at 0x5A6768: 0x1E2C1C stores params[1] - 1 in the
  context's wait counter at +0x88).
- **Ghidra's wrong no-return marks** (fixed 7 October, `ghidra/FixNoReturn.java`, run by `session_setup.sh` before
  `ApplyNames`). Its analysis marked 172 functions no-return; 8 hold a return: `memclr` 0x301FBC (357 callers), the global
  getters 0x14E348 (275) and 0x139660 (44), 0x3FE5C8 (the script context getter every native calls, through DllField's stub
  0x10243040), 0x34C9E4, 0x365034, 0x164060 and, on the second pass, `Event_Character_ResetForReuse` 0x3FB03C; 138 import
  stubs inherited the mark. Every call to them ended its caller, so code after it was missing from 1,753 functions: the
  natives decompiled as one line and 18 had been guessed as "no-return stubs" (renamed). **Checked**: after the fix
  `TalkMdlMsg_Seq` decompiles whole (its call to 0x102C1B74 shown); the function count is unchanged (47,557); 26 functions
  stay no-return, none with a return. `ApplyNames` now makes a function where a named address has none (177 named addresses,
  natives reached only through their table), so all the names apply (3,299 at the end of 7 October). **Checked** on a fresh
  `session_setup.sh`: 47,750 functions, the 47,557 of the analysis and 193 more (190 at named addresses), none lost.
- **The names reviewed** (7 October, after Ghidra's no-return fix): the earlier names were given from traces and from code
  Ghidra had cut short, and several were wrong. Three passes over `function_names.tsv`: (1) every named function whose code
  grew once read whole (600; 577 natives named from the game's tables, unaffected): the other 24 read again, 12 renamed (and
  `Zone_LoadZoneScript`, which they led to); (2) every **guess** whose subsystem prefix matches none of its named callers
  and callees (105, from Ghidra's call graph, `Util_`, `Sys_` and `Mem_` left out as used everywhere): each read, 19 renamed
  where the code contradicts the name (four `Warp_LargeUpdate_*` that only draw models, `Field_PopupIcon_Show` that opens
  the message balloon...), the rest kept where the code fits; (3) every placeholder naming a subsystem by the trace it ran
  in and an address (`Warp_LargeUpdate_`, `Warp_CheckedStep_`, `Warp_AllocInit_`, `Res_ReleaseHandles_`,
  `Gfx_ProcessEntries_`, `Util_Accessor_`..., 137): 72 named from their code, 6 whose role could not be determined left
  unnamed rather than wrongly named, and the 59 player-state and move-command handlers (`Field_PlayerStateHandler_`,
  `Field_PlayerStateCheck_`, `Event_MoveCmd_`) kept, each setting the state code its role says. 99 names changed in this
  review; each renamed role says what it was guessed before. The 1,113 names still marked **guess** pass the neighbour check
  of pass 2, but their code was not read in this review: a guess is a lead, to be read before it is relied on. To review
  again: `prototype/name_review.py` (`grew`, `suspects`, `placeholders`, then `summary` of the Export.java decompilation);
  on the names of 7 October `suspects` still lists 88, all read that day and kept.
- **The message command is `TalkMdlMsg_Seq`** (DllField 0x10296260, hash 0x9ADF1616, 324 scripts), **checked** (run `actE2`,
  recipe below): reading the Littleroot sign shows "Maison d'Andene", and the trace holds the native (running 0x747260,
  DllField at 0x6F3000) and `Msg_Balloon_Show` (0x102C1B74, running 0x772B74; guessed `Field_PopupIcon_Show` before), which
  the idle trace does not. Each script wraps it in a 19-parameter function (zone 6: 0xAC8) behind smaller ones (0x130:
  message, model, ...). **params[1] is the line of the zone's text file**, **checked** (run `msgdump`: the sign's recipe
  below with `wait 500`, then `dump 0x08000000 0x6000000` while "Maison d'Andene" is on screen): in the running copy of
  Littleroot's script (0x8D73898, data + 0x11B8) the call frame is 19 cells whose first is **16**, the line "Maison [VAR
  1408]" of Littleroot's text (`a/0/8/2` member 66, French), and whose third is -1 (no model: a sign). Littleroot's script
  also passes 6 to 13, the lines its townsfolk say (line 8 "Les hautes herbes qui bordent cette route…"). Read: params[2] &
  0xFF; params[3] sign-extended, a model id (-1 none: checked on the sign); params[4] a u16 stored in the request at +0x1C
  (role unknown); then 8 floats and 6 flags; the request goes to 0x102C1B74, a slot of 6 (a speech balloon: read, it opens
  the message window). Story texts: `a/0/7/9`-`a/0/8/6`, 637 members each, one archive per language (French `a/0/8/2`). The
  other text natives: `MsgLoad`, `MsgIsLoaded`, `MsgRelease`, `MsgSwap` (131 scripts), `MsgWinCloseNo`, `YesNoWin_Seq`,
  `ListMenuInit_Seq`/`ListMenuStart_Seq`, `WordSet*` (text variables). The recipe (`run_local.sh <work> actE2 - "6 103.5
  172.5" "<script>" 420`, `POMEGRADE_INTERPRETER=1`, 6 to 13 min alone on 4 cores): `mash a 15; wait 500; trace on; wait
  120; trace off idle.txt; hold up 4; wait 30; trace on; press a; wait 90; shot sign1; press a; wait 60; press a; wait 60;
  trace off sign.txt; shot sign2`. The field's load time varies between runs (DllField loaded at frame 886, 1,100 and
  1,361): with `wait 200` the field came up only during the sign trace.
- **The start menu**: `Field_CreateProcessByRequest` (0x3D7DD0, guess: one factory creating each field sub-screen by request
  id), `Field_CreateSimpleProcess` (0x52AF94, read), `Menu_LoadLayoutResources` (0x330A4C, read),
  `Menu_UpdateItemPanes` (0x102C0CDC, read: six entries shown by their enable bits), `Menu_CreateItemList` (0x103102F0,
  guess: the 6-slot list), `Menu_SetPaneVisible` (0x102AF4EC, read).

## 5. The archives

The game opens its archives by number: the code holds a table of 299 pointers (0x5F5050) to their paths in UTF-16
(`rom:/a/0/0/0` ... `rom:/a/2/9/8`), and an archive's number is its path's digits, `a/X/Y/Z` = X*100 + Y*10 + Z (`a/0/1/3`, the
zones, is 13: the number the zone loaders pass). **Read.** `ghidra/ArchiveUsers.java` lists every call to the open function
(`FUN_0011c94c(obj, heap, number, flag)`) with its number: 215 calls, 174 with a constant number, which tie **89 of the 299
archives** to the code that reads them; the others are opened with a number computed from a table or a parameter (their
callers not yet read). The module that opens an archive gives its domain; what each archive holds is recorded only where a
reader in `tools/remake/src` or `ORAS_LITTLEROOT.md` shows it.

| Archive | Members | Bytes | Opened (constant number) by |
|---|---|---|---|
| `a/0/0/0` | 8 | 98,992 | (a computed number) |
| `a/0/0/1` | 3 | 4,152 | DllWazaOboe |
| `a/0/0/2` | 5 | 44,968 | DllPokeList |
| `a/0/0/3` | 139 | 1,147,800 | (a computed number) |
| `a/0/0/4` | 1 | 388,348 | DllBattle |
| `a/0/0/5` | 51 | 4,786,612 | DllBattle |
| `a/0/0/6` | 1 | 16,812 | (a computed number) |
| `a/0/0/7` | 7 | 202,060 | DllBattle |
| `a/0/0/8` | 8067 | 1,070,643,868 | .code |
| `a/0/0/9` | 1 | 129,788 | .code |
| `a/0/1/0` | 3 | 252,460 | .code |
| `a/0/1/1` | 1 | 5,300 | .code |
| `a/0/1/2` | 1 | 6,580 | .code |
| `a/0/1/3` | 538 | 1,042,380 | .code |
| `a/0/1/4` | 229 | 29,406,084 | .code |
| `a/0/1/5` | 2 | 19,948 | (a computed number) |
| `a/0/1/6` | 8 | 2,529,120 | DllNuts |
| `a/0/1/7` | 9 | 2,139,356 | DllHeading |
| `a/0/1/8` | 26 | 800,376 | DllPuzzle |
| `a/0/1/9` | 2 | 120,804 | (a computed number) |
| `a/0/2/0` | 12 | 26,764 | DllStrInput |
| `a/0/2/1` | 544 | 22,538,408 | DllField |
| `a/0/2/2` | 511 | 22,796 | .code |
| `a/0/2/3` | 380 | 3,452,356 | DllField |
| `a/0/2/4` | 2 | 390,840 | (a computed number) |
| `a/0/2/5` | 1 | 80,504 | (a computed number) |
| `a/0/2/6` | 2 | 331,152 | (a computed number) |
| `a/0/2/7` | 1 | 193,488 | (a computed number) |
| `a/0/2/8` | 3 | 209,136 | (a computed number) |
| `a/0/2/9` | 85 | 503,244 | .code |
| `a/0/3/0` | 2 | 205,576 | DllWazaOboe |
| `a/0/3/1` | 2040 | 29,909,380 | (a computed number) |
| `a/0/3/2` | 1030 | 9,931,184 | (a computed number) |
| `a/0/3/3` | 41 | 6,540,572 | (a computed number) |
| `a/0/3/4` | 976 | 1,729,396 | (a computed number) |
| `a/0/3/5` | 16 | 363,776 | (a computed number) |
| `a/0/3/6` | 950 | 41,856 | .code |
| `a/0/3/7` | 280 | 10,144 | .code |
| `a/0/3/8` | 950 | 36,672 | .code |
| `a/0/3/9` | 857 | 108,419,976 | (a computed number) |
| `a/0/4/0` | 431 | 224,696 | .code |
| `a/0/4/1` | 1 | 1,957,116 | DllKawaigari |
| `a/0/4/2` | 1 | 192,764 | DllKawaigari |
| `a/0/4/3` | 1 | 44,140 | (a computed number) |
| `a/0/4/4` | 1 | 35,604 | (a computed number) |
| `a/0/4/5` | 1 | 9,896 | (a computed number) |
| `a/0/4/6` | 1 | 21,996 | (a computed number) |
| `a/0/4/7` | 1 | 11,316 | (a computed number) |
| `a/0/4/8` | 1 | 17,588 | (a computed number) |
| `a/0/4/9` | 1 | 24,580 | (a computed number) |
| `a/0/5/0` | 1 | 15,208 | (a computed number) |
| `a/0/5/1` | 1 | 19,884 | (a computed number) |
| `a/0/5/2` | 1 | 6,484 | (a computed number) |
| `a/0/5/3` | 1 | 10,860 | (a computed number) |
| `a/0/5/4` | 1 | 73,532 | (a computed number) |
| `a/0/5/5` | 1 | 14,484 | (a computed number) |
| `a/0/5/6` | 1 | 52,804 | (a computed number) |
| `a/0/5/7` | 1 | 24,944 | (a computed number) |
| `a/0/5/8` | 1 | 348 | (a computed number) |
| `a/0/5/9` | 6 | 17,752 | .code |
| `a/0/6/0` | 8 | 124,648 | (a computed number) |
| `a/0/6/1` | 1 | 140,244 | (a computed number) |
| `a/0/6/2` | 8 | 30,344 | DllField |
| `a/0/6/3` | 2 | 8,236 | .code |
| `a/0/6/4` | 1 | 743,116 | (a computed number) |
| `a/0/6/5` | 1 | 23,204 | (a computed number) |
| `a/0/6/6` | 1 | 10,164 | .code |
| `a/0/6/7` | 1 | 38,384 | (a computed number) |
| `a/0/6/8` | 1 | 194,748 | (a computed number) |
| `a/0/6/9` | 1 | 18,768 | (a computed number) |
| `a/0/7/0` | 1 | 21,172 | (a computed number) |
| `a/0/7/1` | 175 | 3,197,132 | (a computed number) |
| `a/0/7/2` | 175 | 2,957,724 | (a computed number) |
| `a/0/7/3` | 175 | 2,958,512 | (a computed number) |
| `a/0/7/4` | 175 | 3,122,056 | (a computed number) |
| `a/0/7/5` | 175 | 3,038,020 | (a computed number) |
| `a/0/7/6` | 175 | 3,228,740 | (a computed number) |
| `a/0/7/7` | 175 | 3,158,604 | (a computed number) |
| `a/0/7/8` | 175 | 1,610,864 | (a computed number) |
| `a/0/7/9` | 637 | 1,829,192 | (a computed number) |
| `a/0/8/0` | 637 | 1,696,108 | (a computed number) |
| `a/0/8/1` | 637 | 1,653,648 | (a computed number) |
| `a/0/8/2` | 637 | 1,719,372 | (a computed number) |
| `a/0/8/3` | 637 | 1,626,804 | (a computed number) |
| `a/0/8/4` | 637 | 1,976,212 | (a computed number) |
| `a/0/8/5` | 637 | 1,754,712 | (a computed number) |
| `a/0/8/6` | 637 | 884,368 | (a computed number) |
| `a/0/8/7` | - | 0 | (a computed number) |
| `a/0/8/8` | 741 | 6,610,088 | (a computed number) |
| `a/0/8/9` | 1114 | 452,084 | DllKawaigari |
| `a/0/9/0` | 1 | 168,004 | (a computed number) |
| `a/0/9/1` | 974 | 306,864 | .code, DllSangoZukan |
| `a/0/9/2` | 631 | 174,008 | .code |
| `a/0/9/3` | 1 | 2,324 | DllBattle |
| `a/0/9/4` | 1 | 19,444 | .code |
| `a/0/9/5` | 1 | 532 | DllBattle |
| `a/0/9/6` | 1 | 4,700 | .code, DllSangoZukan |
| `a/0/9/7` | 1 | 3,312 | (a computed number) |
| `a/0/9/8` | 1 | 804 | .code, DllPorocCase |
| `a/0/9/9` | 12 | 16,596 | (a computed number) |
| `a/1/0/0` | 4594 | 17,710,504 | (a computed number) |
| `a/1/0/1` | 161 | 6,202,448 | (a computed number) |
| `a/1/0/2` | 3 | 10,096 | (a computed number) |
| `a/1/0/3` | 1 | 464,428 | .code |
| `a/1/0/4` | 28 | 212,024 | (a computed number) |
| `a/1/0/5` | 52 | 3,778,860 | DllKawaigari |
| `a/1/0/6` | 10 | 193,616 | (a computed number) |
| `a/1/0/7` | 1 | 16,708 | (a computed number) |
| `a/1/0/8` | 304 | 99,752 | (a computed number) |
| `a/1/0/9` | 3 | 92,576 | DllTrainerCase |
| `a/1/1/0` | 1 | 96,108 | (a computed number) |
| `a/1/1/1` | 1 | 8,100 | (a computed number) |
| `a/1/1/2` | 2 | 53,184 | (a computed number) |
| `a/1/1/3` | 1 | 61,280 | (a computed number) |
| `a/1/1/4` | 88 | 911,092 | DllSparring |
| `a/1/1/5` | 47 | 1,568 | DllSparring |
| `a/1/1/6` | 36 | 2,284 | DllSparring |
| `a/1/1/7` | 48 | 2,560 | DllSparringMenu |
| `a/1/1/8` | 80 | 20,544 | (a computed number) |
| `a/1/1/9` | 80 | 18,944 | (a computed number) |
| `a/1/2/0` | 500 | 28,064 | (a computed number) |
| `a/1/2/1` | 500 | 24,064 | (a computed number) |
| `a/1/2/2` | 63 | 8,884 | (a computed number) |
| `a/1/2/3` | 50 | 7,064 | (a computed number) |
| `a/1/2/4` | 80 | 3,264 | (a computed number) |
| `a/1/2/5` | 100 | 26,464 | (a computed number) |
| `a/1/2/6` | 58 | 4,936 | (a computed number) |
| `a/1/2/7` | 80 | 8,064 | DllSparringMenu |
| `a/1/2/8` | 80 | 8,064 | DllSparringMenu |
| `a/1/2/9` | 109 | 9,828,804 | DllSparring |
| `a/1/3/0` | 722 | 23,424 | DllSparring |
| `a/1/3/1` | 2 | 4,676 | (a computed number) |
| `a/1/3/2` | 2 | 30,056 | .code |
| `a/1/3/3` | 75 | 6,283,904 | (a computed number) |
| `a/1/3/4` | 1 | 4,184 | (a computed number) |
| `a/1/3/5` | 2 | 7,704 | .code |
| `a/1/3/6` | 158 | 9,095,164 | (a computed number) |
| `a/1/3/7` | 228 | 41,824 | .code |
| `a/1/3/8` | 1 | 327,980 | (a computed number) |
| `a/1/3/9` | 44 | 14,332 | (a computed number) |
| `a/1/4/0` | 5 | 62,364 | DllHologramMail |
| `a/1/4/1` | 1 | 61,460 | (a computed number) |
| `a/1/4/2` | 1 | 19,832 | (a computed number) |
| `a/1/4/3` | 4 | 1,464 | (a computed number) |
| `a/1/4/4` | 1 | 94,548 | (a computed number) |
| `a/1/4/5` | 241 | 747,936 | (a computed number) |
| `a/1/4/6` | 26 | 31,832 | DllKawaigari, DllKawaigariArrange |
| `a/1/4/7` | 1 | 17,468 | DllField |
| `a/1/4/8` | 1 | 38,320 | (a computed number) |
| `a/1/4/9` | 1 | 161,612 | (a computed number) |
| `a/1/5/0` | 1 | 2,144 | (a computed number) |
| `a/1/5/1` | 1 | 76,580 | (a computed number) |
| `a/1/5/2` | 1263 | 43,296,568 | (a computed number) |
| `a/1/5/3` | 1 | 4,412 | (a computed number) |
| `a/1/5/4` | 7 | 327,756 | .code |
| `a/1/5/5` | 1 | 285,784 | (a computed number) |
| `a/1/5/6` | 6 | 1,033,400 | (a computed number) |
| `a/1/5/7` | 1 | 18,056 | (a computed number) |
| `a/1/5/8` | 26 | 45,720 | DllHologramMail |
| `a/1/5/9` | 1 | 6,116 | (a computed number) |
| `a/1/6/0` | 684 | 34,726,764 | DllTrainerCase |
| `a/1/6/1` | 1 | 73,484 | (a computed number) |
| `a/1/6/2` | 87 | 126,884 | (a computed number) |
| `a/1/6/3` | 1 | 19,452 | (a computed number) |
| `a/1/6/4` | 1 | 48,960 | (a computed number) |
| `a/1/6/5` | 1 | 74,796 | (a computed number) |
| `a/1/6/6` | 1 | 28,448 | (a computed number) |
| `a/1/6/7` | 5 | 3,569,936 | .code |
| `a/1/6/8` | 1 | 10,356 | (a computed number) |
| `a/1/6/9` | 35 | 1,023,008 | .code |
| `a/1/7/0` | 53 | 33,136 | .code |
| `a/1/7/1` | 1 | 3,348 | (a computed number) |
| `a/1/7/2` | 7 | 3,192 | .code |
| `a/1/7/3` | 1 | 210,360 | (a computed number) |
| `a/1/7/4` | 15 | 86,656 | .code |
| `a/1/7/5` | 2 | 8,456 | .code |
| `a/1/7/6` | 1 | 580 | (a computed number) |
| `a/1/7/7` | 1 | 2,044 | (a computed number) |
| `a/1/7/8` | 22 | 7,360 | .code |
| `a/1/7/9` | 1 | 21,140 | (a computed number) |
| `a/1/8/0` | 1 | 191,436 | .code |
| `a/1/8/1` | 1 | 485,268 | (a computed number) |
| `a/1/8/2` | 999 | 36,028 | (a computed number) |
| `a/1/8/3` | 129 | 13,780 | (a computed number) |
| `a/1/8/4` | 999 | 36,028 | .code |
| `a/1/8/5` | 217 | 31,912 | .code |
| `a/1/8/6` | 999 | 36,028 | .code |
| `a/1/8/7` | 1 | 36,720 | (a computed number) |
| `a/1/8/8` | 1 | 10,428 | .code |
| `a/1/8/9` | 1 | 24,972 | (a computed number) |
| `a/1/9/0` | 722 | 23,332 | (a computed number) |
| `a/1/9/1` | 826 | 72,696 | (a computed number) |
| `a/1/9/2` | 826 | 56,232 | (a computed number) |
| `a/1/9/3` | 826 | 36,408 | (a computed number) |
| `a/1/9/4` | 8 | 3,456 | (a computed number) |
| `a/1/9/5` | 827 | 148,764 | (a computed number) |
| `a/1/9/6` | 723 | 18,856 | (a computed number) |
| `a/1/9/7` | 776 | 43,520 | (a computed number) |
| `a/1/9/8` | 1 | 860 | (a computed number) |
| `a/1/9/9` | 1 | 327,356 | (a computed number) |
| `a/2/0/0` | 1 | 120,108 | (a computed number) |
| `a/2/0/1` | 1 | 33,460 | (a computed number) |
| `a/2/0/2` | 1 | 9,752 | (a computed number) |
| `a/2/0/3` | 1 | 1,824 | (a computed number) |
| `a/2/0/4` | 1 | 8,824 | (a computed number) |
| `a/2/0/5` | 1 | 54,424 | (a computed number) |
| `a/2/0/6` | 1 | 13,436 | (a computed number) |
| `a/2/0/7` | 1 | 121,520 | (a computed number) |
| `a/2/0/8` | 1 | 30,548 | (a computed number) |
| `a/2/0/9` | 1 | 50,168 | (a computed number) |
| `a/2/1/0` | 9 | 147,312 | DllField |
| `a/2/1/1` | 1 | 22,016 | (a computed number) |
| `a/2/1/2` | 1 | 202,328 | .code, DllBattle, DllSangoZukan |
| `a/2/1/3` | 1 | 945,740 | DllBattle |
| `a/2/1/4` | 1 | 659,360 | DllBattle |
| `a/2/1/5` | 1 | 244,272 | (a computed number) |
| `a/2/1/6` | 1 | 152,636 | DllHeading, DllNuts, DllPuzzle |
| `a/2/1/7` | 1 | 115,216 | (a computed number) |
| `a/2/1/8` | 1 | 340 | (a computed number) |
| `a/2/1/9` | 1 | 101,004 | (a computed number) |
| `a/2/2/0` | 13 | 1,323,076 | (a computed number) |
| `a/2/2/1` | 1 | 23,024 | (a computed number) |
| `a/2/2/2` | 2 | 928 | (a computed number) |
| `a/2/2/3` | 1 | 300 | .code |
| `a/2/2/4` | 1 | 15,544 | (a computed number) |
| `a/2/2/5` | 1 | 4,551,416 | (a computed number) |
| `a/2/2/6` | 1 | 21,068 | (a computed number) |
| `a/2/2/7` | 3 | 21,948 | .code |
| `a/2/2/8` | 1 | 68,856 | (a computed number) |
| `a/2/2/9` | 1 | 1,856 | (a computed number) |
| `a/2/3/0` | 1 | 29,164 | DllSangoStaffroll |
| `a/2/3/1` | 1 | 728 | (a computed number) |
| `a/2/3/2` | 1 | 644 | .code |
| `a/2/3/3` | 10 | 78,532 | (a computed number) |
| `a/2/3/4` | 1 | 1,047,108 | (a computed number) |
| `a/2/3/5` | 1 | 301,904 | DllSangoZukan |
| `a/2/3/6` | 1 | 7,992 | DllSangoStaffroll |
| `a/2/3/7` | 7 | 192,296 | DllDendouDemo |
| `a/2/3/8` | 1 | 2,684 | DllFatalErrorProc |
| `a/2/3/9` | 1 | 5,196 | (a computed number) |
| `a/2/4/0` | 1 | 295,480 | (a computed number) |
| `a/2/4/1` | 1 | 804 | (a computed number) |
| `a/2/4/2` | 11 | 13,196 | .code, DllContestCommon |
| `a/2/4/3` | 1 | 95,216 | (a computed number) |
| `a/2/4/4` | 1 | 152,768 | (a computed number) |
| `a/2/4/5` | 1 | 13,620 | (a computed number) |
| `a/2/4/6` | 1 | 39,696 | (a computed number) |
| `a/2/4/7` | 1 | 4,196 | (a computed number) |
| `a/2/4/8` | 1 | 128,428 | (a computed number) |
| `a/2/4/9` | 1 | 27,424 | (a computed number) |
| `a/2/5/0` | 1 | 322,260 | DllBattle, DllContest, DllContestOhirome, DllContestResult |
| `a/2/5/1` | 4 | 1,316,668 | (a computed number) |
| `a/2/5/2` | 1 | 3,052 | (a computed number) |
| `a/2/5/3` | 2 | 188,592 | (a computed number) |
| `a/2/5/4` | 4 | 852,700 | (a computed number) |
| `a/2/5/5` | 1 | 96,532 | (a computed number) |
| `a/2/5/6` | 1 | 269,908 | DllSangoZukan |
| `a/2/5/7` | 173 | 4,419,652 | (a computed number) |
| `a/2/5/8` | 1 | 52,444 | (a computed number) |
| `a/2/5/9` | 1 | 76,432 | (a computed number) |
| `a/2/6/0` | 1 | 94,488 | (a computed number) |
| `a/2/6/1` | 1 | 132,792 | (a computed number) |
| `a/2/6/2` | 1 | 768 | (a computed number) |
| `a/2/6/3` | 2017 | 7,450,204 | (a computed number) |
| `a/2/6/4` | 10 | 162,060 | DllContest |
| `a/2/6/5` | 1 | 10,160 | (a computed number) |
| `a/2/6/6` | 1 | 50,260 | (a computed number) |
| `a/2/6/7` | 30 | 236,896 | (a computed number) |
| `a/2/6/8` | 1 | 3,796 | (a computed number) |
| `a/2/6/9` | 2 | 103,952 | (a computed number) |
| `a/2/7/0` | 1 | 136,536 | (a computed number) |
| `a/2/7/1` | 1 | 329,840 | (a computed number) |
| `a/2/7/2` | 1 | 46,704 | (a computed number) |
| `a/2/7/3` | 1 | 121,628 | (a computed number) |
| `a/2/7/4` | 1 | 1,095,928 | (a computed number) |
| `a/2/7/5` | 1 | 26,104 | (a computed number) |
| `a/2/7/6` | 1 | 107,832 | (a computed number) |
| `a/2/7/7` | 386 | 409,972 | (a computed number) |
| `a/2/7/8` | 1 | 526,388 | (a computed number) |
| `a/2/7/9` | 1 | 154,592 | (a computed number) |
| `a/2/8/0` | 1 | 119,496 | DllContestResult |
| `a/2/8/1` | 1 | 99,568 | (a computed number) |
| `a/2/8/2` | 1 | 17,024 | (a computed number) |
| `a/2/8/3` | 1 | 1,840 | (a computed number) |
| `a/2/8/4` | 1 | 109,784 | (a computed number) |
| `a/2/8/5` | 1 | 123,520 | (a computed number) |
| `a/2/8/6` | 1 | 45,548 | (a computed number) |
| `a/2/8/7` | 1 | 87,260 | (a computed number) |
| `a/2/8/8` | 1 | 44,804 | (a computed number) |
| `a/2/8/9` | 1 | 10,652 | (a computed number) |
| `a/2/9/0` | 1 | 102,576 | (a computed number) |
| `a/2/9/1` | 1 | 57,724 | (a computed number) |
| `a/2/9/2` | 1 | 32,944 | (a computed number) |
| `a/2/9/3` | 1 | 24,368 | (a computed number) |
| `a/2/9/4` | 1 | 182,612 | (a computed number) |
| `a/2/9/5` | 1 | 22,156 | (a computed number) |
| `a/2/9/6` | 1 | 27,808 | (a computed number) |
| `a/2/9/7` | 1 | 254,412 | (a computed number) |
| `a/2/9/8` | 1 | 21,372 | (a computed number) |

## 7. Pokémon X beside ORAS: can their assets be merged (16 October)

Read on the owner's dumps of Pokémon X (Europe, decrypted, program 0004000000055D00, 653 RomFS files) and Omega Ruby in
a session's scratch space (CLAUDE.md §8: never committed), with `remake_tool oras-list`, `oras-extract`, `garc` and a
throwaway script over the members; the scenario that wants it is `docs/Equilibrium_Scenario.md`. `oras-list` and
`oras-extract` open X's RomFS as they do Omega Ruby's.

| | Omega Ruby | X | Checked |
|---|---|---|---|
| Map pieces (`GR` containers) | `a/0/3/9`, 857 | `a/0/4/1`, 1,030 | seen: same container |
| Tile grid (GR file 0) | 6,528 bytes in all 857 | 6,528 bytes in all 1,030 | seen: same size, 40 x 40 |
| Piece files | grid, BCH, `coll`, three of 128-256 bytes, `KAGE` (7) | the same without `KAGE` (6) | seen on one piece each; counts 7/9/11 against 6/8/10 over all |
| Area packs (`AD`) | `a/0/1/4` | `a/0/1/3`, 170 | seen: same container |
| Characters | `a/0/2/1`, 544 members | `a/0/2/1`, 286 members | seen: same archive and container |
| Character skeleton | 129 of 328 field models have the 25 standard bones (Origin ... RToe) | 125 of 177 | names only; bone positions and hierarchy not compared |
| BCH | compatibility 0x21, revision 42607, every character and piece model | compatibility 0x07, revision 38669, every character (286) and piece model (1,115) | read in the headers |

**X's characters in Omega Ruby** (16-17 October): `remake_tool bch` reads X's BCH 0x07 (a material's 3 texture mappers
inline, 0x58 bytes, SPICA's `TextureMappersCompat`). A whole X character (its `a/0/2/1` member) is made Omega Ruby's by
`oras-copy ... <dst>=xy:<x.3ds>:<member>` (or `bch-upgrade` on the member; `XyCharacter.h`):
- its model and its blinking animation (a BCH of material animations, 52 of the 286) rewritten as 0x21 (`BchUpgrade.h`:
  header, materials, texture relocations, key frame groups). All 286 characters convert whole; the 286 models read back
  identical to X's (same glTF);
- its motion pack moved from X's 173 slots to ORAS's 239 (`GfMotionPack.h`, which gives each move and its evidence):
  same motion format; the slots **inferred** from runs of equal frame counts, the characters both games share, the
  characters of the same name in both, the two games' players, and the slot sets of whole characters (X {1, 3, 80},
  90 of them, is ORAS's {1, 3, 94}, 70). X's 75 motions with no ORAS one (its players', the features ORAS lacks) go to
  slots no ORAS character fills, always the same slot (`remake_tool xy-motion-slots` prints the table), for a later
  feature to play: **every X character keeps every motion** (all 159 packs, checked motion by motion). X's players
  (members 1 and 2) as NPCs walk with their converted pack (run `xplayer`) but show only the shadow: their model file is
  a placeholder (one 12-triangle cube, no texture; X's customisable player is presumably built from parts kept
  elsewhere, not looked for), unlike ORAS's player (member 171, a 277 KB model). ORAS's 157, 158
  and 170 have no X motion; 70 ORAS characters lack them too.

How a character's model number finds its member: through the area pack's character list (`a/1/3/7`, 24 bytes an entry,
the member at word 10: Littleroot's model 4153 is member 503, 284 is 284).

**Checked live** (headless, character 284 of the sandbox replaced): X's member 10 left as 0x07 loads but only its shadow
shows (run `xchar`); converted it is drawn (`xchar2`: a white-coated trainer, its texture white and grey); X's member 31,
coloured, is lit as the player is (`xchar3`), turns on its own (`xmove`, movement 47), turns to the player and talks, then
walks two tiles north and back with its walking motion (`xwalk2`, the scene of `sandbox/scene_demo.txt`). Not seen: the
blinking, and the motions of X's slots dropped (ORAS's 157-158, 8 frames, have no X counterpart; what the game plays when a
slot is empty was not looked at). X's map pieces (1,115) not tried.

**Tile values**: X uses 173 distinct values, Omega Ruby 208, 40 of them shared; by count, 93.3 % of X's tiles hold a value
Omega Ruby also uses (walkable 0x00000020 and 0x00000004, wall 0x01000021). Omega Ruby's tall grass 0x20004004 and sea
0x3D1A0006 never occur in X: grass, water and the other special tiles need a value-by-value table, each checked on the
phone. Zones, scripts and X's code were not compared.

**The whole of X's map against ORAS's limits** (16 October, `prototype/game_census.py <remake_tool> <oras.3ds> <x.3ds>
[archives.tsv]`, 8 s; X's archives found by their members' magics):

| | ORAS | X | Together | ORAS's limit | |
|---|---|---|---|---|---|
| Zones (`ZO`) | 536 (`a/0/1/3`) | 360 (`a/0/1/2`) | 896 | 1,024 once raised (section 2) | fits |
| Largest zone file | 17,416 bytes | 18,588 bytes | | 19,032 (the zone buffer, section 2) | fits, by 444 bytes |
| Zone container | 5 files | 4 files | | | same first four (56-byte header, events, script, encounters); ORAS adds a 12-byte fifth, not read |
| Map pieces (`GR`) | 857 (`a/0/3/9`) | 1,030 (`a/0/4/1`) | 1,887 | a matrix holds them as u16 (0xFFFF none: `OrasMatrix.h`) | fits the numbering; the archive's own reader not read |
| Matrices (`MM`) | 431 (`a/0/4/0`) | 227 (`a/0/4/2`) | 658 | u16: the zone header's word 2, `Map_StartMatrixLoad` (0x4074C4) stores it as a u16 (read 16 October) | fits |
| Area packs (`AD`) | 228 (`a/0/1/4`) | 170 (`a/0/1/3`) | 398 | none found: appended packs load once a/0/1/4's per-pack table (its member 228) and a/1/3/7 have their entries (section 2's table, run `apkA`); the zone table holds a pack number in a u16 (as the tools read it; the game's reader not read) | fits (checked with 2 appended packs, not 170) |
| Script natives | 384 called | 473 called | | | 137 of X's are not registered by ORAS; 159 of X's 360 zones call at least one (the most called, `0xE3B5A658`, in 118) |

So X's ground, zones and matrices are ORAS's formats; X's scripts cannot run as they are, and a
mod that brings X's places writes its own (as `docs/Equilibrium_Scenario.md` does anyway). Not yet known: the
X's map pieces converted, and a run of an X town in ORAS.

**Order of work**: a BCH 0x07 reader and its conversion to 0x21 (done, 16 October); one X character in an Omega Ruby
town, run on the phone (headless done: drawn, lit, walking); the tile table; one X piece in Omega Ruby.

**Vaniville Town in Omega Ruby** (18 October, checked headless, run hVan4): X's zone 0 (outdoors, interiors 1-7) lies on
matrix 0's cells (11, 14)-(12, 15), map pieces 12-15 of `a/0/4/1`, textures in X's area pack 2. `oras-sandbox --xy <x.3ds>`
with `xy-piece X Y M P` lines (`sandbox/vaniville.txt`, `OrasXyPiece.h`) puts them on a new 2 x 2 matrix as zone 538: each
model upgraded to BCH 0x21, its 31 + 9 + 2 textures added to the sandbox's pack (renamed `_x2` where pack 8 holds another
of the same name), the cell rewritten in file 4, Littleroot's `KAGE` added as file 7. The player stands and walks on X's
paving, the Navi-Map names the zone. Vaniville uses four tile values: 0x20, 0x01000021 and 0x28 occur in Omega Ruby's pieces
(0x28 on 203 tiles), 0x31000020 (one tile) in none and is made 0x20 (inferred). Found on the way: the engine's
`--zone-rows` must equal the zone table's rows (FUN_00112b50 stopped the boot with 541 given for 539, run hVan2); X's door
models (file 3: types 1, 2, 5, 10) in a zone without warps stopped the field in `Map_PlacePropsFromContainer`
(DllField 0x102608AC: the door-model manager, `[field + 0xAC] + 0x170`, null; run hVan3), so they are left out until doors
are converted. Not done: the doors and the houses' interiors, Vaniville's people, X's model names (`map01_11_14` kept, a
length unlike the sandbox's `world16_00_00`; whether the game reads it is not known).

**Vaniville's doors and props** (18 October, checked headless): a piece's file 3 holds its doors **and props** (Vaniville:
X's types 1 street lamps, 2 a door, 5 house doors, 10 the town map). A type is a member of the prop archive (X `a/0/2/4`,
Omega Ruby `a/0/2/3`, BM containers: model, then its animations) listed in the area pack's file 0 (u32 count, 0x50-byte
entries: the type twice, 8 flag bytes, 16 u16 file references; read in `Event_SpawnFieldModelInstance`, DllField
0x102691A8). X's props are appended to `a/0/2/3` (380 up), their entries copied into the pack under the new numbers and the
pieces' types renumbered (`XyPropsToOras`). Found on the way, each a fatal in `Map_BuildAreaModels` (DllField 0x10269594):
X's skeletal animations (a door's opening) are not converted by the BCH upgrade (dictionary 8), so their references are
cleared and the doors do not swing (run hVan5); props bind their textures from the pack's file 1, where Omega Ruby keeps no
`projection_dummy` (runs hVan6, hVan7: the first X entry failed, r7 = 7), copied there. Earlier `Map_PlacePropsFromContainer`
fatal (run hVan3) was these props unknown to the pack, not a missing warp. **Doors** (`door X Z xy ZX height H`): the inside
made of X's zone (`XyInsideToOras`: its piece and props, X's matrix copied with the piece renumbered, a copy of house 223's
pack 112 with X's textures, the way out at X's warp 0). A warp must stand at the ground's height (word 5): X's Vaniville
is at 144, and a warp at 0 did not fire (run hDoor2); at 144 the player enters X's house (zone 2's ground floor) and walks
back out (runs hDoor4, hDoor5). **Stairs**: X's two-floor houses link their floors by warps of kinds 0x501/0x503 (height
+-9: zones 4-5, 6-7); an X/Y inside now brings the zones its other warps lead to, X's warps kept with their destinations
renumbered (not checked: see below). **Houses 2 and 3** (zones 4 and 6) first stopped the game on entering: the block trace of the entry (run hTr, `trace
on`/`off`, the one traced block calling the fatal) named `Gfx_SetupCharacterModelInstance` (DllField 0x102E2F30), its first
check (the piece's model object's byte 6 == 1) failing. Their terrain materials name `projection_dummy` (house 1's do not),
which their pack, a copy of house 223's, holds in no file; with it copied to the pack's file 11 they load (runs hSt6, hD3).
On the way: my first try read the materials through a destroyed temporary (`Bch::Read(...).Models.at(0)` in a range-for),
fixed. Ruled out before: the pack slot, the piece's files, the matrix's areas (file 1 made whole: no change, reverted).
**Stairs** (checked, run hSt10): the upstairs of house 2 is reached by walking up into the stairs warp from the tile below
(the stairs' tiles are solid, like a door's). **The doors' and lamps' animations** (18 October): the BCH upgrade now converts skeletal animations too (dictionary 8):
a `Transform` element's 9 slots get the key frame group rewrite of the material animations, a `QuatTransform` or
`MtxTransform` element (X's door `t01_door`: 59 frames, its two leaves) is kept, SPICA showing no version-dependent field in
them. Checked: SPICA reads every bone's pose of every frame identical before and after (the door, the lamp's `gaitoh_in`);
the props load with their animations bound and the doors still enter (runs hOp2, hOp3). Not checked: that the leaves are
seen to swing (the headless frames are too small and the camera moves). **Checked on the phone (owner, 18 October, mod
vaniville)**: the three houses enter and exit; the stairs work in the houses at x 12 and 35 (X's zones 6 and 4), not in the
one at x 27 (zone 2: no stairs warp, as expected); the windows' lighting works. **Seen wrong on the phone, not fixed yet
(owner's choice)**: no door is drawn in the doorways (entering works); the street lamps are not seen, or do not light.
Not done: X's zone 3 (house 1's upper floor, no warp to it), the people.

## 8. Ultra Sun/Moon characters on Omega Ruby (17 October)

The owner's Ultra Moon dump (Europe, `3DS1772`, 3.7 GB, Drive folder "Pokemon Project - Radiant Platinum") is read by the
same tools as Omega Ruby's: `remake_tool oras-list`/`oras-extract` open its RomFS (program 00040000001B5100, 747 files) and
`spica` reads its models (GFL2). Its player models, made Omega Ruby field characters for a preview, by
`prototype/usum_character.py` (its header gives the commands; prototype/README.md how to run it in a session).

**The USUM player** (seen, Ultra Moon):
- built from parts, one GARC per kind, every member a `CM` package (model pack, motion pack, mask texture): Elio
  `a/1/7/6` face, `a/1/7/7` hair, `a/1/8/1` bottoms, `a/1/8/2` hat, `a/1/8/3` legs, `a/1/8/4` shoes, `a/1/8/6` tops
  (also `a/1/7/8` eyewear, `a/1/7/9` bag, `a/1/8/0` bangle, `a/1/8/5` riding poses); Selene `a/1/8/8`... `a/1/9/9` in the
  same order; a second copy of the same sets from `a/2/0/3` (not compared). Member 0 of each is the starting outfit
  (`p1_tops_stripe`, `p1_bottoms_dpan`, `p1_hat_cap`...); the members of one model differ only in colour (same size).
  Found by the model names inside (`p1_hair_...`); `a/1/7/4` (316 members) holds the characters' bases with their
  skeleton motions (member 0, `p1_base`: 4 motions, the first the idle).
- every part's model has the 79-bone skeleton `pc0001_00` (Selene `pc0002_00`): realistic proportions (waist at 76
  units, about 150 tall), against ORAS's chibi field characters (Brendan: waist at 12.9, 33 tall, the head a third).
  A part rooted at `Neck_attach` or `Head_attach` (Elio's hair; Selene's hair and hat) is modelled in that bone's space.
- skin and hair colours are not in the textures: an item's `_m0N` texture (alpha) marks where the runtime colour goes
  (`_m03` hair, `_m01`/`_m02` skin: the arms on a top, the legs, the face); the clothes' colours are drawn in. The
  colours themselves (the player's choice) were not found; 58,46,40 for the hair and the ORAS character's skin are chosen
  by eye.
- the face: a sheet of eye cells (`p1_face01_on`, its material animations move v by +0.5 to blink, read in its motion
  pack) and a mouth part whose UVs at v > 0 are the inside of the mouth (teeth, tongue), shown only when the jaw opens.
- spica's PNG export is bottom-up against the SMD's UVs: a texel's row is v, not 1 - v (the face sheet shows it: with
  1 - v the eyes are upside down and a blink lands on an empty cell).

**On an ORAS body** (`usum_character.py build`; previews approved by the owner on 17 October: Elio on Brendan):
re-proportioning the USUM model itself (bone lengths scaled, head enlarged) was tried first and dropped: the face sheet,
the inside of the mouth and the hair all needed fixes, and its skeleton would have no ORAS motion. Kept instead: ORAS
character 172 (Brendan, `rstr0002_00_fi`; 171 is May, `rstr0001_00_fi`) whole (body, skeleton, face sheet, proportions:
its motions fit), without its hat and hair (texture `_head`), its bag (bones `Bag*`) and its switchable parts
(`sw_parts`: bike, surf); its body texture repainted texel by texel through the triangles that use it, each triangle's
region from its dominant bone (torso and upper arms: shirt; hips and thighs: shorts; feet: shoes; skin kept), with the
outfit's colours read from its textures, the base's shading kept; its irises made grey; the USUM hair and cap fitted on
its Head bone (scaled on the face's width, 0.398 for Elio; the hair below the head joint shortened by half), the cap
grown x1.1 around the head so the hair stays inside it (pushing the hair under the cap instead crushed the bangs under
the visor). `spica smd` now names an ORAS field character's triangles after their real texture (their texture 0 is a
placeholder, `projection_dummy`).

**USUM's field** (seen, Ultra Moon, 17 October): `a/0/8/3` holds 336 zones of 11 files each (3,696 members; their
2-letter containers `BG` x2, `ED`, `ZS`, `ZI`, `aa`, `AS`, `TR`, `AE`, `AC`, `EA`, roles not read). A `BG` package nests
further `BG` packages around bare GF models and textures, which SPICA's own BG reader does not find: `spica bg <member>
<out prefix> <tex dir>` walks them (99 members hold models, 172 in all; a zone's models and textures sit in different
members). It skips what SPICA cannot convert and counts it (36 in member 1: their format not identified). `a/0/8/1` (168
members) holds the battle backgrounds (`btl_G_city`...).

**Not done**: the in-game model. `build` writes what it needs (the repainted textures, the hair and cap on the base's
skeleton as `<out>_head.smd`); the BCH holding them (a copy of member 172 with the head mesh replaced and the textures
swapped) is not written by any tool yet, nor tried headless or on the phone. Selene's result (on May) was generated, not
reviewed by the owner.
