# Omega Ruby's engine, decomposed

The atlas of the game's code and data, built from the code itself (Ghidra's decompilation and the emulator's GDB stub on the
running game), not from guesses: what each part does, the tables it reads, the limits it enforces and how to lift them. The
goal (owner, 7 October): a sandbox in which Sinnoh and a story are built on a blank map, nothing tied to Hoenn. Each fact is
marked **checked** (seen in memory or under the debugger), **read** (from the decompiled code) or **inferred**.
`ORAS_LITTLEROOT.md` keeps the history of the Littleroot and Route 201 work; this file keeps the engine.


## 0. Start here (the state on 7 October, for whoever continues)

**The owner's objectives, in order** (their words, 7 October): (1) take the game apart to the binary and learn every part;
(2) reference every table and asset; (3) a limitless sandbox; (4) remove the memory, cache, zone and asset limits; (5)
patch the game where it is in the way; (6) Sinnoh rebuilt on a blank map, nothing tied to Hoenn; (7) a story mod. Step
1-5 are under way here; 6 and 7 wait on them. The owner tests mods on their phone (Pomegrade, `ORAS_LITTLEROOT.md` 0).

**Where each stands**:

| Objective | State |
|---|---|
| 1 Decomposition | the whole code (`.code` + 145 modules) in one Ghidra program, checked against the game's own load; Ghidra's wrong no-return marks cleared (`FixNoReturn.java`: code was missing from 1,753 functions); **3,443 functions named** (`ghidra/function_names.tsv`, read or guess), from coverage traces (section 6: entering a zone, a door warp, a sign, the start menu) and from the game's own tables: **every script native** (799, all 16,321 native calls of the 1,072 zone scripts resolved), the script machine (load, run, wait, natives by mask), the message command `TalkMdlMsg_Seq` **checked live** with its first argument (section 6); the names **reviewed** on 7 October (99 changed: section 6), 1,113 still marked guess, to be read before they are relied on. talking to a character **traced and checked** (section 6: 307 functions beyond a sign, all but 10 named); the wild encounter selection **read and checked live** (section 6: the step check, the zone's encounter file, its tables, the slot and level pick; a wild Wurmple at level 2 in run `enc2`). **Next**, in this order: trainers; saving; the region map (needs touch input in retro_host). Then objectives 3 and 6 |
| 2 Tables and assets | zones (section 2), map pieces (3), the boot memory map (4.2), the 299 archives tied to their code where opened by a constant (5); not yet: the 210 archives opened by computed numbers, the asset formats beyond `tools/remake/src`'s readers |
| 4 Limits | **done for building a world** (section 4.6): all 927 fatal checks listed, the field's and the `.code`'s classified; lifted and checked headless: zones 536 -> 1024 (2), application memory 64 -> 124 MB (New 3DS mode) and the linear heap 43.3 -> 88 MB, the normal heap and heap 4, heap 0xC 2 -> 8 MB, heap 0x17 28.4 -> 64 MB (4.4), characters past 26 (4.5); refused at build time where they cannot be raised: a zone's events file under 0xC84 bytes (4.5), a piece model's 51 textures (4.6), the 178 MB mode (4.4); **found, not lifted yet**: a zone script's native mask table holds 536 entries (section 6: zones from 536 read past it); left with their reason: the 8-deep load queue per object, collision objects per cell, the 174-entry Secret Base table (4.6) |
| 5 Patches | `remake_tool oras-engine` writes them all (`exheader.bin`, `exefs/code.ips`); **checked on the phone (owner, 7 October): mod `all6`** (the whole of Sinnoh as r12, the title, the save in Twinleaf, and `engine --memory 124 --linear-heap 0x5800000 --normal-heap 0x1800000 --heap 0xC:0x800000 --heap 0x17:0x4000000 --characters 64`, Remake mod run 147) with the APK of `main` at PR #33: "everything works fine" |
| 3, 6, 7 | not started on this basis: Sinnoh's region tools (`oras-region`, `ORAS_LITTLEROOT.md`) still borrow Hoenn's zones; next is building Sinnoh's zones from 538 up with the tools above |
| After 6: Platinum's music | added by the owner (7 October), after building Sinnoh's map: a tool that moves Platinum's music into Omega Ruby. Not started; nothing in `tools/remake` reads either game's sound yet. Platinum keeps sequences played by the DS sound hardware (SDAT: SSEQ with SBNK/SWAR instruments), Omega Ruby recorded streams in its sound archive (BCSTM, inferred from the format's common use, not checked on this game): the tool must extract, render, encode and replace. First step: how Omega Ruby stores and picks its songs (`Snd_ChangeZoneBgm`, section 6) |

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
  saves the screen (the host's own "field up" detection is wrong under some mods: look at the picture). `report` lists every
  thread; one in the fatal-error loop shows pc 0x11EF50 / 0x11ABxx / 0x110Axx, `0011EF60` in its stack code addresses, and
  its registers and raw stack, from which the frames' saved registers give the object that failed.
- **Show that a function runs**: `trace on` / `trace off FILE` around the action (needs `POMEGRADE_INTERPRETER=1`), then
  look for the function's running address in the file (a DllField function at its linked address - 0x10242000 + 0x6F3000) or
  map the whole trace with `prototype/coverage_map.py`. Not GDB: `gdb-multiarch` is not installed by `session_setup.sh` and
  the stub dropped the connection in both tries of 7 October.
- **Read a value while it runs**: script commands `mem`, `watch` (a word's changes frame by frame), `dump ADDRESS LENGTH
  FILE`. A script's state: `dump 0x08000000 0x6000000 heap.bin` while the thing is on screen, then
  `prototype/amx_dump.py heap.bin --script <size>` lists the loaded scripts, their contexts (native mask, wait) and the call
  frames left in their stack (how `TalkMdlMsg_Seq`'s first argument, 16, was read).
- **Read code**: `analyzeHeadless <work>/ghidra_proj oras -process code.bin -noanalysis -readOnly -scriptPath
  tools/remake/ghidra -postScript Export.java <out> <address>...` decompiles the functions holding those addresses (a
  module's address is its linked one: `linked/modules.tsv` gives each base, DllField 0x10242000; in the running game
  DllField sits at 0x6F3000). `prototype/code_find.py <work>/dumps/code.bin --at|--imm|--word|--calls ...` finds an
  instruction, a constant or every call to an address (`--base 0x10000000` on `linked/modules.bin` for the modules);
  `prototype/cro_dis.py <module.cro> dis <offset> <n>` reads a module with its imports resolved (`symbols` on `static.crs`
  and a module lists which module function each `.code` import stub reaches).
- **Read a script**: `remake_tool oras-script <oras.3ds> <zone> [init]` disassembles it; `prototype/amx_natives.py tables
  <code.bin> <linked/modules.bin> natives.tsv`, then `... | amx_natives.py names natives.tsv` puts each native's name beside
  its hash, and `amx_natives.py check` proves every zone script's natives are registered (section 6). A zone's text:
  `remake_tool oras-text <oras.3ds> a/0/8/2 <member>` (French; `a/0/7/9`-`a/0/8/6` one archive per language).
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

**Talking to a character** (7 October, run `house1`; **checked**: the mother's words on screen, `TalkMdlMsg_Seq` (running
0x747260) and `Msg_Balloon_Show` (0x772B74) in the talk trace and not in the idle one). Beyond what reading a sign runs,
talking loads and runs the character's own script (`Script_Load`, `Script_LoadFieldScript`,
`Script_RegisterNativeTablesByMask`, the core, float and console natives), turns the two to face each other
(`_TalkMdlStartInit`, `TalkMdlSetEyeToEye`, `TalkMdlSetTalkMotion`, `MdlAcmd*`), reads the story state (`FlagGet`,
`WorkGet`, `GetMonth`, `BadgeGetFlag`) and plays a jingle (`MEPlay` and the sound stream functions). **How to reach a
character** (two failed runs before this one): a zone's characters are placed when the player enters it, so a save moved
inside the zone has none (run `talk1`: the tile in front of Littleroot's character 6 was empty); walking in from the next
zone places them, but a walk by held frames ends where it ends (`talk2` stopped beside the lab). A house is the short way:
the door recipe enters the zone, and its characters stand a few tiles from the arrival. `remake_tool oras-inspect <oras.3ds>
zone <n>` lists each character's tile, movement, flag, facing and range (movement 0 stands still), the way the mother was
chosen. The walk counted about 17 frames a tile (`hold right 90` for five tiles, checked on the screenshot). The talk
trace's functions: `prototype/coverage_map.py <work> house1 house_talk.txt --minus house_idle.txt`. Its 138 unnamed
functions were read on 7 October and 128 named: the speech balloons and their windows (`Msg_Balloon_*`, `Msg_Window_*`: six
slots, the tail pointed at the speaker), the look-at controller that turns the two characters' heads
(`Gfx_LookAtController_*`, `Field_LookAt_*`, the joint "Spine2"), the music saved around the jingle (`Snd_BgmStack_*`), the
script's resource slots and work values (`Script_*`), matrix helpers (`Util_Mtx34_*`) and 27 DllField import stubs; the
meaning of the look-at and music names is a guess, their mechanics read.

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
their offsets, the battle side.

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
  owner (0x3FCA2C, 0x400D20: the `g_ai_flag` scripts). **A limit, not lifted yet**: the mask table has 536 entries, so a
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
