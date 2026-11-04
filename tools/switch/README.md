# Switch tools (Mali custom version of Scarlet)

Our own scripts for opening a Switch game dump and reading its code, for the plan in
`docs/Eden_Optimisation_Xiaomi.md` section 12. The dumps, keys and anything extracted from them
stay in the session's scratch space and are never committed (CLAUDE.md §7).

| File | What it does | Checked |
|---|---|---|
| `xci_contents.py` | Lists the NCAs of an XCI's secure partition with offsets and sizes, to cut one out without extracting the cartridge | Its first version (secure partition at a fixed 0xF600) was used on Scarlet's dump; this version finds the partition from the header and gives the same offset by computation, not rerun on the dump |
| `nso2elf.py` | Converts an NSO module (`main`, `sdk`, `subsdk*`) to an ELF that Ghidra's own ELF loader reads, with the dynamic symbols and relocations | Used on Scarlet 4.0.0's exefs; imported into Ghidra 11.4.2 |
| `trpfs_extract.py`, `flatbuffer_reader.py` | Extract the files of the packs whose name contains a text from the asset archive `arc/data.trpfs` (index `arc/data.trpfd`), decompressing Oodle with `oozdec` | Used on Scarlet 4.0.0's two shader packs: output identical to the first hand-run extraction |
| `ooz_linux/` | `build_libooz.sh` builds ooz as `libooz.so` (in-process decompression for `scarlet_assets/`: `stdafx_portable.h` replaces ooz's Windows header, only the decompressor is kept, ooz fetched at commit 0503806). The older one-block command line: ooz (powzix, GPL, open-source Oodle decompressor) on Linux: `Windows.h` shim and `oozdec.cpp`, a one-block decompressor. Build: clone ooz, cut `kraken.cpp` before `ParseCmdLine` into `kraken_lib.cpp`, then `g++ -O2 -w -fpermissive -Iooz_linux -o oozdec ooz_linux/oozdec.cpp kraken_lib.cpp bitknit.cpp lzna.cpp` (with an empty `SDKDDKVer.h`, `tchar.h`, `intrin.h` beside the shim) | Decompressed all of Scarlet's shader files, sizes as the pack states |
| `maxwell_compiler/` | `spirv_to_maxwell.cpp`: compiles a SPIR-V compute shader to Maxwell instructions with Mesa's nouveau codegen (GM200 target). Storage buffer binding *i* follows NVN's compute ABI, read from the game's own compute shader `gpu_instancing_culling`: address and size at `c0[0x310 + 16 * i]`. `skinning.comp`: compute replacement of the game's `system/skinning` (variations 0 and 1; `-DWITH_NORMAL` for 1), four storage buffers (model vertex buffer of stride 48 bound from the first vertex, its size giving the vertex count; bones; position and normal outputs in the transform feedback layout) | Both variants compile (2800 and 3552 bytes, 31 and 33 registers, no local memory); not run on a GPU, not yet compared with the original's output |
| `bnsh_compute.py` | Puts Maxwell code from `spirv_to_maxwell` into a copy of a game BNSH holding one compute shader (template: the game's `system/gpu_instancing_culling`), setting the NVN control fields: end of instructions (control `+1784`), embedded constant size (`+1788`, 0), registers per thread (`+1804`), workgroup size (`+1864`). The rest of the template's control (local and shared memory sizes, barrier count) is kept, which over-allocates rather than under-allocates; the control's hash words (`+2000..2020`) are not recomputed | Builds both skinning variants (code read back identical); field meanings inferred by comparing the game's vertex and compute shaders, not checked on hardware; the output contains the game's template and is never committed |
| `scarlet_mali/` | The Mali version of Scarlet's own code: an exlaunch module (`subsdk9`) Eden loads beside the game from a mod's `exefs/`. `source/` holds our code (`nvn_slots.hpp`: the game's NVN function pointers; `main.cpp`), `build.sh` builds it with devkitPro's `devkitpro/devkita64` Docker image and exlaunch (shadowninja108, GPL-2.0, commit f9f4b0d) into `build/mod/Mali Scarlet/exefs/subsdk9`. Current build: a probe that intercepts program creation and transform feedback draws and only logs them (`[scarlet_mali]` lines in Eden's log); nothing is drawn differently | Builds (3 November); not run yet |
| `trace_metrics.py` | The same measurements for every Punica Perfetto trace (game FPS from Eden's presents, frame intervals, GPU clock, busiest threads), for before/after comparisons | Used on the 3 November baseline |
| `scarlet_assets/` | Reads Scarlet's assets straight from the owner's XCI, decrypting only what is asked for (`SCARLET_DUMP`: folder of `scarlet.xci` and `prod.keys`): `xci.py` (XCI, NCA3, RomFS), `pack.py` (trpfd index, trpfs packs, any file by its path's FNV-1a hash), `bntx.py` (Switch textures, Tegra block-linear), `model.py` (`.trmsh`/`.trmbf`/`.trmtr` meshes to JSON and PNG for `tools/terrain`), `ground_layers.py` (the 60 terrain ground materials). Used by `tools/terrain` | 4 November: run from the repo into empty folders, the fir model and ground layers came out byte-identical to the session's |
| `check_nvn_slots.py` | Checks each NVN slot of `scarlet_mali/source/nvn_slots.hpp` against main.elf's relocations and names the function it holds; run before every module build | Added after probe v9 swapped the wrong functions (3 November) |
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

**Building `spirv_to_maxwell`.** Clone Mesa, `pip install meson mako pyyaml packaging`, then
`meson setup build -Dgallium-drivers=nouveau -Dvulkan-drivers= -Dtools=nouveau -Dplatforms= -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dllvm=disabled -Dgles1=disabled -Dgles2=disabled -Dzstd=disabled -Dexpat=disabled -Dlibunwind=disabled -Dvalgrind=disabled -Dbuildtype=release`,
`ninja -C build` the static libraries `libnouveau_codegen.a`, `libvtn.a`, `libnir.a`, `libcompiler.a`, `libmesa_util*.a`, `libblake3.a`, `libmesa_util_c11.a`, and compile `spirv_to_maxwell.cpp` against them with the `-D` flags Mesa uses for `nv50_ir_from_nir.cpp` (from `build/compile_commands.json`) and Mesa's `include`, `src`, `src/gallium/include`, `src/gallium/drivers/nouveau`, `src/compiler/nir` and matching `build/src/...` include folders. Shaders: `glslangValidator -V --target-env vulkan1.2`. The Mesa checkout used was its main branch of 1 November 2026 (version 27.0.0-devel).
