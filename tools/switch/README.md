# Switch tools (Mali custom version of Scarlet)

Our own scripts for opening a Switch game dump and reading its code, for the plan in
`docs/Eden_Optimisation_Xiaomi.md` section 11. The dumps, keys and anything extracted from them
stay in the session's scratch space and are never committed (CLAUDE.md §8).

| File | What it does | Checked |
|---|---|---|
| `xci_contents.py` | Lists the NCAs of an XCI's secure partition with offsets and sizes, to cut one out without extracting the cartridge | Its first version (secure partition at a fixed 0xF600) was used on Scarlet's dump; this version finds the partition from the header and gives the same offset by computation, not rerun on the dump |
| `nso2elf.py` | Converts an NSO module (`main`, `sdk`, `subsdk*`) to an ELF that Ghidra's own ELF loader reads, with the dynamic symbols and relocations | Used on Scarlet 4.0.0's exefs; imported into Ghidra 11.4.2 |
| `trpfs_extract.py`, `flatbuffer_reader.py` | Extract the files of the packs whose name contains a text from the asset archive `arc/data.trpfs` (index `arc/data.trpfd`), decompressing Oodle with `oozdec` | Used on Scarlet 4.0.0's two shader packs: output identical to the first hand-run extraction |
| `ooz_linux/` | Builds ooz (powzix, GPL, open-source Oodle decompressor) on Linux: `Windows.h` shim and `oozdec.cpp`, a one-block decompressor. Build: clone ooz, cut `kraken.cpp` before `ParseCmdLine` into `kraken_lib.cpp`, then `g++ -O2 -w -fpermissive -Iooz_linux -o oozdec ooz_linux/oozdec.cpp kraken_lib.cpp bitknit.cpp lzna.cpp` (with an empty `SDKDDKVer.h`, `tchar.h`, `intrin.h` beside the shim) | Decompressed all of Scarlet's shader files, sizes as the pack states |
| `hactool_newer_updates.patch` | Local changes to hactool (SciresM), applied with `git apply` in its checkout | Used on Scarlet 4.0.0 |

**Why hactool needs a patch.** Scarlet's update 4.0.0 (SDK 21) lays out its patch RomFS
differently from what upstream hactool accepts: tables follow the AesCtrEx table inside the
section, and the AesCtrEx table ends at the indirect table rather than at its own start. Upstream
hactool stops with "Invalid BKTR layout!" or "BKTR section seems to be corrupted". The patch
accepts both layouts (Eden reads these updates; its `fssystem` code was the reference). It also
prints each RomFS file's size with `--listromfs`, and extracts only the paths containing the text in
the `HACTOOL_ONLY` environment variable.

**Steps used for Scarlet 4.0.0** (update NCAs need the base program NCA):

```
python3 xci_contents.py scarlet.xci                       # find the program NCAs
# cut base and update program NCAs at those offsets
hactool -k prod.keys -t nca --exefsdir=exefs upd.nca      # main, sdk, subsdk0, rtld, main.npdm
python3 nso2elf.py exefs/main main.elf                    # then import main.elf into Ghidra
hactool -k prod.keys -t nca --basenca=base.nca --listromfs upd.nca
HACTOOL_ONLY=/arc/ hactool -k prod.keys -t nca --basenca=base.nca --romfsdir=romfs upd.nca
```
