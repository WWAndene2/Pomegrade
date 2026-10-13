# Eden optimisation: Xiaomi 13T first, then low-end phones

Plan to make the Switch core (Eden v0.2.1, `eden/`) faster on the owner's Xiaomi 13T (MediaTek Dimensity 8200-Ultra,
Mali-G610 class GPU), and then on weaker phones (the owner's goal: "Switch even on a potato"). Test game: Pokemon
Sword/Shield.

Status (13 October): **audit only, nothing measured, no code changed.** Everything below comes from reading the code.
Each cause is marked **seen** (read in the code), **inferred** (likely, not checked) or **unknown**, and no gain is
measured until the A/B tests of section 6 have run on the phone.

Rules kept throughout: no game-specific hacks that hide rendering or simulation errors; correctness and
synchronisation first; lower graphics settings stay the user's choice, not the fix. Eden is upstream code
(CLAUDE.md §4.9): each edit is minimal and says why in its commit.

---

## 1. What the code does today

Pomegrade does not modify Eden; it runs with Eden's Android defaults.

| Area | State | Where | Mark |
|---|---|---|---|
| Native build | `CMAKE_BUILD_TYPE=Release` (CI); no CPU tuning (`YUZU_BUILD_PRESET` "custom": plain armv8-a) | `.github/workflows/android-apk.yml`, `eden/CMakeLists.txt` (build presets) | seen |
| APK | Debug APK (`assembleProdDebug`, debuggable) | `android-apk.yml` | seen |
| CPU backend | NCE (guest code run natively) by default, Dynarmic JIT as the fallback | `common/settings.h` `cpu_backend`, `eden/CMakeLists.txt` `HAS_NCE` | seen |
| Emulated CPU threads | Each `CPUCore_N` thread pinned to host core N (0 to 3) | `core/cpu_manager.cpp` `CpuManager::RunThread` → `common/thread.cpp` `PinCurrentThreadToPerformanceCore` | seen |
| Thread priority | "Critical" requested, but through `SCHED_OTHER` whose priority range is 0 to 0 on Linux: no effect. The level formula `max(prio + 1, 4)` also always gives the top level | `common/thread.cpp` `SetCurrentThreadPriority` | seen |
| Android power hints (ADPF) | Not used | grep `APerformanceHint`: none | seen |
| Shaders | Built synchronously (async off); disk shader cache and the Vulkan driver's pipeline cache on; 4 to 8 compile threads | `settings.h` `use_asynchronous_shaders`, `use_disk_shader_cache`, `use_vulkan_driver_pipeline_cache`; `vk_pipeline_cache.cpp` `GetTotalPipelineWorkers` | seen |
| Extended dynamic state | Disabled on Android: one pipeline per state combination | `settings.h` `dyna_state` | seen |
| GPU accuracy | Low on Android; reactive flushing off | `settings.h` `gpu_accuracy`, `use_reactive_flushing` | seen |
| ASTC textures | Used natively when the GPU supports them (Mali does) | `vulkan_device.cpp` `ComputeIsOptimalAstcSupported`, `vk_texture_cache.cpp` | seen |

### Drivers

| Point | Where | Mark |
|---|---|---|
| Mali paths: compute shared memory clamped, dynamic storage buffers limited, occlusion query sync and host conditional rendering skipped (the last two are correctness risks, not gains) | `vk_pipeline_cache.cpp` (`needs_shared_mem_clamp`), `vk_buffer_cache.cpp` (`limit_dynamic_storage_buffers`), `vk_query_cache.cpp` (`PresyncWrites`, conditional rendering) | seen |
| No Mali driver-version checks: every feature the driver reports is trusted | `vulkan_device.cpp` | seen |
| `has_broken_parallel_compiling` is never set: Mali always gets parallel shader compiling | `vulkan_device.h` | seen |
| The comment naming Mali for the depth/stencil blit fallback sits inside the `is_nvidia` block; the fallback itself is generic | `vulkan_device.cpp` | seen |
| Custom drivers (`adrenotools`) are Adreno-only; a Mali phone always uses the HyperOS driver | `android/app/src/main/jni/native.cpp` | seen |
| Driver version, reported extensions, whether the driver pipeline cache works | Eden's log at game start (`Render_Vulkan` lines) | unknown |

---

## 2. Main hypothesis: the emulated CPUs run on the small cores

The comment above the pinning says it targets the Snapdragon 8 Elite, whose cores are all large. On most MediaTek
chips, cores 0 to 3 are the small Cortex-A55; the Dimensity 8200 is listed as cpu0-3 A55, cpu4-6 A78 at 3.0 GHz,
cpu7 A78 at 3.1 GHz (**inferred**, check `/sys/devices/system/cpu/cpu*/cpu_capacity`). If so, Sword/Shield's four
emulated CPUs are kept off all four large cores, which the GPU thread and shader compilers then share.

This matters even more on cheap phones: most low-end chips (4 or 6 small cores numbered first, 2 to 4 large ones
after) have the same layout, so the fix serves both goals.

---

## 3. Measurement plan (baseline)

1. **Conditions:** same build, same save, battery above 50 %, no case, room at about 25 °C, fixed brightness, flight
   mode. Shader cache warmed by one pass first; one extra run with the cache cleared measures compile stalls.
2. **Scenes (10 minutes each, 3 runs):** Wedgehurst by day, Motostoke, Wild Area with weather, a gym battle, a Max
   Raid.
3. **Metrics:**
   - frame time: average, 1 % low, frames over 33 ms (Eden's `show_performance_overlay`);
   - CPU load and frequency per core, per thread (`CPUCore_*`, GPU thread, `VkPipelineSerialization`, shader
     workers);
   - GPU load and frequency;
   - stutters matched to pipeline builds (`gpu_logging_enabled`);
   - memory (PSS) and temperature and clocks over the 10 minutes.
4. **Access:** per-thread and per-core data needs Perfetto or simpleperf over ADB, a debug bridge: it needs the
   owner's authorisation first (CLAUDE.md §3). Without it, only the overlay and logs.

**Reading the result:**

| Observation | Bottleneck |
|---|---|
| `CPUCore_*` on cores 0-3 at 100 %, GPU not busy | CPU placement (section 2) |
| Stutters at new pipelines, smooth once cached | Shader compilation |
| GPU near 100 %, CPU threads waiting | GPU translation or memory bandwidth |
| Speed falls after 5 to 10 minutes as temperature rises | Thermal throttling |
| GPU and CPU threads both waiting on each other | Synchronisation |

---

## 4. Changes, Xiaomi 13T (in order, each only once measurements confirm it)

| # | Change | Where | Expected gain (estimate) | Difficulty | Risk |
|---|---|---|---|---|---|
| 1 | Pin the emulated CPUs to the actual largest cores, ranked by `cpu_capacity`, or leave placement to the scheduler when the large cores are fewer than the emulated ones | `common/thread.cpp` `PinCurrentThreadToPerformanceCore`, `core/cpu_manager.cpp` `RunThread` | Large if section 2 holds: an A78 does roughly 2 to 3 times an A55's work per clock | Low | Low; Snapdragon 8 Elite keeps its behaviour (its largest cores come out the same) |
| 2 | ADPF performance hint sessions for the CPU and GPU threads, with the frame time as target | Beside `core/cpu_manager.cpp` and `video_core/gpu_thread.cpp` | Steadier frame times, later throttling | Medium | Low (Android 12+, ignored before) |
| 3 | A thread priority that works (`setpriority` nice values; fix the level formula) | `common/thread.cpp` `SetCurrentThreadPriority` | Small to medium | Low | Low |
| 4 | Extended dynamic state level 1 for Mali drivers that report it correctly | `settings.h` `dyna_state` default, driver check in `vulkan_device.cpp` | Fewer pipelines, fewer stutters | Medium | Medium: known driver bugs; needs the driver log (section 1) |
| 5 | Compile for armv8.2-a (the Cortex-A78/A55 generation) | `YUZU_BUILD_PRESET` in `eden/CMakeLists.txt` | Small | Low | Low on the 13T; drops armv8.0 phones, so an extra build, not a replacement, for the potato goal |
| 6 | Release APK instead of debug | `android-apk.yml` | Small (hot path is native) | Low | Low |

Not proposed as a fix: asynchronous shaders (they hide stutter by drawing frames with missing effects).

---

## 5. Low-end phones ("potato")

What a weak phone hits first, beyond the changes above (all **inferred** until measured on one):

| Limit | What exists | Proposed |
|---|---|---|
| Few large cores (2 large + 6 small) | Pinning to cores 0-3 | Change 1: with fewer large cores than emulated ones, pin only the busiest core thread, leave the rest free |
| RAM (4 to 6 GB) | `memory_layout_mode` (4 GB default), `vram_usage_mode` Conservative | Measure PSS in section 3; look for caches that grow without a limit (shader environments kept in memory, texture cache) before anything else |
| Slow storage, cold start | Disk shader cache | Measure first-launch stalls; check the driver pipeline cache is reused across launches |
| Old Vulkan drivers | Driver checks for Adreno and NVIDIA only | Driver-version checks for Mali and PowerVR, from logs of the phones tested |
| GPU too weak for 1x | Resolution down to 1/4x (`resolution_setup`) | User's choice, not the fix; left as it is |
| No NCE (32-bit or unsupported) | Dynarmic JIT | Out of scope: Pomegrade's Switch core is arm64 only |

---

## 6. A/B tests

- One change per build against the same baseline build; runs alternate A, B, A, B on the same scenes, phone cooled
  between runs.
- A gain counts when it exceeds the run-to-run spread and holds in at least 4 of the 5 scenes.
- Correctness on every build: the same frames compared with the baseline, audio checked, one full battle played.
- Change 1 re-tested on a Snapdragon phone if one is available, and on one low-end phone for section 5.

---

## 7. Next steps

1. Owner sends Eden's start-of-game log (driver version and extensions) and the core layout (`cpu_capacity`).
2. Baseline of section 3 on the 13T.
3. Change 1 and its A/B; results written here, measured values marked as such.
4. The rest of section 4, then section 5 on a low-end phone.
