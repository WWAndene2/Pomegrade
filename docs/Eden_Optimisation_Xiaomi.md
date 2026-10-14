# Eden optimisation study: Xiaomi 13T first, then low-end phones

Study of how to make the Switch core (Eden v0.2.1, `eden/`) faster on the owner's Xiaomi 13T (MediaTek Dimensity
8200-Ultra, Mali-G610 MC6) and then on weaker phones (the owner's goal: "Switch even on a potato"). Test game:
Pokemon Sword/Shield (64-bit, so run by NCE, section 1).

**Status (14 October): code audit, the owner's start-of-game logs read, a first baseline measured by the owner
(section 4.1). No code changed.** Each statement is marked:
- **seen**: read in the code, with its file and function;
- **inferred**: follows from what was read, not checked by running anything;
- **unknown**: needs the phone (a log, a trace, a measurement);
- **measured**: read by the owner on the phone (overlay) or in the logs he sent.

No gain in this document is measured. Estimates are given as orders of magnitude and stay hypotheses until the A/B
tests of section 7 have run.

Rules kept throughout: correctness and synchronisation first; no game-specific hacks that hide rendering or
simulation errors; lowering graphics settings stays the user's choice, never the fix. Eden is upstream code
(CLAUDE.md §4.9): each edit is minimal, and its commit says why.

All paths below are relative to `eden/src/`.

### Direction (owner, 13 October)

The goal is to **raise the ceiling**, not to lower quality or flip settings. Work goes deep into the emulator's
code paths, and into binaries (the game's and the Switch firmware's) when that is what it takes. Setting changes
(async presentation, frame pacing, build flags) are kept only as measurement aids or side items; the catalogue's
weight is on sections 5.2 and 5.4. Working on binary code to make a Switch game run close to an Android-native one is in scope (owner,
13 October): patching the game's code at load, recompiling it, replacing its graphics driver.

**How far from native a Switch game is today, stage by stage** (from section 2):

| Stage | Today | Native already? | Binary-level route to native |
|---|---|---|---|
| Game CPU code | NCE runs it on the phone's CPU, patched at load (`patcher.cpp`) | **Yes**, apart from exits (SVCs, faults) | Fewer exits (D4); patch the hottest SVC call sites into direct calls |
| Operating system | Eden's kernel and services in C++ (HLE) | Close: host code, but reached through SVC round trips | Fast paths answered in the trampoline (D4), checked against the firmware's modules |
| Graphics driver (NVN, linked into the game) | Runs as guest code, builds Maxwell command streams | Native CPU code, but its output must then be decoded | Replace it (D7): calls go straight to a Vulkan layer, no command stream |
| GPU commands | Decoded and translated by Eden's command processor and caches | **No**: the largest remaining layer | Removed by D7; until then D3, D6 |
| Shaders | Maxwell code translated to SPIR-V on first use | **No** at first use, yes once cached | Translated ahead of time from the game's files (D1, D2) |
| Memory tracking | Write-protected pages and faults | **No**: a cost a native game never pays | Disappears for buffers D7 hands straight to Vulkan; coarser tracking (D4, O10) otherwise |

Patches apply to the owner's own dumps, at load or in the session's scratch space; game and firmware code is never
committed, uploaded or put in an artifact (CLAUDE.md §8). A patch targets a library version (NVN, SDK), not a game's
behaviour, so it is a translation layer, not a game-specific hack.

---

## 1. How Eden runs a Switch game, and where time can go

```
 guest code (ARM64)          HLE services            GPU emulation                  host GPU
 ┌───────────────┐  SVC   ┌───────────────┐ nvdrv ┌──────────────────────┐  Vulkan  ┌─────────┐
 │ NCE: runs on  ├───────►│ kernel, nvdrv,├──────►│ command processor     ├────────►│ Mali    │
 │ the host CPU  │        │ vi/nvnflinger │       │ (Maxwell 3D methods)  │         │ driver  │
 │ (4 CPUCore_N  │◄───────┤ audio, hid    │       │ buffer/texture caches │         └────┬────┘
 │  threads)     │ faults └───────────────┘       │ shader recompiler     │              │
 └──────┬────────┘                                │ (Maxwell → SPIR-V)    │         present
        │ writes to GPU-cached pages → SIGSEGV    └──────────────────────┘         (swapchain)
        └──────────── memory tracking ─────────────────────▲
```

| Stage | Cost when it goes wrong |
|---|---|
| Guest CPU (NCE) | Host core too slow (placement), signal round trips (SVCs, memory faults) |
| HLE services | Host time spent in service code on the guest's core |
| GPU command processing | Decoding methods, cache lookups, Vulkan recording; with async GPU off it runs on a guest core |
| Shader recompiler | Maxwell to SPIR-V, then the driver's compile: stalls when a pipeline is new |
| Synchronisation | CPU waiting for the GPU (`Scheduler::Finish`), GPU data read back for the CPU |
| Host GPU | Fill rate, bandwidth (shared with the CPU on a phone), driver overhead |
| Thermal | Clocks fall after minutes of load |

---

## 2. Findings by subsystem

### 2.1 Build

| Finding | Where | Mark |
|---|---|---|
| Native code `CMAKE_BUILD_TYPE=Release` in CI | `.github/workflows/android-apk.yml` (`POMEGRADE_NATIVE_BUILD_TYPE`) | seen |
| No CPU tuning: `YUZU_BUILD_PRESET` "custom" sets no `-march`, so plain armv8-a | `eden/CMakeLists.txt` (build presets) | seen |
| The APK is a debug build (`assembleProdDebug`, debuggable); Eden ships `release` with minify | `android-apk.yml`; `android/app/build.gradle.kts` (`buildTypes`) | seen |
| Eden's `UseLTO` module is included; whether LTO is on in Pomegrade's configuration | `eden/CMakeLists.txt` | unknown |

### 2.2 Guest CPU

| Finding | Where | Mark |
|---|---|---|
| NCE is the default backend on Android arm64: guest code runs natively, patched at load (SVC, `MRS TPIDR*`, `CNTPCT`, `CNTFRQ` replaced by trampolines) | `common/settings.h` (`cpu_backend`), `core/arm/nce/patcher.cpp` | seen |
| Dynarmic only runs 32-bit games or when NCE is turned off; its "Auto" accuracy already uses unsafe FMA unfusing and 64-bit fastmem checks | `core/arm/dynarmic/arm_dynarmic_64.cpp` (`MakeJit`) | seen |
| Each `CPUCore_N` thread is pinned to host core N (0 to 3). The comment says it targets the Snapdragon 8 Elite | `core/cpu_manager.cpp` (`CpuManager::RunThread`) → `common/thread.cpp` (`PinCurrentThreadToPerformanceCore`) | seen |
| On the owner's 13T, cores 0 to 3 are the Cortex-A55 small cores (2.0 GHz); cpu4-6 are A78 at 3.0 GHz, cpu7 an A78 at 3.1 GHz. So the four `CPUCore_N` threads are pinned to the four small cores | CPU-Z on the phone (13 October): topology 3x A78 3.0 + 4x A55 2.0 + 1x A78 3.1, cpu0-3 idling at 450 MHz | seen (topology); which core each thread runs on is still to measure |
| Thread priority "Critical" is requested through `SCHED_OTHER`, whose priority range is 0 to 0 on Linux: no effect. The level formula `max(prio + 1, 4)` also always gives the top level | `common/thread.cpp` (`SetCurrentThreadPriority`) | seen |
| No Android performance hints (ADPF `APerformanceHint`) | grep: none | seen |
| A data abort NCE cannot handle is skipped (`pc += 4`) so the game continues: hides errors rather than fixing them | `core/arm/nce/arm_nce.cpp` (`HandleFailedGuestFault`) | seen |
| Each SVC and each interrupt (`tkill` of `BreakFromRunCodeSignal`) is a signal or trampoline round trip; their rate in Sword/Shield | `arm_nce.cpp` (`SignalInterrupt`) | unknown |

### 2.3 Memory and GPU-memory tracking

| Finding | Where | Mark |
|---|---|---|
| Pages the GPU caches are write-protected in the fastmem arena; reactive flushing is off on Android, so cached pages stay readable but not writable | `core/memory.cpp` (`RasterizerMarkRegionCached`), `settings.h` (`use_reactive_flushing`) | seen |
| The guest's first write to such a page faults (SIGSEGV) into `HandleGuestAccessFault` → `Memory::InvalidateNCE` → `InvalidateGPUMemory`: one signal round trip (several microseconds) per page per cache cycle | `arm_nce.cpp`, `core/memory.cpp` | seen (path), unknown (rate) |
| Writes from the system core take a lock (`sys_core_guard`) in `HandleRasterizerWrite` | `core/memory.cpp` | seen |
| The separate heap is mapped lazily on Android: first touches fault too | `core/memory.cpp` (`InvalidateNCE`, `DeferredMapSeparateHeap`) | seen |
| Eden protects memory in 4 KiB pages; a phone whose kernel uses 16 KiB pages would protect 4 times more than asked (the module builds with flexible page sizes) | `YUZU_PAGESIZE`, `melonDS-android/eden/build.gradle.kts` | inferred; the 13T's kernel uses 4 KiB pages (`6.12.38-android16-...-4k`, CPU-Z), so not a concern there |

### 2.4 GPU emulation and synchronisation

| Finding | Where | Mark |
|---|---|---|
| **Asynchronous GPU emulation is off by default on Android** (on elsewhere): GPU command processing then runs on the guest core thread that submits, so CPU emulation and GPU translation take turns instead of overlapping. Eden's Android text calls the option a hack "at the cost of graphical issues and increased crash rates" | `common/settings.h` (`use_asynchronous_gpu_emulation`), `core/core.cpp`, `video_core/gpu_thread.cpp`; string `renderer_asynchronous_gpu_emulation_description` | seen |
| GPU accuracy Low on Android (Medium elsewhere) | `settings.h` (`gpu_accuracy`) | seen |
| Full CPU-waits-for-GPU (`Scheduler::Finish`) in hot paths: texture download **once per image inside a loop**; buffer download; the texture garbage collector downloading an image; growing the quad index buffer | `video_core/texture_cache/texture_cache.h` (`DownloadMemory`, the `Cleanup` lambda of the garbage collector, `PopAsyncFlushes`), `video_core/buffer_cache/buffer_cache.h` (`DownloadBufferMemory`), `renderer_vulkan/vk_buffer_cache.cpp` (`QuadIndexBuffer::UpdateBuffer`) | seen |
| ASTC decode on the GPU ends with `Finish` for each texture; not used on Mali (native ASTC), used on GPUs without ASTC | `renderer_vulkan/vk_compute_pass.cpp` (`ASTCDecoderPass::Assemble`) | seen |
| Without async presentation (off by default), presenting waits for the worker thread (`WaitWorker`) and copies to the swapchain on the calling thread | `renderer_vulkan/vk_present_manager.cpp` (`PresentManager::Present`) | seen |
| Timeline semaphores waited by a dedicated thread; core timing waits on a condition variable (no spinning on Android) | `vk_master_semaphore.cpp`, `core/core_timing.cpp` (`ThreadLoop`) | seen |

### 2.5 Shaders and pipelines

| Finding | Where | Mark |
|---|---|---|
| Pipelines built synchronously (async shaders off); disk cache and the driver's pipeline cache on | `settings.h`, `vk_pipeline_cache.cpp` | seen |
| 4 to 8 compile threads (`pipeline_worker_count`, clamped); `has_broken_parallel_compiling` is never set | `vk_pipeline_cache.cpp` (`GetTotalPipelineWorkers`), `vulkan_device.h` | seen |
| Extended dynamic state disabled on Android: each state combination is its own pipeline, so more compiles and a larger cache | `settings.h` (`dyna_state`) | seen |
| Shaders that need more compute shared memory than Mali offers are clamped (a correctness risk if a shader really uses it) | `vk_pipeline_cache.cpp` (`CreateComputePipeline`) | seen |
| Share of frame time lost to compiles once the cache is warm | | unknown |

### 2.6 Presentation and frame pacing

| Finding | Where | Mark |
|---|---|---|
| On Android 11+ (API 30), the "Frame pacing mode" setting is ignored: the swapchain always waits for the image's last use only. The 13T runs Android 13+, so the setting does nothing there | `renderer_vulkan/vk_swapchain.cpp` (`Swapchain::AcquireNextImage`) | seen |
| The display's frame rate is requested through `ANativeWindow_setFrameRate` | `android/app/src/main/jni/emu_window/emu_window.cpp` | seen |

### 2.7 GPU memory budget

| Finding | Where | Mark |
|---|---|---|
| On integrated GPUs (all phones), the device memory Eden plans for is `max(min(available − 8 GiB, 4 GiB), min(local heap, 4 GiB))`: **4 GiB on any phone whose heap reports 4 GiB or more**, 6 GiB with "Aggressive" | `vulkan_common/vulkan_device.cpp` (memory heaps, `device_access_memory`) | seen |
| From it the texture cache derives about 1.6 GiB before collecting and about 3.2 GiB before aggressive collection | `texture_cache/texture_cache.h` (constructor), `texture_cache_base.h` | seen (formula), inferred (values) |
| On a 6 GiB phone, that budget plus the guest's memory can exceed the RAM: Android's low-memory killer, or heavy reclaim. The owner's 13T has 8 GB (7347 MB total, 1.7 GB free at idle, CPU-Z): 4 GiB of GPU budget plus Sword/Shield's guest memory is already tight there | CPU-Z on the phone | inferred (seen: RAM) |

### 2.8 Drivers

| Finding | Where | Mark |
|---|---|---|
| Mali paths: shared memory clamp, dynamic storage buffer limit, occlusion query sync and host conditional rendering skipped (the last two are correctness risks) | `vk_pipeline_cache.cpp`, `vk_buffer_cache.cpp`, `vk_query_cache.cpp` | seen |
| No Mali driver-version checks; every feature reported is trusted | `vulkan_device.cpp` | seen |
| The comment naming Mali for the depth/stencil blit fallback sits inside the `is_nvidia` block; the fallback itself is generic | `vulkan_device.cpp` | seen |
| Custom drivers (`adrenotools`) are Adreno-only; a Mali phone uses the HyperOS driver | `android/app/src/main/jni/native.cpp` | seen |
| Driver version, extensions, whether the driver pipeline cache survives a restart | Eden's log at game start (`Render_Vulkan`) | unknown |

---

## 3. Bottleneck hypotheses, ranked

| Rank | Hypothesis | Why | What confirms it |
|---|---|---|---|
| H1 | The emulated CPUs run on the small cores | 2.2: pinning to cores 0-3 | `CPUCore_*` threads on cpu0-3 at full load in a trace |
| H2 | CPU emulation and GPU translation are serialised | 2.4: async GPU off | Guest core threads spending a large share of time in `video_core` (simpleperf); FPS gain with async on, same scenes |
| H3 | CPU-to-GPU waits stall frames | 2.4: `Finish` in download and collection paths | Time in `MasterSemaphore::Wait` from guest or GPU threads; count of `Finish` per frame |
| H4 | Pipeline compiles cause the stutters | 2.5: sync compiles, no dynamic state | Stutters matching new pipelines in `gpu_logging_enabled`; gone with a warm cache |
| H5 | Memory-tracking faults cost CPU time | 2.3 | Count of `HandleGuestAccessFault` per second (temporary counter, removed after) |
| H6 | Thermal throttling after a few minutes | Phone, sustained load | Clocks and FPS falling together over 10 minutes |
| H7 | Mali bandwidth or fill rate | Phone GPU | GPU at 100 % while the CPU threads wait |

H1 and H2 can act together: with async off and the guest cores on small cores, GPU translation also runs on the
small cores.

---

## 4. Measurement protocol

1. **Conditions:** same build, same save, battery above 50 %, no case, room at about 25 °C, fixed brightness,
   flight mode. Shader cache warmed by one pass; one extra run with the cache cleared for compile stalls.
2. **Scenes (10 minutes each, 3 runs):** Wedgehurst by day, Motostoke, Wild Area with weather, a gym battle, a Max
   Raid.
3. **Metrics and tools:**

| Metric | Tool | Access |
|---|---|---|
| FPS, frame time, 1 % low, frames over 33 ms | Eden's performance overlay (`show_performance_overlay`) | none |
| Pipeline builds and their time | `gpu_logging_enabled`, Eden's log | none |
| Driver and features | Eden's log at start | none |
| Per-thread CPU time, core placement, frequencies | Perfetto (`sched`, `cpufreq`), `ps -T` | ADB |
| Where CPU time goes in the guest threads | simpleperf on `CPUCore_*` | ADB |
| GPU load and frequency | Perfetto (Mali counters if exposed) | ADB |
| Memory (PSS), low-memory kills | `dumpsys meminfo`, logcat | ADB |
| Temperature, throttling | Perfetto thermal, `dumpsys thermalservice` | ADB |

**No ADB (owner's decision, 14 October: debug access strictly forbidden, CLAUDE.md §3).** The rows marked ADB are
out. What they would tell is read from inside the app instead, in a diagnostic build whose output goes to Eden's
log (which the owner shares): each Eden thread's CPU time and last core from `/proc/self/task/*/stat` (the app's
own threads, no permission), the cores' capacities from `/sys/devices/system/cpu/cpu*/cpu_capacity`, and the
counters of item 4. Removed once read (CLAUDE.md §2.4).

4. **Temporary counters** (H3, H5): a counter of `Finish` calls and of access faults per second, printed in the log,
   in a diagnostic build only, deleted once read (CLAUDE.md §2.4).

### 4.1 Baseline (measured, 14 October)

Build `claude/admiring-goodall-xadvmb` (eden_log.txt, two runs), Pokemon Sword, the owner's Xiaomi 13T:

| | Measured |
|---|---|
| Phone | Xiaomi 2306EPN60G (13T), MediaTek MT6896 (Dimensity 8200), 4x Cortex-A78 + 4x Cortex-A55, 8 GB, Android API 36 |
| GPU, driver | Mali-G610 MC6, Mali driver 54.1.0 (r54p1), Vulkan 1.3.305 |
| Missing on this GPU (Eden works around them) | `VK_EXT_vertex_attribute_divisor`; features `fillModeNonSolid`, `multiViewport`, `shaderClipDistance`, `shaderCullDistance`, `vertexPipelineStoresAndAtomics`; `maxViewports` 1, `maxClipDistances` 0 |
| Settings | CPU backend NCE, CPU accuracy Auto, multicore on; Vulkan, resolution 1x, vsync Fifo, GPU accuracy Low, asynchronous GPU emulation **off**, asynchronous shaders off, async presentation off, ASTC decoded on the GPU, disk and driver pipeline caches on, handheld |
| Pipelines, first run (cache empty) | 1083 graphics pipelines built over 290 s of play, in bursts (up to 189 in 10 s) |
| Pipelines, second run | the disk cache gives 1085 at start; rebuilt in the first 20 s |
| Cutscene | about 28 to 32 FPS (overlay) |
| Postwick, outside the starting house, standing still | about 16 FPS (overlay) |
| Noise in the log | 73 `Service.LDN` "Couldn't bind callback!" errors; 12 `getSetting` with an empty name; `eden_gpu.log` holds its header only (no call recorded) |

Core frequencies across three moments (menu, cutscene, standing still in Postwick), read by the owner with a
monitoring app (14 October; ranges and averages over the three together; on the
Dimensity 8200, cpu0-3 are the Cortex-A55, cpu4-6 Cortex-A78, cpu7 the fastest A78):

| Cores | Min | Max | Average | Owner's note |
|---|---|---|---|---|
| cpu0-3 (A55) | 1050 MHz | 1850 MHz | 1400 MHz | the steadiest |
| cpu4-6 (A78) | 700 MHz | 3200 MHz | 1800 MHz | the busiest |
| cpu7 (A78, fastest) | 700 MHz | 3200 MHz | 1200 MHz | steady, follows cpu4-6 under heavy load |
| GPU load | 0 % | 0 % | 0 % | never moved |

Inferred, not measured: a frequency is the governor's choice, not a load. The big cores averaging well under their
maximum, and the fastest one the least, suggest threads that wait (on each other or on the GPU) more than threads
that compute without pause, so H2 and H3 come before raw CPU power; H1 is neither confirmed nor ruled out (the A55 are
busy but not at their top clock). The GPU's 0 % is most likely a reading the app is not allowed to make (Android
keeps the Mali load file from apps), not an idle GPU: unknown. The diagnostic build's per-thread times decide. The owner reads the
unused clock as room to double the speed: it is room only once the wait that keeps the cores from it is removed,
so the gain is unknown until that wait is found.

Second set of logs (build `main-7446e64c`, with the diagnostic, 14 October), measured: at every start Eden's own
game list opened the Switch firmware's content files one by one (`<32 hex>.nca`, `<32 hex>.cnmt.nca`, some two or three
times), each rejected as "a different type (unknown) than its extension", for 29 s and 36 s in the two runs. The
firmware lies in a game folder. That is CPU and storage work at the start of a session, not the 16 FPS floor: fixed
in Eden's scan (GameHelper skips such files) and in Punica's game list. The uploads were cut at 28 672 bytes (both
logs, mid-line; the GPU log likewise), before the game started: no `[Pomegrade diag]` line arrived. The full logs
are needed, zipped or through the Drive folder.

16 FPS standing still in the first town is the floor to raise: the game targets 30. The disk cache works, so H4
explains first-run stutters, not this floor.

---

## 5. Optimisation catalogue

Gains are estimates; difficulty and risk are judgements from the code.

### 5.1 Root-cause fixes, small

| # | Change | Where | Expected gain | Difficulty | Risk |
|---|---|---|---|---|---|
| O1 | Pin the emulated CPUs to the largest cores, ranked by `cpu_capacity`; with fewer large cores than emulated ones, pin only the busiest and leave the rest to the scheduler | `common/thread.cpp` (`PinCurrentThreadToPerformanceCore`), `core/cpu_manager.cpp` (`RunThread`) | Large if H1 holds (an A78 does roughly 2 to 3 times an A55's work per clock) | Low | Low; the 8 Elite keeps its placement |
| O2 | A thread priority that works: `setpriority` nice values for the guest cores and the GPU thread; fix the level formula | `common/thread.cpp` (`SetCurrentThreadPriority`) | Small to medium | Low | Low |
| O3 | ADPF hint sessions for the guest cores and the GPU thread, target 16.6 ms (or 33.3 ms) | Beside `core/cpu_manager.cpp`, `video_core/gpu_thread.cpp` | Steadier frames, later throttling | Medium | Low (Android 12+, ignored before) |
| O4 | Batch texture downloads: record every image's copy, then one `Finish`, then swizzle them all (as `PopAsyncFlushes` already does) | `texture_cache/texture_cache.h` (`DownloadMemory`) | Fewer stalls where the CPU reads GPU-written textures | Low | Low; same order of copies |
| O5 | Make the frame pacing setting work on Android 11+ again, or hide it there | `renderer_vulkan/vk_swapchain.cpp` (`AcquireNextImage`) | Lets the user cap at 30 for steadier frames and heat | Low | Low |
| O6 | Release APK; `-march=armv8.2-a` as an extra build for current phones | `android-apk.yml`; `YUZU_BUILD_PRESET` | Small | Low | Low; armv8.0 phones keep the generic build |

### 5.2 Root-cause work, larger

| # | Change | Where | Expected gain | Difficulty | Risk |
|---|---|---|---|---|---|
| O7 | Make async GPU emulation correct on Android, then on by default: find which syncs break (fences, flushes, `WaitForSyncOperation`) on Mali and fix them rather than turning it on blind | `video_core/gpu_thread.cpp`, `video_core/gpu.cpp`, `renderer_vulkan/vk_rasterizer.cpp` | Large if H2 holds: GPU translation leaves the guest cores | High | High: Eden reports glitches and crashes; each must be traced to a missing sync |
| O8 | Extended dynamic state (level 1, then 2) for Mali drivers that report it correctly, with a driver-version list built from logs | `settings.h` (`dyna_state`), `vulkan_device.cpp` | Fewer pipelines and stutters | Medium | Medium: driver bugs |
| O9 | GPU memory budget from the phone's RAM (e.g. a share of total RAM minus the guest's memory layout) instead of a fixed 4 GiB | `vulkan_device.cpp` (`device_access_memory`) | No kills on 6 GiB phones; fewer collections on 12 GiB phones | Low | Medium: too low a budget means more collection stalls (O4 then matters more) |
| O10 | Fewer memory-tracking faults: track writes at a coarser granularity for regions the guest rewrites every frame (vertex streams, uniform buffers), or upload them without protection | `core/memory.cpp`, `buffer_cache/buffer_cache.h` | Depends on H5's count | High | High: missed writes show as stale geometry |
| O11 | Async presentation on by default if the measurements show the present copy on the critical path | `settings.h` (`async_presentation`) | Small to medium | Low | Low to medium (one frame more latency) |

### 5.4 Raising the ceiling: deep projects

Each removes work the emulator does today rather than doing it faster. Ordered by expected ceiling gain against
cost; all are hypotheses until the measurements of section 4 say which stage dominates.

| # | Project | What it removes | Where it starts | Mark | Cost |
|---|---|---|---|---|---|
| D1 | **Ahead-of-time shader translation from the game's binaries.** NVN shaders are Maxwell machine code stored in the game's files; scan them, translate each to SPIR-V offline with Eden's own recompiler, and ship the result as a pre-filled cache, so no shader is first seen in play | Translation stalls (H4) at the first meeting of each shader | `video_core/shader_environment.h` (`FileEnvironment` already replays a shader from a file with its texture and constant buffer reads), `shader_recompiler/` | inferred: the per-shader environment (texture types, constant buffer values read at translation) is only partly known offline, so some shaders still need the runtime pass | High |
| D2 | **Pipeline pre-building from recorded state.** Record the pipeline keys a full play-through meets (the disk cache already stores them), and build them all at load on the compile threads | Driver compile stalls (H4), including the ones extended dynamic state cannot remove | `vk_pipeline_cache.cpp` (`LoadDiskResources`) | seen: the cache is replayed at load; inferred: a shared, complete key set covers more than one player's cache | Medium |
| D3 | **Async GPU made correct** (O7): the guest cores stop doing GPU translation | Serialisation of CPU and GPU work (H2) | `video_core/gpu_thread.cpp`, `gpu.cpp`, the syncs in `vk_rasterizer.cpp` | seen: off on Android | High |
| D4 | **Fewer NCE exits**: SVCs whose result does not need the kernel (e.g. `GetSystemTick`, some `GetInfo`) answered in the trampoline; memory-tracking faults replaced by coarser tracking for streamed buffers (O10) | Signal and context round trips (H5) | `core/arm/nce/patcher.cpp` (`WriteSvcTrampoline`), `core/memory.cpp` | seen: every SVC goes through the trampoline to the kernel | High |
| D5 | **GPU unswizzle and decode on the GPU by default**, with its correctness checked: the code exists (`gpu_unswizzle_enabled`, `BlockLinearUnswizzle3DPass`) but is off | CPU time spent unswizzling textures | `renderer_vulkan/vk_texture_cache.cpp` | seen: off by default | Medium |
| D6 | **Command processor fast paths**: profile the Maxwell 3D method decoding (`video_core/engines/maxwell_3d.cpp`, `dma_pusher.cpp`) on the guest's real command streams; batch the methods games send in bulk | Per-method interpretation cost | `video_core/engines/`, `video_core/dma_pusher.cpp` | unknown: needs simpleperf (section 4) | Medium to high |
| D7 | **High-level emulation of NVN** (research): each game links Nintendo's NVN driver into its own binary; recognising its functions and translating NVN calls straight to Vulkan would skip building and decoding Maxwell command streams | The guest's driver code and the whole command processor | Signature scanning of the game's main NSO | inferred: NVN versions differ between games and the binary has no symbols; very large project, high correctness risk | Very high |

**The firmware's place.** The firmware (installed by `SwitchSystemFiles.kt` from `Pomegrade/BIOS/Switch/Firmware`)
holds the system modules (sysmodules) that Eden replaces with its own C++ (HLE) and the system archives it reads
(fonts, applets). Running those modules from the firmware (LLE) would be slower, not faster. Its use here is as a
**reference**: checking that a faster HLE path (D4) returns what the real module returns, and reading the system
NVN and nvservices binaries to understand the GPU syscalls D3 and D7 depend on. The owner's firmware and keys are
never committed or uploaded (same rule as the game dumps, CLAUDE.md §8).

### 5.3 Not proposed

- **Asynchronous shaders**: they hide stutters by drawing frames with missing effects.
- **Lower resolution, lower accuracy, "unsafe" CPU options**: user choices, not fixes.
- **Skipping more GPU syncs on Mali**: the query skips in 2.8 are already a correctness risk.

---

## 6. Low-end phones ("potato")

| Limit | What Eden does | Proposed |
|---|---|---|
| 2 large cores + 6 small | Pins 4 guest threads to cores 0-3 (small) | O1 (busiest guest core on a large core), O3 |
| 4 to 6 GiB RAM | GPU budget 4 GiB whatever the RAM; guest layout 4 GiB minimum | O9; measure PSS and kills first |
| Weak GPU | Resolution down to 1/4x | User's choice; not the fix |
| No native ASTC (rare on phones) | GPU decode with one `Finish` per texture | Batch the decodes as O4 does for downloads |
| Old drivers | Version checks for Adreno and NVIDIA only | Mali and PowerVR checks built from tested phones' logs |
| Slow storage | Disk shader cache | Measure first-launch stalls; check the driver cache is reused |
| armv8.0 CPUs | Generic build | Keep the generic build (O6 adds a second, never replaces) |
| 32-bit only phones | Not supported (arm64 only) | Out of scope |

---

## 7. A/B tests

- One change per build, against the same baseline build; runs alternate A, B, A, B on the same scenes, the phone
  cooled between runs.
- A gain counts when it exceeds the run-to-run spread and holds in at least 4 of the 5 scenes.
- Correctness on every build: the same frames compared with the baseline, audio checked, one full battle played,
  10 minutes without a crash. O7 and O10 also get a longer session (an hour) across scene changes.
- O1 re-tested on a Snapdragon phone if one is available, and O1, O3, O9 on one low-end phone.

---

## 8. Not inspected yet

- HLE service costs (vi/nvnflinger composition, audio renderer, hid) on the guest cores.
- The shader recompiler's passes and SPIR-V output quality on Mali (instruction count, float16 use).
- The command processor's per-draw cost (Maxwell 3D state tracking, descriptor updates).
- Audio output (sink, buffer size) and its effect on frame pacing.
- Whether LTO is active in Pomegrade's build.

---

## 9. Next steps

1. Done (14 October): the logs, the first baseline (4.1); ADB refused for good.
2. The diagnostic's lines: the logs sent on 14 October were cut by the upload before the game started (4.1); to
   send again whole (zipped, or in the Drive folder). Diagnostic build (section 4, no ADB): per-thread CPU time and core, core capacities, `Finish` and fault counts
   per second, in Eden's log, standing still in Postwick; tells H1, H2, H3, H5 apart.
3. A/B of async GPU on and off with the current build (a setting, no code change): measures H2 before any code.
4. O1 and its A/B; results written here, marked as measured.
5. Then by measured impact, deep projects first: D2 and D1 if compiles dominate, D3 if H2 is confirmed, D4 if
   exits dominate, D6 if the command processor does; O2, O3, O4, O8, O9 alongside.
