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
- **Code modules**: 145 CRO files in the RomFS (655 files in all), loaded above the `.code` (DllField at 0x6F3000 on the field);
  `prototype/cro_dis.py` reads one with its relocations and imports. They include Game Freak's debug modules `DllFieldDebug`,
  `DllDebugPokeMake` and `DllDebugProcLoop` (not yet read).

## 2. Zones

- A zone is three records in `a/0/1/3`: its own member (0-535, a `ZO` container: header copy, events, scripts), a 56-byte
  entry in the **zone header table** (member 536, 30,016 bytes = 536 x 56) and an entry in the **encounter container**
  (member 537, `EN`, 536 files). **Read.**
- The header loader (`FUN_003d9740`): `if (zone > 0x217) fatal; header = table + zone * 0x38`, the table kept in memory whole;
  its other branch reads a zone file whose decompressed size must stay under a constant (`DAT_003d9884`). **Read; the
  `cmp r5, #536` at 0x3D9774 checked under the debugger** (zone 6 on the field).
- Other comparisons with 536/537 to classify: `.code` 0x3D99F8, 0x4E4C64, 0x4E4CA4 (cmp), 0x112BC0, 0x1D6C7C, 0x31D60C,
  0x3CBDF4, 0x444964, 0x444B84 (mov); DllField 0x1B8EC, 0x35FE4, 0x9D998.

## 3. Map pieces

- The piece slot table: 4 slots, filed under the word opening each piece's file 4 (`row | column << 8`); full table = fatal
  (`FUN_003c8a24`), a slot freed for the cell leaving the player's window (`0x3C8C84`). **Checked** (the Route 201 freeze,
  `ORAS_LITTLEROOT.md` 0).
- The piece buffer: 1 MiB (`0x100000`), checked in memory with piece 857.
