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
  (black screen); **with it** the field loads, the player walks, thread 1 idle. So the 536-zone limit is lifted by data plus
  three code words. Not yet checked: the phone; save data kept per zone (flags, visited places); the region map; zones far
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
