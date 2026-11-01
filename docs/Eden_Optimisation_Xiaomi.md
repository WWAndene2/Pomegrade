# Eden optimisation study: Xiaomi 13T first, then low-end phones

Study of how to make the Switch core (Eden v0.2.1 until 31 October, then Eden's master of 26 October: section 9, `eden/`) faster on the owner's Xiaomi 13T (MediaTek Dimensity
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
| On the owner's 13T, cores 0 to 3 are the Cortex-A55 small cores (2.0 GHz); cpu4-6 are A78 at 3.0 GHz, cpu7 an A78 at 3.1 GHz. **But the pinning never took effect on Android**: `sched_setaffinity` takes a thread id and was given `pthread_self()`, an address on Android, so the call failed and its error was ignored; the guest cores ran wherever the scheduler put them, small cores included | CPU-Z on the phone (13 October): topology; the 13T's kernel source (Xiaomi `bsp-aristotle-s-oss`, `mt6895.dts`: cpu0-3 `cortex-a55` capacity-dmips-mhz 380, cpu4-7 `cortex-a78` 1024, cpu7 alone in its cluster); bionic's `pthread_self` | seen (topology, the failing call from the code); the threads' real placement is still to measure |
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
| Custom drivers (`adrenotools`) were Adreno-only (only Qualcomm's `/dev/kgsl-3d0` was checked; the loading itself does not depend on the GPU). Since 15 October, Mali's `/dev/mali0` also allows them, for Mesa PanVK built for `mali_kbase` (e.g. `zenithblue-oss/panvk-kbase-android`, G610 tested on one phone by its authors) | `android/app/src/main/jni/native.cpp` (`SupportsCustomDriver`) | seen; PanVK on the 13T with Eden: the game does not start (owner, 26 October), dropped |
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
counters of item 4. Since 15 October it also logs the frame rate and slowest frame, Android's thermal status and headroom, the battery's temperature (when readable) and memory (the app's, the system's available). Removed once read (CLAUDE.md §2.4).

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

Owner's restatement of the same readings (15 October): cpu0-3 1200 to 1800 MHz, about 1400 steady; cpu4-6 700 to
2300 MHz, about 1800 on average; cpu7 between the two groups. The maxima seen differ from the table above (2300
against 3200 for cpu4-6): both are the owner's app readings, the later one is kept as the reference for an
app-to-app comparison.
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

Third log (build `main-7dc7d185`, the diagnostic, 14 October, whole: 361 samples over 290 s of Pokemon Sword), measured.
The owner had changed settings since 4.1: asynchronous GPU emulation **on**, async presentation on, custom CPU ticks
21000, fast CPU time Boost; GPU accuracy Low, resolution 1x, ASTC uncompressed as before. The firmware scan is gone.
Means per 30 s window (the sampler's "per s" counters cover its 0.8 s interval):

| Window | Busiest threads (share of a core) | GPU waits per s (time) | NCE faults per s | Big cores' clock |
|---|---|---|---|---|
| 0-60 s | CPUCore_0 27-29 %, GPU 10-14 % | 1 (12-14 ms) | 160-370 | 2.3 then 1.1 GHz |
| 60-180 s | CPUCore_0 34-38 %, GPU 27-31 %, CPUCore_1 and _2 25-30 %, VulkanWorker 14 % | 6-12 (60-150 ms) | 680-800 | 1.3-1.5 GHz |
| 180-270 s | GPU 30-40 %, CPUCore_0 33 %, CPUCore_1 and _2 20-24 % | 2-3 (22-32 ms) | 990-1560 | 1.2-1.4 GHz |
| 270-300 s | GPU 46-60 %, CPUCore_0 27 %, VulkanWorker 18-23 % | 35-41 (1.1-1.3 s) | 2470-3190 | 1.8-2.0 GHz |

Read from it: no thread is saturated in any window (the busiest under 60 %), and the big cores stay near half their
3 GHz: the CPU is not the limit (H1 ruled out for this run). At 270-300 s the threads spend over a second per second
waiting on the GPU's fences (several at once): the Mali is the limit there (GPU-bound). NCE faults follow the GPU load
and cost little alone (some thousand a second). The owner (14 October): up to 270 s a cutscene; 270-300 s the start of
play, a light scene (no large area). So the light start of play is where the threads wait most: 35-41 waits a second,
about one a frame, some 30 ms each. Either the Mali is slow on those frames (GPU-bound: a lower resolution helps), or
the game reads GPU results back each frame and the waits serialise CPU and GPU (a lower resolution does not help;
fixable in Eden). Pomegrade's two picture passes (rich colours, deep black) are two full-screen passes at the end:
small, not a second a second, but in the A/B too. Next: an A/B at the heavy spot, resolution 1x against
0.5x (a setting): a large FPS gain confirms GPU-bound and points to GPU work (render passes, texture formats,
bandwidth) rather than the CPU.

A/B at the start of play (owner, 14 October; fourth log, `main-7dc7d185`): resolution 0.5x with the picture passes
off is about 5 FPS smoother than 1x. A quarter of the pixels for 5 FPS: the Mali's pixel work is not what limits.
Measured in that log at 0.5x: the waits on the GPU are unchanged (41-46 a second, 1.1-1.4 s a second, as at 1x),
while NCE faults climb from about 700 to 2 900 a second at the start of play. Reactive flushing is already off,
ASTC is native on the Mali. So something waits on the GPU about once a frame whatever the resolution. The diagnostic
now splits the waits by the waiting thread (GPU, present, worker, guest CPU, other) to tell which part stops:
the presenter waiting for a frame is expected; the GPU thread points to staging or resource pools running dry; a
guest CPU thread to the game reading GPU results back.

Fifth log (`main-32560c62`, waits by thread, 14 October, 919 samples, still at 0.5x): every counted wait is put on
"GPU", 26-43 a second, 0.65-1.4 s a second at the start of play. Corrected, not a finding: those waits come from
InnerFence::Wait, which the fencing thread ("GPUFencingThread", 15 characters "GPUFencingThrea") calls by design
to release fences in the background; the classifier took every name starting with "GPU" for the GPU thread. The
fencing thread waiting is expected. What would stall the GPU thread is ReleaseFences (a guest semaphore or syncpoint
wait), where it blocks on a condition variable until the fencing thread has released every pending fence; that was
not counted. The diagnostic now puts the fencing thread apart and times the GPU thread in ReleaseFences.

16 FPS standing still in the first town is the floor to raise: the game targets 30.

After O1, O2 and O3 (PR #106: large cores, nice values, ADPF hints for the guest cores and the GPU, Vulkan worker and
fencing threads), owner's reading (15 October, overlay, resolution 1x): about 21 FPS on average in the field, 29 FPS
in battle, against 16 FPS standing still in Postwick before. Measured by the owner, not by a log: which of the three
changes gave what, the clocks and the heat are not known yet (the diagnostic's log decides).

Sixth log (after PR #106, owner, 15 October, short: 297 samples, Pokemon Sword at 1x, asynchronous GPU emulation
on), measured:

| | Field (173 samples) | Battles and light scenes (124 samples) |
|---|---|---|
| FPS | 19-25, mostly 21-22 | 28-31 |
| GPU thread waiting on the GPU | about 22 times a second, 0.75 s a second (some 34 ms a frame) | 0.1 s a second |
| Large cores' clock (mean of cpu4-7, the sampler's reading) | 2.1 GHz (before, same sampler, third log: 1.2-1.5; the owner's app before: about 1.8 on average, 2.3 at most) | 1.6 GHz |
| Busiest threads | GPU 20-22 %, CPUCore_0 15-20 % | CPUCore_0 29-37 %, GPU 21-31 % |

Read from it: the guest cores now run on cpu4-7 (O1 works) and the large cores' clocks rose (O3 works). No thread is
saturated, and in the field the GPU thread spends three quarters of each second waiting for the Mali to finish
the previous frame: **the field is now GPU-bound**, the CPU side no longer limits there. Thermal status stayed 0, headroom
0.20-0.30 (a short run: sustained heat untested). Memory: the app at 1.8-3.5 GiB, 0.5-1.5 GiB left to the system.
Next: an A/B at 0.5x in the field (before O1-O3 it gave about 5 FPS, when the CPU side still limited); a large gain
now confirms the Mali's pixel work, a small one points to its per-frame work (passes, formats, synchronisation).

Seventh log (owner, 15 October, the same build at 0.5x, 270 samples), measured: 29.3 FPS on average where the GPU
thread barely waits (237 samples, 0.1 s a second), 28.3 where it still waits (33 samples, 0.6 s a second); the game's
30 is held nearly throughout. At 1x the field gave 21-22 FPS with 0.75 s a second of waiting. So at 1x the field is
bound by the Mali's pixel work (a quarter of the pixels removes the wait), which before O1-O3 was hidden behind the CPU
side (the 0.5x A/B then gave only 5 FPS). Thermal status 0, headroom 0.28-0.36. Next: the resolution between the two
(0.75x) with a sharpening upscaler, and in Eden the per-pixel cost on Mali (render pass loads and stores, attachment
formats, the picture passes).

Eighth log (owner, 15 October, PR #107: the per-pixel counters and the compressed render target option; three
sessions in one process), measured:

| Session | Samples | FPS mean (median) | GPU thread waiting | Render passes per frame | Screens of pixels per frame (1280x720 = 1) | Full clears first in their pass, per frame |
|---|---|---|---|---|---|---|
| 0.5x, option off | 13 | 28.5 (29.9) | 0.01 s/s | 67 | 9.3 | 18.7 |
| 1x, option off | 199 | 24.7 (22.4) | 0.49 s/s | 142 (169 in the waiting samples) | 36.1 | 16.1 |
| 1x, option on | 233 | 25.4 (24.8) | 0.45 s/s | 139 (168) | 36.2 | 16.2 |

Read from it: (1) the option works and is safe in Pokemon Sword: the 444 renderable images of its session were all made
without STORAGE usage, and the game never bound a renderable image as storage (0 storage views on one in every
session), so nothing was dropped; its gain is small, 22.4 against 21.7 FPS in the samples where the GPU thread waits
(about 3 %, near the noise of two short runs). Either AFBC was not what held the Mali back, or the Mali's driver did not
compress these images for another reason (format, MUTABLE_FORMAT): unknown. (2) The size of the work: 140 to 170 render
passes per frame, covering 36 screens of pixels, each loaded from memory and stored back; about 16 of those passes
per frame begin with a clear of their whole area, whose load is wasted (it could be LOAD_OP_CLEAR); no full clear comes
later in a pass. (3) The owner had Eden's GPU logging on with every Vulkan call logged (`gpu_logging_enabled`,
`gpu_log_vulkan_calls`): 1.25 million calls written, a 267 MB log, CPU and storage work on every render pass begin.
Next: that logging off for every measurement; then the CLEAR load for the passes that begin with a full clear (16 of
the 140 per frame), then why a frame splits into so many passes.

Ninth log (owner, 15 October, PR #108, 1x, compressed render targets on in both sessions; Eden's GPU logging off in
both: the 567 MB GPU log sent with it is the eighth log's, from its third session, the app not having rewritten it),
measured:

| Session | Samples | FPS mean (median) | FPS where the GPU thread waits | Render passes per frame | Screens per frame | Clears made a pass's load, per frame |
|---|---|---|---|---|---|---|
| Clear on load off | 113 | 25.8 (24.9) | 23.3 | 134 | 33.7 | 0 |
| Clear on load on | 126 | 26.2 (24.9) | 24.3 | 139 | 34.7 | 23.1 |

Read from it: clear on load works (23 attachment clears a frame become their pass's load, color and depth counted
apart, against 16 full clears first in their pass); no error, the owner to say whether anything looks wrong. Its gain
is small, about 4 % where the GPU limits (23.3 to 24.3), as was compression's (21.7 to 22.4, eighth log). The two logs
are not compared with each other: the eighth ran with Eden's GPU logging on, the ninth without. No visible defect with
either option (owner); clear on load is made the default, since it changes no pixel; compression stays an opt-in, a
game that writes a render target from a shader losing that write. The loads they remove are not the bulk of the
Mali's work; the 135-140 passes a frame are the next thing to understand.

Tenth log (owner, 15 October, PR #109: what ends each render pass; 1x, clear on load and compression on, 212
samples, 25.3 FPS mean), measured, per frame: the guest's "wait for idle" (`RasterizerVulkan::WaitForIdle`, from the
Maxwell `wait_for_idle` and `invalidate_texture_data_cache` methods and the puller's) ends **104** passes; the guest's
fragment barrier (`FragmentBarrier`, a render target read back while drawn) 23; a draw needing other attachments 11;
buffer uploads 4.5, texture copies 1.6, queries 1, the rest under 0.2. So three quarters of the passes are cut by a wait
that the Switch's GPU makes for almost nothing, and that Eden makes by ending the pass: on the Mali each end stores the
attachments and the next pass loads them back. Change: an option (`pomegrade_defer_wait_for_idle`, off by default)
that, inside a render pass, makes the wait at the pass's end (a full memory barrier after it) instead of ending it.
Its risk: draws of the same pass after the wait are not ordered after those before, which matters only where a draw
reads what an earlier one wrote through a storage buffer or image.

Eleventh log (owner, 15 October, PR #110, defer wait for idle on): no gain, 22.6 FPS where the GPU limits and still
139 passes a frame. The waits were deferred (122 a frame) but each was followed by a buffer copy that also ends the
pass (`BufferCacheRuntime::CopyBuffer`, 83 a frame): the guest's pattern is wait, write data, draw.

Twelfth log (owner, 15 October, PR #111, the test cycle, 1x, compression, clear on load, defer on; 45 s a phase,
playing in the field), measured:

| Phase | Samples | FPS mean (median) | GPU thread waiting | Passes per frame | Ended by buffer copies | Ended by fragment barriers |
|---|---|---|---|---|---|---|
| 1 reference | 47 | 28.3 (29.7) | 0.10 s/s | 79 | 11 | 30 |
| 2 + every upload at frame start | 56 | 25.5 (25.5) | 0.78 s/s | 163 | 108 | 25 |
| 3 + fragment barriers ignored | 56 | 28.4 (28.6) | 0.24 s/s | 117 | 63 | 0 |
| 4 defer wait for idle off | 55 | 27.1 (28.4) | 0.43 s/s | 135 | 7 (waits end 91) | 23 |
| 5 clear on load off | 56 | 22.8 (23.0) | 0.75 s/s | 167 | 112 | 25 |
| after (settings restored) | 230 | 25.5 (23.6) | 0.47 s/s | 140 | 87 | 23 |

Read from it, with a caveat that dominates: the phases ran one after another while the owner moved through the field,
and the passes a frame vary with the scene (79 to 167 in phases whose options barely differ), so the phases are not
a controlled comparison. Two things hold anyway. (1) Phase 2 did not test what it was meant to: forcing every upload
to the upload command buffer left the buffer copies that end passes (108 a frame), so they do not come from the
reorderable uploads (`MappedUploadMemory`, `InlineMemory`) but from the other callers of `CopyBuffer`, which never
reorder: the guest's own GPU buffer copies (`BufferCache::DMACopy`), inline index draws (`UpdateIndexBuffer`) or
buffers merged when they overlap (`JoinOverlap`); which one is unknown (the diagnostic records the line in
`CopyBuffer`, not its caller). Buffer renaming of uploads would not remove them. (2) Ignoring fragment barriers
removed their 23-30 passes without a clear effect on the frame rate. The owner ended the measurements here
(the twelfth log was announced as the last). State reached: 1x in the field went from 16 FPS (before O1-O3) to 22-25
on average, 28-30 in battles and light scenes; the remaining limit is the Mali's work on 80-170 render passes a frame,
most of them ended by the guest's buffer copies. Next, if the work resumes: record the caller of each `CopyBuffer`
that ends a pass, then keep those copies inside the frame's flow (renaming or moving them before the pass), or D7.

Thirteenth log (owner, 15 October, the same test cycle standing still in the field, as asked after the twelfth;
first three samples of each phase left out as transitions): phase 1 is unusable (the scene changed, 14.9 to 29.9
FPS); phase 2 (every upload at frame start) 25.0 FPS, 170 passes; phase 3 (+ fragment barriers ignored) 24.7, 170;
phase 4 (defer wait for idle off) 24.1, 178; phase 5 (clear on load off) 22.5, 177; the restored settings 23.1, 177.
Read from it: clear on load is worth 1 to 2.5 FPS; the other options and both tests move the rate by about 1 FPS,
within the noise and a slow drift over the run; even phase 3 keeps 170 passes, the copies through `CopyBuffer`
(108-118 a frame) taking over from what was removed. The changes to Eden's render passes are spent, about 5 %
together; what remains is the guest's own buffer copies, as written above.

The measurements ended there (owner, 15 October). The TEMPORARY diagnostic was then deleted (CLAUDE.md 2.4): the
sampler and its counters (`common/pomegrade_diagnostics.h`), the driver diagnostic
(`PomegradeDriverDiagnostic.kt`), the two ceiling tests and the test cycle. To measure again, they are in the
history (commits 7629a4b5 to c9374467). Kept: O1-O3, the large cores, compressed render targets (opt-in), clear on
load (on), defer wait for idle (opt-in, no measured gain alone), the crash fixes. The disk cache works, so H4
explains first-run stutters, not this floor.

Follow-up without a new log (owner, 15 October: no more test rounds, the fixes in one build). Of the callers of
`CopyBuffer` that can end a pass, the one fixable without changing what is drawn was done: inline index draws
(`BindHostIndexBuffer`) bind their indices from the staging memory they were written to, as uniform buffers already
are (`BindMappedUniformBuffer`), instead of copying them into the inline index buffer. Left as they are, with the
reason: the guest's own GPU copies (`DMACopy`) must stay ordered with the draws around them; the uploads
(`MappedUploadMemory`, `InlineMemory`) are already moved out of the pass when the region is unused; `JoinOverlap`
and the downloads are rare or required. Which share of the 108-118 copies a frame were inline index draws is
unknown (not measured), so the gain is unknown until the owner plays the build.

---

## 5. Optimisation catalogue

Gains are estimates; difficulty and risk are judgements from the code.

### 5.1 Root-cause fixes, small

| # | Change | Where | Expected gain | Difficulty | Risk |
|---|---|---|---|---|---|
| O1 | Pin the emulated CPUs to the largest cores, ranked by `cpu_capacity`; with fewer large cores than emulated ones, pin only the busiest and leave the rest to the scheduler | `common/thread.cpp` (`PinCurrentThreadToPerformanceCore`), `core/cpu_manager.cpp` (`RunThread`) | Large if H1 holds (an A78 does roughly 2 to 3 times an A55's work per clock) | Low | Low; the 8 Elite keeps its placement. **Done (15 October): each guest core may run on the large cores (cpu_capacity at least half the largest, with the thread's id), untested on the phone** |
| O2 | A thread priority that works: `setpriority` nice values for the guest cores and the GPU thread; fix the level formula | `common/thread.cpp` (`SetCurrentThreadPriority`) | Small to medium | Low | Low. **Done (15 October) on Android: nice values (guest cores and GPU thread -10, High -4, Low +10) with the thread's id, untested on the phone.** The 13T's kernel refuses an app's own `uclamp` (`kernel/sched/core.c`: no CAP_SYS_NICE, EPERM), so frequency floors go through O3 (ADPF) |
| O3 | ADPF hint sessions for the guest cores and the GPU thread, target 16.6 ms (or 33.3 ms) | Beside `core/cpu_manager.cpp`, `video_core/gpu_thread.cpp` | Steadier frames, later throttling | Medium | Low (Android 12+, ignored before). **Done (15 October): one session for the guest cores and the GPU thread (the Critical threads), target 33.3 ms, each guest frame's duration reported (`common/android/performance_hint.cpp`); untested on the phone** |
| O4 | Batch texture downloads: record every image's copy, then one `Finish`, then swizzle them all (as `PopAsyncFlushes` already does) | `texture_cache/texture_cache.h` (`DownloadMemory`) | Fewer stalls where the CPU reads GPU-written textures | Low | Low; same order of copies. **Done (15 October), untested on the phone** |
| O5 | Make the frame pacing setting work on Android 11+ again, or hide it there | `renderer_vulkan/vk_swapchain.cpp` (`AcquireNextImage`) | Lets the user cap at 30 for steadier frames and heat | Low | Low |
| O6 | Release APK; `-march=armv8.2-a` as an extra build for current phones | `android-apk.yml`; `YUZU_BUILD_PRESET` | Small | Low | Low; armv8.0 phones keep the generic build |
| O12 | Draws that capture transform feedback and rasterize nothing run in a render pass with no attachment (1x1 framebuffer, pipelines built for it) instead of the game's. Pokémon Scarlet past the title screen (owner's logs, 28-29 October): about 237 such draws a frame, all with rasterizer discard, no fragment or geometry shader, in 2 runs, read later as vertices by about 240 draws; each closed a pass over the game's framebuffer (337 passes, 600 screens of pixels a frame, 13.7 FPS). A first try that read their counters inside the pass gave glitches and no gain, reverted | `renderer_vulkan/vk_graphics_pipeline.cpp` (`uses_null_render_pass`), `vk_rasterizer.cpp`, `vk_scheduler.h` | Large in Scarlet (the game's pass closed about twice a frame instead of 237 times, inferred) | Medium | Low to medium: commands and barriers unchanged; only with rasterizer discard in the pipeline key (extended dynamic state 2 off). **Done (29 October). Measured (owner's log, 29 October): screens of pixels a frame about 600 to 55-95, FPS unchanged (about 14): not the bottleneck, see O13** |

### 5.2 Root-cause work, larger

| # | Change | Where | Expected gain | Difficulty | Risk |
|---|---|---|---|---|---|
| O7 | Make async GPU emulation correct on Android, then on by default: find which syncs break (fences, flushes, `WaitForSyncOperation`) on Mali and fix them rather than turning it on blind | `video_core/gpu_thread.cpp`, `video_core/gpu.cpp`, `renderer_vulkan/vk_rasterizer.cpp` | Large if H2 holds: GPU translation leaves the guest cores | High | High: Eden reports glitches and crashes; each must be traced to a missing sync |
| O8 | Extended dynamic state (level 1, then 2) for Mali drivers that report it correctly, with a driver-version list built from logs | `settings.h` (`dyna_state`), `vulkan_device.cpp` | Fewer pipelines and stutters | Medium | Medium: driver bugs |
| O9 | GPU memory budget from the phone's RAM (e.g. a share of total RAM minus the guest's memory layout) instead of a fixed 4 GiB | `vulkan_device.cpp` (`device_access_memory`) | No kills on 6 GiB phones; fewer collections on 12 GiB phones | Low | Medium: too low a budget means more collection stalls (O4 then matters more) |
| O10 | Fewer memory-tracking faults: track writes at a coarser granularity for regions the guest rewrites every frame (vertex streams, uniform buffers), or upload them without protection | `core/memory.cpp`, `buffer_cache/buffer_cache.h` | Depends on H5's count | High | High: missed writes show as stale geometry |
| O11 | Async presentation on by default if the measurements show the present copy on the critical path | `settings.h` (`async_presentation`) | Small to medium | Low | Low to medium (one frame more latency) |
| O13 | Transform feedback done by Eden in the vertex shader on Mali, not by the driver: the shader recompiler stores each captured output into a storage buffer at the place the hardware would (buffer offset + (base vertex + vertex index, or instance for instanced draws) x stride + the output's offset), the counters (bytes written per buffer, which the game reads and draws from) are updated by Eden, and the driver's VK_EXT_transform_feedback is not used. Why (owner's logs, 30-31 October, Punica Opt with the GPU timestamps and Mali counters of the diagnostics): in Pokémon Scarlet past the title screen the GPU is busy 94-97 % of the time, about 57 ms a frame, of which **about 40 ms (70 %) in the 2 runs of about 230 transform feedback draws** (rasterizer discard, no fragment shader: skinning, inferred); the fragment queue is used about 17 %, memory about 0.4 GB/s each way, the CPU threads mostly wait for the GPU (fence and present waits). The Mali has no transform feedback in hardware and its driver emulates it (inferred from the cost); the rest of the frame (about 17 ms) would hold 30 FPS | `shader_recompiler/backend/spirv/` (outputs of the vertex stage, `emit_spirv_context_get_set.cpp`, XFB decorations in `spirv_emit_context.cpp`), `renderer_vulkan/vk_pipeline_cache.cpp` (which pipelines take it), `vk_query_cache.cpp` (`TFBCounterStreamer`: counters from Eden instead of EndTransformFeedbackEXT), `buffer_cache/buffer_cache.h` (transform feedback buffers bound as storage) | Large in Scarlet: up to about 40 ms of the 57 ms a frame (from about 15 toward 30 FPS, inferred; not measured) | High: the recompiler, the buffer bindings and the counters change together | Medium to high: a wrong offset, stride or counter shows as broken or missing characters; Mali only, the driver path kept elsewhere. Not started. **Read 31 October**: the place above holds only for non-indexed point or list draws without strips: transform feedback writes the vertices of the assembled primitives in order, so an indexed draw writes a shared vertex once per primitive, a strip or fan is written as separate triangles, and instances follow one another (instance x vertices + vertex); the counter follows the same rule. Which case Scarlet's draws are is logged since 31 October (`transform feedback shapes`: topology, indexed, instanced, each buffer's stride and varyings, indirect draws); read it before starting. A vertex shader may store: vertexPipelineStoresAndAtomics is one of the features Eden requires (FOR_EACH_VK_MANDATORY_FEATURE) |

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
- **An OpenGL ES renderer** (owner's question, 26 October): Eden's OpenGL renderer needs desktop OpenGL 4.6
  (`video_core/renderer_opengl/gl_device.cpp`: "OpenGL 4.6 is not available" ends it; seen), which Android does not
  offer: the Mali gives OpenGL ES 3.2, so on Android Eden runs Vulkan only. An ES renderer would be a whole new
  renderer, emulating what ES lacks and the Switch's games use, and it would give less control than Vulkan over the
  render passes, the Mali's measured limit (4.1). Not measured, inferred: Mali's ES driver is not known to be faster
  than its Vulkan one. Not proposed.

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

**Step 1 of the owner's plan (26 October): which copies end the render passes.** The TEMPORARY diagnostic of 14-15
October is back (commit 68faafea reverted), and `BufferCacheRuntime::CopyBuffer` now hands its caller's file and line to
the pass-end count, so each copy that ends a pass is counted at the line of `buffer_cache.h` that asked for it (on
26 October: 242 `DMACopy` (the guest's GPU copy), 656 `CommitAsyncFlushesHigh` and 1812 downloads, 761
`BindHostIndexBuffer` (an index upload), 1559
`JoinOverlap`, 1719 `MappedUploadMemory`, 1761 `InlineMemory`). To read: a log of two minutes in the field at 1x with
the same settings as the thirteenth, then the fix of the callers whose copies can leave the pass. The diagnostic is
deleted again once read (CLAUDE.md 2.4).

Fourteenth log (owner, 26 October, main at PR #136; the game left running all night standing still in Pokemon Shield's
field, 1x, asynchronous GPU, compressed render targets, clear on load and defer wait for idle on; the old test cycle,
still on in the owner's settings, ran its 5 phases in the first 308 s, left out here): 32 132 samples, 13 039 in play,
measured. FPS 22.8 mean (22.4 median), steady over 9 hours (no heat drift: thermal status 0); 175 render passes a
frame. What ends them, per frame: **`buffer_cache.h:242` (`DMACopy`, the guest's own GPU buffer copies) 97.0**,
`vk_rasterizer.cpp:892` (`FragmentBarrier`) 25.0, a draw needing other attachments 20.9, `buffer_cache.h:1719`
(`MappedUploadMemory`, uploads that could not move) 19.2, texture copies 5, queries 2, submits 2. So more than half of
the passes are cut by the guest's DMA copies.

Change (26 October): a DMA copy whose source and destination no GPU work has used this frame (the buffer cache's usage
tracking, reset each frame) is recorded in the upload command buffer, which runs ahead of the frame's commands in the
same submission, after a barrier ordering it after everything submitted before; so it no longer ends a render pass.
Ordering kept: uploads that could not move mark their region used (a copy touching it stays in place), uploads that
moved are in the same buffer in the guest's order, and a copy whose regions were used this frame stays where it was.
Unmeasured: how many of the 97 qualify, and the FPS. Not built here: CI.

Fifteenth log (owner, 26 October, main at PR #137, the same place and settings, test cycle off, 125 samples in play,
about 2 minutes), measured: FPS 23.3 mean (23.6 median) against 22.8; 168 passes a frame against 175; `DMACopy` ends 88
a frame against 97. So about one DMA copy in ten qualified: the others touch regions this frame's draws already used
(the guest draws, then copies). The gain is within the noise of a short run; the owner judges it too small. The change
is kept (it only moves copies whose order cannot change). What could take the other copies out of the pass: renaming
the destination when this frame only read it (a new region for the copy, as drivers do); a source this frame wrote
must wait. Not started.


**State on 26 October** (no new measurement since the thirteenth log, 15 October): the CPU side no longer limits in the
field; the Mali's work on 140-170 render passes a frame does, most of them ended by the guest's buffer copies
(`CopyBuffer`). What stays planned on the Mali: record which caller of `CopyBuffer` ends each pass and keep those
copies out of the passes where ordering allows; Mali driver-version checks (2.8); D7 in the long run. PanVK on the
13T (owner, 26 October): the game does not start at all with it, so it is dropped (no log kept: why it fails is
unknown). The inline index draws' fix (15 October) is unmeasured.


1. Done (14 October): the logs, the first baseline (4.1); ADB refused for good.
2. The diagnostic's lines: the logs sent on 14 October were cut by the upload before the game started (4.1); to
   send again whole (zipped, or in the Drive folder). Diagnostic build (section 4, no ADB): per-thread CPU time and core, core capacities, `Finish` and fault counts
   per second, in Eden's log, standing still in Postwick; tells H1, H2, H3, H5 apart.
3. A/B of async GPU on and off with the current build (a setting, no code change): measures H2 before any code.
4. O1 and its A/B; results written here, marked as measured.
5. Then by measured impact, deep projects first: D2 and D1 if compiles dominate, D3 if H2 is confirmed, D4 if
   exits dominate, D6 if the command processor does; O2, O3, O4, O8, O9 alongside.

**Eden updated (31 October, branch `eden-update-nx2-base`): from v0.2.1 to Eden's master of 26 October (commit
26a03d24, the base of Eden-NX2), Punica's changes carried over.** Eden's master now does three of this study's
optimisations itself; Punica's versions are removed rather than kept beside them (two of each would fight), seen in
the code:
- **O1, O2 (large cores, nice values)**: Eden's `common/thread.cpp` (#4254, its multithreading refactor) gives the same
  nice values (Android's steps, clamped to `RLIMIT_NICE`) and picks the performance cores by `cpu_capacity` (at least
  half the fastest, four at least), recomputed when the affinity changes. Punica's version taken out.
- **O3 (ADPF)**: Eden's `common/adpf.cpp` opens render and background sessions for the guest cores, the GPU and Vulkan
  threads and the fence thread, reports each displayed frame (`nvdisp_disp0`) against the frame period (`conductor`).
  Punica's `common/android/performance_hint.*` and its frame report in `perf_stats.cpp` deleted.
- **Clear on load**: Eden's scheduler defers a full clear made before its pass begins and begins the pass with
  `LOAD_OP_CLEAR` (`DeferColorClear` / `DeferDepthStencilClear`, `RenderPassVariant`, also discarding MSAA stores).
  Punica's `pomegrade_clear_on_load` (its setting, Android option, render pass key bit and scheduler code) deleted; the
  test cycle has four phases (its fifth compared clear on load).
Kept: compressed render targets (O13), defer wait for idle, O12 (no-attachment pass for transform feedback), the
upload reordering, the picture passes, the TEMPORARY diagnostic. Asynchronous GPU emulation is on by default on
Android now (Eden's master; v0.2.1 had it off there): H2 above. **Unmeasured**: whether Eden's deferred clear keeps
clear on load's 1 to 2.5 FPS (the twelfth log), and the frame rate with all of it; the next log, same place and
settings as the fifteenth, answers both.

## 11. "Mali custom" version of Pokemon Scarlet (owner's request, 1 November)

Numbered 11 because section 10 (the measurement of 1 November: skipping every transform feedback draw takes Scarlet from 13.7 to 18.5 FPS) lives on the branch `claude/app-ui-redesign-v1n2ii`.

**Goal.** Patch the game itself (its code `main` in the exefs, and its shaders) so its GPU work suits the Mali (e.g. skinning in the vertex shader instead of a transform feedback pass), still run by Eden as an Eden mod (IPS patches on the exefs, replaced files), not a recompilation. Patches are tied to one game version. Scarlet first, Z-A later for comparison; long term, an open-world mod (Scarlet, Z-A, the next game).

**Rules.** The owner's dump, keys (`prod.keys`, `title.keys`) and anything extracted from them stay in the session's temporary scratch space, never committed, uploaded or put in an artifact (CLAUDE.md §8). The repository only gets our own scripts, patches and documentation.

**Approved tools (owner, 1 November, once for all).** Using anything not on this list needs the owner's approval first. No file or information of the owner's goes to an outside service, except the services named here.

| Step | Tools |
|---|---|
| Open the game | hactool (SciresM), hactoolnet / LibHac (Thealexbarney, .NET), nstool (jakcron) |
| Read the code | Ghidra, Ghidra Switch Loader (Adubbz), nx2elf / nsnsotool, Capstone, llvm-objdump |
| GPU and shaders | Eden's own code (shader decoder, draw logs), Ryujinx's shader code (ShaderTools), SPIRV-Tools / glslang, RenderDoc, Mesa nouveau (Maxwell compiler) and envytools (Maxwell disassembler), both approved 1 November |
| Write patches | Keystone or aarch64 GNU binutils, IPS / IPS32 (written by our own script), our own C++ tools |
| Game data (later) | ooz (powzix, open-source Oodle decompressor; approved 1 November, needed for every file of the asset archive), pkNX (kwsch), the Scarlet/Violet Trinity tools (flatc schemas, TRPAK/TRPFS readers), FlatBuffers flatc, Blender with a Trinity import plugin |
| Run and measure | Eden headless on Linux, Mesa lavapipe, the GitHub Actions workflow building the mod the owner tests |

Not without asking again: any online service not listed, other emulators or tools, access to the phone (adb).

**Workflow, one step at a time, each merged before the next.**
1. Identify the dump's contents: base game, update and DLC, and their versions. Done (checked with hactool, 1 November): the cartridge dump holds the base game 0100A3D008C5C000 version 1.0.0 (program NCA 6.7 GB), the update 0100A3D008C5C800 v786432, shown as 4.0.0 (program NCA 5.2 GB, SDK 21.0.0) and one DLC 0100A3D008C5D001 (a 114 KB PublicData NCA). Patches target update 4.0.0.
2. Extract the exefs of the version Eden runs (base + update) and the shaders, and set up Ghidra on `main`. Done 1 November (tools and steps in `tools/switch/README.md`): exefs of 4.0.0 extracted (`main` build ID 421c5411b487eb4d049dd065fec95477); `main` converted to ELF and imported into Ghidra 11.4.2 (the Adubbz Switch loader does not build against 11.4.2), full analysis started; the update's RomFS holds 30 files, the assets in `arc/data.trpfs` (8 GB, magic `ONEPACK`) indexed by `arc/data.trpfd` (20 MB, FlatBuffers). Shaders (checked 1 November with `tools/switch/trpfs_extract.py`): two packs, `…shader/NX64/env_titan/material/standard.bnsh` (one BNSH file, 12 MB) and `…shader/NX64/env_titan/special/imgui.trsha` (186 files: 95 BNSH, 91 small FlatBuffers files, likely the shader descriptors; that pack name holds far more than a debug UI, what each shader draws is not checked). Files are Oodle compressed (type 3). The files in that pack are named by the FNV-1a 64 hash of `system_resource/` + the path the code names (`gfx2/shader/NX64/env_titan/...`, with the `.trsha` or `.bnsh` extension): 168 of 186 named that way from the 87 shader paths in `main`.
3. Find which game code issues the transform feedback draws and the other costly passes (Eden's draw log, section 10's measurement), and record it here as checked, seen, inferred or unknown. In progress (1 November, Scarlet 4.0.0 `main`, Ghidra addresses = ELF virtual addresses from `tools/switch/nso2elf.py`):
   - Checked: the game loads the NVN transform feedback functions in its NVN loader `FUN_00cdff20` into globals (`0x485d610` BindTransformFeedbackBuffer, `0x485d618` …Buffers, `0x485d688` Begin, `0x485d690` End, `0x485d6a8` DrawTransformFeedback).
   - Checked: one function uses Begin/End, `FUN_00c52e20`: it binds up to 4 transform feedback buffers (slots 0-3), calls one NVN function with a flag before (byte at `+0x29` of its pass object) and with 0 after, which by position is likely the rasterizer-discard switch (inferred, not checked), then Begin, the draw (`FUN_00c51ef8`), End. When the pass object's byte at `+0x28` is set it calls `FUN_00c59d54` instead, the only user of DrawTransformFeedback. Callers: `FUN_00c4fa44` (from `FUN_00c4f868` ← `FUN_00c4f724`, and from `FUN_00c64f10` ← `FUN_00c64c10`); no strings in them, so which effect they draw is not known yet.
   - Seen: the game has a dedicated skinning shader `system/skinning` (BNSH 33 KB, options EnableNormal / EnableTangent / EnableBinormal), plus `blend_shape`, `partial_blend_shape_pass1/2` and `lattice_deformer`; that `FUN_00c52e20` runs these (a GPU skinning pass writing skinned vertices to buffers) is the working hypothesis, not checked. Checked (BNSH read: `grsc` at 0x60, variation count at `grsc+0x1C`, variation array at `grsc+0x20`, 0x40 bytes each with the program at `+0x10`; the program's stage pointers vertex, hull, domain, geometry, pixel, compute at `+0x8`): exactly five shaders have a vertex stage and nothing else, in every variation: `skinning` (8 variations), `blend_shape` (16), `partial_blend_shape_pass1` and `pass2` (32 each), `lattice_deformer` (8). A vertex shader without a pixel shader only makes sense with transform feedback, so these five are the stream-out passes. For contrast, every one of `standard`'s 6074 variations has vertex and pixel, `gpu_instancing_culling` is compute only. Checked (envytools `envydis -m gm107` on `skinning`'s first variation; a stage's code sits at the stage record's `+0x10` offset with its size at `+0x1C`, starting with a 0x30-byte NVN header and the 0x50-byte shader program header, instructions at `+0x80`): plain linear blend skinning with 4 bones. It reads the position from attribute 0 (`a[0x80..0x88]`), 4 bone indices from attribute 4 (`a[0x140..0x14C]`) and 4 weights from attribute 5 (`a[0x150..0x15C]`), fetches each bone's 3x4 matrix from constant buffer 13 (64 bytes per bone, index × 64), sums the weighted transforms and writes only the position back (`a[0x80..0x88]`), then exits: no pixel output, so the result only exists through transform feedback. The other variations (EnableNormal / Tangent / Binormal) are not read yet. A compute shader doing the same reads and writes gives the same vertices, so the replacement keeps full fidelity. Still not traced: which caller of `FUN_00c52e20` loads which of the five (seen in the code only through the engine's resource tables, no direct string reference).
   - Inputs and outputs of every variation (checked with envytools; a stage record holds the code offset at `+0x10`, the code size at `+0x18` and the control size at `+0x1C`; attribute *n* is `a[0x80 + 16n]`): `skinning` has 5 distinct programs (variations 0, 2, 4, 6 share one): position only (in attributes 0, 12, 13; out 0), +normal (in 0, 1, 12, 13; out 0, 1), and three with tangent data (in up to 0-3, 12, 13; out 0, 1, 2), bones in constant buffer 13, two also read constant buffer 1. `blend_shape` (16): in 0, 1, 3 plus up to 4 morph targets of 3 attributes each, out 0-2, weights in constant buffer 10. `lattice_deformer` (5 distinct): like `skinning`'s outputs, 850-1100 instructions, constant buffers 1 and 13. `partial_blend_shape_pass1` (32) writes no attribute: it stores to global memory (3 global accesses), so it does not need transform feedback at all; `partial_blend_shape_pass2` (2 distinct) outputs attributes 0-2. So 4 of the 5 use transform feedback, with outputs at most attributes 0-2 (position, normal, tangent). Not known yet: the vertex buffer formats and strides the game binds (set by game code at run time, to read from `FUN_00c4fa44`'s callers) and the transform feedback buffer layout.
   - Checked (Ghidra, NVN calls named from the loader's 532 function slots): `FUN_00c51ef8`, the draw `FUN_00c52e20` makes between Begin and End, is the engine's generic draw-command executor. The command object it reads (`param_1`) holds the topology (`+0x64`), vertex or index counts (`+0x70`, `+0x74` instances, `+0x78`), the index buffer (`+0x58`, index size `+0x60`: 8/16/32 bits) and indirect buffers (`+0x88`, `+0x98`); and when its byte at `+0xD0` is set it calls `nvnCommandBufferDispatchCompute` with the group counts at `+0xC4`, `+0xC8`, `+0xCC` instead of drawing. So the engine already runs compute work through the same command path (used by `gpu_instancing_culling`, a compute-only shader); the patch can reuse it rather than add NVN calls of its own. Checked next: the engine's whole NVN backend is a handful of functions, each NVN call made from one place only: `FUN_00c4fa44` binds the program (`nvnCommandBufferBindProgram`, its only caller) for every draw item, compute included; `FUN_00c51920` binds vertex buffers, vertex attribute state and stream state; `FUN_00c537f0` binds storage buffers; `FUN_00c507dc`, `FUN_00c6035c`, `FUN_00c60930` bind uniform buffers (`FUN_00c6035c` also issues barriers); `FUN_00c51ef8` draws or dispatches. A patch can therefore add one code path (bind compute program, storage buffers for the pass's vertex buffers and its transform feedback buffers, dispatch) in `FUN_00c52e20` with the NVN slots already loaded. Not known yet, and not readable from the code alone: the vertex formats and strides the skinning passes' attribute state sets; they come from the model data at run time, so a run of the game under Eden (headless, on Linux) logging that state is the next step.
   - Faster way, used instead of the run (1 November): the vertex formats are in the model files. Each mesh's `.trmsh` (FlatBuffers; files found in the archive by the FNV-1a 64 hash of their path, e.g. `pokemon/data/pm0025/pm0025_00_00/pm0025_00_00.trmsh`) lists its vertex declaration: attribute id, format, byte offset, and the stride. Checked on 9 Pokémon models (30 meshes): 29 meshes use one layout, stride 48: position (id 1) RGB 32-bit float at 0, normal (2) RGBA 16-bit float at 12, tangent (3) RGBA 16-bit float at 20, UV (6) RG 32-bit float at 28, bone indices (7) RGBA 8-bit unsigned at 36, bone weights (8) RGBA 16-bit unorm at 40; one adds a colour (id 5, format 20) at 36, stride 52. Format numbers 51, 43, 48, 22, 39, 20 read as named by their sizes and the offsets that follow (inferred, consistent in every mesh). Trainer and NPC models not checked yet (their paths differ). Transform feedback output layout (read 1 November from `skinning`'s NVN control data, magic `0x98761234`, and checked against the code): each output goes to its own buffer. The control lists one 0x80-byte entry per captured output from offset 220 (values `0x20`, `0x21`, `0x22` = attributes 0, 1, 2), their component counts from offset 1244 (3, 1, 1) and the buffer strides as 16-bit values from offset 1756 (12, 4, 4). So buffer 0 = skinned position, 3 × 32-bit float (12 bytes); buffer 1 = normal, one 32-bit word; buffer 2 = tangent, one 32-bit word. The code packs the normal as signed 10:10:10 (each component × 511, truncated, masked to 10 bits, x in bits 0-9, y 10-19, z 20-29); the tangent's packing (likely the same plus its sign) is not read yet. Everything the compute replacement reads and writes is now known for Pokémon models.
   - Compiler (1 November): `tools/switch/maxwell_compiler/` turns a GLSL compute shader into Maxwell instructions (Mesa nouveau codegen); a 4-bone skinning test compiles. Left for step 4: the game's real vertex layouts and the other variations, wrapping the code as a BNSH compute stage, and patching `FUN_00c52e20`'s callers to dispatch it instead of the transform feedback draw.
   - Step 3 groundwork (1 November, checked in Ghidra): resource binding in the engine is data-driven. A draw item's program object lists its uniform buffers (`+0x68..+0x70`) and storage buffers (`+0xA0..+0xA8`) as entries {stage mask, slot, buffer resource}; `FUN_00c507dc` binds them (stage mask bit 1 → vertex … bit 0x20 → compute) through `FUN_00c537f0` for storage buffers. Vertex buffers come from the draw item's input description at `+0x1A0` (list at `+0x20` of {stream index, buffer resource}, each bound whole: `nvnBufferGetAddress` / `GetSize`). The transform feedback buffers are the pass object's list at `+0x38` (slots 0-3), and `FUN_00c52e20` passes the same object to the draw `FUN_00c51ef8`, whose `+0xD0` byte switches to a compute dispatch. So the patch can stay at data level: give the skinning draw item the compute flag and group counts, storage-buffer entries (compute stage) for the vertex buffer, the bones and the output buffers, and skip Begin/End, with the compute BNSH loaded in place of `skinning.bnsh`. The skinning deformer's constructor is `FUN_010bf058` (labels "[Skinning]" / "Skinning", vtable `0x45D70F8`); blend shape's is `FUN_010bf5e8`. Its vtable methods (checked) are generic job and scene plumbing; the draw item is filled from data, not by skinning-specific code. The executor `FUN_00c4fa44` works on a render node: uniform-buffer list `+0x68`, storage-buffer list `+0xA0`, vertex input `+0x1A0`, and the draw object `+0x1C0`, whose `[0..1]` is the program list (entries of 0x30 bytes: stage mask, 0 meaning all, then the program) and whose `[7]` (`+0x38`) is the transform feedback list; non-null sends it to `FUN_00c52e20`, null to `FUN_00c51ef8`. DrawArrays takes first vertex `+0x78` and count `+0x70`.
     Patch design chosen: one code cave reached from `FUN_00c52e20` (the only place every transform feedback pass goes through), with the compute BNSH loaded in place of `skinning.bnsh` so the node's program is the compute one. The cave binds, for the compute stage: the vertex buffer (stream 0 of `+0x1A0`) at its address + first × 48 with size count × 48, so the shader takes the vertex count from the buffer size and needs no parameter buffer; the bones (the node's uniform-buffer entry for slot 13); the transform feedback buffers 0 and 1 as outputs; then dispatches ceil(count / 64) groups and issues a barrier with the flags the game itself uses in `FUN_00c6035c`; Begin/End are skipped. Still open: how the cave tells a skinning pass from the other three transform feedback passes (not replaced yet), and the barrier flags.
4. First patch: skinning without transform feedback, as an Eden mod; check that it loads headless, then the owner tests it on the phone (FPS against section 10's figures).
5. Next patches only after measuring step 4, aimed at the ≤33 ms per frame needed for 30 FPS.
