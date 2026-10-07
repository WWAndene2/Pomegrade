# Omega Ruby's engine, decomposed

The atlas of the game's code and data, built from the code itself (Ghidra's decompilation and the emulator's GDB stub on the
running game), not from guesses: what each part does, the tables it reads, the limits it enforces and how to lift them. The
goal (owner, 7 October): a sandbox in which Sinnoh and a story are built on a blank map, nothing tied to Hoenn. Each fact is
marked **checked** (seen in memory or under the debugger), **read** (from the decompiled code) or **inferred**.
`ORAS_LITTLEROOT.md` keeps the history of the Littleroot and Route 201 work; this file keeps the engine.

## 1. How the engine is read

- **The dumps** (Drive folder "Pokemon Project - Radiant Platinum": the cartridge, Platinum, the owner's save `main.zip`) are
  fetched into the session's scratch space, never into the repository (`CLAUDE.md` §8).
- **Ghidra** (11.4.2, headless): `remake_tool oras-code` writes the ExeFS `.code` decompressed (5,439,488 bytes, loaded at
  0x100000); imported raw as `ARM:LE:32:v6` at 0x100000 and auto-analysed (256 s on 4 cores): **16,402 functions, 3.9 MB of
  code**. `ghidra/Export.java` lists them (entry, size, callers, callees) and decompiles given addresses:
  `analyzeHeadless <project> oras -process code.bin -noanalysis -scriptPath tools/remake/ghidra -postScript Export.java <out dir> <address>...`
- **The running game**: the headless core (`tools/remake/headless`) with the GDB stub; script command `gdb 24689`, then
  `gdb-multiarch` with `set architecture arm`, `set osabi none` (the stub's "3DS" OS type crashes gdb), `target remote :24689`.
  Breakpoints stop the emulated CPU; the core runs a scripted walk meanwhile.
- **All the code in one program**: `prototype/cro_link.py <out> static.crs <all .cro>` links the 145 modules at once from 0x10000000 (8.9 MB, to 0x108D9000), their imports resolved to the `.code` through `static.crs`'s absolute segments (code 0x100000, rodata 0x57A000, data 0x5EC000) and to each other; 0 unresolved. **Checked** against the game's own load of DllField (memory dump on the field, script command `dump`): every code word matches but the 340 that call other modules (placed elsewhere by design); 101 data words differ (written at run time). `ghidra/AddModules.java` adds the image to the `.code`'s program, names the exports and disassembles from them before auto-analysis.
- **The whole program analysed** (7 October, 417 s): the `.code` and the 145 linked modules, **47,602 functions** (18,373 in the `.code`, 4.05 MB; 29,229 in the modules, 3.87 MB). **Checked**: DllField's piece handler (offset 0x1E8CC, linked at 0x102608CC) decompiles with its calls into the `.code` resolved (the file count `< 7` test, the fatal call 0x11EF4C), as read by hand in `ORAS_LITTLEROOT.md` 0.
- **Code modules**: 145 CRO files in the RomFS (655 files in all), loaded above the `.code` (DllField at 0x6F3000 on the field);
  `prototype/cro_dis.py` reads one with its relocations and imports. They include Game Freak's debug modules `DllFieldDebug`,
  `DllDebugPokeMake` and `DllDebugProcLoop`: **checked**, empty in the retail game (8 KB each, 220 bytes of code, no text): no
  developer menu to switch back on.

## 2. Zones

- A zone is three records in `a/0/1/3`: its own member (0-535, a `ZO` container: header copy, events, scripts), a 56-byte
  entry in the **zone header table** (member 536, 30,016 bytes = 536 x 56) and an entry in the **encounter container**
  (member 537, `EN`, 536 files). **Read.**
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
- **What raising the zone count takes** (derived first, now built as above): the table member 536 grown by 56 bytes a
  zone and the size word at 0x112C0C patched to match; the bound 0x217 in the two loaders raised (the immediate `cmp r5,
  #0x218` at 0x3D9774 and the one in `FUN_003d99b8`); the encounter container (member 537) grown with them (its reader not yet
  read); the new zones' members appended from 538. Other per-zone tables (flags, names, the region map) still to find.

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
