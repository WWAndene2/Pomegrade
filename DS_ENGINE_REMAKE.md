# DS Engine "Remake" Project: Solutions and Ideas

Custom engine based on melonDS, hardware limits removed, OpenGL backend, target example: Dragon Quest Monsters: Joker.

> This is a restructured version of the original notes. Content is kept; sections are regrouped into parts that follow the logical order of work, all cross-references were renumbered, and the three overlapping DS-limit sections (old 21, 21B and 24) are merged into section 5 with duplicate ideas combined. Appendix A maps old numbers to new ones.

> **Status in Pomegrade** (updated 2026-10-04): ✅ done, 🟡 partly done, each with what was measured. Done means built and tested on the desktop (`tests/`, and a save state of Dragon Quest Monsters: Joker); nothing below has been tried on a phone yet. Details and history: `DEVELOPMENT_NOTE.md` and the git log.

---

## How to read this document

### Work steps at a glance

| Step | Part | Goal | Sections | Exit criterion |
|---|---|---|---|---|
| 1 | I. Foundations | Fix scope, hook point and scene format | 1 to 4 | A DS frame re-renders at parity through the Scene IR, with model and view matrices captured separately |
| 2 | II. Capture and understanding | Instrument the emulator, identify and classify assets, rework them into entities, compile the remake, become a native engine, understand any ROM, remaster by round-trip | 5 to 7E | Every draw has a stable ID, a class and a confidence score, visible in a debug overlay |
| 3 | III. Camera and world | Own the camera, add depth presentation, stitch the world | 8 to 10 | Free camera works, neighbor maps stream in, boundary handoff is masked |
| 4 | IV. Rendering quality | Raise the baseline, then add sky, RT, volumes, 2D-to-3D | 11 to 16 | Original-versus-upgraded A/B view looks coherent across a whole scene |
| 5 | V. Simulation | Wind, grass, hair, cloth, entity interaction, destruction and division | 17 to 17A | One monster replaced end to end with physics running |
| 6 | VI. Platform and performance | Profile, budget, warm up, compress, shared field, GPU-driven geometry, virtual memory | 18 to 20A | Stable frame time on the target phone after 10 minutes of play |
| 7 | VII. Expansion | Extra features and multi-game fusion | 21 to 23 | Single-game pipeline proven first, then a second game of the same family |
| 8 | VIII. Planning | Roadmap, open questions, next deep-dives | 24 to 26 | Decisions recorded and next topic chosen |

### Section index

| # | Section | Part | Delivers | Needs first |
|---|---|---|---|---|
| 1 | [Context and goals](#1-context-and-goals) | I | Scope, target game, backend choice | - |
| 2 | [Core architecture](#2-core-architecture) | I | Hook point, frame order, pipeline overview | 1 |
| 3 | [Scene IR: one scene description, several backends](#3-scene-ir-one-scene-description-several-backends) | I | One shared scene format; renderers become replaceable | 2 |
| 4 | [World scene: separating the camera from the game render](#4-world-scene-separating-the-camera-from-the-game-render) | I | Camera-free world space for ray tracing and ambient light | 2, 3 |
| 5 | [The DS engine: limits, leaks and ideas to go past them](#5-the-ds-engine-limits-leaks-and-ideas-to-go-past-them) | II | How the DS renders and leaks; recover identity, read free semantics, reinterpret DS tricks, frame interpolation, ghost cameras | 2 |
| 6 | [Live remake: asset identification, replacement and game state](#6-live-remake-asset-identification-replacement-and-game-state) | II | Replacement tiers, manifest, state reader | 5 |
| 7 | [Detecting and upgrading asset types without AI](#7-detecting-and-upgrading-asset-types-without-ai) | II | Classifier, material record, upgrade profiles | 5, 6 |
| 7A | [Entity rework pipeline: from flat asset to reworked entity](#7a-entity-rework-pipeline-from-flat-asset-to-reworked-entity) | II | Classify, destructure, map, volume, texture, physics and verify in one deterministic pipeline; flat painted cloth to 3D cloth | 5, 7, 14, 17 |
| 7B | [The remake compiler: the whole game as a build pipeline](#7b-the-remake-compiler-the-whole-game-as-a-build-pipeline) | II | Oracle plus semantic scene, remake pack, events, skeleton families, play-trace priorities, degradation ladder, reproducible builds | 5, 6, 7, 7A |
| 7C | [The DS as raw material: from emulation to a native engine](#7c-the-ds-as-raw-material-from-emulation-to-a-native-engine) | II | Emulator as extractor, oracle and temporary runtime; independence ladder N0 to N6; lifting rules without AI; hardware limits removed; classic and extended rulesets | 5, 6, 7B |
| 7D | [Understanding any ROM automatically: library subtraction and a game control surface](#7d-understanding-any-rom-automatically-library-subtraction-and-a-game-control-surface) | II | Library code subtraction, boundary anchoring, function-as-RPC, hooks and seam stubbing, ROM profile | 5, 6, 7C |
| 7E | [Preimage remaster: assets that must round-trip, family templates and motion inversion](#7e-preimage-remaster-assets-that-must-round-trip-family-templates-and-motion-inversion) | II | Round-trip validity test, constraint tightness map, family template fitting, animation-to-physics inversion | 5, 7A, 16.1 |
| 8 | [Free camera](#8-free-camera) | III | Own view matrix, stick controls, culling workarounds | 4, 6 |
| 9 | [Depth effect on a smartphone](#9-depth-effect-on-a-smartphone) | III | Tilt parallax and off-axis projection | 8 |
| 10 | [Open world: all map instances in one continuous world](#10-open-world-all-map-instances-in-one-continuous-world) | III | Map graph, stitching, streaming, boundary handoff | 6, 8 |
| 11 | [Modernizing flat and low-quality assets](#11-modernizing-flat-and-low-quality-assets) | IV | Sampling, AA, HDR, normals, ground blending, particles | 7 |
| 12 | [A real sky instead of a skybox](#12-a-real-sky-instead-of-a-skybox) | IV | Sky as a function of ray direction | 7, 4 |
| 13 | [Ray tracing](#13-ray-tracing) | IV | Hybrid RT, BVH, denoising | 4, 12 |
| 14 | [Volumetric textures and surface shaping](#14-volumetric-textures-and-surface-shaping) | IV | Parallax, shells, raymarching, cloth conforming | 7, 13 |
| 15 | [Turning 2D into 3D](#15-turning-2d-into-3d) | IV | 2D capture and conversion to depth and geometry | 6, 7 |
| 16 | [Procedural materials, vector UI, terrain, tooling and audio](#16-procedural-materials-vector-ui-terrain-tooling-and-audio) | IV | Procedural materials, SDF UI, smooth terrain, glTF export | 7, 15 |
| 17 | [Physics: hair, grass, wind and secondary motion](#17-physics-hair-grass-wind-and-secondary-motion) | V | Wind field, grass, hair, spring bones | 5, 7 |
| 17A | [Entity interaction and object integrity: contact, fields, deformation, destruction and division](#17a-entity-interaction-and-object-integrity-contact-fields-deformation-destruction-and-division) | V | Contact and field coupling, mechanical materials, bond graph, deform, tear, fracture, detach, split, merge, multiply | 7A, 14, 17, 20A |
| 18 | [Android optimization](#18-android-optimization) | VI | Profiling, capture overhead, quality governor | 2 |
| 19 | [Shader warm-up, pipeline cache, texture compression and memory budget](#19-shader-warm-up-pipeline-cache-texture-compression-and-memory-budget) | VI | Hitch-free loads, KTX2 pipeline, memory budget | 18 |
| 20 | [Engine internals: structure, threading, resilience and measurement](#20-engine-internals-structure-threading-resilience-and-measurement) | VI | Thin capture layer, snapshot ring, render graph, device-lost recovery, GPU timers (E1 to E13) | 2, 18 |
| 20A | [Engine substrate: spatial field, GPU-driven geometry and virtualized memory](#20a-engine-substrate-spatial-field-gpu-driven-geometry-and-virtualized-memory) | VI | One shared distance field and radiance cache, cluster-based GPU-driven rendering, paged memory with prefetch, full-preload mode and precomputed caches | 4, 13, 18, 19, 20 |
| 21 | [Extra ideas](#21-extra-ideas) | VII | Optional features sorted by theme | 12 |
| 22 | [Improvements to existing ideas](#22-improvements-to-existing-ideas) | VII | Sharper versions of ideas already in this document, grouped by area | - |
| 23 | [Fusing several games into one world](#23-fusing-several-games-into-one-world) | VII | GameAdapter, fusion levels, saves | 3, 6, 10 |
| 24 | [Roadmap](#24-roadmap) | VIII | Global step order | - |
| 25 | [Open questions](#25-open-questions) | VIII | Decisions still needed | - |
| 26 | [Possible next deep-dives](#26-possible-next-deep-dives) | VIII | Candidate follow-up topics | - |

---

## Part I. Foundations

_Decide the scope, the hook point in the emulator and the scene description everything else builds on._

### 1. Context and goals

- The renderer is yours, so you are not faking effects inside DS limits. You are building a modern pipeline on top of the melonDS core.
- Backend: **OpenGL 4.3+** (compute shaders, SSBOs, image load/store, 3D textures, tessellation).
- Core OpenGL has no hardware ray tracing API, so RT means compute-shader tracing. DS scenes are tiny (a few thousand triangles), so this is cheap.
- Goals:
  1. Ray tracing and volumetric textures.
  2. Cloth/painted textures that get 3D shape and conform to walls, water or faces.
  3. Physics on hair, grass, wind, capes and so on.
  4. Make the engine act like a live "remake" of a DS game (Dragon Quest Monsters: Joker).
  5. Treat the DS game as base material, not as the platform: the emulator becomes an importer and an oracle, and the engine grows into a native engine that owns rendering, camera, simulation, world, UI, audio and rules (section 7C).

### 2. Core architecture

#### 2.1 Where to hook into melonDS

melonDS's 3D path: GPU3D geometry engine -> software rasterizer or OpenGL renderer.

Cleanest design: **leave the geometry engine alone and replace the rasterizer stage.**

1. Take the transformed polygon list, vertices, textures and per-polygon attributes (alpha, fog, toon, etc.) after the geometry stage.
2. Convert them into your own GPU scene representation (vertex/index buffers, material table).
3. Render with whatever you want: modern raster, hybrid RT, or full path tracing.

Side-channel idea: add unused GXFIFO command IDs or a memory-mapped register block so that game code (or patches) can tag materials, for example "this polygon is cloth, conform to surface X".

#### 2.2 Frame order

1. Emulate the frame.
2. Capture draws (geometry, matrices, IDs).
3. Run compute simulations (cloth, hair, grass, water).
4. Render (G-buffer, lighting, RT effects, post).

Run simulations on their own fixed timestep (for example 120 Hz with substeps) and scale it by emulation speed so pause and fast-forward do not break the physics.

#### 2.3 Pipeline overview

1. **Capture and re-render the DS scene.** Persistent-mapped VBO/IBO plus an SSBO of per-polygon attributes. Render into a G-buffer FBO (albedo, world normal, depth, material ID). Reach parity with the original look first.
2. **Material sidecar.** Material table (SSBO/UBO) keyed on texture hash, asset ID or a custom tag. Each entry says "cloth", "water", "wall", "hair" and so on, and points to volume or height textures and sim parameters.
3. **Volumetric and displacement rendering** (section 14).
4. **Simulation passes** (sections 14.4 and 17).
5. **Ray-traced effects** (section 13).

### 3. Scene IR: one scene description, several backends

Fusing two emulators does not help. What does work is splitting the engine at the point where the game stops being "DS logic" and becomes "things to draw". That intermediate scene description is the **Scene IR**.

#### 3.1 Architecture

```
 DS game logic  ->  Scene IR  ->  Backend
 (emulated or       (assets,       - Full: GL/Vulkan, RT, volumes, sims, sky
  recompiled)        materials,    - Android: same, with quality governor
                     lights, tags) - 3DS: PICA200, baked-lite (optional, later)
```

- **Shared:** the logic layer, the asset manifest, the material classifier (section 7), the world graph (section 10), the game-state reader and the animation maps.
- **Per backend:** only the renderer and the asset converter.
- **Why it is good:** the hard reverse-engineering and classification work is written once, and each platform only costs a renderer plus a conversion step.

#### 3.2 What the IR carries

- Draw items: mesh reference, transform, previous transform (for velocity), material record (class, parameters, sim tag).
- Lights (captured DS light vectors and colors, or time of day), fog, and the sky parameters from section 12.
- Camera, including the free camera and optional stereo.
- 2D layers and sprites in the converted form from section 15.
- World position and streaming hints from section 10.
- Fixed-timestep simulation state, so sims and interpolation behave the same on every backend.

Keep it small and declarative: what to draw and with which material class. The backend decides how.

#### 3.3 The logic layer

On desktop and Android the emulated melonDS core can stay as it is. On a native 3DS port the logic would have to be recompiled or decompiled (section 18.7). Both versions of the logic layer must expose the same interface to the IR builder, so the backends do not care which one they sit on.

#### 3.4 Asset pipeline: one source, per-target outputs

- Keep one canonical HD asset set (models, textures, height maps, material records).
- A converter produces the target format: full assets for desktop, ASTC for Android, ETC1/PICA buffers with baked lightmaps for the 3DS.
- Class-based upgrades from section 7 decide what each target gets. Example: the Water class becomes animated normal maps and RT reflections on desktop, and a scrolling texture with an environment map on 3DS.

#### 3.5 Notes on a 3DS backend (for later)

- The 3DS runs DS games through its built-in compatibility mode, which cannot be hooked, so a remake there means a **native homebrew port of one game** (libctru and citro3d), not an emulator.
- The PICA200 has vertex shaders, fragment lighting with lookup tables, shadow maps and fog, but no compute shaders, no 3D textures and no ray tracing. The 3DS profile is therefore baked lighting, shadow maps, vertex-shader wind, small CPU sims and optional stereo 3D.
- RAM is 128 MB (old) or 256 MB (New 3DS), so open-world chunks must be small with aggressive LOD.
- Taking a 3DS emulator apart to add DS features is not recommended: the renderer would be thrown away, and melonDS already emulates the DS side.
- Stereo 3D and 3DS-style screen layouts can also be added to the desktop renderer as options.

#### 3.6 Pitfalls

- Designing the IR around the full backend only. Keep it small and let backends reduce.
- Letting the 3DS memory budget leak into shared data. Do the reduction in the converter, not in the logic layer.
- Timing differences between backends. Carry fixed-timestep state in the IR.
- Adding the IR late. It is much harder to retrofit than to start with.

#### 3.7 Build order

1. Define the Scene IR and put the existing desktop renderer behind it.
2. Make the logic layer talk only to the IR builder.
3. Add the asset converter with per-target profiles.
4. Prove a second backend with a small scene (Android is the closest to the first).
5. Only then prototype a 3DS backend with one battle scene.

### 4. World scene: separating the camera from the game render

The cause of the problem is that everything the game submits is **camera-dependent**: it is culled by the game's camera, positioned relative to the game's view, and often lit in view space. Ray tracing and ambient light need a **world-space scene that exists independently of any camera**. The fix is a persistent world scene between the capture and the renderers: the game's camera only decides what is captured, and the render camera only decides what is shown.

#### 4.1 Why the camera currently breaks lighting

- **Missing occluders.** The game only submits what its own frustum sees, so objects behind or beside the camera vanish. They stop casting shadows, blocking AO and appearing in reflections, so lighting changes when the camera turns.
- **View-space data.** DS light vectors are transformed by the matrix active when they were set, so they arrive in the game's eye space. When the camera rotates they appear to rotate too.
- **Camera-attached objects.** Skyboxes, billboards and some effects follow the camera, so they look like moving geometry to the ray tracer.
- **Unstable history.** A denoiser or probe cache that assumes a static world is invalidated by every game camera cut or shake.
- **Precision.** Positions relative to a moving camera, in fixed point, jitter in the BVH.

#### 4.2 Architecture: a world scene between capture and render

```
Capture (game camera) -> World Scene (camera-free) -> Render views
                              |                       - free camera (main)
   static chunks (sec 7)      |                       - game camera (cutscenes)
   dynamic objects (live)     |                       - shadow / probe / reflection views
   lights, sky, probes        |
```

- **Capture** stores the model matrix and the view matrix separately: `model = inverse(gameView) * positionMatrix`. Get the view from the matrix stack or from the game's camera struct in RAM (section 8.1).
- **The world scene** holds everything in world coordinates: static geometry from the converted map chunks, dynamic objects with their world transforms and previous transforms, lights, the sky, and the lighting caches.
- **Render views** are consumers only. The free camera, the game camera for cutscenes and the shadow or probe cameras all read the same world scene.

#### 4.3 Ray tracing against the world, not the frustum

- Build the BVH over a **relevance region** around the player or focus point (a radius), not the camera frustum.
- Split it: static chunks get a BVH built once and cached, dynamic objects get a small BVH that is refit every frame, and a top-level structure ties them together.
- Rasterization still culls with the render camera. The ray tracer sees everything inside the region, including objects behind the camera.

#### 4.4 Camera-independent ambient light

- **World-anchored probe grid.** Irradiance probes (or SH9 probes) on a grid that follows the player but is snapped to world cells, updated incrementally a few probes per frame. Moving the camera changes nothing, because the probes are a property of the world.
- **Baked per-chunk probes** for static areas, plus the live grid only for dynamic changes. Good for Android (section 18).
- **Lights in world space.** Convert captured DS lights once: `lightWorld = rotation(inverse(gameView)) * lightEye`. The sun then comes from the sky system (section 12).
- **Interiors** get their own lighting volume and no sky.

#### 4.5 Shadows

- Sun shadows (RT or shadow maps) must include casters outside the render frustum. Fit the shadow cascades around the render camera, but fill them from the world scene.
- Contact shadows and AO use world-space rays or the probe grid, not screen-space data only. Screen-space AO can stay as a detail layer on top.

#### 4.6 Temporal stability

- Keep history in **world space** (probe caches, light reservoirs, accumulated irradiance), so a camera move or cut does not invalidate it.
- Motion vectors come from the render camera and from per-object world-space transforms.
- Only a real world change resets history: a map transition (section 10), a time-of-day jump or a scripted lighting event.

#### 4.7 Things that must stay out of the world scene

Tag these as **view-attached** and render them in a separate layer:

- Skybox pieces (already replaced by the real sky), sun and moon billboards.
- HUD-like 3D elements, first-person-style effects, screen-attached particles.
- Detection rule: a draw whose captured position matrix stays constant while the recovered view changes is attached to the view, not to the world.

#### 4.8 Other details

- **Floating origin / chunk-local coordinates.** Store positions as chunk offset plus local position, and rebase the origin as the player moves, to avoid precision loss in a large open world.
- **Retained dynamic objects.** For things the game stopped submitting because its camera left them (section 8.2), keep their last transform in the world scene for lighting purposes, with a timeout.
- **DS screen-space effects** (fog, toon table, edge marking) were done relative to the game camera. Re-create them in your own pipeline using the render camera.
- **Cutscenes.** Switch the render camera to the game camera with the world scene unchanged, so lighting stays consistent.

#### 4.9 Pitfalls

- A recovered view that is slightly wrong makes static scenery drift in world space. Verify by checking that static objects keep fixed world coordinates across frames.
- Objects that are scaled or skinned by the game with camera-dependent matrices need care when extracting the model matrix.
- A relevance region that is too small causes visible light changes when distant objects enter it, so fade contributions in with distance.

#### 4.10 Relation to the Scene IR

This is a more explicit version of the Scene IR (section 3), with the world scene at its center. The IR's draw items, lights and sky parameters should be stored in world space as described here.

The world scene is also the source for the shared spatial field (section 20A.2), which serves RT, collision, ambient light, audio occlusion, navigation and grass interaction from one structure.

#### 4.11 Build order

1. Capture model and view matrices separately, with a debug view showing world-space positions.
2. Build the persistent world scene, with the static chunks and retained dynamic objects.
3. Move lights and the sky into world space.
4. Build the BVH over the relevance region and trace shadows against it.
5. Add the world-anchored probe grid for ambient light.
6. Make temporal history world-space.
7. Add the view-attached layer and the cutscene camera switch.

---

## Part II. Capture and understanding the game

_Instrument the emulator, learn what the DS leaks, then identify and classify every asset._

### 5. The DS engine: limits, leaks and ideas to go past them

Hardware numbers in this section come from memory of the usual DS documentation (GBATEK, melonDS source). Treat them as approximate and verify against the version you build on. Ideas are grouped by what they do: recover lost information, read free semantics, reinterpret DS tricks, undo budget workarounds, and use the emulator's special position.

#### 5.1 How the DS renders, and what each stage leaks

The key observation: the DS is not a framebuffer machine, it is a **command-stream machine**. The game does not draw pixels, it sends commands. A command stream is far richer than the image it produces, and a custom renderer can read it.

| Stage | Hardware fact (approx.) | Limit | Information it leaks to your engine |
|---|---|---|---|
| CPUs | ARM9 ~67 MHz, ARM7 ~33 MHz, 4 MB main RAM | Tiny scene logic, aggressive streaming | The game's own code is small and deterministic, so it can be traced and hooked |
| Geometry engine | Fixed-point, matrix stacks (projection 1 deep, position about 31), 4 lights, per-vertex lighting | About 2048 polygons and 6144 vertices per frame | Explicit model, bone and camera matrices; push/pop nesting; light vectors and colors |
| Polygon attributes | 6-bit polygon ID, alpha, fog flag, depth-write flag, shadow-mask mode, toon/highlight, culling mode | Few materials, no shaders | Free semantic tags (polygon ID, shadow polygons, fog and blend intent) |
| Textures | 4 formats (paletted 4/8-bit, A3I5, A5I3, direct color), a few hundred KB of texture VRAM | Tiny, low-color textures | Palette indices act as region labels; palette animation is explicit |
| 3D rasterizer | 256x192, 15-bit color, no real AA, edge marking, toon table, fog | Low resolution, hard edges | Original look parameters (fog, toon table) to keep art direction |
| 2D engines (two) | Tile layers, affine layers, OAM sprites, windows, blend, mosaic, per-scanline register changes | Fixed layer count, tile and sprite size limits | Tile IDs, sprite IDs, scroll and affine parameters, per-scanline distortion |
| Display capture | Copies rendered output back into VRAM | Used for blur, glow, mirrors | Marks "render-to-texture" effects explicitly |
| Two screens | Top 3D view, bottom touch UI | Fixed 4:3 screens, fixed layout | The bottom screen is a second view of the same game state |
| Cartridge | NitroFS file system | Slow reads, small RAM | A complete offline database of every asset |

**Three kinds of limit**

1. **Hardware limits:** about 2048 polygons and 6144 vertices per frame, 256x192, 4 lights, per-vertex lighting only, small texture memory, 15-bit color, fixed-point math. The rest of this document already attacks these.
2. **Design assumptions** (the "conception"): immediate-mode drawing with no persistent objects, no camera object, lighting at vertices, a 2D engine and a 3D engine composited per scanline, two separate screens, and effects done by abusing hardware (display capture, per-scanline DMA).
3. **Information loss:** by the time a draw reaches the rasterizer, the original asset name, hierarchy, material intent and hidden geometry are gone.

The ideas below recover lost information (5.2 and 5.3), reinterpret design tricks (5.4 and 5.5), use the emulator's special position, which is that it is deterministic and observable (5.6), and rethink the screens (5.7). Each idea says what the DS does, what you can do about it, what it gains and what can go wrong.

#### 5.2 Recover identity and structure

**Display-list fingerprinting.** Games usually draw models by sending pre-packed command lists (display lists) to the geometry FIFO, often through DMA. Treat that stream as bytecode:

- Hash each display list's content. Identical hash means the same mesh, so you get a mesh identity without needing the NitroFS hook (6.2) or relying on texture hashes.
- Keep meshes in model space, uploaded once to the GPU, and draw repeats as instances. This avoids rebuilding vertex buffers every frame.
- It works for any game, including ones whose files you cannot easily identify.

**Call-site attribution**
- Fact: every GX command is written by game code, so the emulator knows which instruction (or which DMA source buffer) produced it.
- Idea: record the ARM9 program counter and a short return-address chain (two or three levels, because code is shared between many draws) when a geometry command or a DMA to the FIFO starts. The call site becomes a stable key such as "the function that draws monsters" or "the function that draws map tiles".
- Gain: "which function drew this" is often a better class than "what texture is this", because the monster, terrain, UI and effect functions are different call sites. It survives asset variants and texture reuse and gives the classifier (section 7) a strong new layer, above metadata and below the manual manifest.
- Risk: how CPU state is exposed in the GPU3D write path depends on your melonDS version.

**Memory provenance (taint tracking).** Section 6.2 records which file lands at which RAM address. Extend it: tag memory ranges with their source file ID and propagate the tag through memcpy, DMA and decompression (LZ77-style loaders). Then a display list that was decompressed and copied still points back to its original file and offset.

**Skeleton and hierarchy recovery from the matrix stack**
- Fact: skinned DS models place bones with push, pop, mult, store and restore operations on the matrix stack, so the sequence of those operations encodes the bone hierarchy.
- Idea: log the nesting per model and rebuild the transform tree with a stable index per node, with no model file needed.
- Gain: bone IDs for retargeting HD models (6.4), for attaching physics to the right bones (17.4), and for attachment points (weapons, hats, eyes) on any model, including unknown ones.
- Risk: some games flatten matrices on the CPU. Fall back to the model file's skeleton from the asset hook (6.2).

**Automatic RAM-map discovery by value correlation**
- Fact: the DS has only 4 MB of main RAM, so scanning all of it every frame is cheap.
- Idea: take the position and rotation values that appear in the submitted model matrices (fixed-point numbers) and search RAM for words that hold the same values. Every hit is likely an entity struct. Track those words over time to find the position, facing and animation fields of each object.
- Gain: the state reader (6.3) gets built mostly automatically, with no manual memory searching, and each draw is tied to a game object.
- Risk: derived values (a matrix computed from a position, not the position itself) need small tolerance and scaling rules. Confirm candidates by moving the player and checking that the matching word changes.
- Experiment: find the player's position word in one map and check that it follows movement.

**Turn these into a full analysis pipeline.** Call-site attribution, taint tracking and RAM-map discovery are the inputs to the automatic ROM understanding in section 7D (library subtraction, boundary anchoring, function-as-RPC and seams).

#### 5.3 Read free semantics from render state

**Material registers become PBR hints.** The DS lighting registers (diffuse/ambient, specular/emission, and the shininess table) are per-polygon material data.

- Map them to roughness, specular tint and emissive strength. A polygon with strong specular and a steep shininess table is a hard, shiny surface, and a polygon with emission is a light source.
- This is real material information that texture statistics only guess at. Add it to the fusion step in 7.1 with a high weight.

**Polygon IDs and attribute flags**
- Fact: each polygon carries a small ID that games use for edge marking and shadows, and many games give each object its own ID. The fog flag and depth-write flag also separate world geometry from effects and sky.
- Idea: use the ID buffer as an object mask (outlines, selection glow, per-object post effects) and as a class channel and tie-breaker for classification. Use the fog and depth-write flags to split world geometry from effects and sky.
- Idea: vertex colors usually carry baked lighting. Invert them into an irradiance estimate for light probes (a stronger use than the AO multiplier in 11.4), then relight with the real lights.
- Risk: games differ in how they use these fields, so check the target game first.

**Shadow polygons as real caster geometry**
- Fact: the DS has a shadow polygon mode for shadow volumes, and games also use blob shadows. Both submit geometry that marks where shadows fall and where the game thinks the ground contact is.
- Idea: capture those volumes and use them as a hint for which objects cast shadows and how large the shadows are. Use blob-shadow positions as ground-contact markers for foot placement.
- Gain: replace them with real shadow maps or ray-traced shadows.

**Palette-index material masks**
- Fact: paletted textures store indices, not colors, and artists use specific indices for specific things (eyes, metal trim, glow). Pixels sharing an index were painted as the same material.
- Idea: a tiny per-asset table maps palette index to material: eyes glossy, cloth sheen, skin soft, metal reflective, glowing indices emissive, all without image analysis or extra textures.
- Gain: per-region materials without repainting, independent normal strength per region (7.5), and palette animation (glow, shimmer) becomes material animation.
- Risk: indices differ per texture, so it is manifest work. The scanner can show index histograms to speed it up.

**Texgen-based detection**
- Fact: the texture-coordinate generation mode tells you how a texture is used. A normal-based mode (`TEXGEN_NORMAL`) is a fake reflection, and a position-based projected mode (`TEXGEN_POSITION`) is often a fake shadow or light map. A scrolling texture matrix is a flow.
- Idea: add texgen mode and texture-matrix behavior to the render-state rules in 7.1. Replace fake reflections with real probes, SSR or ray-traced reflections, replace projected fake shadows with real shadows, and turn scroll vectors into flow maps for water, lava and conveyors.
- Gain: a cheap and reliable classifier signal, and the original's fake effects become real ones in the same place.
- Risk: some games use texgen modes for other things. Treat the signal as a vote (7.1 fusion), not as the final answer.

**Texture-upload animation to blendshapes**
- Fact: DS games animate eyes, mouths and faces by swapping texture data in VRAM instead of changing geometry.
- Idea: watch VRAM texture uploads (source address, destination slot, frequency) and recognize swap patterns. Map each swapped state (open eyes, blink, mouth shapes, hurt face) to a blendshape or morph target on the HD face.
- Gain: HD faces animate in sync with the original game, without parsing animation files.
- Risk: some games use palette changes or texture matrix tricks instead of swaps. The manifest needs a state-to-blendshape table per model.
- Experiment: log uploads for one monster during idle and hurt animations, and see whether a small set of textures repeats.

**Sprite composition as part structure.** Big 2D characters are often assembled from several OAM entries. Those pieces roughly match body parts.

- Treat each piece as a bone-like part: it gets its own depth offset, secondary motion (spring tails, swinging ornaments) and shadow.
- It is cheap rig data for 2D that is already in the capture.

**Recover the lights the game faked**
- Fact: the DS has four directional lights, no point lights. Torches, lamps and fire are faked with emissive polygons or glow sprites.
- Idea: detect emissive and unlit bright polygons (emission register, high vertex color, no lighting, additive blend) and glow sprites, and place real point or spot lights at their positions, with color from the texture and a falloff radius from size.
- Gain: lit rooms, flickering flames and glowing items light their surroundings and cast shadows, which is a strong visual change for cheap.
- Risk: false positives (bright windows, white objects). Use class and call-site tags (5.2) to restrict, and cap the number of active lights per scene.

#### 5.4 Reinterpret the DS's own tricks

**Display capture and render-to-texture**
- Fact: games capture the rendered 3D image into VRAM and reuse it for motion blur, ghosting, water reflections, heat haze, mirrors, glow and fades.
- Idea: detect the pattern (capture, then reuse as a bitmap or texture) with the render-state rules from 7.1, tag it `PostEffect`, and replace it with the proper version: a real bloom, true motion blur from motion vectors, planar or ray-traced reflections, real depth-based haze.
- Gain: the original's cheapest-looking effects become modern ones, in the same place at the same time. A hardware hack becomes a feature instead of a source of artifacts.
- Risk: needs per-game recognition.

**Scanline effects inverted into 3D**
- Fact: Mode-7 floors, curved horizons and wavy backgrounds are made by changing scroll or affine parameters on each scanline. Section 15.4 says to capture them. The next step is to solve for what they mean.
- Idea: the per-scanline scale and offset of a floor layer encode a camera height and pitch. Fit them to a plane homography and rebuild the layer as a real textured plane in 3D that your free camera can look at. Wave effects fit a displacement function that can become a real wave field.
- Gain: pseudo-3D floors become real geometry that you can light, shadow and view from a free camera.
- Risk: only some games and screens use it, and only when the effect follows a clean formula. Check whether DQMJ does before investing.

**Unify the 2D/3D split.** The DS can show the 3D image as background 0 and mix it with sprites and layers. Resolve the whole stack into one scene with true depth (section 15), where HUD parts and sprites can sit in front of, behind or inside 3D objects, not just on top.

✅ **Rebuild the effect stack in float.** *(High-precision geometry: sub-pixel vertex positions, frame-to-frame jitter 27.2 px → 3.0 px.)* Fixed-point vertices cause jitter, wobbling edges and z-fighting at distance. Capturing model-space vertices and projecting in floating point removes most of these without any asset change.

**Reversed-Z float depth**
- Fact: the DS depth range is small, and the game's fog, LOD and culling hide the lack of precision.
- Idea: once you remove those limits (sections 8 and 5), the view distance grows and a standard depth buffer loses precision. Use reversed-Z with a floating-point depth buffer, which spreads precision evenly, and combine it with the floating origin from section 4.
- Gain: no distant z-fighting, and cleaner inputs for ambient occlusion, SSR and TAA.
- Risk: every shader and projection matrix must follow the reversed convention. Decide this early, as with the scene IR (section 3).

**Translucency with DS rules**
- Fact: the DS blends translucent polygons in submission order (or auto-sorted, depending on the setting), and polygons with the same polygon ID blend only once over the same pixel.
- Idea: at higher resolution use order-independent transparency (weighted blended OIT, or per-pixel linked lists on desktop), but apply the DS rules: respect the sort mode, and skip the second blend for equal polygon IDs.
- Gain: glass, water, ghosts and effects look identical in behavior to the original, with none of the popping or wrong ordering that a naive sort introduces at high resolution.
- Risk: weighted blended OIT is approximate. Keep an exact path (sorted by submission order) for important translucent objects, and use OIT for particles.

✅ **Decal and coplanar ordering** *(Depth-equal decals in the OpenGL renderer: DS margin applied, each vertex given its plane's depth at its rounded position; `tests/decals/`.)*
- Fact: games stack decals (road markings, blob shadows, signs, floor patterns) on the same plane and rely on draw order and the depth-equal polygon flag, because the DS depth buffer is coarse and does not z-fight the same way.
- Idea: detect groups of coplanar polygons in a draw. Give each one a depth bias by submission order (or draw them as projected decals), and honor the depth-equal flag instead of the default depth test.
- Gain: no flickering or disappearing decals when rendering at high resolution with float depth.
- Risk: coplanar detection needs a tolerance, and some games depend on precision quirks. Compare against the original rendering with the parity oracle (5.6).

#### 5.5 Undo the budget workarounds

**LOD, culling and fog inversion**
- Fact: to stay inside the polygon budget, games swap in low-detail models at distance, cull aggressively and hide pop-in behind fog.
- Idea: detect sets of display lists for the same object at several detail levels (same call site, same skeleton, different counts, a distance threshold). Map all LODs of an asset to one high-detail replacement with continuous LOD. Treat the game's fog distance as the draw-distance setting to extend or remove, and patch distance thresholds in the game code where practical (the same patching idea as the free camera in section 8).
- Gain: no visible pop-in and a longer view distance, which pairs with the ghost cameras in 5.6.
- Risk: needs asset grouping.

**Imposter detection and replacement**
- Fact: to stay under about 2048 polygons, games draw distant trees, buildings and crowds as flat billboards.
- Idea: detect imposters with the render-state rules (camera-facing, alpha-cutout, repeated, same call site) and replace them with real instanced 3D models inside the view distance. Keep the billboard beyond that distance.
- Gain: forests and towns gain real depth and parallax, and cast and receive real shadows. It is the counterpart of reversing LOD (5.5).
- Risk: each imposter type needs a matching 3D model in the pack. Start with the most common (trees, bushes).

**Sprite-limit inversion**
- Fact: the 2D engine has per-frame and per-scanline sprite limits, so games rotate which sprites are drawn each frame, producing flicker.
- Idea: compare the OAM across frames. When sprites alternate visibility with a regular pattern, treat them as persistent and draw all of them every frame.
- Gain: removes flicker and missing sprites, and makes sprite-based effects (particles, crowds) look full.
- Risk: the game may hide a sprite deliberately. Only merge when the pattern is regular and the position is continuous across frames.

**Tilemap flattening and re-rasterization**
- Fact: upscalers see only 8x8 tiles, which gives visible seams and no context across tile borders. The hardware also samples rotated and scaled layers and sprites with nearest-neighbor.
- Idea: stitch each map's tile layers into one large image (from the tilemap, scroll and tile data), upscale it with full context, and draw it back in the original tile order. For rotated and scaled sprites and layers, re-rasterize from the source tiles at output resolution with proper filtering, instead of upscaling the already-sampled result.
- Gain: clean large backgrounds with no tile seams, and smooth rotation and scaling.
- Risk: palette animation and per-tile flips must be handled before stitching, and per-scanline effects (15.4) need a separate path.

**Animation curve reconstruction**
- Fact: DS animation data is sparse (few keyframes) and often stored at a low frame rate, then sampled and stepped by the game.
- Idea: through the asset hook (6.2), read the animation keyframes directly, and rebuild continuous curves (Hermite or Catmull-Rom) with proper easing, instead of sampling the game's output. Drive the HD rig from the curve at any frame rate.
- Gain: smooth motion at 120 Hz without stepping, and cleaner results than interpolating between sampled frames (5.6).
- Risk: some games blend or modify animations in code. Keep game-driven blending as input and use the curves only for the base clips.

#### 5.6 Use the emulator's superpowers

🟡 **Transform interpolation and exact motion vectors** *(Frame generation interpolates polygons between frames, with no added latency; motion vectors for TAA and motion blur are not done.)*
- Fact: the game submits matrices for frame N and frame N+1, and you capture current and previous model and view matrices. 2D layers have scroll deltas and sprite positions.
- Idea: because you hold the objects (5.2) you can render in-between frames by interpolating bone and object transforms, which is true motion interpolation with no pixel smearing and no optical flow. The same exact per-pixel motion drives TAA and motion blur (11.3). For 2D layers, use scroll and OAM deltas.
- Gain: many games animate at 30 Hz, so this alone makes them feel like 60, 120 or 144 Hz. Combines with the decoupled render rate in section 18.6.
- Cost and risk: one frame of latency, since you need the next frame. Mitigate by extrapolating from velocity, or by running one emulator frame ahead of presentation when input latency allows. Teleports, animation cuts and scene changes must disable interpolation (detect large matrix jumps or map changes).

**Forked and look-ahead emulation (use determinism)**
- Fact: the emulator is deterministic and a savestate is cheap.
- Idea 1, look-ahead: run a second instance one or two frames ahead. You know the future positions and animation states, so interpolation is exact, cameras can anticipate and animations can blend into what is coming.
- Idea 2, ghost cameras: periodically fork a headless instance, patch the game's camera values (section 8) to other angles, and collect the geometry it submits. Merge it into the world scene (section 4). This gets off-screen and behind-the-camera geometry without parsing any map format, which attacks the culling problem in 8.2 directly.
- Gain: a world scene for ray tracing and widescreen with very little reverse engineering.
- Risk: CPU cost on mobile (run ghost forks at low rate, geometry only, no rasterization) and camera patching that breaks game logic that reads the camera. Prove it on one map first.

**Scene memory.** A free camera reveals back sides the game never drew. As the player moves, accumulate geometry and sprite views into a persistent per-map cache (section 8.2).

- Over play time the engine learns the hidden sides of walls and objects.
- A deterministic emulator makes this scalable: a bot or recorded input script can walk each map and fill the cache offline, then ship it as part of a community asset pack (without original data, see 6.7).

**Frame-coherent caching.** Most of a DS scene does not change between frames. With object identity you can cache: skip re-rendering static objects' shadow maps, reuse probe-grid lighting (4.4), and update only dirty objects. This is also the best mobile saving in this document.

**Time-coherent sprite upscaling.** Collect every frame of a sprite animation into an atlas keyed by tile hash. Upscale and clean the whole set together, so edges and colors do not flicker between frames, and so hand-authored normals or depth for a sprite are drawn once.

**Parity oracle**
- Idea: the original software or OpenGL renderer is available in the same emulator. Run both on the same frame and compare silhouettes, positions and animation phase against your replacement scene. Flag drift automatically.
- Extension: the oracle also supplies the frames and score for the round-trip test in section 7E, which makes the remade asset valid only if it degrades back to the original.
- Gain: automatic regression testing for replacement models and the `anim_map` (section 6.4), which is the most laborious part of tier 2 and 3. It also gives you a before/after view for free.

**Retroactive capture from state history**
- Fact: a DS state is small, so the emulator can keep a ring buffer of recent savestates (or deltas).
- Idea: when you notice a glitch, rewind a few seconds and replay it with full capture, debug overlays and logging turned on, instead of trying to reproduce it live. It also allows the engine to classify an asset retroactively, using frames from before it first appeared.
- Gain: faster debugging, and testing without needing perfect repro steps. It complements the parity oracle above (5.6).
- Risk: memory cost on mobile, so make it a developer option, and keep the buffer short.

#### 5.7 Rethink the screens and dress the world

**Dissolve the two-screen concept**
- Fact: the bottom screen shows the same game state as the top (map, party, menus), and touch is just a coordinate.
- Idea 1, bottom screen as data: read the dungeon or map tile layer shown on the bottom screen to get layout, visited areas and markers for the world graph (section 10).
- Idea 2, diegetic UI: draw stats, health bars and prompts next to monsters in the 3D scene, and place menus and maps as 3D panels or floating layers (section 9.4), with real depth and lighting, using the state reader (section 6.3).
- Idea 3, touch retargeting: when the player taps a monster or object in the 3D world, find which bottom-screen element the game expects and inject the DS touch coordinate for it. The game still thinks it was touched on the bottom screen.
- Idea 4, foldables and tablets: render both screens as one continuous canvas, with the world on top, UI flowing into the lower half and a larger map overlay on spare space.
- Idea 5, context UI: the state reader knows what the game is doing, so the engine can show or hide UI by context (battle, shop, overworld) instead of mirroring the DS layout.
- Gain: one large screen, a layout that fits a phone, and no need to re-implement menu logic.
- Risk: menus that rely on drag or multi-step touch, so start with simple taps.

**Rule-based procedural dressing**
- Idea: seed deterministic procedural dressing from the tile and attribute grid. A grass tile gets tufts and flowers, a wall tile gets facade detail, a coast tile gets rocks and foam, all from a hash of the tile position so the result never changes between visits. Wave-function-collapse or simple rule sets work, with no machine learning.
- Gain: dense, varied environments from a sparse tile grid, with no per-tile hand work.

#### 5.8 Conceptual limits to break

The DS design assumes things that no longer have to hold:

| Assumption | What you can change | Ideas that enable it |
|---|---|---|
| 4:3 view and 256x192 framing | Widescreen and ultrawide | Forked and look-ahead emulation (ghost cameras), LOD, culling and fog inversion |
| Two separate screens | One continuous layout, contextual UI | Dissolve the two-screen concept |
| Fixed or limited camera | Free camera, photo mode, VR | Ghost cameras, free camera (section 8), world scene (section 4) |
| 30 or 60 fps | 120 Hz and beyond | Transform interpolation and exact motion vectors |
| Static, repeating world art | Dressed, lit, living environments | Palette-index masks, procedural dressing, classifier (section 7), real sky (section 12) |
| Opaque game code | Call-site and hierarchy-aware engine | Call-site attribution, skeleton recovery |
| Menu logic tied to touch | Any input device | Touch retargeting in the two-screen idea |

#### 5.9 Effort versus impact

| Idea | Impact | Effort | Main risk |
|---|---|---|---|
| Call-site tagging | High | Low | CPU state exposure |
| Attribute channels | Medium | Low | Games differ in use |
| Palette-index masks | Medium | Low-medium | Manifest labor |
| Frame generation | High | Medium | Scene-cut handling |
| Render-to-texture detection | Medium | Medium | Needs per-game recognition |
| Parity oracle | High (for tooling) | Medium | Alignment of two renderers |
| Hierarchy recovery | High | Medium | Matrix flattening |
| Dissolve two screens | High | Medium-high | Complex touch menus |
| LOD and fog inversion | Medium | Medium | Needs asset grouping |
| Procedural dressing | Medium-high | Medium | Style coherence |
| Forked instances | Very high | High | Mobile CPU, camera patching |
| Scanline-to-plane | Low-medium | Medium | Game may not use it |
| RAM-map discovery | Very high | Medium | Derived values |
| Texture-upload animation | High | Medium | Palette or matrix tricks |
| Translucency with DS rules | High | Medium | OIT approximation |
| Recovered lights | High | Low-medium | False positives |
| Texgen detection | Medium-high | Low | Ambiguous modes |
| Decal ordering | Medium (but visible) | Low-medium | Precision quirks |
| Tilemap flattening | High for 2D | Medium | Palette animation |
| Curve reconstruction | Medium-high | Medium | Game-side blending |
| Imposter replacement | Medium-high | Medium | Needs 3D models |
| Sprite-limit inversion | Medium | Low | Deliberate hiding |
| Reversed-Z | Medium | Low (if early) | Convention changes |
| Retroactive capture | Medium (tooling) | Low-medium | Memory on mobile |

#### 5.10 Cheap experiments

1. ✅ Log the ARM9 program counter on every GX command write and group draws by call site. Check whether categories fall out cleanly. *(Inspector. In Joker they don't: 779 of 790 polygons go through one SDK routine that sends queued display lists; the display list is the identity.)*
2. 🟡 Dump the matrix push/pop trace for one monster and check that a skeleton tree can be rebuilt. *(The trace is in the inspector report; the skeleton rebuild is not done.)*
3. 🟡 Color every draw by polygon ID and look at what the game uses it for. *(The view exists; Joker's harbour scene uses 6 IDs, what each means is not analysed yet.)*
4. 🟡 Compare the palette index histograms of two textures from the same monster to see whether indices are consistent. *(Histograms are in the report. In Joker's scene the 256-colour textures use nearly every index, as colour ramps rather than materials; comparing one monster's textures is not done.)*
5. ✅ Interpolate one scene using captured matrix motion and look for artifacts at turns and cuts. *(Frame generation: polygons paired from frame to frame, cuts detected; see the enhancement table in `DEVELOPMENT_NOTE.md`.)*
6. Fork a savestate, move the game's camera by patching its RAM struct, and see whether geometry for the new angle appears.
7. Check whether DQMJ uses per-scanline affine changes or display capture at all, and where.

#### 5.11 Honest limits of these ideas

- Display-list fingerprinting and call-site attribution depend on how each game issues draws. Check the target game first. Some games build vertex data on the CPU every frame, which defeats hashing.
- Scanline inversion works only when the effect follows a clean formula.
- Interpolation can reveal animation cuts (teleports, scene changes). Detect those and disable blending for that frame.
- Everything that hooks game code is version- and region-specific, so key it by ROM hash.
- Each idea here adds capture and bookkeeping cost in the emulator, which matters on mobile (section 18). Gate them behind settings.

#### 5.12 Suggested build order

1. ✅ Logging and quick looks (days, not weeks): call-site and display-list logging with an overlay, polygon-ID color view, palette-index histograms, one matrix push/pop trace. They show how each game really draws and make the classifier and manifest much stronger. *(The inspector: in-game menu > Inspector, colour views and a saved report; also the caller (LR) of each site and the cartridge files read.)*
2. ✅ Texgen detection and decal ordering. Cheap, and they prevent visible errors. *(Texgen modes and scrolling textures in the report. Decals: OpenGL drew 0 of the DS's 770 decal pixels in a test scene; fixed, none missing now.)*
3. RAM-map discovery, because it automates the largest manual task.
4. Persistent object identity and previous-frame matrices (also needed by TAA, 11.3).
5. ✅ Transform interpolation and frame generation for 30 to 60+ Hz. *(Frame generation, 120 fps, OpenGL renderer.)*
6. 🟡 Parity oracle, so every later change can be checked. *(Desktop tests compare OpenGL against the software renderer; not yet a tool inside the app.)*
7. Material registers, polygon IDs and palette-index segmentation feeding the classifier (section 7).
8. Translucency rules and reversed-Z, before more rendering features depend on the old assumptions.
9. Display-capture interception for the first effect found in the game, LOD and fog inversion, procedural dressing.
10. Skeleton recovery, then retargeting (6.4); texture-upload animation and curve reconstruction for characters.
11. Recovered lights and imposter replacement for scenes.
12. Tilemap flattening and sprite-limit inversion for the 2D side. Scanline inversion only if the cheap experiments show the game needs it.
13. Dual-screen redesign, starting with the bottom screen as a data source.
14. Forked instances and scene memory with a scripted map crawler, once the world scene (section 4) works for one map.
15. Retroactive capture whenever you want better tooling.


### 6. Live remake: asset identification, replacement and game state

The DS game only knows its own low-res assets and logic, so the engine must **understand what the game is showing** and substitute something better.

#### 6.1 Three levels

1. **Enhancement layer.** Same assets, better rendering: higher internal resolution, filtering, generated normal/PBR data, dynamic lighting and shadows, plus the physics systems above. Cheapest tier.
2. **Asset replacement.** Monsters, maps and effects swapped at runtime for high-poly models, new textures and new rigs. The game logic keeps running while you draw something else.
3. **Semantic remake.** The DS game is only a logic back end. You read its state (map ID, party, enemy species, battle phase, camera) and render a scene built by your own engine.

4. **Native engine.** The DS game is only an importer and a reference. The engine owns the rules, world, camera and content tools, and can add things the DS game never had (section 7C).

Most projects land at tier 2, with tier 3 for parts that matter (for example battles). Tier 4 is where the project becomes its own engine instead of a layer on an emulator.

#### 6.2 Knowing what is on screen

Texture hashing works but is fragile. Better: **hook asset loading.**

- DS games load assets from the cartridge filesystem (NitroFS). Nintendo formats are common: `.nsbmd` (models), `.nsbtx` (textures), `.nsbca` (animations). Verify against the actual ROM.
- Hook the file-read path in the emulator and record which file ID/name lands at which RAM address.
- When a draw call later uses geometry from that region, you know its asset exactly (for example "monster 047 model").
- The manifest then keys on asset ID rather than texture hash and survives variants and texture reuse.

#### 6.3 Reading game state

For tier 3 you need a **RAM map**: party slots, monster IDs, HP, battle state machine, current map, player position.

- Find them with a memory searcher, RAM diffing between actions, or by following the game's own pointers.
- Expose them through a small struct that the renderer reads every frame.
- With species ID and animation state you can place a replacement model and drive its animation from the original's.

#### 6.4 Replacement manifest example

```json
{
  "monster_047": {
    "source": { "nsbmd": "mons/047.nsbmd" },
    "replace": { "model": "hd/slime_knight.glb", "rig_map": "hd/slime_knight.rigmap" },
    "anim_map": { "idle": "Idle01", "attack": "Atk01", "hurt": "Dmg01", "faint": "Die01" },
    "sim": { "type": "hair", "bones": ["plume"], "stiffness": 0.6 },
    "material": "pbr_slime"
  }
}
```

The `anim_map` is the key part: the DS game says "play animation 3 at frame 12" and the engine maps that to the matching animation on the HD rig. It is laborious per monster, but it keeps gameplay timing intact.

#### 6.5 The 2D side

Much of a DS game is 2D (tile layers, OAM sprites, often both screens).

- Capture each layer separately instead of the final composite, then upscale or replace.
- Redrawing the bottom-screen UI as vector or high-res UI is the biggest quality jump for the least work.
- Text can be re-rendered with a real font if you hook the font or tilemap writes.

#### 6.6 Dragon Quest Monsters: Joker specifically

- Battles use a mostly fixed presentation with one model per monster, so **tier 3 for battles is practical**.
- Overworld areas are more varied, so start with tier 1 or 2 there.
- Start with the ~200 monster models and battle backgrounds, since that is where players look most.

#### 6.7 Legal note

Keep it a **replacement layer** that loads from the user's own ROM and a separate asset pack. Do not ship Nintendo or Square Enix data inside the engine.

### 7. Detecting and upgrading asset types without AI

Goal: the engine recognizes what an asset is (wood, water, sun, face, grass, stone, metal...) and applies a matching upgrade profile. No machine learning: only rules, metadata, render state, image statistics and a manual override file.

#### 7.1 Layered classifier (strongest evidence first)

1. **Manual manifest by asset ID or texture hash.** Highest confidence, always wins. Use it for important things (main characters, key monsters, hero props).
2. **File and model metadata.** NSBMD/NSBTX contain model, material and texture names, and many are descriptive (`water`, `tree`, `kusa`, or romaji). The NitroFS path and file name help too. Cheap and often very accurate: match with a keyword table (`water|sea|umi|mizu`, `wood|ki|tree`, `grass|kusa`, `sun|taiyo`, `face|kao`, ...).
3. **DS render state.** Each polygon already tells you a lot:
   - Translucent polygon (alpha 1 to 30) plus a flat, upward-facing mesh points to **water or glass**.
   - Texture matrix changing every frame (UV scrolling) points to **water, lava, waterfalls, conveyors**.
   - No lighting, always facing the camera, drawn without depth write points to **sun, particles, glows**.
   - Huge sphere or dome drawn first with depth write off points to **sky**.
   - Skinned mesh (matrix palette per bone) points to **character or monster**.
   - Alpha-tested cutout texture (1-bit alpha or A5I3), repeated over a ground plane points to **foliage or grass**.
   - Polygon facing flags, wireframe and fog flags can separate special cases.
4. **Texture statistics (computed once per texture hash and cached).** Plain image analysis, no learning:
   - Dominant hue and saturation (green points to vegetation, blue-green low contrast to water, brown to wood or earth).
   - Directional gradient energy (strong single-direction grain points to wood, isotropic noise to stone or dirt).
   - High-frequency energy and entropy (grass and foliage are busy, metal and plastic are smooth).
   - Tileability (edges match, so it is a surface material rather than a unique painted object).
   - Palette size (few colors points to flat toon-shaded or UI-like art).
5. **Geometry hints.** Bounding box shape and orientation (thin horizontal quad is a water or floor plane), vertex count, normal distribution (all normals up is ground or water), position relative to the camera or world (above everything is sky).
6. **Fusion.** Each layer casts weighted votes for a class. Sum them per class, pick the best, and keep a confidence score. Below a threshold the class stays `unknown` and gets the safe generic enhancement.

Priority and weights (starting point, tune per game): manifest = override, metadata = 0.8, render state = 0.6, texture stats = 0.3, geometry hints = 0.2.

#### 7.2 The record each asset gets

```cpp
struct MaterialRecord {
    uint64_t    assetKey;        // asset ID or texture hash
    MatClass    cls;             // Wood, Water, Sun, Face, Grass, Stone, Metal, Fabric, Skin, Foliage, Sky, Fire, Glass, Unknown
    float       confidence;      // 0..1
    Source      source;          // Manifest | Metadata | RenderState | TextureStats | Geometry
    UpgradeProfile profile;      // resolved from cls + overrides
    float       params[8];       // roughness, bump strength, wind weight, etc.
};
```

Resolve once per asset (not per frame) and re-resolve only when the key changes.

#### 7.3 Class to upgrade profile

| Class | Upgrade |
|---|---|
| Wood | Filtered albedo, grain-derived normal map, roughness ~0.6, parallax |
| Stone / brick | Height-from-luminance bump, POM, AO in cracks |
| Metal | Low roughness, environment reflections (cubemap or RT) |
| Water | Animated normal maps, reflection and refraction, depth fog, foam at intersections, optional wave sim (cloth can float on it) |
| Grass / foliage | Instanced blades or shells, wind sway, trample map, translucency |
| Sun | Emissive, bloom, god rays, and use it as the scene's main light |
| Sky | Procedural atmospheric scattering or HDR sky, clouds as a volume |
| Face | Subsurface scattering, eye specular and cornea, soft shadows, very low bump strength, optional blendshapes |
| Hair | Strand sim and Kajiya-Kay (section 17.4) |
| Fire / lava | Emissive, volumetric flicker, light flicker |
| Cloth | XPBD sim (section 14.4), fabric BRDF (sheen) |
| Unknown | Safe generic enhancement only (resolution, filtering, lighting) |

#### 7.4 DS-specific wins

- **Sun and lights.** The DS geometry engine already has up to four light vectors and colors (`LIGHT_VECTOR`, `LIGHT_COLOR`). Capture them and use them as the real light direction for shadows and RT, instead of guessing from the sun object.
- **Fog and toon table.** Capture fog color, density and the toon table so the upgraded look keeps the game's art direction.
- **Texture format.** Format (paletted, A3I5, A5I3, direct color) and the transparency flag are free hints from the texture parameters.

#### 7.5 Generating upgraded textures without AI (offline tooling)

- **Upscaling:** classic filters, such as nearest-neighbor 2x/4x for pixel-art look, xBRZ or hqx for edge-preserving, Lanczos or bicubic for soft surfaces. Choose per class.
- **De-light the albedo** before deriving a normal map. DS textures have painted shading, and a normal map computed straight from luminance would carve the painted shadow into fake geometry. Simple approach: subtract a heavily blurred luminance copy (high-pass) to keep detail and drop baked shadow.
- **Normal / height from luminance:** Sobel filter with a per-class strength (high for wood and stone, near zero for faces).
- **Roughness and metallic:** class constants, modulated by local contrast.
- **Palette tricks:** because DS textures are often paletted, replacing or tinting palette entries is a cheap way to make variants (wet, burnt, frozen).

#### 7.6 Workflow loop

1. A batch tool scans the whole ROM and writes `manifest.generated.json` with class, confidence and source.
2. An in-engine **debug view** colors every draw by class and confidence, and shows the asset ID on hover or with an overlay.
3. A hotkey reassigns a class, which writes to `manifest.overrides.json`. The override file always wins and is never overwritten by the generator.
4. The same overrides can be shared as a community asset pack for a given game.

#### 7.7 Pitfalls

- Faces are the hardest to detect at DS resolution. Prefer the manifest and structural signals (skinned mesh with a head bone, morph data, model name) over image statistics.
- Water by render state (translucent, flat, scrolling UVs) is far more reliable than by color.
- Do not classify per frame. Resolve once per asset.
- Keep `unknown` a valid, safe result so a wrong guess never ruins a texture.
- Test on several games before trusting the weights, since each studio names and builds assets differently.

#### 7.8 Suggested build order

1. Override manifest and debug overlay (so you can see and fix everything by hand).
2. Metadata keyword rules.
3. Render-state rule engine.
4. Texture statistics and geometry hints.
5. Fusion and confidence scoring, then the batch scanner tool.
6. Per-class upgrade profiles, starting with water, sun/light and grass because they are the most visible.

### 7A. Entity rework pipeline: from flat asset to reworked entity

Goal: take one DS asset in and produce a **reworked entity** out: classified, split into parts, mapped, given volume, textured and given physics. The engine does this automatically from its own signals. The central example is **a flat painted cloth (cape, robe, banner, flag) becoming real 3D cloth**: the DS draws a few polygons with folds painted into the texture, and the entity pipeline turns it into a thick, lit, simulated fabric that keeps the painted design.

No machine learning. "Generative" here means **procedural and deterministic**: every random choice (fold noise, weave offset, strand jitter) is seeded from `hash(assetKey, templateId, pipelineVersion)`, so the same asset always gives the same result, on every device, in every run.

Pieces that already exist and are connected here: classifier (7.1), skeleton recovery (5.2), polygon IDs and palette-index masks (5.3), smooth normals (11.4), shells and cloth (14), sprite extrusion (15.2), procedural materials (16.1), spring bones and cloth sims (17), parity oracle (5.6).

#### 7A.1 Where it sits in the tiers

Section 6.1 has three levels: enhancement, asset replacement, semantic remake. The entity rework is a **tier 1.5**: an automatic upgrade of the game's own assets that adds geometry, materials and motion, with no hand-made HD model. It suits the long tail of monsters, props and cloth you will not replace by hand. Anything important still goes to tier 2 through the manifest, and the manifest always wins.

#### 7A.2 The pipeline

| Stage | Signals it uses | Output | Fallback if it fails |
|---|---|---|---|
| 1. Classify | Section 7.1 layers plus call-site tag (5.2) | Class, family template, confidence | `unknown`: generic enhancement only |
| 2. Destructure | Recovered skeleton (5.2), polygon IDs and palette-index regions (5.3), mesh connectivity, UV islands, OAM pieces for sprites (5.3) | List of parts, named by rule (leaf bone chain becomes tail, ear or plume; mirrored pair becomes limbs or wings; wide low-poly sheet hanging from a bone becomes cloth) | One part for the whole asset |
| 3. Map | Original UVs, part masks | Clean second UV set per part (rectangular for sheets), plus masks: anchor, hem, trim, free | Keep the original UVs |
| 4. Volume | Smooth normals (11.4), shells (14.1), sprite extrusion (15.2), fold height from de-lit luminance | Resampled simulation mesh, thickness, fold relief, SDF per entity | Flat mesh with smoothed normals |
| 5. Texture | De-light, palette masks (5.3), procedural fit (16.1) | Per-part material parameters and maps | Plain upscale |
| 6. Physics | Part type plus skeleton | Sim rules per part (see 7A.6) | No sim |
| 7. Verify | Parity oracle (5.6) | Silhouette and pose comparison against the original, plus sim sanity checks, with a score per stage | Drop the stage that scored badly |

Each stage is **independent**: a bad result in one never blocks the others, because each has a fallback and stage 7 can switch a stage off. Section 7E turns the verify stage into a constraint (the remade asset must round-trip) that the other stages search under, and replaces hand-tuned strengths with per-region bounds.

#### 7A.3 The entity record

An extension of `MaterialRecord` (7.2). Each stage writes into it, later stages read earlier results: physics needs the parts, volume needs the masks, texture needs the classes.

```cpp
struct Part {
    uint32_t   id;
    PartKind   kind;          // Body, Limb, Tail, Ear, Plume, Wing, Sheet, Hair, Rigid, Unknown
    uint32_t   boneChain[8];  // skeleton nodes (5.2); empty for sprite or mesh-only parts
    uint32_t   meshRange[2];  // triangle range or polygon-ID set
    MaskSet    masks;         // anchor, hem, trim, free (per vertex weights 0..1)
    UvSet      uv2;           // clean UVs from stage 3
    float      confidence;    // naming confidence for this part
    Source     source;        // Manifest | Skeleton | PolyId | Palette | Connectivity
};

struct EntityRecord {
    MaterialRecord base;      // 7.2: assetKey, class, confidence, profile
    uint64_t       seed;      // hash(assetKey, templateId, pipelineVersion)
    TemplateId     templ;     // family template (7A.4)
    Part           parts[16];
    uint32_t       partCount;
    VolumeData     volume;    // thickness, shell count, SDF handle, fold height map
    PartMaterial   mats[16];  // one per part
    SimRule        sims[16];  // one per part, kind + parameters
    StageScore     score[7];  // verify results; score below threshold means stage disabled
    uint32_t       disabledMask;
};
```

Section 17A adds a `MechMaterial` per part, an `IntegrityGraph` and fracture sets to this record.

Resolve once per asset in the scanner or pack build (not per frame). The result ships as data next to `manifest.generated.json`. Re-resolve only when the asset key, template or pipeline version changes.

#### 7A.4 Family templates (what makes it adaptable)

A small data file per family says which stages run and with which rules. Adding a family means adding data, not code.

```json
{
  "cloth_hanging": {
    "match": { "class": "Cloth", "min_confidence": 0.5 },
    "destructure": { "mode": "sheet", "anchor": "parent_bone_edge", "min_width_ratio": 1.5 },
    "map":      { "uv": "rectify", "grid": [16, 24] },
    "volume":   { "thickness": 0.004, "double_sided": true, "fold_from_luminance": 0.35, "weave_normal": true },
    "texture":  { "delight": true, "keep_painted": true, "trim_by_palette": true, "brdf": "sheen" },
    "physics":  { "type": "xpbd_cloth", "pin": "anchor", "collide": "skeleton_capsules", "wind_weight": 0.8,
                  "stretch": 0.98, "bend": 0.15, "damping": 0.02 },
    "verify":   { "silhouette_iou_min": 0.88, "max_stretch": 1.15 }
  },
  "slime_body": {
    "match": { "class": "Skin", "tags": ["jelly"] },
    "destructure": { "mode": "single" },
    "volume":   { "shells": 3, "sdf": true },
    "physics":  { "type": "soft_body", "stiffness": 0.5 }
  },
  "bird": {
    "match": { "tags": ["bird"] },
    "destructure": { "mode": "skeleton", "mirrored_pairs": "wings", "leaf_chains": "tail" },
    "physics":  { "wings": "xpbd_cloth", "tail": "spring_chain" }
  }
}
```

A slime gets a soft body and no hair. A bird gets wings as cloth and a feather tail as chains. Per-asset overrides in the manifest (same keys, any depth) always win over the template.

#### 7A.5 Worked example: flat painted cloth to 3D cloth

Starting point: a cape drawn as a handful of polygons, one texture with painted folds, shadow bands and a trim color.

1. **Classify.** Name keywords (`cape`, `mant`, `cloth`, `hata`), skinned to a back bone, translucent-free, texture with low high-frequency energy and a distinct trim palette index. Class `Cloth`, template `cloth_hanging`.
2. **Destructure.** The cape polygons form one connected sheet, wider than tall, with one edge near a parent bone (shoulders). Name it `Sheet`. Palette-index regions give the trim (hem, border) and the body. If the cape shares a mesh with the character, polygon IDs (5.3) or connectivity split it off.
3. **Map.** Find the sheet's boundary loop in the original UV island, **rectify** it into a clean `[0,1]²` second UV set (u across the width, v down the drop), and write masks: `anchor` (v near 0), `hem` (v near 1), `trim` (palette index). The original UVs stay for sampling the painted texture, so the design is untouched.
4. **Volume.**
   - Resample the sheet onto a regular grid (for example 16 by 24 vertices) so the cloth sim has well-shaped constraints, whatever the DS mesh looked like.
   - Make it **two-sided with thickness**: offset front and back shells along smoothed normals (11.4), with a rounded hem.
   - **Fold relief.** De-light the albedo (7.5), take the removed low-frequency luminance as a height map, and scale it by `fold_from_luminance`. This turns painted shadow bands into low real relief, so the folds also shade correctly under the new lights. Add a seeded weave normal on top.
5. **Texture.** Keep the painted design as the base color (de-lit), use the palette masks for per-region parameters (trim glossier, body matte), fabric sheen BRDF. For plain tileable cloth, 16.1 can replace the texture with a fitted procedural weave, keeping the blend slider.
6. **Physics.** XPBD cloth (14.4) pinned along the `anchor` mask to the parent bone, colliding with capsules built from the recovered skeleton, driven by the wind field (17.2) and the parent's velocity (17.1). The painted folds become the **rest shape**, so the cloth settles into its original look instead of going flat.
7. **Verify.** Render the rest pose and compare the silhouette against the original (parity oracle, 5.6) with IoU. Run a short seeded sim and check stretch and penetration. If the sim fails, ship the rest-shape mesh (still 3D, no motion). If the volume fails, ship the flat mesh with the cloth sim off.

The same chain handles banners, flags, curtains, wings and robes. Only the template values change.

#### 7A.6 Part type to physics rule

| Part kind | Rule |
|---|---|
| Leaf bone chain (tail, ear, plume, antenna) | Spring bones or hair chain (17.5) |
| Sheet (cape, banner, wing membrane) | XPBD cloth pinned at the anchor edge (14.4) |
| Jelly body | Soft body: shell layers plus volume-preserving spring lattice, or SDF-driven squash |
| Hair or fur region | Guide strands with interpolation (17.4), or shells (14.1) |
| Rigid or unknown | No sim; keep the original animation |

These rules cover one part at a time. How parts and entities affect each other, and how an entity breaks, splits or merges, is in section 17A.

#### 7A.7 Where it runs

Stages 1 to 5 run **offline** in the scanner or pack build and ship as data (resampled meshes, second UV sets, masks, fold maps, SDF volumes, parameters). On the phone only the sims and rendering run, so it fits the mobile budget (18). Sim density follows the quality governor (18.5), and entities far from the camera drop to rest shape.

#### 7A.8 Honest limits

- Part naming from skeleton shape will be wrong on odd monsters. The manifest override is the safety net.
- Some assets have no recoverable skeleton. Destructuring then falls back to polygon IDs, palette regions or connectivity, which is coarser.
- Fold relief from painted shading is a guess: a painted shadow is not always a fold. Keep the strength low and per-class, and let the verify stage and the A/B view (11.10) catch bad cases.
- Resampling a sheet assumes a roughly developable surface. Cloth with holes, ragged edges or tubes (sleeves, skirts) needs a different template or a manual mask.
- The quality ceiling is below a hand-made replacement.
- Volume and mapping are the hardest stages. Physics from parts is the quickest visible win.

#### 7A.9 Pitfalls

- Do not run the pipeline per frame or per session: resolve once, ship as data, keep it deterministic.
- Keep the seed independent of load order and device, or results will differ between runs.
- Never let a stage overwrite the manifest overrides, and never let the generator write into `manifest.overrides.json` (7.6).
- Anchor edges that are chosen wrongly make the cloth hang from the wrong side. Show anchor and part masks in the debug overlay (7.6).
- Keep the original UVs and mesh available so any stage can be dropped without losing the base look.
- Sim parts must not collide with the whole character mesh (expensive). Use simple capsules or an SDF from the skeleton.

#### 7A.10 Suggested build order

1. Entity record, template loader and debug overlay (part colors, anchors, scores).
2. **First prototype:** one monster with a recoverable skeleton. Run classify, destructure and physics only, and check that spring bones on its leaf chains look right. This tests the core assumption (parts from structure) before any volume work.
3. Cloth sheet case: sheet detection, anchor mask, XPBD on the original mesh with no resampling.
4. Mapping: rectified second UV set and grid resampling.
5. Volume: thickness, double-sided shell, fold relief from de-lit luminance.
6. Texture: palette masks and per-part materials, then optional procedural fit (16.1).
7. Verify stage with silhouette IoU and per-stage disable.
8. More templates (slime, bird, plume) and the batch scanner that writes entity data for the whole ROM.

### 7B. The remake compiler: the whole game as a build pipeline

Section 7A reworks one asset. This section lifts the same pattern to the whole game: the engine behaves like a **compiler**. It takes the original game as source and produces a **remake pack** as output. A runtime then plays the pack, with the original game still running underneath as the source of truth.

#### 7B.1 The oracle and the semantic scene

- The emulator keeps running the original logic. It is the **oracle** for game state: it decides what happens and when. It is a scaffold, not a permanent core: subsystems move out of it over time, all the way to rules, world and content (see 7B.6 and section 7C).
- The engine never draws the oracle's pixels in the final image. It draws a **semantic scene**: entities, parts, events and layout, each resolved to remake-pack data.
- When the pack has nothing for something, the engine falls back to the Scene IR of the original draw (section 3). So the remake can be partial, and it always works.

This is the same split as tier 3 in 6.1, but applied everywhere, with the fallback built in.

#### 7B.2 Phases

| Phase | What it does | Runs | Main sections |
|---|---|---|---|
| Recover | Capture assets, structure, state and render-state semantics | Offline scan plus live hooks | 5, 6 |
| Understand | Classify, destructure, name parts, extract events | Offline | 7, 7A |
| Rework | Volume, materials, physics, layouts, animation, audio, all seeded and deterministic | Offline | 7A, 11, 14 to 17 |
| Verify | Compare against the original with the parity oracle, score every result | Offline and in CI | 5.6 |
| Run | Draw the semantic scene, run sims, stay in sync with the oracle | Live on the phone | 17, 18 |

The output is plain data that can be diffed, shared and overridden. It contains no original game data (6.7).

#### 7B.3 What generalizes beyond entities

1. **Scenes are entities too.** A map has parts (ground, walls, props, water, sky). The 7A chain applies, with tile-grid terrain (16.3) and tile-to-prop conversion (15.2) as the volume stage. One data model for everything.
2. **Events, not just state.** Diffing the state reader (6.3) each frame produces **semantic events**: "monster A took 34 damage", "door opened", "menu cursor moved". Events drive camera cuts, particles, sound and animation blending. This is what makes tier 3 battles feel authored.
3. **Animation rework.** DS animation is low-rate keyframes. Re-interpolate with splines, add foot IK using blob-shadow ground contacts (5.3), and layer spring bones on top. The original timing stays authoritative so gameplay does not drift (6.4 `anim_map`).
4. **UI as recovered widgets.** Windows are usually tilemap frames. Detect 9-slice patterns, rebuild a widget tree, and render it as vector UI with a real font (6.5, 16.2).
5. **Audio.** Re-render the game's own sequence data with better instruments, and take reverb from the world scene (16.6).

#### 7B.4 The remake pack

A pack is a folder of content-addressed files plus one index. It never contains original game data.

```json
{
  "pack": { "game": "dqmj", "rom_hash": "sha1:...", "pipeline": "0.7.2", "faithfulness": 0.5 },
  "entities": {
    "monster_047": {
      "record": "entities/monster_047.rec",
      "template": "slime_body",
      "inputs_hash": "b3:91ac...",
      "score": { "silhouette_iou": 0.93, "sim_sanity": 1.0, "overall": 0.94 },
      "fallback": "original"
    }
  },
  "scenes":  { "map_012": { "record": "scenes/map_012.rec", "score": { "overall": 0.81 } } },
  "events":  { "battle": "events/battle.rules.json" },
  "ui":      { "widgets": "ui/widgets.json" },
  "overrides": "manifest.overrides.json"
}
```

Rules:
- Every output carries `inputs_hash`, a hash of its inputs (asset key, template, overrides, pipeline version). The compiler skips anything whose hash did not change, so a rebuild after one fix takes seconds.
- The ROM hash is stored so a pack is only loaded for the game it was built from.
- `overrides` is never written by the compiler (7.6).

#### 7B.5 Ideas that multiply the effect

- **Skeleton families.** Many monsters share a skeleton topology. Cluster assets by topology (tree shape plus bone count and proportions), label parts once per cluster, and the names propagate. One manual correction fixes dozens of assets. The same families can carry a detailed template fitted to each member (section 7E.6).
- **Palette variants for free.** Recolored monsters share meshes and differ in palette. Rework the base once, then apply the palette swap as a material parameter (7.5).
- **Play-trace heat map.** The emulator is deterministic, so a recorded input script can replay a whole playthrough and measure on-screen area, time and closeness per asset. That ranks every asset by how much players see it, which says where manual effort pays and where the automatic pipeline is enough. It also feeds the budget-aware compile (7B.6).
- **Trust score per entity.** Every reworked entity carries its verify score. At runtime the engine can fall back to the original draw if a live check fails (stretch, penetration, silhouette drift), so a bad result degrades gracefully instead of breaking the scene.
- **Faithfulness slider.** A global and per-class control from "faithful" (original look, cleaned up) to "reimagined" (full PBR, relief, new lighting). It reads the captured toon table and fog (7.4) so the art direction survives at the faithful end.
- **Regression by replay.** Run the same input script through the original renderer and the remake, and compare silhouettes and positions frame by frame. Every pipeline change gets a score, like a test suite for visual quality.

#### 7B.6 Going further

**Pass contracts and a dependency graph.** Treat every stage as a pass with a declared contract: `reads`, `writes`, `version`, `deterministic: true`. The compiler builds a dependency graph from the contracts and runs only the passes whose inputs changed. A pass that breaks its contract (reads something undeclared, gives a different result for the same seed) fails the build. This keeps a large pack maintainable and makes "why did this entity change?" answerable by following the graph.

**Graceful degradation ladder.** Each entity ships several rework levels, and the runtime picks one per frame from distance, importance and the quality governor (18.5):

| Level | Content | Cost |
|---|---|---|
| 0 | Original draw through the Scene IR | Lowest |
| 1 | Cleaned upscale, smooth normals | Low |
| 2 | Materials and volume, static pose | Medium |
| 3 | Plus physics (spring bones, cloth) | High |
| 4 | Plus shells, SDF contact, hero lighting | Highest |

The same ladder is the fallback path when verify fails, so failure and load shedding use one mechanism.

**Freeing the game from its own camera and layout.** The oracle is authoritative about game *state* (who is where, who has how much HP, what happens next). It is **not** authoritative about *presentation*: camera, layout, picture and the screen-space tricks the DS used. The compiler's job is to find every place where logic quietly depends on presentation and **lift** that dependency out, so the camera, the world and the picture become the engine's own. Nothing is frozen to keep it "safe". A dependency that is not yet lifted is a work item with a temporary fallback, not a restriction.

1. **Find the dependencies by perturbation.** The emulator is deterministic, so this can be measured instead of guessed. Fork headless instances (5.6), patch the camera (or hide a layer, or change the culling), replay the same input script, and diff the game state. If the logic state stays identical, the map does not depend on the camera. If it diverges, the diff names the field and the frame. The result is a **camera dependence map** per map, with one entry per dependency:

   | Dependency | How it shows up | Lift |
   |---|---|---|
   | Input direction | Stick or d-pad is read relative to the game's camera | Remap stick to the 8-way directions the game expects, relative to the free camera (8.3), or inject analog values into the movement code |
   | Culling | Objects outside the game's view are not updated or not submitted | Patch the culling (8.2), or use ghost cameras and scene memory (5.6) so the world scene holds everything |
   | Screen-space triggers | Events fire when something is at a screen position or inside the view | Convert the trigger to a world-space volume at the position it had under the original camera |
   | Touch and picking | Touch input on the bottom screen or on a 2D layer maps to game objects | Ray-pick against the world scene, then translate the hit to the coordinates the game expects |
   | Scripted camera | Cutscenes and set pieces move the camera on rails | Read the rail as animation data, then let the engine author the shot (event grammar below) |
   | Hidden geometry | The game never drew the back side or the area outside the view | Recover it (8.2, ghost cameras), generate it through the entity and scene chains, or mark the area as a fade-out |

2. **Replace the oracle's logic, one subsystem at a time (full plan in section 7C).** The long-term goal is not a better picture on top of the DS game. It is a native engine that uses the DS game's data and rules. Move subsystems out of the oracle with a strangler approach: camera first, then movement and collision, then battle presentation, then UI, then rules. Each ported subsystem runs in **shadow mode** first (engine and oracle both compute the result, differences are logged), and only becomes authoritative when replay shows they agree or the engine is deliberately better. When enough is ported, the oracle becomes optional for that map or battle.
3. **Collision in true 3D.** The DS often uses tile or plane collision. Start from the oracle's collision as reference, then build continuous 3D collision from the world scene (slopes, real heights, proper contact with reworked props and cloth). Keep the oracle's result for game rules that depend on it, until the engine's version is verified in shadow mode.
4. **Geometry is free.** Added thickness, folds, new props, extended terrain, wider fields of view, different aspect ratios and new camera angles are all allowed. The silhouette comparison with the original is a **parity test at the original camera only** (a regression check, 5.6). It does not limit what the free camera may show. From other angles the check is for artifacts: holes, stretching, popping and penetration.
5. **Camera authorship.** Once the camera is the engine's, cutscenes and battles can get real cinematography: shot lists driven by events, depth of field, dolly moves and look-at targets from the recovered skeleton (attachment points, eyes). Keep a "classic camera" mode that reproduces the original angle exactly, for A/B and parity.

The per-entity and per-map **trust score** (7B.5) still applies. It decides whether the *rework* is shown, not whether the camera may move.

**Scene-level rework in more detail.** Maps differ from monsters because they have unique structure, so the scene chain uses coarser parts first:

1. Split the map by call site (5.2) and polygon ID (5.3) into ground, walls, props, water, sky and foliage.
2. Ground and walls go to smooth terrain (16.3) and relief (14.1). Props go through the entity chain one by one, with repeats instanced. Foliage and water use the class profiles in 7.3.
3. The world scene (4) and the free camera (8) then see one coherent space.
4. Verify against the original frame with the fixed camera. A freely moved camera is checked only for artifacts (holes, stretch), since there is no ground truth.

**Event grammar.** Events are described as small rules over state diffs, as data, not code:

```json
{
  "damage": { "when": "hp[any] decreases", "emit": { "type": "hit", "target": "$id", "amount": "$delta" } },
  "faint":  { "when": "hp[any] == 0",      "emit": { "type": "faint", "target": "$id" } },
  "cursor": { "when": "menu.index changes", "emit": { "type": "ui_move" } }
}
```

Camera rigs, particles, sound and animation blends subscribe to events. The physics layer (section 17A) also emits events such as `Fracture`, `Detach`, `Split` and `Spawn`, and the rules or the camera can subscribe to them. A new game needs a new rules file, not new engine code, which is also what makes the GameAdapter (23.2) cheaper.

**Reproducible builds.** The whole compile is deterministic: same ROM, same overrides, same pipeline version, same pack byte for byte. The pack index stores the hash of every output. Two machines can compare packs by hash, and community packs can be verified. This also makes bug reports exact: "pack hash X, frame N" reproduces a problem.

**Override layering.** Overrides come in layers, lowest to highest: template defaults, generated data, community pack, user overrides. Higher layers win per key. Every value in the debug overlay shows which layer it came from, so fixing a wrong result starts with knowing who set it.

**Budget-aware compile.** Build effort is also limited. Using the play-trace heat map, the compiler spends its time on assets by on-screen importance: heroes get full rework and extra verify samples, rarely seen assets get level 1 or 2 only. The remaining time goes to a second pass over assets with low scores.

**Learning from corrections without AI.** When you fix a part name or class by hand, record it as a rule candidate (for example "bone chain with this shape under a skeleton of this family is a tail"). Apply the rule to the other members of the family and show the matches for confirmation. This is still plain rules and data, and it makes manual work compound.

**Remake coverage report.** After each build the compiler prints a report: how many assets and scenes are at each ladder level, which have low scores, which are overridden, and what share of play-trace screen time is covered by level 3 or better. This turns "how far along is the remake?" into a number.

#### 7B.7 Where it breaks

- Logic that depends on camera, layout or hidden geometry is the main source of work. It is found by perturbation testing and lifted dependency by dependency (7B.6). Until a dependency is lifted, that map falls back to the classic camera for that dependency only. Some lifts will need real reverse engineering of the game's code.
- Porting logic out of the oracle risks subtle rule changes (timing, rounding, RNG order). Shadow mode and replay comparison are the defense.
- Heuristic part naming and fold-from-shading will fail on strange assets. The design only works if failure is cheap: fallbacks, scores and overrides everywhere.
- Fully automatic quality stays below hand-made. The honest target is "good enough for the long tail, hand-made for the heroes".
- Scene-level rework is much harder than entity-level, because maps have far more unique structure than monsters.
- Event rules rely on a good RAM map (6.3). Without it, events are limited to what render state shows.
- A deterministic compile only holds if every pass really is deterministic. Floating-point differences between machines can break byte-for-byte equality, so pass outputs should be quantized or compared with a tolerance.

#### 7B.8 Pitfalls

- Do not let a pass read live emulator state during the compile. Capture to files first, then compile from files, or the build will not be reproducible.
- Do not let the pack grow without limit. Dedupe by content hash and share meshes across palette variants.
- Keep the fallback to the original draw working at all times. A remake with no fallback fails visibly on the first unrecognized asset.
- Keep scores honest. A verify stage that always passes is worse than none.
- Do not treat a camera dependency as a reason to give up the free camera. Record it in the dependence map and lift it.
- When porting a subsystem out of the oracle, never switch it to authoritative without shadow-mode agreement or an explicit decision that the engine's behavior is the new intended one.

#### 7B.9 Suggested build order

1. Leaf-chain physics on one monster (the 7A prototype).
2. Pack index, content hashes and the fallback to the original draw, so a pack can exist with one entity in it.
3. Skeleton clustering across one monster family, to show that labels propagate.
   Camera dependence map for one map by perturbation testing, then lift the first dependency (input direction, then culling) so the free camera works there.
4. Play-trace heat map, so priorities come from data.
5. Cloth sheet rework with verify scores and the degradation ladder.
6. Event extraction for one battle, driving camera and effects.
7. One map reworked through the scene chain.
8. Pass contracts, incremental rebuilds and the coverage report.
9. Regression by replay in CI.

### 7C. The DS as raw material: from emulation to a native engine

The emulator is not the platform. It is **base material**: a source of content, rules and reference behavior. If the project stays inside what the emulator allows, it can only ever be a better-looking emulator, and most ideas in this document (a living world, new camera, new systems, generated content) cannot happen. So the engine core must be designed as **its own engine**, with the DS game as one input.

This section widens 7B: the question is not only "who owns the camera" but "who owns everything".

#### 7C.1 Three roles of the emulator, demoted over time

| Role | What it does | Fate |
|---|---|---|
| Extractor | Reads content, tables, scripts and structure out of the ROM and memory | Stays as an importer, run once per ROM |
| Oracle | Gives the reference answer for verification (7B.1, 5.6) | Stays as a test tool, not in the shipped runtime |
| Runtime | Runs the game logic every frame | Replaced subsystem by subsystem until it is optional |

The engine core never includes emulator code. The emulator sits behind a narrow interface (`SourceRuntime`), the same way a game sits behind a `GameAdapter` (23.2).

#### 7C.2 What a DS game is made of, and what happens to each part

| Layer | Examples | What the engine does with it |
|---|---|---|
| Content | Models, textures, animations, maps, text, music, sound | Convert to open formats once, then rework (7A) |
| Rules | Stats, damage formulas, move effects, AI, encounter tables, RNG, battle state machine | Lift into data plus engine code (7C.4) |
| Behavior code | Movement, collision, camera, menus, scripting glue | Reimplement natively, verified against the oracle |
| Hardware glue | GPU3D and 2D engines, VRAM banks, OAM, DMA, timers, 4 MB RAM, touch, sound chip, cartridge, save chip | **Deleted.** This is where the limits live (7C.5) |

The hardware layer is not "emulated better". It is removed from the design. Nothing in the engine should have a slot count, a palette limit or a polygon cap because the DS had one.

#### 7C.3 The independence ladder

Independence is per subsystem, not global. Each level is a shippable state, and each is better than the one before.

| Level | The engine owns | The emulator still does |
|---|---|---|
| N0 | Nothing: it draws captured frames | Everything |
| N1 | Rendering and camera (sections 4, 8, 11 to 16) | All logic and simulation |
| N2 | Presentation: animation, physics, effects, events (7A, 7B, 17) | All rules, movement and UI |
| N3 | World: movement, collision, map streaming (10) | Rules, battles, menus |
| N4 | UI, audio, input and save format (6.5, 16.2, 16.6) | Rules and battles |
| N5 | Rules: stats, battle state machine, AI, RNG (7C.4) | Nothing at runtime; oracle for verification only |
| N6 | Content authoring: new content, new mechanics; the ROM is only an importer | Nothing |

The coverage report from 7B.6 gets a second table: for each subsystem, its level, and the share of play-trace time it covers. "How native is the remake?" becomes a number.

#### 7C.4 Lifting rules and logic without AI

The engine can learn the game's rules from the game itself, because the emulator is fast and deterministic. Section 7D describes how to remove the library code first and how to drive the game's own functions as a control surface, which these methods run on.

- **Table scanning.** Stats, move lists, encounter and growth tables are fixed-size records in the ROM. Detect them by stride (repeating structure), check values for plausibility (ranges, monotonic level curves, valid references to other tables), and write them out as typed tables. The engine then reads tables from the pack, not from RAM.
- **Differential testing for formulas.** Fix the RNG, vary one input at a time (level, attack, defense, move power), and record the outcome. Small input spaces can be enumerated fully. Fit a closed form or keep the table, and compare against the oracle over the whole space. Damage, hit chance, experience and stat growth usually fall to this.
- **State machines from traces.** Record the battle-phase field (6.3) over many scripted battles. The transitions form a graph. Store it as data, and let the engine drive the phases.
- **Script bytecode.** Many games use a script VM for events and cutscenes. Write a disassembler and interpreter in the engine, and scripts become data that the engine runs.
- **Known library code.** Match standard SDK and library routines by byte signature, so tracing and decompilation skip noise and focus on game code.
- **Decompilation for the hard cases.** Section 18.7: static recompilation or full decompilation of the ARM9 code where black-box methods fail. Use existing community decompilation projects as reference where they exist.
- **RNG.** Reproduce the exact generator and its call order for the classic ruleset (7C.8), so replays and seeds behave identically. For the extended ruleset, use the engine's own seeded generator.
- **Verification.** Each lifted rule runs in **shadow mode** (7B.6) against the oracle: same state in, compare result out, exhaustive where the space is small, sampled where it is large. Disagreements are logged with the input script and frame, so they are reproducible.

#### 7C.5 Hardware limits: where each one lives and how it goes away

The DS and its games are raw material. No DS limit is a design goal or something to preserve. Every limit is removed as soon as the engine can remove it. The only place a DS number survives is the **classic ruleset** (7C.8), as a compatibility mode, never in the engine core or in the engine's own formats.

What decides *how* a limit goes away is **where it lives**:

- **Renderer.** The rasterizer's own caps. The engine already owns this layer, so these are removed first.
- **Submission.** What the game's code chooses to send each frame. Raising the renderer's cap does not help, because the game never sends more. These need accumulation, rework or native content.
- **Logic and state.** Limits built into game code, tables and RAM. These go away only as subsystems leave the original game (the ladder in 7C.3).

| Limit | Lives in | Renderer status | What removes it |
|---|---|---|---|
| Polygon cap (about 2000 per frame) | Renderer and submission | Renderer cap already lifted (64k and growing). The game still submits about 2000 | World scene and scene memory (4, 8.2, 5.6), rework volume and shells (7A, 14), native content (N3 and up) |
| Resolution, two fixed 256 by 192 screens | Renderer | Lifted | Layout is still tied to the 2D layers: recovered widgets and native UI (6.5, 16.2, N4) |
| Texture size, palette, color depth | Renderer and data | Lifted by filtering, HDR and upscaling | Real detail comes from rework and procedural materials (7A, 16.1) |
| Four lights | Submission | Passed: real lights placed from emissive polygons and glow sprites (5.3) | Native lights for reworked entities and scenes (N1 and up) |
| Filtering and anti-aliasing | Renderer | Lifted | Nothing more needed |
| Camera, view and culling | Submission and logic | Own view matrix | Camera dependence map and lifts (7B.6, 8.2, 5.6) |
| Sprite count (128 OAM entries) and layers | Submission and logic | Renderer can draw any number | 2D-to-3D conversion (15), native sprites and world (N3) |
| Logic tied to a 30 or 60 Hz tick | Logic | Visuals are interpolated (5.6) | Logic rate changes only when movement and presentation leave the original game (N2, N3) |
| Object slots and entity count | Logic and state | Not touched | Native entities and spawning (N3, N5); ghost forks can show more of what exists but cannot add objects |
| 4 MB RAM | Logic and state | Not touched | Engine-side world residency and streaming (10), then native state as subsystems move over (N3 and up) |
| Input (d-pad, touch only) | Logic | Stick remap (8.3) | Native input mapping and picking (N4) |
| Sound channels and sound chip | Hardware glue | Not touched | Re-rendered sequence data with many voices (16.6, N4) |
| Save chip size and format | Hardware glue | Not touched | Native structured save with import from DS saves (N4) |
| Single ARM9 thread for game logic | Logic | Engine sims already run in parallel | Parallel jobs for ported subsystems (N3 and up) |

Reading the table:

- **Renderer rows are done or nearly done.** The polygon cap is the clearest case. Your renderer already takes 64k, but the number of polygons the game submits is the real ceiling.
- **Submission rows are solved by adding geometry that the game never sent**: accumulated over time (world scene, scene memory, ghost cameras), generated by rework, or supplied by native content.
- **Logic rows are what 7C is for.** Nothing in the renderer can touch them.

#### Rules for the engine's own formats

Because the engine will outgrow every DS number, its formats must not inherit them:

- Wide, versioned types for IDs, stats and counts. No 8-bit stat fields, 16-bit slot IDs or palette-sized fields.
- Budgets (polygons, lights, entities, sprites) are **runtime settings** set by the quality governor (18.5), not format limits. 64k polygons should be a default budget, not a constant in a file layout.
- No fixed slot tables. Entities are created and destroyed freely and addressed by stable keys.
- A mapping layer converts to and from DS numbers only inside the importers and the classic ruleset.

#### 7C.6 What becomes possible

These are things the DS base could never do, built from parts that already exist in this document.

- **Generated monsters from the part grammar.** The entity record (7A.3) stores parts by kind. Monster synthesis (a feature of the DQM series) can become visual: pick parts from the two parents by rule, blend palettes, and seed the choice from `hash(parentA, parentB, seed)`. Physics rules follow the part kinds, and the verify stage (7A.2) checks that the result is sane. Result: unlimited, stable, consistent offspring that really look like both parents.
- **A living world.** Monsters wander with simple autonomy (the recovered AI state machines plus new behaviors), react to time and weather (21.2), and leave persistent traces (16.7). Hundreds of entities in the field become possible.
- **Systemic interactions.** Fire spreads through grass using the wind field (17.2), water carries floating cloth (14.3), terrain can deform, and battles can use real physics. The engine side of this (contact, field channels, reaction rules, destruction, division) is section 17A.
- **Real cinematography and presentation.** Event-driven cameras, depth of field and lighting (7B.6), on the engine's own timeline.
- **Modding and tools.** Templates (7A.4), event rules (7B.6), rule tables and the pack format (7B.4) are all data, so a mod is data. An in-engine editor can build on the debug overlay (7.6): select a part, change a template value, and see the result.
- **Several games, one engine.** Importers for different games feed the same engine, with namespaced content (23.2). Rules come as per-game rulesets.
- **New platforms.** Because the native scene format is the Scene IR (3), new backends (3DS, console, VR) are renderer work, not game work.
- **Deterministic netplay and replays.** An engine-native deterministic simulation allows exact replays and, if wanted, online play.

#### 7C.7 Architecture consequences

```
ROM (user's own)
  -> Importers (use SourceRuntime / melonDS as extractor)
  -> Content store (open formats, pack format 7B.4)
  -> Engine core: Scene IR, rules, sim, world, UI, audio
  -> Platform backends (desktop, Android, others)

Oracle (melonDS): verification and replay only, not in the shipped runtime
```

- **melonDS behind an interface.** The engine core has no emulator includes. Capture hooks become one producer of Scene IR (section 3). The engine's own simulation is another.
- **Scene IR is the native scene format**, not only a capture target.
- **Open, versioned data formats** for everything the importers produce. The pack holds converted content derived from the user's ROM. The engine ships no original data (6.7).
- **Native save format** with an importer for DS saves.
- **Data-driven rule engine.** Tables, formula graphs and state machines, executed in the engine, versioned by ruleset.
- **Fixed-point option** in the rule engine so the classic ruleset can be bit-exact with the original.
- **Sim rate decoupled** from any frame rate (2.2).

#### 7C.8 Classic and extended rulesets

Two rulesets run on the same engine:

- **Classic.** Bit-exact with the original, verified against the oracle by replay. This is the proof that the lifting is correct, and the reference for fans and speedrunners.
- **Extended.** Everything new: more entities, new mechanics, generated content, systemic interactions, rebalancing. A save belongs to one ruleset, and a clear import path leads from classic to extended.

The classic ruleset also protects the project: it is the regression suite that lets the extended one change freely.

#### 7C.9 Honest limits

- Lifting all rules of a full game is a very large job, and it is per game. Content import, presentation and world come first because they give the most visible value for the least work.
- Black-box fitting finds what it can observe. Rare branches (special moves, edge cases, glitches) need coverage-guided input scripts, and some will need decompilation.
- Faithful bugs and glitches are a decision, not a given: keep them in classic, fix or drop them in extended.
- Legal: implement rules as original engine code that reproduces observed behavior, and keep original code and data out of the shipped engine. How copyright and clean-room practices apply to a given project is a question for a lawyer, not for this document.
- Scope grows quickly once the engine is independent. Every level of the ladder must be justified by a visible gain, or the project never ships.
- Once the oracle is gone for a subsystem, only the classic ruleset and recorded replays keep it honest. Keep those.

#### 7C.10 Pitfalls

- Do not let melonDS types leak into the engine core. One leak, and the independence ladder stops at N1.
- Do not read game state from emulator RAM inside shipped systems. Read from the engine's own state, and keep RAM reads in the importers and the oracle.
- Do not port a rule without shadow-mode agreement or an explicit decision to change it.
- Do not lock content formats to DS limits (palette sizes, slot ids, 8-bit stat fields). Use wide, versioned types, and keep a mapping for classic.
- Do not freeze the project at "emulator plus effects" because it already looks good. That is a stopping point, not the goal.

#### 7C.11 Suggested build order

1. `SourceRuntime` interface, with melonDS behind it, and a rule that the engine core never includes emulator headers.
2. Importer for one content class (monster models and the stats table) into open formats in the pack.
3. Table scanner for stats, moves and growth, with the engine reading tables from the pack.
4. Damage formula by differential testing, in shadow mode for one battle.
5. Battle state machine as data, engine-driven, oracle verifying.
6. Native movement and collision on one map, then map streaming (10).
7. Native UI, audio and input, then the native save.
8. Classic ruleset complete for one game, with the oracle optional at runtime.
9. First extended feature: monster synthesis through the part grammar.
10. Mod format and in-engine editor.

### 7D. Understanding any ROM automatically: library subtraction and a game control surface

The problem: a ROM is mostly **code you do not care about**. Most commercial DS games link the official SDK, a C runtime, a sound engine and other middleware, and that library code is large, repetitive and expensive to reverse by hand. The game's own logic is a small part of the binary, buried in it. Reversing everything to make the engine "own" the game is the wrong cost model.

The solution has four parts, all automatic and without AI (rules, fingerprints, traces and set operations):

1. **Subtract** the library code, so only game code is left to study.
2. **Anchor** the remaining code to meaning through its *boundaries* (hardware, SDK calls, files, data), instead of reading it.
3. **Expose a control surface** so the engine can drive the game, call into it, intercept it and replace parts of it **without understanding its insides**.
4. **Save the result as a shareable ROM profile**, so each new game is cheaper than the last.

The guiding rule: **understand boundaries and data, not code.** The engine never needs to modify the ROM. All hooks live in the emulator.

#### 7D.1 Stage A: subtract the library code

**Free segmentation from the ROM itself**
- The ROM header and file tables give the ARM9 and ARM7 binaries and the **overlay table**. Overlays are code modules loaded on demand, usually one per feature (field, battle, menus). Log every overlay load with the game state at that moment (6.3). An overlay ID is a free semantic label for "this code belongs to this part of the game".

**Function recovery.** Find function boundaries (ARM and Thumb) from entry points, call targets and literal pools, then confirm them at runtime from the program counters the emulator actually executes (5.2). Static guesses plus observed execution gives far fewer wrong boundaries than either alone.

**Fingerprints.** For every function, compute a fingerprint over **normalized** instructions: mask out absolute addresses, branch targets and relocation-dependent literals, and keep the shape (control-flow graph, constants, referenced strings). This is the same idea as the signature systems in existing reverse-engineering tools, built to be fully local and deterministic.

**Three ways to tell library from game code**

| Method | How | Strength |
|---|---|---|
| Cross-ROM and cross-version diffing | Library code is identical (up to its link address) in unrelated games, and between regions and revisions of one game. Game code is unique. Any fingerprint that appears in many ROMs is library | Needs no reference symbols. Gets better with every ROM added |
| Signature corpus | A database of known SDK, runtime and middleware functions with names, built from ROMs that have symbols, public SDK documentation and community work | Gives names, not just "this is library" |
| Behavior | A function whose only job is to write to a known hardware register block (FIFO, DMA, sound, touch, SPI) is a driver or library wrapper | Works when fingerprints fail (different compiler flags, inlining) |

**Output.** A code map where every function is tagged `Library`, `Runtime`, `Game` or `Unknown`, with a coverage percentage. The aim is to reduce the code a human or the engine must study to a small fraction of the binary, and to make the remaining share measurable.

#### 7D.2 Stage B: anchor game code to meaning through boundaries

Instead of reading game code, observe where it touches things whose meaning is already known.

| Boundary | What is observed | What it gives |
|---|---|---|
| Hardware | Every I/O register access, attributed to its call site (5.2): geometry FIFO, 2D engine and OAM, sound, touch, buttons, RTC, save chip | A function that reaches the geometry FIFO "draws". One that reads buttons "handles input". Labels propagate up the call graph |
| SDK calls | With the library identified (7D.1), calls such as file read, thread, message queue, timer, sound and memory-block functions are logged with their arguments | A semantic trace: which file loads when (better than the raw hook in 6.2), what threads exist, what the audio system is asked to play |
| Files | The file ID and name behind each load, tied to memory with taint tracking (5.2) | Which code consumes which asset |
| Data | Tables, arrays and structs found by scanning (7C.4) and by value correlation (5.2) | Species, move, party and map data, tied to the functions that read and write them |
| Overlays | Overlay loads (7D.1) | Which module is active in which game situation |
| Time | VBlank handler, per-frame functions versus per-event functions | The main loop and its structure |

**Recover state machines and handlers automatically.** Scan memory and code for **dispatch tables**: arrays where every entry points to a function start. These are state handlers, script opcode handlers, battle command tables and menu actions. The variable used to index the table is the *state*. This turns a whole class of "figure out the game's structure" work into a mechanical scan.

**Infer struct layouts from usage.** Functions that read and write fields at fixed offsets and strides reveal arrays of structs. Field types follow from usage: used as an address means pointer, shifted or multiplied with fixed-point scaling means fixed-point, bit-tested means flags, compared to a small range means enum or counter. Group struct instances by the allocator or constructor call site. The result is a typed view of RAM (7D.3), so the engine can read `party[2].hp` instead of a raw address.

**Trace contrast.** Run two situations that differ in one thing (monster A versus monster B, menu open versus closed, hit versus miss, map X versus map Y) and take the **set difference of executed functions and touched memory**. What is unique to one side is related to the difference. This is plain set math on recorded traces, it is deterministic, and it localizes code and data without anyone reading either.

#### 7D.3 Stage C: the control surface (the puppet layer)

The point is to let the engine **do whatever it wants with the game**, even where the game's code is not understood. The control surface offers these abilities:

- **Function as RPC.** Save the emulator state, set the registers and arguments, run until the function returns, read the result, and restore. The engine can call *any* game function directly and get the answer. This reuses the game's own code as a library, headless and at emulator speed, until the engine replaces it. It is also exactly the differential testing from 7C.4, as a general tool.
- **Purity detection.** From a memory trace, a function whose reads are only its arguments and known tables, and whose only write is its return value, is **pure**. Pure functions are safe to call out of context, to memoize or tabulate, and to port later. Functions that touch the RNG or hidden globals are flagged and handled with explicit state.
- **Hook registry.** Entry and exit callbacks on any function: observe arguments, change them, replace the result, or skip the call. Hooks live in the emulator, not in the ROM. Each hook is stored by fingerprint, so it can be relocated if addresses differ in another version or region.
- **Typed memory views.** Read and write game state through the inferred struct layouts. Spawn, set stats, change map, force a battle phase: all as typed writes, with the same layouts used for reading.
- **Controlled execution.** Run until a function is called, until a field changes, until an overlay loads, or for N frames. Combine with the snapshot ring (E7, 5.6) and forked instances for rewind and branching.
- **Seam stubbing.** Replace a whole subsystem at its boundary. The function that draws monsters is stubbed and the engine draws instead. The input read is replaced by the engine's own input. The sound calls are stubbed and the engine plays its own audio from the events. File reads are served from converted assets. Because the seams are at the hardware and SDK level, **stubbing needs no understanding of the code behind them**. It also saves emulation work, since stubbed code never runs.

Seam stubbing is the independence ladder (7C.3) applied at function level: each seam taken over is one more piece of the game the engine owns.

#### 7D.4 Stage D: automated exploration at scale

To observe enough behavior, the scanner explores the game by itself:

- **Coverage-guided exploration.** The emulator is deterministic and savestates are cheap, so run scripted and random inputs, keep the ones that execute new code, and branch from them. This is the same loop used in fuzzing, aimed at coverage instead of crashes. It reaches battles, menus, shops and rare events without anyone playing.
- **Labeling by correlation.** Tie each function to the situations it appears in (overlay, map, battle phase, menu) and to the state fields it changes. Code that only runs in battle and writes the HP table is battle damage code.
- **Verification by replay.** Every label and every seam is checked by replaying scripted scenarios with and without the hook or stub, and comparing state (7B.6, shadow mode).

#### 7D.5 The ROM profile

Everything found is saved as a **ROM profile**, plain data in the pack (7B.4), keyed by ROM hash:

```json
{
  "rom": { "hash": "sha1:...", "region": "EU", "revision": 1 },
  "code_map": { "functions": 4120, "library": 3310, "runtime": 190, "game": 540, "unknown": 80 },
  "overlays": { "12": { "label": "battle", "load_addr": "0x021A0000" } },
  "seams": {
    "draw_monster":  { "fingerprint": "fp:7c1e...", "kind": "stub", "args": ["id", "pos", "anim"] },
    "read_input":    { "fingerprint": "fp:03bd...", "kind": "stub" },
    "calc_damage":   { "fingerprint": "fp:a91f...", "kind": "rpc", "pure": true, "args": ["atk", "def", "move"] }
  },
  "structs": { "Party": { "stride": 56, "fields": { "hp": { "off": 12, "type": "u16" } } } },
  "dispatch_tables": { "battle_state": { "addr": "0x020F3A40", "entries": 14 } },
  "confidence": { "calc_damage": 0.9, "draw_monster": 0.95 },
  "provenance": { "calc_damage": ["trace_contrast", "rpc_probe", "manifest_override"] }
}
```

- A profile holds **fingerprints, offsets, names and layouts only**. It contains no game code and no game data (6.7).
- Fingerprints let a profile **carry over** to other regions and revisions of the same game, and to sibling games that share the same code base (for example DQMJ 1 and 2, 23.2).
- Manual corrections are the top layer (7B.6 override layering), and each correction becomes a rule candidate applied across families.
- A shared **signature corpus** grows with every ROM scanned: more cross-ROM votes, more named library functions. Each new game starts with more already subtracted. This is learning by accumulation, with no machine learning.

#### 7D.6 How it connects

| Section | Effect |
|---|---|
| 5.2 | Call-site attribution, taint and RAM-map discovery become inputs to the boundary stage |
| 6.2, 6.3 | The file hook and state reader come from the SDK trace and typed views |
| 7C.4 | Table scanning, differential testing and state-machine extraction run on top of the control surface |
| 7C.3 | Seam stubbing is the ladder at function granularity |
| 18.7 | Full decompilation is the **last resort**, for the small game-code remainder that boundary analysis could not cover |
| 23.2 | A GameAdapter is mostly a ROM profile plus importers |
| 20 (E2) | The thin capture ABI is the place where hooks and seams live |

#### 7D.7 Honest limits

- Heavy inlining or different compiler flags change fingerprints. The behavior method and cross-ROM voting reduce, but do not remove, the number of unnamed functions.
- Function boundary recovery in mixed ARM and Thumb code, and in code with computed jumps, produces errors. Runtime observation corrects many but not all.
- Calling a function out of context can touch hidden state (RNG, globals, caches). Purity detection must run first, and non-pure functions need explicit state capture.
- Trace contrast shows *correlation*, not purpose. It localizes code and data, and a human or a later test confirms the meaning.
- Coverage-guided exploration cannot reach states that need long story progress without a save or scripted setup. Start from savestates created at key points.
- Struct inference from usage can mislabel fields that are used in several ways. Keep confidence scores and show them in the overlay.
- Fingerprints are hashes derived from library code. They are not code, but how the law treats a shared corpus is a question for a lawyer, not for this document. Keep corpora to hashes, offsets and names, and run analysis on the user's own ROM.
- Tracing and fuzzing are heavy, so they run offline in the scanner, not on the phone.

#### 7D.8 Pitfalls

- Do not patch the ROM. Hooks live in the emulator, so the original data stays untouched and the profile stays portable.
- Do not trust a seam until replay checks it. An unverified stub can silently change game rules.
- Do not mix profiles across ROM hashes without fingerprint relocation and a check.
- Keep the code map honest: `Unknown` is a valid tag, and the coverage number must not count guesses as success.
- Do not let hooks depend on raw addresses alone. Store the fingerprint first, the address second.

#### 7D.9 Suggested build order

1. Header and overlay table parser, with an overlay-load event log tied to game state.
2. Function boundary recovery, confirmed by executed program counters, and normalized fingerprints.
3. Cross-ROM and cross-version diffing with a small corpus, and the first library subtraction with a coverage percentage.
4. Hardware-domain attribution of call sites, with label propagation over the call graph.
5. SDK call trace (file reads, threads, timers, sound) once the library is named.
6. Function-as-RPC with save and restore, then purity detection.
7. Hook registry stored by fingerprint, and the first seam stub (for example "draw monster" or "read input").
8. Dispatch-table scan and struct inference, with the typed memory view.
9. Coverage-guided exploration and trace contrast.
10. ROM profile schema, override layering, relocation to other regions, and the shared signature corpus.

### 7E. Preimage remaster: assets that must round-trip, family templates and motion inversion

Section 7A adds missing information to an asset (volume, folds, materials, physics) with rules and tuned numbers. This section replaces "tuned numbers" with a principle: **the remade asset is correct if it round-trips.**

The DS pipeline throws information away in known ways: it downsamples textures, snaps colors to a palette, decimates meshes, quantizes vertices to fixed point and bakes lighting. So the question is not "how do we guess the detail?" but "which assets would degrade into exactly what the game stored and showed?" The original artist's asset is one of them. So is every other asset that degrades the same way, and that set is where the engine is free to choose. The choice inside the set is made by priors (procedural materials, symmetry, smoothness, family templates), never by machine learning.

This section also holds two siblings that use the same loop: **family template fitting** (7E.6) and **animation-to-physics inversion** (7E.7).

#### 7E.1 Preimage in one paragraph

Define `degrade(HD asset)` as the DS-style reduction: area-filter the textures down to the original size and snap them to the original palette, decimate the mesh to the original polygon budget and snap it to fixed point, and bake lighting the DS way. An HD asset is **valid** if `degrade(HD)` matches the original DS asset within a tolerance, and if the degraded asset, lit with the captured DS lights and drawn by the DS renderer, reproduces the oracle's frames (5.6). Validity is a *consistency* test. It does not claim the artist used the same degrade tool.

#### 7E.2 What each loss gives as a constraint

| What the DS discards | Constraint on the HD asset | What stays free |
|---|---|---|
| Color precision (palette snapping) | A texel's true color lay inside that palette entry's color cell. Re-quantizing the HD texture with the original palette must give back the same indices | Gradients, banding removal and shading inside each cell |
| Texture resolution | The HD texture, filtered down, must equal the DS texture | All detail above the DS frequency, filled by priors |
| Mesh detail and vertex precision | The true surface lay inside a tolerance shell around the DS mesh, and inside the silhouette seen from every captured view | Fine relief, folds and rounded edges, inside the shell |
| Baked lighting | The DS lighting equation and the captured light vectors are known, so `albedo * shading = observed` is a constrained solve (5.3, 7.5) | The split itself is solved, not guessed |
| Frame rate and keyframe sparsity | The simulation, undisturbed, must reproduce the keyframes (7E.7) | Behavior under disturbance |

#### 7E.3 The round-trip test and the search

```
candidate (HD asset, parameters p)
  -> degrade (palette snap, area filter, decimation, fixed point, DS lighting)
  -> DS renderer in the emulator, same poses and lights as the oracle frames
  -> compare with the oracle: palette-index match rate, silhouette IoU, color error
  -> round-trip score
```

- **Constraint, not objective.** The engine looks for the candidate with the **best prior plausibility** (smooth, symmetric, procedurally consistent, family-consistent) **subject to** a round-trip score above a threshold. Candidates below the threshold are rejected, not penalized.
- **Operator robustness.** Test against several reasonable degrade operators (box, area and bilinear filters; nearest palette entry in different color spaces). A candidate that passes only one is suspect, because it may overfit that operator.
- **Search.** Classical optimization over the stage parameters of 7A (fold strength, thickness, procedural material parameters, anchor placement): grid, hill climbing or CMA-ES. All seeded with the asset hash (7A), so results are reproducible.
- **Cost.** One round-trip is a headless render of the asset's captured frames. It runs offline in the compiler (7B), prioritized by the play-trace heat map (7B.5).
- **Output.** The round-trip score and the parameters that produced it are written into the entity record (7E.4), and become the trust score used by the runtime fallback (7B.5).

#### 7E.4 The constraint tightness map

Different parts of an asset are constrained very differently. The engine computes a **tightness map** per texel (texture) and per vertex (geometry):

- **Tight:** small palette cells, strong edges, painted marks, thin shell tolerance. These keep the original data, upscaled.
- **Loose:** flat regions, large palette cells, wide shell tolerance. These are free, so procedural materials (16.1) and geometric priors fill them.

This **replaces the manual blend slider** in 16.1 with a computed per-region blend, and turns the fold strength in 7A.5 into a per-vertex bound instead of a global number.

Extension of the entity record (7A.3):

```cpp
struct ConstraintData {
    MapHandle   texTightness;   // per texel, 0 = free, 1 = fixed
    MapHandle   shellTolerance; // per vertex, allowed distance from the DS surface
    float       roundTrip;      // best score reached, 0..1
    OperatorSet degradeOps;     // which degrade operators it was checked against
};
// EntityRecord gains: ConstraintData constraints;
```

#### 7E.5 Worked example

**A 16-color texture.** A slime body painted in a 64 by 64 paletted texture.

1. For each palette index, record its color cell and mark texels next to a different index as edge texels.
2. Upsample to the target size with an edge-directed method, then solve inside the cells: gradients are smooth inside a cell and cannot cross into a neighbor's cell.
3. Compute the tightness map. Edges, eyes and highlights are tight. Large flat body regions are loose.
4. In loose regions, blend in the fitted procedural material (16.1, for example a subtle subsurface-style variation), weighted by the tightness map.
5. Round-trip: area-filter to 64 by 64, snap to the original palette, and compare indices. Accept at or above the threshold (for example 99 percent of texels). Otherwise lower the procedural strength and retry.

**The flat cloth from 7A.5.** The fold relief is limited by the shell tolerance, so a painted shadow band can only become relief that the original silhouette allows. The albedo and shading split uses the captured DS lights. The round-trip then checks the silhouette and the lit colors against the oracle at the original camera. From other cameras, there is no ground truth, so only the artifact checks of 7B.6 apply.

#### 7E.6 Sibling: family template fitting

**Idea.** Instead of reworking each monster independently, author **one detailed template per family**, then fit it to every low-poly member. A handful of authored templates then produce hundreds of rigged, textured HD monsters.

| Step | What happens |
|---|---|
| 1. Author | One template per family: detailed mesh, part grammar (7A.3), UV layout, rig, material set and physics rules. Done once by hand or procedurally |
| 2. Match | Skeleton families (7B.5) say which template a monster belongs to, with a confidence score |
| 3. Align | Fit the template skeleton to the recovered skeleton (5.2), part by part |
| 4. Deform | Non-rigid registration of each template part onto the DS part, inside the shell tolerance (7E.4), using skeleton, silhouette and the round-trip score |
| 5. Texture | Transfer the DS palette-index regions (5.3) onto the template UVs by part correspondence, then run the 7E.3 loop for materials |
| 6. Verify | Round-trip against the oracle frames. On failure, fall back to the 7A result for that monster |

Variants (7B.5 palette variants) reuse the fitted template and only swap material parameters. Overrides in the manifest still win.

**Limits.** Monsters that do not fit a family need the 7A path. Template authoring is real work, so start with the families that cover the most monsters (use the play-trace heat map, 7B.5). Over-fitted templates make different monsters look alike: keep per-monster shape parameters and let the round-trip constrain them.

#### 7E.7 Sibling: animation-to-physics inversion

**Idea.** Tails, capes, ears and plumes in DS animations are keyframed, not simulated. The engine can fit a simulation that **reproduces the keyframes when undisturbed** and **reacts physically when disturbed**.

1. Take the leaf chains from the entity record (7A.3) and their keyframe trajectories relative to the parent bone.
2. Model each chain as a spring-damper (or XPBD) chain tracking the animated pose (17.5), with parameters for stiffness, damping, drive strength and rest offset.
3. Fit the parameters so that the simulation, driven only by the keyframes and the parent's motion (17.1), matches the original trajectories. Use the same optimizer as 7E.3.
4. If the keyframes show clear lag and overshoot, the fit finds soft parameters. If the motion is rigid, it returns stiff ones, and the chain is left unsimulated.
5. Verify twice: the undisturbed simulation stays close to the keyframes (error threshold), and an impulse test (a push, a turn, wind from 17.2) stays stable.

The result is written into `SimRule` (7A.3). It means a monster keeps its original animation timing and feel, and gains physical response on top, with no hand tuning.

**Limits.** Sparse keyframes make fits ambiguous, and the result is plausible rather than true. Stylized animation (squash and stretch, non-physical motion) may not fit a spring model: keep the stiff fallback.

#### 7E.8 Where it plugs in

| Section | Effect |
|---|---|
| 7A.2, stage 7 | Verify becomes the constraint that all stages search under, not a separate last check |
| 7A.5 | Fold strength and thickness become per-vertex bounds from the shell tolerance |
| 16.1 | The blend slider is replaced by the tightness map |
| 5.6 | The parity oracle supplies the frames and the score |
| 7B.5, 7B.6 | Round-trip score is the trust score. The budget-aware compile spends search time by heat-map priority |
| 7B.5 | Skeleton families select the template for fitting |
| 17.4, 17.5 | Motion inversion supplies spring-bone and chain parameters |

#### 7E.9 Honest limits

- The preimage is huge. Priors decide how the free regions look, and they can look wrong or generic. The tightness map only says where the original is certain.
- The artist's real degrade process is unknown, so the constraint is consistency, not an exact inverse. Testing several operators reduces, but does not remove, the risk of overfitting one.
- Detail the DS never stored (a one-texel eye, text on a sign) stays blurry unless a prior or a family template supplies it.
- Round-trip passing at the original camera says nothing about other angles. Those rely on priors and the artifact checks.
- Search costs compile time. Spend it by priority, and keep the plain 7A result as the fallback.

#### 7E.10 Pitfalls

- Do not use the round-trip score as the only quality measure. A faithful but ugly result passes it. Keep the A/B view (11.10) and human review for important assets.
- Do not let the search change the DS asset to make it pass. The DS asset and palette are fixed inputs.
- Keep the degrade operators versioned. A changed operator invalidates stored scores (7B.4).
- Never let priors override a tight region. The tightness map must win over procedural fill.

#### 7E.11 Suggested build order

1. `degrade` operators (palette snap with the original palette, area filter, fixed point) and the round-trip scorer on one texture, run headless through the oracle.
2. Color-cell constraint and dequantization for one paletted texture, with the index match rate as the pass test.
3. Tightness map, then replace the 16.1 blend slider with it.
4. Shell tolerance per vertex from the decimation error, and bounded fold relief for the cloth example (7A.5).
5. Parameter search (grid first, then CMA-ES) over the 7A stage parameters, with the score written into the entity record.
6. Animation-to-physics inversion on one leaf chain, with the undisturbed and impulse checks.
7. One authored family template and the fitting loop on a few members of that family.
8. Add operator robustness, per-asset budgets from the heat map, and the trust score in the pack.

---

## Part III. Camera and world

_Own the view, add depth presentation, then assemble the world._

### 8. Free camera

A free camera comes down to two things: the engine must **own the view matrix**, and the game must keep working while the camera points somewhere it never expected. The analogue stick covers movement input. The camera is the harder part.

#### 8.1 Own the view: separate the camera from the model matrices

The DS geometry engine keeps a projection matrix and a position matrix. The position matrix already combines the model transform and the camera, so a captured vertex only knows its final position relative to the game's camera. To move the camera yourself, recover world space:

```
world = inverse(gameView) * positionMatrix     // per draw, captured at submit time
clip  = freeProj * freeView * world            // your own camera
```

Ways to find `gameView`:

- Log every matrix stack operation (`MTX_LOAD`, `MTX_MULT`, `MTX_TRANS`, `MTX_PUSH`, `MTX_POP`) per frame. The camera is usually the matrix sitting at the bottom of the stack before the first object is pushed.
- Read the game's camera struct (position, target, up) from RAM, using the RAM map from section 6.3. This is the most reliable, and it lets you drive the camera yourself too.
- Cross-check with a test: the real view matrix is the one that, applied to every draw in a frame, makes static scenery land at fixed world positions across frames.

#### 8.2 The biggest problem: the game only submits what its own camera sees

DS games cull objects, map chunks, NPCs and LOD against their own frustum and draw distance. When your camera looks elsewhere, those objects simply do not exist in the frame. Options:

- **Retained scene.** Keep every draw captured by asset ID and last transform, and keep drawing objects the game stopped submitting for a while, with a staleness timeout. Cheap and game-independent, but dynamic objects freeze in place when out of sight.
- **Static from extracted data.** Terrain and buildings come from the converted map chunks (section 10), so they exist regardless of what the game submits. Only dynamic objects (player, monsters, NPCs) come from the live capture. This is the clean solution and fits the open world.
- **Patch the culling.** Find the frustum test or visibility range in the game code and widen or disable it. Per-game work, but it gives live data for everything.
- **Fog and draw distance.** Replace the DS fog and far plane with your own, since the game may hide its pop-in behind them.

#### 8.3 Controls: left stick moves, right stick looks

The analogue stick gives a vector in the camera's space. The game interprets direction in its own space, so remap it:

```cpp
// stick: x right, y forward, in free-camera space
float delta = gameYaw - freeYaw;                 // angle between the two cameras
vec2 dirForGame = rotate(stick, delta);          // direction in the game's frame
// then feed it as analogue velocity if you patched the game's movement,
// or quantize it to the nearest of the 8 d-pad directions otherwise
```

- Moving the left stick forward means "away from the free camera", even if the game's own camera is rotated differently.
- Quantizing to 8 directions is lossy. The better route is patching the game's movement code to accept an analogue angle.
- Camera-relative movement needs the game's yaw, so read it from the camera struct or derive it from the view matrix.

#### 8.4 Camera modes worth having

- **Follow/orbit (default).** The right stick orbits around the player. Add a spring arm: cast a ray from the player toward the desired camera position using the collision data, and shorten the arm on a hit so the camera never goes through walls. Add smoothing, pitch limits, and auto-recenter behind the player after a few seconds without input.
- **Free fly / photo mode.** Detached camera with speed control, no collision, plus depth of field and filters (section 21.3).
- **Game camera.** Pass-through of the original camera for scripted scenes and cutscenes, so nothing breaks.
- **Battle camera.** Orbit around the arena, with cinematic presets.

Switch to the game camera automatically when a cutscene or scripted camera is detected, then blend back, so nothing is shown with broken framing.

#### 8.5 Other things to handle

- **Billboards and sprites** must face your camera, not the game's. For 2D-derived scenes, see section 15.
- **Aspect ratio and FOV.** A wider FOV exposes more of what the game culled, so widen it only as far as the retained or static scene allows.
- **Occlusion and visibility logic** that the game uses for gameplay (hidden NPCs, triggers) stays based on the game camera. Your camera only changes what you see.
- **Input feel.** Dead zones, response curves, invert-Y, sensitivity, smoothing and a recenter button. Add a virtual stick on touch for phones.
- **Combining with the depth effect.** Tilt parallax from section 9 is just an extra offset applied on top of the free camera.
- **Culling and LOD** for your own meshes should use your camera, not the game's.

#### 8.6 Build order

1. Matrix logging and view recovery, with a debug overlay showing the recovered camera.
2. Render with your own view matrix and confirm static scenery stays put.
3. Right-stick orbit around the player, with the left-stick direction remap.
4. Retained scene for submitted objects, then static map chunks.
5. Spring arm collision and auto-recenter.
6. Free-fly and photo mode, then battle presets.
7. Widen the FOV and patch culling where the game still pops.

### 9. Depth effect on a smartphone

The 3DS effect is glasses-free stereoscopic 3D, done with a parallax barrier over the top screen. Ordinary phone screens have no such layer, so true stereo is not possible without outside material (lens viewer, glasses, special display). Staying fully inside the engine, the option that works is **motion parallax**: the scene shifts as the phone tilts, so the screen behaves like a small window into a diorama. Both eyes see the same image, so it is not true stereo, but with good depth cues it reads as 3D, and it needs only one render, so it costs almost nothing.

#### 9.1 Tilt parallax (recommended default)

- Read the gyroscope (on Android, the game rotation vector sensor) and compute the phone's tilt relative to a calibrated neutral pose.
- Move the virtual camera sideways and up/down by a small amount per degree of tilt, keeping it aimed at a fixed focus point. Tilting right lets you see around objects on the left.
- Needs no camera and no permission, and works for anyone holding the phone.

#### 9.2 Head tracking (optional, off by default)

- Use the front camera to estimate the viewer's head position and feed it to the same camera offset. More accurate than tilt because it follows the head, not the hand.
- Needs the camera permission, adds battery cost and latency, relies on a face detector (which conflicts with the no-AI preference), and should be clearly labeled. Tilt parallax does not have these drawbacks.

#### 9.3 Off-axis projection (the key rendering trick)

Moving the camera is not enough. The projection should be asymmetric, so the screen rectangle stays fixed in space, as a real window would:

```glsl
// eye = virtual eye position relative to screen center (x, y, z > 0 toward the viewer)
// screen half-size (w, h) at distance z = 0, near plane n, far plane f
float l = (-w - eye.x) * n / eye.z;
float r = ( w - eye.x) * n / eye.z;
float b = (-h - eye.y) * n / eye.z;
float t = ( h - eye.y) * n / eye.z;
mat4 proj = frustum(l, r, b, t, n, f);
mat4 view = translate(-eye);
```

This keeps the focus plane (the screen) still and moves everything in front of it and behind it in opposite directions, which is what sells the depth.

#### 9.4 Depth cues that make it convincing

- Layer stacking and billboards from section 15, so even 2D-derived scenes have real depth separation.
- Real shadows, contact shadows and ambient occlusion.
- Depth of field with the focus on the main subject (monster or player).
- Aerial perspective and fog from section 12, so distance fades toward the sky.
- A very small automatic camera drift (idle sway), so depth is visible even when the phone is still on a table.
- UI at different depths: menus and HUD floating in front of the scene with a soft drop shadow, like the 3DS interface.
- Pop-out effects for particles and attacks, which can come toward the camera.

#### 9.5 Settings and pitfalls

- A depth slider scaling the camera offset (like the 3DS one), plus a hard clamp so the view never leaves a sensible range.
- Smooth the sensor input (a One Euro filter or a simple low-pass filter) to avoid jitter, and add a recenter button or slow automatic recentering for gyro drift.
- Keep text and the main UI readable by limiting how much they shift.
- Strong offsets cause motion discomfort for some players, so default to subtle.
- It works for one viewer only, and tilting changes the viewing angle of the screen itself, which slightly fights the effect. Keep the range modest.
- For a 3DS-style top-screen look, apply it only to the 3D scene view and keep touch controls fixed.

#### 9.6 Build order

1. Off-axis projection with a fake input (mouse or touch drag) to tune the look on desktop.
2. Tilt input from the gyro with filtering, calibration and the depth slider.
3. Idle sway and UI depth layers.
4. Optional head tracking later.

### 10. Open world: all map instances in one continuous world

The game can only ever have one map loaded, so the world is assembled on the engine side: **your engine renders the world, and the DS core only runs the logic of the map the player is currently in.**

#### 10.1 Extract the maps into world data

Per map you need:
- The visual model and textures (NSBMD/NSBTX or the game's own format).
- The collision or attribute grid (walkable, water, slope, trigger tiles).
- Object, NPC and encounter-zone placement.
- The **link table**: warps, doors and edge exits, with source position, destination map and destination position.

Two ways to get links:
- **Static:** read the map headers from the ROM. Cleanest, but needs per-game reverse engineering.
- **Dynamic discovery:** walk the game (or run a bot) along every exit and log `(mapID, x, z)` just before and after each map change. This builds the link table automatically and also catches script-driven warps that are hard to find in the data.

#### 10.2 Build the world graph and stitch it

Nodes are maps, edges are links. For links where the player walks off one map's edge and appears at another's edge, solve each map's position in a global coordinate space:

```
offset[B] = offset[A] + exitPos_A - entryPos_B
```

- Run a BFS from an anchor map and apply the rule.
- When you find a loop, check that it closes consistently. If it does not, the game's geography is not Euclidean (two exits lead to the "same" place from different directions). Flag the conflict and resolve it by hand: nudge a map, or turn one link into a non-seamless portal.
- Keep a hand-edited `world_layout.json` that overrides the solver. Many DS maps were never designed to fit together, so some overlap or simply do not connect geometrically.

```json
{
  "world_pos": { "map_012": [0, 0, 0], "map_013": [256, 0, 0] },
  "links": [
    { "from": "map_012", "exit": { "edge": "east", "range": [10, 14] },
      "to": "map_013", "entry": { "x": 0, "z": 12 }, "kind": "seamless" },
    { "from": "map_012", "door": [40, 22], "to": "int_004", "kind": "instance" }
  ]
}
```

#### 10.3 What is seamless and what stays an instance

- **Seamless:** outdoor maps that join at edges. They are streamed into one landmass.
- **Instances:** interiors, caves, dungeons, cutscene areas, anything with a scripted transition. Two options:
  - Keep them as instances with a clean transition (cross-fade or a camera move through the door).
  - Place the interior in world space off to the side (a "pocket") and play the door animation, so the player never sees a loading screen.

#### 10.4 Rendering and streaming

- The engine loads converted map chunks around the player's world position and draws them natively, including neighbors the game itself never loaded. Unload by distance and use LOD for far chunks. Memory is not the problem (hardware limits are removed), asynchronous loading is.
- The camera becomes free. The game's fixed or bounded DS camera no longer applies, so your culling must not depend on the game's per-map visibility.
- At map borders, blend what the game treated as separate: terrain height, texture seams, **fog, sky color and light direction** (interpolate the per-map values across the boundary), and per-map music (crossfade).

#### 10.5 The hard part: game logic across maps

The emulator core only simulates the active map. Neighbor maps are static to it.

- **Neighbors render as static scenery.** NPCs are placed at their default positions from the map data, idle, with no AI. They only start behaving when the player enters their map. This is the practical default.
- **Boundary handoff.** When the player's world position crosses into a new map's area, trigger the game's own transition for real: write the destination map ID and local position into the game's state (or fire the link) and let the game load the new map normally. The renderer keeps drawing the world continuously, and the player's local coordinates are `worldPos - offset[map]`.
- **Hiding the load.** The game will fade out, load for a moment and fade in. To hide that: hook or skip the fade, run the emulator fast-forwarded through the load frames while the engine keeps rendering, freeze player input for that moment, then resume. Because the renderer is yours, the player only sees continuous motion.
- **Encounters, NPC scripts, events:** these belong to the game and only run in the active map. Decide per feature whether the neighbor may look alive (ambient animation only, which is cheap) or stays inert until entered.
- **Event flags, saves and cutscenes** work as before because the game itself is unchanged. Scripted map changes in cutscenes are the exception: leave those as ordinary instance transitions.

#### 10.6 Collision and physics for streamed neighbors

Use the attribute grid from the extracted map data to give neighbors collision for your own systems (grass trample, cloth, hair, particles). The authoritative gameplay collision stays with the game in the active map.

#### 10.7 Extras you get from the graph

- A generated world map and minimap, since you know where everything is.
- Fast travel between visited nodes.
- Sound occlusion and ambience blending based on what is near the player.

#### 10.8 Pitfalls

- Maps that overlap in world space (common when a game reuses coordinates). Needs manual placement.
- Different tile scale or height conventions between maps. Normalize at conversion time.
- Time-of-day, weather or state-dependent map variants. Treat each variant as its own node, or load the variant that matches the game state.
- One-way warps and script-gated exits. Mark those link kinds so the engine never treats them as seamless.
- Map storage details differ per game, so confirm how the target game stores maps and links first.

#### 10.9 Suggested build order

1. Map-ID overlay plus a position logger (write `(mapID, x, z)` on every map change).
2. Dynamic link discovery, which outputs the first draft of the graph.
3. The stitch solver and `world_layout.json`, with a debug view that draws all maps in one coordinate space.
4. Converter: map model and collision into your chunk format.
5. Streaming renderer for neighbors with border blending.
6. Boundary handoff with a masked load.
7. Interiors as pockets or clean transitions, then the world map UI.

---

## Part IV. Rendering quality

_Raise the visual baseline, then add the headline effects._

### 11. Modernizing flat and low-quality assets

Goal: take a DS game whose assets are low-res, flat and plain, and make it look modern. Most of the big ideas already exist in this document. This section maps them, then fills the gaps.

#### 11.1 Already covered (where to find it)

| Need | Where |
|---|---|
| Upscale textures, de-light, normal/height from luminance, palette variants | 7.5 |
| Per-class upgrade profiles (wood, stone, water, grass...) | 7.3 |
| Detect what an asset is, override manifest, debug overlay | 7.1 to 7.6 |
| Real lighting, shadows, RT, real sky, clouds, fog | 13, 12, 4 |
| Parallax planes, billboards, tile-to-prop, heights, sprite extrusion, depth-painted backgrounds | 15 |
| Parallax mapping, shells, volumetric textures, tessellation + displacement | 14 |
| Wind, grass, hair, cloth, secondary motion | 17 |
| Replace models and animations, UI and font redraw | 6.4, 6.5 |
| Depth feel on a phone (tilt parallax, off-axis projection) | 9 |
| Weather, photo mode, look presets | 21 |

#### 11.2 Missing: texture sampling and edges

Upscaling alone does not fix how textures are sampled. These are the usual reasons a replaced asset still looks cheap.

- **Mipmaps, trilinear and anisotropic filtering.** DS textures are sampled with no mips. At high resolution and at grazing angles (ground, walls) this gives shimmer or blur. Generate mips for replaced textures and use 8x to 16x anisotropic filtering on ground and wall classes.
- **Respect the DS wrap flags.** Repeat, flip-repeat and clamp are per-texture parameters. Keep them, or a flipped tile breaks at its seam.
- **Alpha fringing.** Cutout textures (1-bit alpha, A5I3, foliage, sprites) show dark or bright halos when upscaled or filtered. Dilate (bleed) the color into transparent pixels offline and use premultiplied alpha.
- **Tileset and atlas bleed.** When a tileset or sprite sheet is scaled or rotated, neighbors leak into each tile. Extrude each tile by 1 to 2 pixels at conversion time, or store tiles in a texture array.
- **Alpha-to-coverage** for foliage and fences, so cutouts anti-alias instead of crawling.

#### 11.3 Missing: anti-aliasing and resolution

- The DS has no real anti-aliasing, and rendering at a higher internal resolution alone leaves shimmering on thin geometry.
- Desktop: MSAA for geometry plus SMAA or TAA. Mobile: FXAA/SMAA first, then TAA if the budget allows.
- TAA needs motion vectors. They come from the model and view matrices captured separately (section 4) plus the previous frame's matrices, so keep last frame's matrices per object.
- On mobile, a temporal upscaler (FSR-style) lets you render at lower scale and reconstruct. Tie render scale to the quality governor (section 18).
- Offer the DS edge-marking effect as an optional style, not as the default.

#### 11.4 Missing: low-poly geometry

Flat texture is only half the problem. The silhouettes are also blocky.

- **Smooth normals.** Recompute normals with an angle threshold, so round things look round and hard edges stay hard. Flag materials that must stay faceted (crystals, boxes).
- **Phong or PN-triangle tessellation** (GL 4.0+, the same stage as section 14) rounds silhouettes without new assets. Tessellate after skinning, and fix UV seams so they do not crack.
- **Vertex-color lighting.** DS models often carry baked lighting in vertex colors. Section 7.5 de-lights textures but not vertex colors. Treat vertex color as an AO or tint multiplier (or remap it), not as light, or the object gets lit twice.

#### 11.5 Missing: color depth, banding and tone

- DS color is 5 bits per channel. Lighting and fog applied to it produce visible banding.
- Work in linear HDR internally, convert sRGB at load, and finish with tone mapping, a per-map color-grading LUT and a small amount of dithering to hide banding.
- Keep the game's original palette mood as the LUT's starting point (the fog and toon capture in 7.4 helps), so the remake does not drift away from the art direction.

#### 11.6 Missing: repetition and ground blending

Tiled ground and walls look flat because the same small texture repeats visibly.

- **Anti-tiling:** stochastic or hex tiling, plus a low-frequency macro noise that varies brightness and hue across the surface.
- **Detail normal layer:** a second, higher-frequency normal map overlaid on the generated one, tiled at a different scale.
- **Transition blending.** Use the tile map (section 15) to blend neighboring tile types at their borders (grass into dirt, sand into water), instead of hard tile edges.
- **Decals and scatter.** Stamp decals for paths, puddles, stains and cracks. Scatter small props (pebbles, flowers, tufts) per tile class, instanced and wind-driven (section 17).

#### 11.7 Missing: raster fallbacks for effects that RT does in this document

Sections 13 and 4 treat ray tracing as the main path for shadows and AO. A phone (or a modest PC) needs cheaper stand-ins.

- Cascaded shadow maps for the sun, plus contact shadows in screen space.
- GTAO or SSAO for ambient occlusion, SSR for water and metal reflections, with a cubemap fallback where SSR fails.
- Each is a quality-governor tier (section 18), with RT as the top tier. The same G-buffer feeds all of them.

#### 11.8 Missing: effects and particles

DS effects (magic, hits, smoke, sparkles) are small flat billboards and sprite flipbooks, and they are among the poorest-looking things on screen.

- Detect them with the render-state rules in 7.1 (unlit, camera-facing, no depth write) and give them a class.
- Replace with soft particles (fade near geometry using the depth buffer), additive or emissive blending, upscaled or regenerated flipbooks, and light emission so a fireball lights the scene.
- Keep timing identical to the original, since gameplay and animation sync depend on it.

#### 11.9 Missing: sprite placement details

Billboards from 15.2 have known problems:

- **Foot pivot.** Anchor each sprite at its feet, not its center, or it sinks into the ground or floats.
- **Cylindrical billboards.** Rotate only around the vertical axis, so sprites do not tilt backward when the camera looks down or tilts.
- **Depth against 3D props.** Give each sprite a small depth thickness and alpha-test it, so it intersects extruded walls and trees correctly instead of always drawing on top.
- **Sprite normals and shadows** come from the pivot and silhouette (15.2), so lighting looks consistent with extruded tiles.
- **Style choice per asset.** Decide pixel-faithful (nearest-neighbor, crisp) versus smoothed (xBRZ) per class, and do not mix them in one scene.

#### 11.10 Missing: prioritizing the work

"As far as possible" has no end, so measure what is worst.

- **Coverage report.** For each frame or play session, log the screen area (pixels times frames visible) of every asset ID, grouped by class and by upgrade status. Sort by area. The top of the list is what players see most and what you should replace first.
- **A/B split view.** A draggable divider between original and upgraded rendering, per asset, to judge each change.
- **Consistency guard.** Mixed upgraded and original assets in one scene clash. Use one shared color grade, one outline and AA treatment, and consistent texel density, so a half-upgraded scene still looks coherent.
- Reuse the manifest and override workflow from 7.6 to record decisions.

#### 11.11 Suggested build order

1. Coverage logger and asset-ID overlay (decide what to fix first).
2. Texture sampling fixes: mipmaps, anisotropic, alpha dilation, tileset extrusion.
3. Linear HDR pipeline, tone mapping, color-grade LUT, dithering.
4. AA (FXAA/SMAA first, TAA with motion vectors later).
5. Smooth normals, then vertex-color rework, then optional tessellation.
6. Anti-tiling, detail normals, tile transition blending, decals.
7. Raster fallbacks (shadow maps, GTAO, SSR) behind the quality governor.
8. Particle and effect replacement.
9. Sprite pivots, cylindrical billboards and depth intersection (alongside section 15).

### 12. A real sky instead of a skybox

A skybox is a mesh, so ray tracing treats it as real geometry at a finite distance. That breaks things: shadow rays can hit it, reflections show a box with visible edges, and bounce light comes from its surface instead of from "the sky". The fix is to **delete the box from the ray-traced world and make the sky a function of ray direction**, evaluated whenever a ray escapes the scene (the "miss" case).

#### 12.1 Detect and remove the sky mesh

Use the classifier from section 7: a huge sphere or dome drawn first, no lighting, depth write off, centered on the camera, often with a gradient or cloud texture. Once it is tagged `Sky`:

- Drop it from the G-buffer and from the BVH, so it is no longer geometry anywhere.
- Capture its texture before discarding it, because it carries the game's art direction (horizon color, zenith color, sunset tint).
- Also remove the sun and moon billboards, and keep their direction as light data instead.

#### 12.2 Sky as a full-screen function

Anywhere the G-buffer depth is at the far plane, evaluate `sky(rayDir)`. There are no box edges, no corner distortion and no parallax problem when the camera moves. It also works with the free camera and the open world (section 10), because the sky is infinitely far away by definition.

#### 12.3 Where the sky color comes from

Choose one, or blend them:

- **Procedural atmosphere.** Rayleigh and Mie scattering, either a cheap analytic model (Preetham or Hosek-Wilkie) or the precomputed LUT approach used in modern engines (transmittance LUT plus a small sky-view LUT). Gives time of day, sunsets and a correct horizon.
- **Art-directed gradient.** Sample the original sky texture's vertical gradient and use it as zenith and horizon colors. Keeps the look of the original game while still being a real sky.
- **Hybrid (recommended).** A procedural sky whose parameters are fitted to the per-map colors from the game, interpolated across map borders in the open world.

#### 12.4 How it plugs into ray tracing

- **Shadow rays toward the sun:** a ray that misses everything means the point is lit. Treat the sun as a directional light with a small angular size so shadows get soft penumbras.
- **Reflection rays:** on a miss, return `sky(dir)`. Water and metal now reflect a real sky instead of a box.
- **AO and GI rays:** on a miss, return sky radiance. Cheaper alternative: an ambient probe (SH9 or a tiny irradiance cubemap) built from the sky whenever it changes, instead of tracing many rays.
- **Sun direction:** capture the DS `LIGHT_VECTOR` as the default, or let the engine's time of day own it and drive the light color from the sky.

#### 12.5 Sky shader core

```glsl
uniform vec3  uSunDir;           // normalized, from DS light or time of day
uniform vec3  uSunColor;         // HDR, e.g. 20.0 * tint
uniform vec3  uZenith, uHorizon; // fitted from the original sky texture
uniform float uSunSize;          // cos of angular radius

vec3 skyRadiance(vec3 dir) {
    float h = clamp(dir.y, 0.0, 1.0);
    vec3 col = mix(uHorizon, uZenith, pow(h, 0.5));          // gradient base
    float mu = dot(dir, uSunDir);
    col += uHorizon * pow(max(mu, 0.0), 8.0) * 0.4;          // glow near the sun
    col += uSunColor * smoothstep(uSunSize, uSunSize + 0.0005, mu); // sun disc
    return col;
}

// ray tracing miss:
//   shadow ray  -> visible = true
//   reflection  -> radiance = skyRadiance(rayDir)
//   AO/GI ray   -> radiance = skyRadiance(rayDir)  (or sample SH probe)
```

Swap `skyRadiance` for LUT lookups when you want a physical atmosphere.

#### 12.6 Clouds, stars, fog

- **Clouds:** simple version is a scrolling 2D noise layer projected onto a plane (cheap, mobile-friendly). Better version is a low-resolution volumetric cloud raymarch, the same technique as the 3D texture raymarch in section 14, reprojected over several frames. Clouds should also **cast shadows** on the ground, using the same noise sampled along the sun direction.
- **Night:** stars from a cubemap or procedural hash, and a moon as a second directional light.
- **Aerial perspective and fog:** blend distant geometry toward the sky color with distance so mountains and far terrain fade into the horizon correctly. Capture the DS fog color and density as the starting values so the original mood survives.

#### 12.7 Open world and per-map variation

Keep one global sky and interpolate its parameters (zenith, horizon, sun direction, fog, cloud coverage) between maps near borders, the same way the fog and light blending in section 10 works. Indoor maps get no sky at all, and their ambient light comes from the map's own lights.

#### 12.8 Mobile cost (Android)

- Compute the sky-view LUT and the ambient probe at low resolution and update them every few frames, since the sky changes slowly.
- Render clouds at half or quarter resolution and reproject.
- Reflection and AO misses only read the cached LUT or probe, so a miss costs almost nothing.

#### 12.9 Pitfalls

- **Painted details on the skybox** (mountain ring, buildings, horizon silhouettes) are not sky. Split them out as far-away backdrop geometry or billboards that stay in the BVH, or they vanish.
- **Ceilings detected as sky.** Interiors sometimes use a dome mesh with the same traits. Use a map-level "indoor" flag in the classifier to prevent it.
- **HDR.** A real sun is far brighter than 1.0, so you need an HDR pipeline with tone mapping, or the sun and sky reflections clip.
- **Skyboxes not centered on the camera.** Some games move or scale the sky with the player, which changes the detection signals. Check per game.
- **Underwater and special effects** (lightning flashes, eclipses, scripted sky changes). Expose the sky parameters as game-state-driven values so scripted changes still work.

#### 12.10 Suggested build order

1. Sky detection and removal from the G-buffer and BVH, with a flat color on miss as a placeholder.
2. Full-screen `skyRadiance` pass using the gradient fitted from the original sky texture.
3. Wire misses into shadow, reflection and AO rays, and use the DS light vector for the sun.
4. Aerial perspective and fog blending.
5. Procedural atmosphere LUTs and time of day.
6. Clouds (2D layer first, volumetric later) and cloud shadows.
7. Backdrop split for painted horizon details.

### 13. Ray tracing

On real DS hardware: no shaders, ~2048 triangles per frame, fixed-point only. In your engine none of that applies.

#### 13.1 Options

- **Hybrid (recommended).** Rasterize primary visibility, then ray trace only shadows, reflections, AO or GI.
- **Full path tracing.** Possible at DS scene sizes, needs a denoiser.
- **Baked.** Offline ray trace into lightmaps or vertex colors. Best look per cost for static scenes.

#### 13.2 Implementation notes

- **Scene data:** rebuild or refit a BVH every frame. An LBVH on CPU is fine for a few thousand triangles, or build it in compute.
- **Tracing:** compute pass reads the G-buffer, traces shadow/reflection rays against the BVH SSBO.
- **Materials:** the DS only gives texture, vertex color and alpha. For PBR use the sidecar (material file keyed by texture hash or asset ID).
- **Denoising:** with 1-2 samples per pixel add a temporal or SVGF-style denoiser.
- **Cheap reflections without RT:** sphere-map or cubemap reflection using normal-based texgen.

For soft shadows, ambient occlusion and rough reflections, a shared distance field can serve the same purpose with less memory (section 20A.2). The BVH stays for exact hits.

#### 13.3 If you ever target real DS hardware (for reference)

- Software raycasting into a 16-bit bitmap at 64x48 or 128x96, with ITCM hot loops and the hardware divide/sqrt.
- Bake lightmaps and AO offline.
- Fake reflections via `TEXGEN_NORMAL`.

### 14. Volumetric textures and surface shaping

The idea: a flat painted cloth texture gets real 3D shape and conforms to a wall, water or a face.

#### 14.1 Techniques (in order of cost)

| Technique | Use for | Notes |
|---|---|---|
| Parallax occlusion mapping | Walls, relief fabric | Heightmap, no extra geometry |
| Tessellation + displacement | Real relief | Needs GL 4.0+ |
| True 3D texture raymarch | Fur, moss, wet fabric, volumetric cloth | `GL_TEXTURE_3D`, density in alpha |
| Shell technique | Fur, short grass | Several offset layers with alpha cutout |
| Slice stacking | Smoke, fog, cloud | Back-to-front semi-transparent quads |
| Hardware fog | Cheap depth-based atmosphere | Built into DS and GL |

#### 14.2 Raymarching a 3D texture inside a shell mesh

```glsl
#version 430
uniform sampler3D uVolume;      // rgb = color, a = density
uniform mat4 uWorldToVol;       // world -> [0,1]^3 volume space
uniform vec3 uCamPos;
uniform float uStepCount;       // e.g. 48
in vec3 vWorldPos;
out vec4 fragColor;

void main() {
    vec3 ro = (uWorldToVol * vec4(vWorldPos, 1.0)).xyz;
    vec3 rd = normalize((uWorldToVol * vec4(vWorldPos - uCamPos, 0.0)).xyz);

    // ray vs unit cube
    vec3 inv = 1.0 / rd;
    vec3 t0 = -ro * inv, t1 = (1.0 - ro) * inv;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), max(tmin.z, 0.0));
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    if (tFar <= tNear) discard;

    float dt = (tFar - tNear) / uStepCount;
    vec3 p = ro + rd * (tNear + dt * 0.5);
    vec4 acc = vec4(0.0);

    for (float i = 0.0; i < uStepCount && acc.a < 0.98; i++) {
        vec4 s = textureLod(uVolume, p, 0.0);
        float a = 1.0 - exp(-s.a * dt * 20.0);   // density -> opacity
        acc.rgb += (1.0 - acc.a) * a * s.rgb;
        acc.a   += (1.0 - acc.a) * a;
        p += rd * dt;
    }
    if (acc.a < 0.01) discard;
    fragColor = acc;                              // premultiplied alpha
}
```

Tips:
- For opaque cloth, write `gl_FragDepth` from the first hit so it composes with the scene.
- For fibers, jitter each step with noise to avoid banding.
- Cheaper alternative: POM over a heightmap.

#### 14.3 Cloth conforming to a wall, water or face

Robust method:

1. Generate a **shell mesh** offset along the target surface normals.
2. Run a **GPU cloth sim (Verlet or XPBD)** against the target's SDF or collision mesh.
3. Keep the painted texture on **shared UVs**.
4. If the target deforms (waves, blendshapes), re-evaluate the shell every frame.

Per target:

- **Wall:** static shell plus contact shadows. Run the sim once at load.
- **Water:** heightfield wave sim or FFT ocean as the target. The cloth floats using buoyancy constraints. Scroll the texture for extra motion.
- **Face:** blendshapes or skinning on the target. Transfer skin weights to the cloth through closest-point skinning so it follows the deformation.

#### 14.4 XPBD cloth in compute (outline)

- SSBOs: `pos`, `prevPos`, `invMass`, and a constraint list (index pairs plus rest lengths, **graph-colored** so one dispatch has no write conflicts).
- Per frame: predict positions, then 4 to 8 constraint dispatches, then collisions against the target SDF (3D texture, gradient as normal), then velocity update.

#### 14.5 Fake bump for fine detail the mesh cannot hold

- **Baked:** compute normals from the heightmap offline (Blender bake or script) and bake lighting into the texture or vertex colors.
- **Emboss two-pass:** draw twice with the texture offset along the light direction, blend the second pass. Doubles polygon cost.
- **Projection:** `TEXGEN_POSITION` with the texture matrix as a decal projector, plus an overlay mesh slightly above the surface to avoid z-fighting.

### 15. Turning 2D into 3D

On the DS, "2D" means the 2D engine's output: tile background layers, OAM sprites, bitmap backgrounds and the UI. To turn it into 3D, capture those layers **separately** (not as the final composite) and give each a depth and a shape. No AI is needed, only rules and a manifest.

#### 15.1 Capture the 2D side as data, not pixels

- Per frame, per layer: tilemap, tileset, palette, scroll, priority, affine parameters, and window and blend settings.
- Per sprite (OAM): tile data, size, flips, palette, position, priority, affine matrix.
- This is the 2D counterpart of the polygon capture in section 2. It also gives stable IDs through tile and sprite hashes, the same way textures get them.

#### 15.2 Techniques, from cheapest to most work

- **Layer stacking (parallax planes).** Each background layer becomes a plane at its own depth, ordered by priority. With a free or tilting camera you get real parallax immediately. This is the cheap first win.
- **Billboard sprites.** Each OAM sprite becomes a camera-facing quad placed in world space. For top-down or isometric games, project the sprite's screen position onto the ground plane to get its world position, and add a blob or projected shadow.
- **Tilemap to 3D mesh.** Treat each tile ID as a key into a **tile-to-prop dictionary**, built per tileset in the manifest: floor tiles become flat ground, wall tiles are extruded into blocks, tree tiles become 3D trees or crossed billboards, water tiles get the water class from section 7, and so on. Use the game's collision or attribute grid to confirm walls, ledges and water instead of guessing from the image.
- **Heights from game data.** Elevation values in the attribute grid give terrain height, so slopes, stairs and cliffs get real geometry.
- **Sprite extrusion.** Give sprites thickness by extruding their opaque pixels, or by voxelizing them in a few layers. It works well for pixel art and gives correct lighting and shadows. Generate normals from the sprite's edges and silhouette.
- **Painted backgrounds (bitmap or static layers).** Project them onto simple proxy geometry (a ground plane plus a back wall, or a box room) with a hand-authored depth map. A small tool for painting depth over the layer is worth building.
- **Fake relief for details.** Use the palette index or luminance as a height for parallax mapping on surfaces such as roofs or cobblestones, as in section 14.

#### 15.3 Lighting and shadows once there is depth

Extruded tiles and sprites cast and receive real shadows, AO and sun light from the sky (section 12), which is where it stops looking like a flat game. Y-sorting (lower on screen means closer) stays the default depth rule for sprites that have no other data.

#### 15.4 Where it gets tricky

- **Per-scanline effects.** Many DS games change scroll or affine parameters mid-frame for waves, curved horizons or Mode 7 style floors. Capture these per scanline, or the layer looks wrong.
- **Blending, windows and mosaic.** These need your own draw order and masks, since the hardware did them while compositing.
- **Interleaved priorities.** A sprite can sit between two background layers. The depth assignment must keep that ordering.
- **A free camera exposes what was never drawn:** the back of walls, the hidden side of sprites, the area behind tall objects. You need hidden geometry, back faces or simple fill, and a camera that is limited or only tilts a little for 2D-heavy scenes.
- **Palette animation** (water shimmer, glowing tiles) must keep running, so resolve palettes at draw time.
- **2D mixed with 3D.** The DS can show the 3D layer as background 0, so detect that and skip it.
- **Visuals that do not match collision.** Some tiles look like floor but block the player. The collision map is the safer source for geometry.

#### 15.5 Suggested build order

1. Layer and sprite capture with an overlay showing tile and sprite IDs.
2. Layer stacking and billboard sprites, which gives parallax and a tiltable camera.
3. A tile-to-prop manifest and a converter for one tileset.
4. Heights and walls from the collision and attribute grid.
5. Sprite extrusion with normals, then lighting and shadows.
6. Depth-painting tool for painted backgrounds.
7. Scanline effects, blending and windows.

This also fits the open world (section 10): a tile-based map converted to 3D chunks is the same data as the stitched world graph.

### 16. Procedural materials, vector UI, terrain, tooling and audio

Same format as section 5: DS fact, the idea, the gain and the main risk. Two ideas were deliberately left out of this section (second device as the second screen, and visual breeding for monster synthesis).

#### 16.1 Procedural materials fitted to DS textures

- Fact: DS textures are tiny and low-color, so any upscale still carries a visible grid, repetition and no extra detail.
- Idea: for tileable surface classes (wood, stone, grass, sand, brick, fabric), replace the texture with a **procedural material** (noise and pattern graph) whose parameters are fitted to the original: color range, dominant hue, grain direction (from the gradient statistics in 7.1) and contrast.
- Gain: no resolution limit, no visible tiling, and the same procedural graph can also output normal, roughness and height consistently. Fits the class system, with one graph per class and per-asset parameters in the manifest.
- Risk: matching the original's look closely enough that the art direction survives. Keep a blend slider between the fitted procedural result and the upscaled original, and use the original for anything the fit does not capture (painted marks, unique details). Section 7E.4 replaces the manual slider with a computed per-texel tightness map.
- Experiment: fit one wood and one stone texture, then compare against the plain upscale in the A/B view (11.10).

#### 16.2 Vector and distance-field UI and sprites

- Fact: DS UI art (icons, frames, menu graphics, stylized text) is made of flat shapes with few colors, which suits distance-field encoding.
- Idea: convert such assets offline into **signed distance fields** (single or multi-channel) and render them with a small shader. They stay sharp at any size and on any screen density, and you get outlines, glow, soft shadows and animation cheaply from the same data.
- Gain: the cleanest possible UI on a phone, with one asset serving every resolution. Also works for text if you hook the font (6.5), and for simple flat sprites such as icons and cursors.
- Risk: shapes with fine interior detail or gradients lose information in a single-channel field. Use multi-channel fields, or keep a normal upscaled bitmap for those assets.
- Experiment: convert the menu frame and a handful of icons, render them at three scales, and compare with the xBRZ upscale.

#### 16.3 Smooth terrain from the tile grid

- Fact: the attribute and collision grid (section 15) gives exact walkable area, walls and heights, but extruding tiles into blocks looks boxy.
- Idea: treat the grid as a **distance field** and blend neighboring cells with a smooth union, then mesh it (marching cubes or dual contouring on a coarse grid). Cliffs, banks and walls get rounded, organic shapes. Keep exact alignment with the game's collision by clamping the surface to the grid where the player walks.
- Gain: overworld maps look sculpted rather than tiled, with sloped transitions between heights, and you still get the tile class for material and blending (11.6).
- Risk: rounded shapes can cover or open areas the game treats as blocked or walkable. Validate with a collision diff view (walkable cells versus meshed surface), as in the parity oracle (5.6).
- Experiment: build one small outdoor map, show the collision mismatch as a color overlay, and tune the smoothing radius.

#### 16.4 Importance-based resolution and shading

- Fact: a phone GPU cannot afford the full remake pipeline on every pixel (section 18).
- Idea: spend quality where the player looks. Render monsters, the player and anything in the focus area at full quality, and distant scenery, sky and large flat surfaces at lower resolution or shading rate (variable rate shading where available, or lower render scale for the background layer composited under a full-res foreground).
- Gain: a large saving with little visible loss, and it plugs straight into the quality governor (section 18) as a tier.
- Risk: visible seams between quality regions. Keep the transition soft and use the class and depth already in the G-buffer to decide the regions.

#### 16.5 Frame-to-glTF export (RenderDoc for the DS)

- Fact: your capture already holds everything needed: geometry, textures, materials, matrices, lights and skeleton data (skeleton recovery in 5.2).
- Idea: export one captured frame, or one asset, as a **glTF scene** with meshes, materials, skeletons and lights, plus the manifest ID of each part. Artists can open it in Blender, redo or retexture assets, and import them back through the replacement manifest (6.4).
- Gain: this unlocks every asset-replacement idea in this document and makes community packs (21.6) practical, since contributors need no emulator knowledge. It is also the fastest way to inspect what the game really sends.
- Risk: texture and palette formats need conversion, and exported data must stay a personal tool, not something redistributable (legal note in 6.7). Export only to the user's own disk from their own ROM.
- Experiment: export one monster with its textures and skeleton and load it in Blender.

#### 16.6 Spatial audio from the world scene

- Fact: the DS has simple sound hardware, but the engine now knows the geometry of the space (world scene, section 4).
- Idea: use the world scene to drive audio. Room-size-based reverb, occlusion through walls, distance filtering, and positional panning that follows the free camera. Material class can adjust it (stone echoes, grass dampens, water muffles).
- Gain: places feel different, and audio matches the free camera and the open world. Remastered music could be a separate user-supplied pack.
- Risk: the game's own sound code decides what plays and where. Map sound events to world positions through the state reader (6.3) and the call-site tags (5.2), and leave unknown sounds unprocessed.
- Experiment: add reverb based on a simple room-size estimate for one indoor map and compare.

#### 16.7 World memory (persistent traces)

- Fact: the grass-trample map and footprints (17.3, 21.4) exist only while you are in the area and fade.
- Idea: store traces per map: scorch marks from battles, worn paths, flattened grass, dropped items fading over days of game time. Save them as a small compact layer (a low-res map per area, not per object), and reload them with the map.
- Gain: the world shows your history, and the world stays cheap to store because only the trace layers are saved.
- Risk: save-state and rewind interactions. Keep traces in the meta save (23.3) and tie them to a world position and a game-time timestamp, so loading an older state does not leave inconsistent marks.

#### 16.8 Effort versus impact

| Idea | Impact | Effort | Main risk |
|---|---|---|---|
| 16.5 Frame-to-glTF export | High (unlocks assets and community) | Medium | Format conversion, legal care |
| 16.2 SDF UI and sprites | High for UI | Low-medium | Detailed art does not fit a distance field |
| 16.3 Smooth terrain | High on overworld | Medium-high | Mismatch with collision |
| 16.1 Procedural materials | Medium-high | Medium | Losing the original's look |
| 16.4 Importance-based resolution | Medium (mobile) | Medium | Quality seams |
| 16.7 World memory | Medium | Low-medium | Save and rewind consistency |
| 16.6 Spatial audio | Medium | Medium | Mapping sounds to positions |

#### 16.9 Suggested order

1. Frame-to-glTF export (also useful for debugging everything else).
2. SDF UI and text, since the UI is the cheapest big visual jump.
3. Procedural materials for two classes (wood and stone), compared in the A/B view.
4. Smooth terrain on one small map, with the collision diff overlay.
5. Importance-based resolution as a quality-governor tier.
6. World memory on top of the interaction maps from section 17.
7. Spatial audio once the world scene works for more than one map.

---

## Part V. Simulation

_Make things move: wind, grass, hair, cloth._

### 17. Physics: hair, grass, wind and secondary motion

The DS game does not tell you what objects are or how they move, so you must recover that information.

#### 17.1 Recover identity and motion

- In the GPU3D hook, capture the **current position matrix** and the **untransformed vertices** when each polygon batch is submitted (not only clip-space output).
- Give each draw a **stable ID**: texture hash plus draw-order index, or a custom tag command, or (better) the asset ID from the file-load hook (section 7.2).
- Keep last frame's matrix per ID. The difference gives linear and angular velocity, which feeds the sim as inertial force. Without it hair will not swing when a character turns.

#### 17.2 Wind as a shared field

One function sampled by hair, grass, cloth and particles:

```glsl
uniform vec3  uWindDir;       // normalized
uniform float uWindStrength, uTime;
uniform sampler2D uNoise;     // tiling noise texture

vec3 windAt(vec3 p) {
    float gust    = texture(uNoise, p.xz * 0.05 + uWindDir.xz * uTime * 0.1).r;
    float flutter = texture(uNoise, p.xz * 0.6  + uTime * 0.5).g - 0.5;
    return uWindDir * uWindStrength * (0.5 + gust) + vec3(flutter) * 0.4 * uWindStrength;
}
```

- Drag: `F = k * (wind - velocity)` (this also damps motion).
- Regional wind (fan, explosion gust): upload a low-res 3D vector field texture and sample it instead.

#### 17.3 Grass: deform blades, do not simulate them

1. Compute shader scatters blade instances over tagged ground triangles, with count proportional to triangle area.
2. Vertex shader bends each blade, weighted by height so the root stays fixed.
3. A top-down **interaction map** (R16F/RG16F) stores displacement from characters. Splat a sphere/capsule each frame and let it decay slowly so grass springs back.
4. Fade blades and drop LOD with distance.

```glsl
// grass vertex shader (instanced), t = 0 at root, 1 at tip
vec3 base = instPos;
float bend = t * t;
vec3 w = windAt(base);
vec2 push = texture(uInteract, (base.xz - uMapOrigin) / uMapSize).rg;  // trample
vec3 offs = vec3(w.x, 0.0, w.z) * 0.15 + vec3(push.x, -length(push) * 0.5, push.y);
vec3 pos = base + localBladePos + offs * bend;
```

#### 17.4 Hair: simulate guide strands, interpolate the rest

- Each strand is a particle chain (6 to 16). Pin the root to the skinned scalp mesh each frame.
- Simulate only ~1 to 5k **guide strands**. Generate extra strands in a tessellation or geometry stage by interpolating between guides with a small random offset.
- Render as camera-facing ribbons or thin tessellated lines with a **Kajiya-Kay** specular highlight.
- One compute thread per strand works well because chain constraints are sequential.

```glsl
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Pos  { vec4 pos[];  };   // xyz
layout(std430, binding = 1) buffer Prev { vec4 prev[]; };
layout(std430, binding = 2) buffer Root { vec4 root[]; };   // skinned root per strand
layout(std430, binding = 3) buffer Rest { vec4 rest[]; };   // rest pose in root space (per particle)
uniform mat4 uRootFrame;            // object matrix this frame
uniform int uSegs; uniform float uSegLen, uDt, uDamp, uDrag, uStiff;
uniform vec3 uGravity;
uniform vec4 uSphere[4]; uniform int uNumSpheres;
// windAt() as above

void main() {
    uint s = gl_GlobalInvocationID.x;
    uint b = s * uint(uSegs);
    pos[b].xyz = prev[b].xyz = root[s].xyz;

    for (int i = 1; i < uSegs; i++) {               // Verlet integrate
        vec3 x = pos[b+i].xyz, xp = prev[b+i].xyz;
        vec3 v = (x - xp) * uDamp;
        vec3 a = uGravity + (windAt(x) - v / uDt) * uDrag;
        prev[b+i].xyz = x;
        pos[b+i].xyz  = x + v + a * uDt * uDt;
    }
    for (int it = 0; it < 4; it++) {                // follow-the-leader + shape
        for (int i = 1; i < uSegs; i++) {
            vec3 target = (uRootFrame * vec4(rest[b+i].xyz, 1.0)).xyz;
            pos[b+i].xyz = mix(pos[b+i].xyz, target, uStiff);   // keeps the hairstyle
            vec3 d = pos[b+i].xyz - pos[b+i-1].xyz;
            pos[b+i].xyz = pos[b+i-1].xyz + normalize(d) * uSegLen;
            for (int k = 0; k < uNumSpheres; k++) {             // head/body collision
                vec3 c = pos[b+i].xyz - uSphere[k].xyz;
                float l = length(c);
                if (l < uSphere[k].w) pos[b+i].xyz = uSphere[k].xyz + c / l * uSphere[k].w;
            }
        }
    }
}
```

Notes:
- `uStiff` blending toward the rest pose is what separates a hairstyle from limp noodles. Use high stiffness near the root, low near the tip.
- Long hair: add strand-to-strand or SDF collision against shoulders.

#### 17.5 Other secondary motion

Spring-bone and chain parameters do not have to be tuned by hand: section 7E.7 fits them to the original keyframed motion.

- **Tails, ribbons, capes, cloth bits:** hair chain code or the XPBD cloth.
- **Loose accessories, antennae, jiggle:** spring bones (damped spring per bone toward the animated pose).
- **Leaves and debris:** GPU particles sampling `windAt()`, with depth-buffer or SDF collision.
- **Bushes and trees:** vertex shader sway with vertex color or UV as a stiffness mask (root 0, tips 1), driven by the same wind function.

#### 17.6 Tagging

Extend the material sidecar with `sim_type` (`hair`, `grass`, `cape`, `sway`) and parameters (stiffness, damping, drag, collider set), so physics is configured per texture or model without touching sim code.

### 17A. Entity interaction and object integrity: contact, fields, deformation, destruction and division

Section 17 simulates one thing at a time: a tail, a cape, a patch of grass. This section covers what happens **between** things, and what happens **to** a thing when a force, event or environment acts on it: it deforms, breaks, loses a part, splits, merges or multiplies.

The two halves share one model. Every physical thing is an entity made of parts (7A.3), each part has a **mechanical material**, parts are joined by **bonds**, and the engine changes that structure through explicit, deterministic operations. Destruction is not a special effect bolted on. It is the structure of the entity changing.

#### 17A.1 Principles

- **Entities are structure plus material.** The entity record (7A.3) already has parts, volume and a skeleton. This section adds mechanical properties and a bond graph.
- **Topology changes are events.** Break, detach, split, merge and spawn are events in the event grammar (7B.6). They are logged, replayable and applied at fixed points between simulation steps.
- **One solver, many constraint kinds.** Contact, joints, bonds, field coupling and soft-body constraints run in the same XPBD framework (14.4), with graph-colored, fixed-order solving so results are deterministic.
- **Authority is explicit.** The physics layer produces presentation and events. Game rules (7C) decide gameplay outcomes. The classic ruleset uses physics for presentation only. The extended ruleset (7C.8) may let physics feed back into rules.
- **Everything is optional per class.** Each class gets an integrity level: `none`, `visual` (hit reactions and damage decals only) or `full`. This is also what the faithfulness slider (7B.5) controls.

#### 17A.2 Interaction between entities

**Contact.** Broadphase and narrowphase run on the shared spatial field (20A.2): per-part capsule or distance-field proxies query the field, and each contact becomes an event `{a, b, point, normal, impulse, relative velocity, material pair}`. Skeleton-based proxies (5.2) keep this cheap, since entities do not collide as full meshes.

**Mass and inertia from the asset.** Mass is `density * volume`, and the volume comes from the entity's signed distance field (7A.4). Center of mass and inertia follow from the same field. No hand-entered mass table is needed. Class defaults give density, and the manifest overrides it.

**Fields as indirect influence.** Entities do not only touch, they also emit into and sample from **field channels** stored as writable layers of the spatial field (20A.2): wind (17.2), water, heat, wetness, charge, pressure, toxicity, light. Fire heats the heat field, the heat field ignites grass, water cools it. Influence travels through the environment, not through pairwise code.

**Reaction rules (data, not code).** A table maps a material pair or a field condition to effects. It uses the same format as the event grammar (7B.6) and is moddable.

```json
{
  "ignite":  { "when": { "heat": "> flammability", "material": "flammable" },
               "do":   [ { "state": "burn", "rate": 0.2 }, { "emit_field": "heat", "amount": 0.5 } ] },
  "quench":  { "when": { "wetness": "> 0.6", "state": "burn" },
               "do":   [ { "state": "burn", "set": 0 }, { "emit": "steam" } ] },
  "shatter": { "when": { "impulse": "> toughness", "failure": "brittle" },
               "do":   [ { "event": "Fracture", "energy": "$impulse" } ] },
  "conduct": { "when": { "charge": "> 0", "material": "conductive" },
               "do":   [ { "emit_field": "charge", "spread": 0.7 } ] }
}
```

**Active ragdoll and physical animation.**
- **Joint limits from observed animation.** The recovered skeleton (5.2) and the monster's animation clips give the range each joint actually moves through. Use it as the joint limit, with no hand setup.
- **Animation as the target.** Each joint tracks the animated pose with a spring or PD controller (the same tracking model as 7E.7). A hit reduces the tracking strength for a short time, so the body reacts physically, then recovers into the animation. The same mechanism covers death (strength falls to zero, full ragdoll).

**Couplings between entities.** Grabbing, carrying, riding and tethering are constraints between attachment points (5.2), added and removed as events. Momentum transfers through the contact solver, and buoyancy and drag come from the water and wind fields.

**Crowds.**
- Near entities get full contact and field coupling.
- Mid-distance entities use capsule proxies only.
- Far entities are kinematic, with no interaction.
- Groups of touching entities form **interaction islands**, which sleep when nothing moves.
- This follows the degradation ladder (7B.6) and the quality governor (18.5).

#### 17A.3 Mechanical materials

Extension of the part material (7A.3):

```cpp
enum FailureMode { Elastic, Plastic, Tear, Shatter, Crumble, Burn, Melt, Dissolve, Squash, Pop };

struct MechMaterial {
    float density;
    float stiffness;                 // elastic response
    float yieldStrain;               // above this, deformation becomes permanent
    float strength[3];               // tension, compression, shear
    float toughness;                 // energy needed to propagate a break
    float restitution, friction, damping;
    FailureMode failure;
    ThermalProps thermal;            // flammability, melt point, conductivity
    float absorption;                // wetness uptake
    FillKind fill;                   // Solid, Hollow, Jelly, Fluid
};
```

DS assets carry no mechanical data. The engine **infers** it: class defaults from the classifier (7.1, 7.3), refined by palette-index masks (5.3) and the family template (7A.4), with manifest overrides on top. Typical defaults:

| Class | Failure mode | Notes |
|---|---|---|
| Wood | Shatter into splinters | Fibrous interior material |
| Stone | Shatter or crumble | Granular interior |
| Metal | Plastic dent, rarely shatter | Dent maps persist |
| Cloth | Tear | Frays along the break |
| Foliage, grass | Tear or cut | Falls as pieces |
| Skin or body | Elastic, `visual` level by default | Separation only by rule |
| Jelly | Elastic, Squash, Pop | Splits and merges (17A.5) |

#### 17A.4 The integrity graph

Nodes are parts. Edges are **bonds**, with a kind and a strength.

```cpp
enum BondKind { Weld, Hinge, Ball, Spring, Seam, Adhesive };

struct Bond {
    uint32_t a, b;
    BondKind kind;
    float    strength;     // from the weaker part's material and the contact area
    float    fatigue;      // damage per cycle below strength
    float    damage;       // accumulated, 0..1
    MeshRef  cap;          // pre-built cap geometry for the wound or stump
};

struct IntegrityGraph {
    Bond     bonds[64];
    uint32_t anchorMask;   // nodes attached to the world or to the parent
    SupportPolicy support; // whether unsupported groups fall
};
```

- **Source of the graph.** Joints come from the skeleton, attach points from the part grammar (7A.3), seams from the cloth masks (7A.3). Strengths derive from materials.
- **Load measurement.** The XPBD solver gives each constraint's force through its multiplier, so each bond's load is known every step.
- **Damage accumulation.** Load above strength adds damage quickly, load below strength adds fatigue slowly. At `damage >= 1` the bond fails and the matching operation runs.
- **Support analysis.** After any failure, flood-fill the graph from the anchored nodes. Part groups not reached are unsupported and become free bodies. This one check covers collapsing walls, falling props and hanging objects.

#### 17A.5 The operations

| Operation | Trigger | Mechanism | Events |
|---|---|---|---|
| Elastic deformation | Load below yield | Solver only | None |
| Plastic deformation | Strain above `yieldStrain` | Shift the rest shape toward the deformed one, write a dent into the entity's damage map | `Dent` |
| Tear | Edge stress above strength (cloth, foliage) | Break constraint edges, split the vertices along the break, keep the UVs, fray the edge with seeded noise | `Tear` |
| Fracture (brittle) | Impulse above toughness | Activate a **pre-fractured fragment set** (see below) | `Fracture` |
| Separation | A bond fails | The part leaves the entity (entity fission, see below) | `Detach` |
| Division | A jelly body is cut or overloaded | Partition the particle cluster into connected groups, each becomes an entity | `Split` |
| Multiplication | A rule or event says clone | Spawn instances with seeded variation | `Spawn` |
| Merge | Two bodies overlap under a merge rule | Smooth union for jelly, new bonds for solids | `Merge` |
| Burn, melt, dissolve | Material state passes a threshold | A per-part state value drives char, emission and distance-field carving | `Ablate` |
| Squash, pop | Compression above limit | Volume constraint fails, burst into particles or fluid | `Pop` |

**Pre-fractured fragments (no runtime boolean operations).** Cutting meshes at runtime is expensive and fragile on thin, open DS meshes. Instead:
- At pack build (7B.4), generate a seeded Voronoi fracture pattern per breakable part, intersected with the part's shell volume (7A.4).
- Build 2 to 3 hierarchy levels (large chunks that can split into smaller ones).
- Give interior surfaces a class-based interior material (wood fibers, stone grain).
- At runtime, only activate fragments near the impact, with detail depending on impact energy. Fragments are rigid bodies joined by bonds, so a crack can be partial.

**Entity fission (separation and division).** The entity record (7A.3) is split, not rebuilt:
- A new record gets the detached parts and shares the original mesh and material data. Its ID is `hash(parentID, eventSequence, childIndex)`, so IDs are deterministic and stable across replay.
- The parent hides the detached parts and uses each bond's pre-built `cap` mesh to close the wound or stump.
- The child inherits the parent's velocity at the bond. A detached tail goes from spring chain to free body, a detached piece of cloth becomes its own cloth.
- Mass is conserved by default, with a flag for rules that want it otherwise.

**Jelly: implicit surface.** For the jelly class, render the body as the **smooth union of its particles' distance fields** (14, 20A.2) instead of a deformable mesh. Division becomes "partition the particles into connected groups", merging becomes "bring groups together", and multiplication is "copy a group". The surface follows for free, with no mesh surgery.

**Persistent damage.** Dents, cracks, scorches and wounds are stored in a per-entity **damage atlas** on the clean second UV set (7A.2, stage 3), so one place holds every kind of visible damage. Part of it can be written back to the world memory layer (16.7) when it should persist on the map.

**Multiplication and budgets.** Spawned copies are instances that share meshes and records. A fragment and debris budget, and debris lifetimes (fade, convert to particles, or despawn), keep counts bounded. The quality governor (18.5) lowers fragment detail first.

#### 17A.6 Determinism and events

- Topology edits run at fixed sync points between simulation steps, never during a solve.
- Event order, ID assignment and the solve order are fixed, and the sim uses a fixed timestep with graph-colored dispatches (14.4).
- All random choices (fracture pattern, fray noise, spawn jitter) use the entity seed (7A), so a replay (7B.6, 7C.8) reproduces the same breaks.
- Integrity state (bond damage, which parts exist, damage atlases, entity splits) is part of the saved state and of the snapshot ring (E7), so save, load and rewind work.
- Physics emits events, and rules consume them through the event grammar. An example: `Fracture` with an energy value can raise a damage value in the extended ruleset, and in the classic ruleset it only drives visuals.

#### 17A.7 Verification

Extend the verify stage (7A.2, 7E) with a headless **stress suite** per entity, run in the compiler (7B) with a fixed seed:

- Standard impulses, drops and pushes: no energy blow-up, no interpenetration, no NaNs.
- Mass and volume conserved across a split.
- Same seed, same result (determinism check, E12).
- Fragment count and chain reactions stay within budget.
- Tear and fracture leave no holes in the visible surface at the original camera.

An entity that fails is downgraded to the `visual` level (hit flash and decals) or `none`, and the failure is recorded in its trust score (7B.5). This uses the same degradation ladder as 7B.6.

#### 17A.8 Where it runs

- **Offline (pack build):** mechanical materials, bond graph, caps, joint limits, colliders, mass and inertia, fracture sets, stress tests. Shipped as entity data (7B.4).
- **Phone:** the solver and field updates on the GPU (14.4, 20A.2), topology decisions on the CPU, fragment pools preallocated.
- Rigid fragments are cheap compared with soft bodies, so brittle destruction fits the mobile budget. Cloth tearing and jelly division are the heavier cases and sit on higher tiers.

#### 17A.9 Honest limits

- Mechanical properties are **inferred**, not known. Class defaults give plausible behavior, and odd assets need manifest overrides.
- DS meshes are thin and open, so fracture depends on the shell volume (7A.4). Meshes without usable volume fall back to non-destructible.
- Division and multiplication only make sense for some classes (jelly, swarms, summons). A dragon has no natural "split". Per-class rules decide, and the default is no.
- Physical outcomes can conflict with game logic (a monster is physically destroyed while the rules say it is alive). Keep the authority split from 17A.1, and use events as the only bridge.
- GPU determinism needs care: avoid unordered float atomics in the solver, or fix the reduction order.
- Destruction changes game feel and readability. The faithfulness slider and per-class integrity levels exist for this reason.
- Chain reactions can explode in cost. Cap them per frame.

#### 17A.10 Pitfalls

- Do not edit topology in the middle of a solve.
- Do not allocate at runtime. Use pools for fragments, particles, entities and events.
- Do not make everything destructible. Default to `visual`, and promote classes deliberately.
- Do not let child IDs depend on frame timing or thread order.
- Do not forget serialization. A broken wall that reloads intact breaks trust.
- Keep reaction rules in data, with a debug view showing which rule fired and why (provenance, 7B.6).

#### 17A.11 Suggested build order

1. Interaction events from the spatial field and per-part proxies, with mass and inertia from the volume.
2. Class defaults for `MechMaterial`, and joint limits from observed animation ranges.
3. Breakable bonds with load measurement and damage on one chain (a tail), then entity fission with caps and deterministic IDs.
4. Physical animation (tracking with hit reactions), then death as full ragdoll.
5. Field channels (heat, wetness) and the reaction table, with grass and fire as the first test.
6. Support graph and collapse for a simple structure.
7. Cloth tearing.
8. Pre-fractured brittle part with the hierarchy and energy-based activation.
9. Jelly implicit surface with division, merge and multiplication.
10. Damage atlas on the second UV set, plastic deformation and ablation.
11. The stress suite, integrity levels per class and the faithfulness tiers.


---

## Part VI. Platform and performance

_Make it run on a phone without hitches._

### 18. Android optimization

"Bridge" here means the stacked layers: emulated DS hardware, then capture and conversion, then a desktop-style OpenGL renderer, all ported to Android. Going native means collapsing those layers and designing for how mobile hardware works. Before changing anything, find out whether the engine is CPU-bound or GPU-bound: the remake features are mostly GPU work, and the emulation itself is usually the cheaper part on a modern phone.

#### 18.1 Profile first

- Use Perfetto, simpleperf and Android GPU Inspector (or the Snapdragon, Mali or Xclipse profilers) to split frame time between emulation, capture and upload, and GPU rendering.
- Set a budget for a 16.6 ms frame, for example: emulation 4 ms, capture and upload 1 ms, GPU 10 ms.
- Phones throttle quickly. Test after 10 minutes of play, not at cold start.

#### 18.2 Emulation core (CPU side)

- Recent melonDS versions have an ARM64 JIT. Check that the fork uses it and that it is built with `-O3`, LTO and PGO.
- Run emulation, rendering and audio on separate threads, with the emulation thread pinned to a big core.
- Skip the software 3D rasterizer completely. The design already replaces it, so it should cost nothing.
- Make sure halt and VBlank-wait states really sleep instead of spinning.
- Use NEON for any 2D compositing still done on the CPU, or move that to the GPU too.

#### 18.3 Remove bridge overhead (capture and upload)

- Avoid copies. Write captured vertices straight into GPU-visible ring buffers (one region per in-flight frame) instead of building intermediate CPU structures.
- Upload only what changed: textures by hash (once), palettes and matrices every frame.
- Keep simulation data (hair, cloth, grass) resident on the GPU and never read it back. Readbacks stall the pipeline on mobile.

#### 18.4 GPU side: where mobile differs from desktop

- **API level.** Desktop GL 4.3 maps roughly to **OpenGL ES 3.1/3.2** (compute, SSBO, image load/store and 3D textures are available, tessellation needs ES 3.2 or an extension). **Vulkan** has lower driver overhead and better control, and is the better long-term target if the engine will grow. Persistent-mapped buffers need `EXT_buffer_storage` on ES, so plan a ring-buffer fallback.
- **Tile-based GPUs.** Most phone GPUs are tilers. Keep the number of render passes low, avoid switching framebuffers mid-frame, and avoid full-screen passes that read and write the same large target.
- **Bandwidth is the bottleneck.** Use compressed textures (ASTC), smaller G-buffer formats, and half-float only where needed.
- **Expensive effects at reduced cost.** Run the volumetric raymarch, ray-traced shadows and reflections at half or quarter resolution with a bilateral or temporal upsample, and cap the step counts. These are the heaviest parts of the plan.
- **Do not trace what you cannot see.** Cull sims and effects by distance and screen size, and only simulate hair, grass and cloth near the player.

#### 18.5 Frame pacing, battery and heat

- Use the Android Frame Pacing library (Swappy) so frames are delivered evenly.
- Use the performance hint API (ADPF) and thermal status listeners to scale quality down before the system throttles.
- Build a **dynamic quality governor**: when frame time or temperature rises, lower the internal render scale first, then volumetric and RT quality, then simulation density.
- Offer quality tiers (Low, Medium, High) with per-device defaults, since phones vary widely.

#### 18.6 Decouple emulation rate from render rate

- The DS runs at about 60 Hz, and many games update logic at 30. Emulate at native rate and render separately. If the GPU cannot keep up, render at 30 and interpolate, or interpolate up to a higher refresh rate on high-refresh screens.
- Heavy effects (RT, volumetrics) can update at lower rates than the base image, for example every second frame with reprojection.

#### 18.7 The real "no bridge" option: a game-specific port

If the goal is a single game (for example Dragon Quest Monsters: Joker), the most native path is to stop emulating the CPU at all:

- **Static recompilation** of the ARM9 code into native ARM64, or a **full decompilation** to source (like the Pokemon decomp projects), so the game logic runs as native code with your renderer on top. This removes CPU emulation cost entirely.
- The cost is very high: it is per-game reverse engineering that takes a long time, so it only makes sense once the rest of the project is proven.
- Same rule as before: load from the user's own ROM data, and do not ship original code or assets.
- This is one of the methods for the logic-lifting steps in section 7C.4, next to table scanning, differential testing and script interpreters.
- Treat it as the **last resort**: first subtract library code and take over the game's boundaries with seams (section 7D), and decompile only the small remainder that is left.

#### 18.8 Suggested build order

1. Profile on the target phone and find the CPU/GPU split.
2. Fix capture and upload overhead (ring buffers, hashing, no readbacks).
3. Add the quality governor and tiers, with half-resolution volumetrics and RT.
4. Move to Vulkan if driver overhead or ES limits hurt.
5. Consider the decompilation path only if the CPU side is still the bottleneck.

### 19. Shader warm-up, pipeline cache, texture compression and memory budget

Two engine-level ideas that belong together: both run at map load and both exist to remove hitches and memory spikes on phones.

#### 19.1 Shader warm-up and pipeline cache

**The problem.** OpenGL drivers often compile and link lazily, so the first draw that uses a new material variant stalls. On a phone this shows up as a hitch in the middle of a battle or when entering an area.

**Plan**

1. **Define a variant key.** A variant is the class profile plus feature flags: skinned, alpha-cutout, fog, wind, shadow, RT on or off. The full combination count explodes, so do not precompile everything.
2. **Log variants actually used.** The coverage logger (11.10) records which variant each draw used. Save the result as a per-map variant list in the asset pack.
3. **Precompile at load.** When a map loads, compile and link its listed variants behind the map transition (the masked load in section 10). Use a second shared GL context on a worker thread, with `KHR_parallel_shader_compile` where available. If you cannot compile ahead, force compilation by drawing each variant once into a tiny offscreen target.
4. **Persist the binaries.** Save linked programs with `glGetProgramBinary` and reload them next launch. Key the cache by GPU, driver version and your shader hash, and discard it whenever any of those change, because binaries from a different driver version can fail to load. Keep a normal compile path as the fallback. After a Vulkan migration (section 18) the same role is played by `VkPipelineCache`.
5. **Fall back gracefully.** If a variant is missing at runtime, draw that object with a cheap generic variant for a few frames while the real one compiles in the background, instead of stalling.

**Risks:** stale or invalid cache files, and a first launch that still compiles everything. Handle the first launch with a one-time "preparing graphics" step.

#### 19.2 GPU texture compression and memory budget

**Why it matters.** A 4096x4096 RGBA8 texture is 64 MiB, plus about a third more for mipmaps. The same texture in ASTC 4x4 or BC7 is about 16 MiB, and ASTC 6x6 is about 7 MiB. A pack of HD replacements would otherwise exhaust phone memory, which on mobile is shared with the emulator itself.

**Plan**

1. **Ship one pack, transcode per device.** Encode replacements to a supercompressed format (KTX2 with Basis Universal) and transcode at install or load to ASTC on mobile, BC7 on desktop, and ETC2 as the baseline fallback. One pack then serves every device.
2. **Pick formats by class.** ASTC 4x4 or 6x6 for albedo, a two-channel format for normal maps (BC5 on desktop, ASTC with an RG layout on mobile), a single-channel format for roughness and height.
3. **Skip compression where it hurts.** Pixel-art sprites, UI and hard-edged palette art show block artifacts and fringing (11.2). Keep those uncompressed or at the highest quality, since they are small anyway. Use texture arrays for extruded tilesets.
4. **Generate mipmaps offline,** as part of the same pack build, not at runtime.
5. **Set a per-map budget** in the manifest, scaled by a device tier from the quality governor (section 18).
6. **Stream by importance.** Use the screen-area numbers from the coverage logger (11.10) to decide each asset's resolution tier, so the budget goes to what players see most. Upload textures through a background context with a time limit per frame, and keep a least-recently-used cache with mip-level streaming.

**Risks:** slow high-quality ASTC encoding (do it offline at pack build, never on the device), visible quality differences between formats (check them in the A/B view), and memory spikes during map transitions.

#### 19.3 How they combine

Both run at map load behind the same masked transition: read the map's variant list, compile and link the shaders, then upload the textures in priority order, with what is on screen first. The quality governor picks the tier for both. One shared "map prep" step is easier to build and test than two separate loaders. Section 20A.4 generalizes this into virtualized memory with prefetch, a full-preload mode and precomputed caches.

#### 19.4 Suggested build order

1. Add variant logging to the coverage tool and save a per-map list.
2. Add the program-binary cache with a safe fallback.
3. Build the KTX2 pipeline with one desktop and one mobile target.
4. Add the per-map budget and priority streaming.
5. Measure frame-time spikes on the target phone before and after, using the profiling setup from section 18.

### 20. Engine internals: structure, threading, resilience and measurement

Engine-level ideas only, with no gameplay features. Each one is tied to a problem the document already raises, and the "Builds on" column names the sections involved. Ideas are numbered E1 to E13 so they can be referenced.

#### 20.1 Structure and portability

| # | Idea | Builds on | Problem it addresses | Change | Gain | Risk |
|---|---|---|---|---|---|---|
| E1 | Render graph | Pass list (2.2, 2.3), quality tiers (18.5, 11.7), tile-GPU pass count (18.4) | The frame is a fixed chain of passes (G-buffer, lighting, RT, post), and every quality tier switches some of them off. Doing that by hand in code gets fragile. | Declare each pass with its inputs and outputs. Let the engine order passes, insert barriers, alias temporary targets and skip passes whose output nothing reads. | Tiers become "disable this pass". Fewer render targets on tile GPUs. A later Vulkan move is easier. | Upfront design cost. Keep it to a small declarative layer, not a framework. |
| E2 | Thin capture ABI | melonDS hook (2.1), open questions (25), version and region limits (5.11) | Hook names and GPU3D write paths differ between melonDS versions, and everything that hooks game code is version-specific. | Put every hook behind one small interface (matrix events, polygon batches, VRAM uploads, FIFO and DMA writes, call-site info). Nothing else in the engine touches melonDS internals. | A melonDS upgrade means fixing one layer. Hooks can be tested against recorded input. | The interface must stay small, or it becomes a second copy of melonDS. |
| E3 | Raw fixed-point alongside float | Float effect stack (5.4), display-list hashing (5.2), RAM-map discovery (5.2), parity oracle (5.6) | RAM-map discovery and display-list hashing need the exact DS values, while rendering wants floats. Converting early loses the exact bits and adds rounding noise to every comparison. | Keep the original fixed-point values next to the converted floats in captured data. Hash and correlate on the raw values, render from the floats. | Exact matching for IDs and RAM search, and rounding-free parity checks. | Roughly double the captured data per vertex or matrix. Keep raw values only where they are used. |
| E4 | Capability probing and fallback matrix | ES versus GL (18.4), `EXT_buffer_storage` fallback (18.4), program-binary cache (19.1) | Phones differ in features, extensions and driver bugs, and a failed shader or missing extension should not crash the game. | At startup, probe features and known-bad driver versions, then pick a safe path per feature (persistent mapping or ring copy, compute or raster fallback). A failed variant falls back to the generic one. | One build runs on many devices, and failures degrade instead of crash. | The matrix grows with every feature. Keep a short, tested list. |
| E5 | Uber-shader per class family | Variant explosion (19.1), class-to-profile table (7.3) | Every class, skinned, cutout, fog, wind, shadow and RT combination multiplies the variant count the warm-up has to compile. | Use one shader per class family. Branch on a class ID where it is uniform across the draw, and use specialization constants only for flags that change rarely. | A much shorter variant list and faster warm-up. | Branching cost on some mobile GPUs. Measure on the target phone. |
| E6 | One BVH per unique mesh | Static and dynamic BVH split (4.3), display-list fingerprinting (5.2) | Fingerprinting already tells you which meshes are identical, but a per-instance BVH would rebuild the same structure many times. | Build one bottom-level BVH per unique mesh and reference it from instances through a transform. Refit only for skinned meshes. | Ten identical trees cost one build, and BVH memory drops. | Needs an instance-aware tracing loop. |

#### 20.2 Threading and latency

| # | Idea | Builds on | Problem it addresses | Change | Gain | Risk |
|---|---|---|---|---|---|---|
| E7 | Lock-free snapshot ring | Separate threads (18.2), per-frame ring buffers (18.3), interpolation and look-ahead (5.6) | The emulator thread produces frames and the renderer consumes them. Interpolation needs access to two consecutive frames, and locks or per-frame allocation cause random hitches. | Use a single-producer, single-consumer ring of numbered, immutable scene snapshots in preallocated memory. The renderer picks any two snapshots to interpolate between. | No locks, no per-frame allocation, and interpolation and one-frame look-ahead fall out naturally. | Ring depth trades latency against safety. Keep it at two or three. |
| E8 | Task graph with core hints | Pinned emulation thread (18.2), CPU BVH build and refit (13.2), hashing (18.3) | BVH refits, hashing, texture conversion and streaming all compete with the emulation thread. | Express that work as small jobs with explicit dependencies, and give each a core hint (emulation on a big core, background conversion on little cores). | The emulation thread stays unblocked, and background work uses idle cores. | Phone schedulers move threads anyway. Verify with traces. |
| E9 | Late latching and reprojection | Tilt parallax (9.1), lower-rate heavy effects with reprojection (18.6) | Tilt parallax feels wrong if the sensor value is a frame old, and heavy effects already update at a lower rate. | Sample the sensor as late as possible, and apply a cheap warp of the last frame to the newest camera pose before presenting. | Lower perceived latency for parallax and for the free camera. | Disocclusion at screen edges. Keep the warp small. |

#### 20.3 Resilience and measurement

| # | Idea | Builds on | Problem it addresses | Change | Gain | Risk |
|---|---|---|---|---|---|---|
| E10 | Device-lost recovery | Scene IR (3), manifest (6.4), Android (18) | On Android the GL context can be lost when the app is paused or the surface changes, and losing every GPU resource would otherwise force a restart. | Treat all GPU resources as rebuildable from the Scene IR, the manifest and the asset pack. On context loss, recreate them in priority order and resume. | Backgrounding or rotating the phone no longer risks the session. | Needs discipline: no GPU-only state that cannot be rebuilt (sims need a CPU-side seed). |
| E11 | Per-pass GPU timers | Profiling (18.1), quality governor (18.5), self-explaining tiers (21.5) | The governor and the tier overlay need real per-effect costs, not guesses. | Wrap every pass in timestamp queries where supported, keep a rolling average, and give each effect a declared cost and fallback. Feed the numbers to the governor and show them in the overlay. | The governor drops what is actually expensive on this device, and tiers can show real costs. | Some mobile GPUs report timers poorly. Fall back to coarse frame time. |
| E12 | Deterministic mode | A/B split view (11.10), TAA jitter (11.3), parity oracle (5.6), IR recorder (22.5) | A/B views, replays and regression checks are only trustworthy if the same input gives the same output. | Add a switch that fixes every random seed, jitter pattern and sim time step, and disables adaptive quality. | Bit-comparable runs for tests, the parity oracle and the A/B view. | Differs from the shipped look slightly. Use it for testing only. |
| E13 | Visibility buffer (conditional) | G-buffer formats and bandwidth (18.4), pipeline (2.3) | G-buffer bandwidth is the main GPU cost on tile-based phones. | Store only triangle and instance IDs in the first pass, and shade each pixel once in a later pass. | Less overdraw and bandwidth. | A large architecture change. Only worth it if profiling shows bandwidth is the limit after smaller formats and fewer passes. |

#### 20.4 Considered and left out

- **GPU-driven culling and indirect draws.** Originally left out, because a DS frame holds about 2048 polygons and the real costs were emulation, capture and upload. That changed once the engine accumulates world geometry and adds rework volume past the 64k mark, so it is now covered in section 20A.3. The CPU path stays for small captured frames.

#### 20.5 Suggested order

1. E2 (thin capture ABI) and E3 (raw fixed-point). Cheap now and costly to retrofit.
2. E7 (snapshot ring) and E1 (render graph), because later passes and interpolation depend on them.
3. E11 (GPU timers) and E4 (capability probing), before the quality governor and tiers.
4. E12 (deterministic mode), once the IR recorder and A/B view exist.
5. E5 (uber-shader) and E6 (one BVH per mesh), when the variant list and RT cost show up in profiling.
6. E8 (task graph), E9 (late latching) and E10 (device-lost recovery) for the Android pass.
7. E13 (visibility buffer) only if profiling asks for it.


### 20A. Engine substrate: spatial field, GPU-driven geometry and virtualized memory

Three engine-level systems that share one idea: **pay the cost once, offline or at load, and make the frame a lookup.** They also remove the DS-era assumptions that sections 18 to 20 still partly carry (a few thousand polygons per frame, everything resident, everything CPU-submitted). Once the engine accumulates world geometry (4, 5.6, 8.2), adds rework volume (7A) and removes the polygon cap (7C.5), these assumptions break.

| System | Replaces | Main gain |
|---|---|---|
| Spatial field service (20A.2) | Separate structures for RT, collision, ambient light, audio occlusion, navigation and grass interaction | One resident structure, built once, shared by all |
| GPU-driven geometry (20A.3) | Per-draw CPU submission | Millions of triangles resident, only visible clusters drawn |
| Virtualized memory and preload (20A.4) | Load-everything or load-on-demand with hitches | Everything addressable, resident by priority, no frame waits on storage |

#### 20A.1 Why now

- The frame budget used to be dominated by the emulator and capture. With accumulated and reworked geometry, the cost moves to **scene size**: draw submission, memory, and per-system spatial structures.
- High quality settings must stay smooth. That only works if expensive things become lookups into precomputed or resident data, not per-frame computation.
- Section 20.4 had left out GPU-driven culling, because a DS frame holds about 2048 polygons. That reasoning no longer holds for the accumulated world scene. The CPU path stays for small captured frames (see 20A.3).

#### 20A.2 Spatial field service

**Idea.** Today RT (13), collision (14.4), ambient light (4.4), audio occlusion (16.6), navigation and grass interaction (17.3) would each build their own spatial structure from the world scene (4). Instead, build **one GPU-resident, multi-resolution field** and let every system query it.

**Structure**

- **Sparse bricks.** Distance values stored in small bricks (for example 8 by 8 by 8) in one brick atlas, with an indirection texture mapping world cells to bricks. Empty space costs almost nothing.
- **Cascades.** Several resolutions: fine near the player, coarser further out, plus a coarse global level. Voxel sizes are budget settings (18.5).
- **Layers on the same pages:**
  - *Static world layer.* Baked from the world scene at pack build (7B.4), streamed as pages (20A.4).
  - *Dynamic entity layer.* Each entity ships its own local distance field (`volume.sdf` in the entity record, 7A.3). A small top-level grid over instances lets a query find the entity fields it touches. Skinned entities use per-part fields or capsule proxies from the recovered skeleton (5.2), updated per frame.
  - *Writable data layers.* Trample and occupancy (grass), wetness, heat, charge and other field channels used by the interaction rules (17A.2), scorching, fog density, world memory (16.7), acoustic absorption. These live in the same brick pages.
  - *Radiance cache.* World-anchored probes (4.4) on the same cascades, storing irradiance plus a visibility term taken from the distance field, to limit light leaking.
- **Exact geometry stays available.** The BVH (13, E6) remains for tasks that need exact hits. The field handles everything that tolerates a distance estimate.

**Query interface (the same for every consumer)**

```
field.sdf(p)              // signed or shell distance at a point
field.grad(p)             // surface normal and push-out direction
field.trace(ro, rd, tmax) // sphere trace, with cone width for soft results
field.radiance(p, n)      // cached irradiance, visibility-weighted
field.occlusion(a, b)     // line-of-sight and soft occlusion between two points
field.layer(id).sample(p) // trample, wetness, fog, absorption, memory
field.clearance(p, r)     // free radius for navigation and spawning
```

**Who uses it**

| Consumer | Query | Replaces |
|---|---|---|
| Soft shadows, AO, rough reflections (13) | `trace`, `occlusion` | A separate ray structure for non-exact effects |
| Cloth, hair, soft bodies, particles (14.4, 17) | `sdf`, `grad` | Per-target SDF textures built separately |
| Ambient light and probes (4.4) | `radiance` | Separate probe grid and its own visibility test |
| Audio occlusion and reverb (16.6) | `occlusion`, `trace` | A separate geometry query for sound |
| Navigation and spawning (7C.6) | `clearance`, `sdf` | A separate navmesh bake, at least as a first version |
| Grass and foliage interaction (17.3) | `layer(trample)` | A standalone interaction map |
| Volumetric fog and world memory (12.6, 16.7) | `layer(...)` | Standalone density and trace maps |

**Building it from DS meshes.** DS meshes are often open, double-sided, thin and not watertight, so a plain signed distance bake fails. Use conservative voxelization plus a GPU distance transform (jump flooding). Take the sign from ray parity or winding number where the mesh is closed, and treat open or thin meshes (cloth, leaves, banners) as **shells of a stored thickness** (the thickness from the volume stage, 7A.5).

**Updates.** The static layer never changes at runtime. The dynamic layer re-splats only entities that moved or animated, and only the dirty bricks. This uses the frame-coherent caching idea from 5.6. A per-frame budget caps the work.

**Limits**

- Thin features smaller than a voxel vanish or leak. Shell thickness and per-cascade voxel size need tuning.
- Distance fields give soft, approximate results. Sharp mirror reflections and exact shadows still use the BVH.
- Skinned dynamic fields are the most expensive part. Use proxies and keep the entity budget tight.
- A radiance cache can leak light through thin walls. The visibility term helps but does not solve it.
- Mobile GPUs have limited 3D-texture bandwidth. Use low-resolution cascades and 8-bit distance on the Low tier.

**Pitfalls**

- A single shared structure is a single point of failure. Version it, and keep a simple fallback per consumer (capsule or plane collision, no occlusion) for when the field is unavailable or invalid.
- Do not let consumers write into static layers. Only writable layers accept writes.
- Keep field build deterministic (E12), so results are cacheable by hash (7B.4).

#### 20A.3 GPU-driven geometry

**Problem.** Once the scene holds far more than 64k triangles (accumulated world, reworked volume, replacement props), issuing draws from the CPU and testing visibility on the CPU becomes the limit.

**Idea.** Store meshes as **clusters** and let the GPU decide what to draw.

- **Clusters.** Split each mesh into small clusters (for example 64 to 128 triangles) with bounding volumes. Build a **cluster hierarchy with LOD groups**: simplified parent clusters with an error value, chosen by projected screen-space error. Build this at pack build (7B.4), not at runtime.
- **Frame, all on the GPU:**
  1. Instance culling (frustum, distance, projected size, class flags).
  2. Cluster culling and LOD selection, writing a visible-cluster list.
  3. Occlusion culling against a hierarchical depth buffer (two passes: reuse last frame's depth, then test what was missed).
  4. Indirect draws from the visible list.
  5. Material lookup through the material table (2.3); texture access through virtual texturing (20A.4).
- **Simulated geometry.** Cloth, hair and soft bodies write straight into vertex buffers on the GPU, and bounds are computed on the GPU, so nothing is read back (18.3). Skinning runs as a compute pre-pass into a deformed vertex buffer, using bone matrices from the snapshot ring (E7).
- **Captured DS geometry.** Frames captured live (a few thousand polygons) go through a ring buffer, with one simple cluster per submitted batch and no hierarchy. Below a triangle threshold, the plain CPU path stays in use (20.4).
- **Shadows and RT.** Shadow cascades reuse the same culling per light. BVH builds take a chosen LOD per mesh (E6).

**Scene IR.** A mesh in the Scene IR (3) becomes a cluster set with its LOD hierarchy and material references, and the scene holds an instance table. Backends that cannot do GPU-driven drawing (older GL, a 3DS profile) get the IR flattened to a fixed LOD at pack build or load.

**Limits**

- OpenGL ES 3.1 has compute and indirect draws, but multi-draw indirect depends on extensions, and mesh shaders are not in core GL or ES. Plan for per-batch indirect draws on ES, and treat Vulkan (18.4) as the better long-term target.
- Hi-Z occlusion adds passes, which tile-based GPUs dislike (18.4). Make occlusion culling a tier option.
- Compute culling has a fixed overhead that small scenes do not repay. Switch by triangle and instance count.
- A visibility buffer (E13) pairs well with this, but stays conditional on profiling.

**Pitfalls**

- LOD popping from bad error metrics. Test with the A/B view (11.10) and the replay regression (7B.6).
- Cluster building for open, thin meshes can split cloth badly. Keep simulated meshes at fixed LOD.
- Keep the CPU path working. It is the fallback and the reference for tests.

#### 20A.4 Virtualized memory, storage and full preload

**Principle.** Everything is addressable by content hash (7B.4). **Residency is a cache decision, not a loading decision.** A frame never waits for storage, shader compilation or computation. A missing piece costs *quality* (a coarser mip or LOD for a few frames), never *time*.

**What is paged**

| Resource | Page unit |
|---|---|
| Textures | Virtual-texture tiles (for example 128 by 128) |
| Geometry | Cluster pages (20A.3) |
| Spatial field | Bricks and probe blocks (20A.2) |
| Precomputed caches | Navigation tiles, bake blocks, rest-state sims |
| Animation and audio | Clips and banks |

**Tiers**

| Tier | Holds | Notes |
|---|---|---|
| VRAM pool | Hot pages | Fixed-size pool with an indirection (page table) texture. Sparse resources if available, otherwise a manual pool |
| System RAM | Decoded or transcoded pages | Warm cache, filled ahead of need |
| Compressed RAM (optional) | Cold pages in LZ4 or similar | Cheaper than storage, for small devices |
| Storage | The full pack, memory-mapped | Page-aligned, chunked, ordered by access pattern |

**Feedback-driven residency.** The GPU writes the page IDs it needed this frame (virtual-texture feedback, cluster requests, brick requests) to a small buffer. An IO thread reads it with a delay of a few frames, loads missing pages, and evicts by least recent use weighted by importance (7B.5 heat map). The **coarsest level of everything is pinned**, so a miss always has something to show.

**Prefetch sources (predict, don't react)**

| Source | What it predicts |
|---|---|
| World graph and map links (10) | Neighbor maps and chunks by distance and direction |
| Event grammar (7B.6) | Battle start loads the battle set; menu open loads UI pages |
| Lifted encounter tables (7C.4) | Exactly which monsters can appear in this area, so their full assets are preloaded |
| Play-trace heat map (7B.5) | Which assets are likely at all, and in what order |
| Camera motion | Pages in the direction of travel |
| Deterministic look-ahead (5.6) | Objects and state a few frames ahead |

**Full-preload mode.** Residency policy is a setting, chosen by memory budget and device tier (18.5):

| Mode | Behavior | For |
|---|---|---|
| Streaming | Small pools, aggressive eviction, load by feedback and prefetch | Low-memory phones |
| Hybrid (default) | Current region, likely battle sets and neighbors resident, rest streamed | Most devices |
| Full preload | Load the whole current region, all possible encounter assets and all caches into RAM, then promote to VRAM by priority, so storage is not touched during play | Desktop and high-memory phones |

Higher quality tiers mean *more resident detail*, not *more per-frame work*, which is why a high setting can stay smooth.

**Precomputed calculation.** The most expensive results are computed ahead and cached by hash:

| When | What |
|---|---|
| Pack build (offline) | Cluster hierarchies, static spatial field and radiance bake, navigation data, KTX2 mips, settled rest-shapes for cloth and soft bodies, shader variant lists (19.1), fold maps and rework data (7A) |
| First launch or install | Transcode textures to the device format (19.2), build the shader and pipeline cache (19.1), device-class caches |
| Load and idle time | Fields and probes for neighbor regions, dynamic caches, warmed sims (settled start state instead of a first-frame settle) |
| Frame | Lookups and small incremental updates only |

Caches are keyed by `(content hash, pipeline version, device class)` and discarded when any part changes.

**Storage layout.**
- Page-aligned chunks, compressed per chunk, memory-mapped.
- Chunk order follows the play-trace (7B.5), so common transitions read sequentially.
- Asynchronous reads on a fixed IO thread pool, with checksums.
- A per-frame **upload budget** (bytes per frame, through persistent-mapped staging buffers, 18.3) so uploads never spike.

**One budget.** A memory budget manager owns VRAM, RAM and cache pools. The quality governor (18.5) resizes pools. It reacts to the OS memory-pressure signal and to context loss (E10): everything is rebuildable from the pack, so shrinking or losing a pool is safe.

**Limits**

- Precomputed fields, high-resolution tiles and caches are large. Offer optional quality packs, and let the engine run at a lower mode when they are missing.
- Full preload only fits on devices with enough RAM. Do not make it the default on phones.
- Preloading costs time and battery at load. Hide it behind the masked transition (10, 19.3) and spread it over idle time.
- Feedback reads arrive a few frames late, so fast camera cuts can show coarse detail briefly. Prefetch and pinned coarse levels reduce it.
- A pipeline-version change invalidates caches and forces a rebuild. Keep rebuilds incremental (7B.6).

**Pitfalls**

- Never block a frame on readback or IO. Use fences and asynchronous mapping (18.3).
- Do not preload blindly. Use the heat map and the encounter tables, or the preload becomes the new hitch.
- Thrashing when pool size sits near the working set. Add hysteresis to eviction and watch page-in rate in the overlay.
- Keep caches deterministic and versioned, or a stale cache looks like a rendering bug.

#### 20A.5 How they combine

```
Pack on storage (content-addressed, page-aligned)
  -> Residency manager (feedback, prefetch, budget, tiers)
      -> Pages: texture tiles, cluster pages, field bricks, caches
          -> GPU-driven culling and draw (clusters, LOD, Hi-Z)
          -> Spatial field service (SDF, radiance, layers)
              -> RT, collision, light, audio, navigation, grass, fog
```

Clusters, tiles and bricks use the same page and residency system, so there is one prefetcher, one budget and one set of debug views instead of three.

#### 20A.6 Suggested build order

1. Page table, pool allocator, IO thread and upload budget, tested on textures first (extends 19.2).
2. Feedback buffer and pinned coarse levels, with a debug overlay for page-in rate and misses.
3. Content-addressed cache with versioning, then precompute passes moved into the pack build.
4. Prefetch from the world graph, then from events and encounter tables.
5. Cluster format and offline builder, then GPU culling and indirect draws with the CPU path kept as fallback.
6. Static spatial field bake and one consumer (cloth collision, 14.4), then occlusion and soft shadows.
7. Dynamic entity layer, writable layers (grass trample first), then the radiance cache.
8. Move the remaining consumers (audio occlusion, navigation, fog) onto the field.
9. Hybrid and full-preload modes with per-tier defaults, validated by frame-time histograms after 10 minutes of play (18.1).


---

## Part VII. Expansion

_Extra features and multi-game fusion, once the single-game pipeline is proven._

### 21. Extra ideas

Since the renderer and data pipeline are custom, these become possible once the limits are gone.

#### 21.1 Make the monsters feel alive

- **Living ecosystem in the open world.** Wild monsters wander, sleep at night, flee from stronger ones, and gather near water or food. The game's own encounter tables still decide the actual fights, so this is only visible life, but it makes the world feel real.
- **Monsters with fur, scales and slime that react.** Use the shell and 3D-texture techniques from section 14 on each species: fur ruffles in wind, slime wobbles with a soft-body sim, skin looks wet after water.
- **Eyes that follow.** Procedural gaze on every monster and NPC, so they look at the player, at other monsters, or at whatever just happened.
- **Idle behaviors from simple state machines.** Yawn, scratch, stretch, nap, so nothing ever stands in a frozen pose.

#### 21.2 Weather and time

- **Real day/night and weather cycle** driven by the real sky (section 12): rain that wets surfaces and darkens albedo, puddles that reflect via ray tracing, snow that accumulates on upward-facing geometry, fog banks that roll through valleys.
- **Lightning that lights the whole scene** with a real shadow pass.
- **Seasons** as palette and foliage-density swaps per map.
- **Day/night monster variants.** Some species only appear or look different at night, using the game's own flags where they exist.

#### 21.3 Camera and presentation

- **Photo mode** with free camera, depth of field, focal length and filters, plus a "monster photo album" you fill by photographing species.
- **Cinematic battle cameras.** Since the battle state is known, add dynamic framing, slow motion on critical hits, and a replay of the last turn.
- **Monster showcase screen.** Rotate any monster in a studio with proper lighting, view its animations and see its stats.
- **Diorama mode.** Tilt-shift miniature look on the whole world, which suits a DS-era toy-like art style.

#### 21.4 Physics playground

- **Destructible or interactive props.** Grass you can cut, bushes that shake, crates that tip, water that splashes with foam and ripples when anything enters it. The engine support for these (bonds, fracture sets, reaction rules) is in section 17A.
- **Cloth and capes on everything,** hair that reacts to wind and speed, loose accessories that jiggle.
- **Footprints** in sand, snow and mud that fade slowly, using the same interaction-map trick as the grass trample (section 17.3).

#### 21.5 Quality of life

- **Generated world map and minimap** from the stitched graph (section 10), with fast travel to visited nodes.
- **Controller and touch layouts** that redraw the bottom-screen UI natively and add a camera stick.
- **Per-game "look" presets:** Original (pixel-faithful), Enhanced, Cinematic, and a purely stylized one such as cel-shaded or watercolor.
- **Quality tiers that explain themselves.** A small overlay showing what each setting costs per frame, which helps on mobile.

#### 21.6 For modders and fans

- **Live reload of the manifest and shaders** while the game runs, so asset swaps show up instantly.
- **Community asset packs** keyed by game and region hash, with a one-click loader and no original data inside the pack.
- **Debug "x-ray" view** that colors everything by class, asset ID and sim type. Useful for development and fun to browse.
- **Stylized replacement packs:** low-poly, claymation, voxel or hand-painted looks that you swap in with a setting.

#### 21.7 Wildly ambitious

- **VR or stereoscopic mode.** The scene is already rendered by the engine, so a second eye is mostly camera work, and a battle viewed up close in VR would be something special.
- **Multiplayer spectating or shared worlds,** using the open-world graph so two players see each other's monsters wandering nearby. Needs careful design around the game's own state, so leave it for last.
- **Accessibility layer:** high-contrast mode, dyslexia-friendly text rendering (text is already re-rendered), color-blind palettes, and audio cues for off-screen events.

### 22. Improvements to existing ideas

Each entry sharpens an idea that is already in this document. Nothing here is a new feature. The first column names the section being improved.

#### 22.1 Capture and identification

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Replacement manifest (6.4), override workflow (7.6), multi-game IDs (23) | Key every manifest entry by ROM hash plus a namespaced ID (`dqmj1:monster_047`) from day one, and let entries inherit from a parent profile (all slimes share a base). | Avoids retrofitting namespaces for multi-game later, and cuts per-monster manual work. | Long inheritance chains are hard to debug. Show the resolved entry in the inspector. |
| Classifier fusion weights (7.1) | Fit the weights against the override file. Every manual override is ground truth, so tune the weights with a plain grid or coordinate search and report accuracy per evidence layer. | Weights match each game instead of staying a starting guess. | Overfitting to the few assets you overrode. Hold some out for testing. |
| Capture and upload overhead (18.3) | Hash textures, palettes and display lists when they are uploaded to VRAM or the FIFO, not at draw time, and keep a table from VRAM slot to hash. | Per-frame hashing becomes a table lookup, which helps the mobile CPU. | Games that rewrite VRAM in place. Invalidate the entry on write. |
| Debug overlays (7.6), call-site and polygon-ID views (5.2, 5.3) | Merge the class view, asset-ID overlay, call-site view, polygon-ID view and world-space coordinates into one inspector with a layer picker and click-to-inspect. | One tool for every hook experiment, instead of one overlay per idea. | Low. Keep each layer cheap when switched off. |
| Hair sim colliders (17.4), skeleton recovery (5.2) | Fit capsule colliders to the recovered skeleton automatically, instead of placing the sphere sets by hand. | Less per-monster work, and colliders follow the real body. | Poor fit on odd silhouettes. Allow a manual override. |

#### 22.2 Rendering

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Ray-traced effects (13.2) | Let the material class set the ray budget per pixel: reflection rays only on water and metal, shadow rays everywhere, AO rays only near contact. | Fewer rays for the same visible result. | Class errors show as missing reflections. `Unknown` safely falls back to shadows only. |
| Volumetric raymarch (14.2) | Skip empty space with a low-resolution occupancy mip, adapt the step count to density, jitter with blue noise and accumulate over frames. | Same look with fewer steps, and no banding. | Temporal ghosting on fast motion. |
| Sky, fog and aerial perspective (12) | Drive sky, fog and aerial perspective from one cached LUT, and let the ambient probe read the same LUT. | Lighting and haze always agree, and updates are cheaper. | Banding near the horizon at low LUT resolution. |
| World-anchored probe grid (4.4) | Update probes by priority: changed cells first, then visible ones, with hysteresis so cells at the edge of the relevance region do not flicker in and out. | Faster convergence and fewer pops. | More bookkeeping. |
| Texture upgrade tooling (7.5, 11.2) | Cache processed textures by key (texture hash plus profile version) so a pack rebuild reprocesses only what changed. Add a quality gate that compares each result with a plain upscale of the original using SSIM, to flag over-processing. | Fast iteration and fewer silent mistakes. | SSIM is a rough proxy. Flag results, do not reject them automatically. |
| Sprite extrusion and tile-to-prop (15.2) | Use palette-index masks to give sprite parts different thickness and normals (eyes, trim, cloth), and reuse a tile-to-prop dictionary across tilesets by matching tile hashes. | Richer extrusion for very little extra work. | Index meaning varies between textures. |

#### 22.3 Camera and world

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Retained scene (8.2), ghost cameras (5.6) | Fade stale retained objects out over a short time instead of dropping them, and prefer ghost-camera results cached at map load over per-frame forks. | No popping, and a much lower CPU cost. | Stale shadows can linger briefly. |
| Camera modes (8.4) | Smooth the camera with critically damped springs, and pick shot presets from the game state (exploring, battle, shop). | A steadier camera that reuses the state reader you already need. | Presets need tuning per game. |
| Link discovery (10.1) | Read static map headers first where available. Use the dynamic walk only to validate them and to catch script warps, and draw the loop-closure error in the debug view. | A faster and more complete graph, with conflicts visible. | Static parsing is per-game work. |
| Streaming and boundary handoff (10.4, 10.5) | Prefetch neighbor chunks toward the exit the player is heading to, using velocity, and measure the real load time to size the masked fast-forward. | Shorter masked loads. | Wasted loads when the player turns back. |
| Transform interpolation (5.6) | Detect cuts with matrix jumps plus animation-frame discontinuities, key interpolation to the game's tick phase (30 or 60 Hz logic), and interpolate the camera separately from objects. | Fewer artifacts at cuts and turns. | Games with irregular tick rates. |

#### 22.4 Simulation

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Hair, cloth and grass sims (17, 14.4) | Run each sim type as one batched dispatch over all instances. Let instances sleep when their velocity is under a threshold, and wake them on camera distance, wind change or contact. | Fewer dispatches and less GPU work, which matters most on mobile. | Wake-up bugs look like frozen hair. |
| Wind field (17.2) | Store regional wind as a world-anchored 3D vector field that game events can inject into (spell effects, explosions), instead of one global function plus special cases. | Wind reacts to what happens in the game. | Needs an event mapping per game. |

#### 22.5 Platform and tooling

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Quality governor (18.5) | Control it with hysteresis or a simple PID on frame time instead of fixed thresholds, predict cost from scene complexity, and use the game state so menus and cutscenes run at lower quality than combat. | No quality oscillation, and the response comes before a spike. | Needs tuning, and wrong state detection looks like stutter. |
| Scene IR (3) | Version the IR schema and add a recorder that writes IR frames (as deltas) to disk and replays them without the emulator. | Offline debugging, benchmarks and regression tests, and an automated check for the animation maps and classifier. | File size. Record deltas, not full frames. |
| Shader warm-up list (19.1) | Log the variants used per map during normal play and merge the logs from many sessions into the shipped list. | The warm-up list improves over time, so fewer first-use hitches. | Low. Keep the logs small. |

#### 22.6 Planning

| Existing idea (section) | Improvement | Gain | Risk |
|---|---|---|---|
| Roadmap (24) | Give each step an exit criterion and a frame-time budget, and gate each step on an A/B coverage check, as the work-steps table at the top does for each part. | Progress is measurable and regressions show early. | Criteria that are too strict stall early steps. |

#### 22.7 Suggested order

1. IR recorder, and namespaced ROM-keyed manifests. Cheap, and much else depends on them.
2. Hash on upload, and the unified inspector.
3. Classifier weight fitting, and automatic colliders from the recovered skeleton.
4. Interpolation cut detection, and sim batching with sleeping.
5. RT budget by class, raymarch optimizations, probe priority, texture cache with the quality gate.
6. Camera presets, retained-object fading, chunk prefetch, link discovery from static headers.
7. Governor hysteresis, shader-variant log merging, roadmap exit criteria.


### 23. Fusing several games into one world

"Same world" can mean three very different levels of fusion. Each level has a different cost.

#### 23.1 Three levels of fusion

**Level 1: one shared world, one game active at a time (realistic)**

- Each game keeps its own logic and runs in its own emulator instance. The engine owns a **meta world graph** that places each game's maps as regions on one global map (section 10).
- Walking into another game's region triggers the same masked handoff as a map boundary: the old game is paused or saved, the new one is loaded with the matching state, and the renderer keeps drawing continuously.
- Visually it feels like one world. Mechanically, it is still separate games.

**Level 2: several cores running at once**

- One emulator instance per game runs the area around the player. Each instance feeds its captured scene into the same world scene (section 4), so both regions render together, including at the seam.
- Works for two games like DQMJ 1 and 2 if the CPU allows it (see section 18 for the Android budget). It does not scale to a dozen Pokemon games.

**Level 3: a unified game (the real fusion)**

- Game logic moves out of the emulators into the engine: a canonical species table, one battle system, one item and move database, one save format. The original games become **data sources**, not running programs.
- This is the semantic remake (tier 3, section 6) applied to several games. It is the only level where a DQMJ monster can truly meet a Pokemon, and it is a very large project. Existing decompilation projects may help for some Pokemon games, but check which ones exist.

#### 23.2 What every level needs: game adapters

Treat each game as a plugin with the same interface:

```
GameAdapter {
  id: "dqmj1" | "dqmj2" | "pt" | "hgss" | "bw2" ...
  mapGraph()          // maps, links, collision (section 7)
  assetManifest()     // namespaced IDs: "dqmj2:monster_047"
  stateReader()       // map, position, party, battle state (section 5.3)
  cameraStruct()      // for free camera (section 14)
  inputRemap()        // analogue stick to the game's movement
  sceneBuilder()      // outputs the Scene IR (section 12)
}
```

The engine host owns the instances, the global coordinate system and the shared world scene. Games in the same family likely share most formats, so one adapter can often be reused for several. DQMJ 1 and 2 are probably close, and the DS Pokemon games likely fall into two or three families, but verify against the actual ROMs.

#### 23.3 Main problems

1. **Namespaces.** Every asset, species and map ID needs a game prefix, or "monster 047" collides. A canonical table can map equivalents (for example a slime in both DQMJ games) to one shared HD asset.
2. **Geography.** Place each game's world on a meta map as islands or regions, and connect them with portals, ferries or air routes. Normalize tile size, units and camera conventions per game at conversion time.
3. **Saves.** Each game keeps its own save or save state. Add a meta save (position, active game, unlocked regions). Switching games means swapping states cleanly.
4. **Transferring monsters or items between games.** Within one family it is realistic: Pokemon already has defined cross-generation data structures, and DQMJ 1 to 2 may be similar. Across families (DQM to Pokemon) stats, skills and mechanics do not match, so you need a translation layer or the Level 3 unified battle system.
5. **Battle systems.** Each region uses its own battle logic at Levels 1 and 2. Cross-game fights need Level 3.
6. **Cores.** The melonDS base only covers the DS. "All Pokemon" includes Game Boy, GBA, 3DS and Switch games, which need other cores behind the same adapter interface, and the 3DS and Switch ones are very demanding to emulate. In practice, "all Pokemon" probably means the DS games, plus maybe the GBA ones.
7. **Art differences.** As far as I know, the DS Pokemon games draw creatures in battle as 2D sprites, not 3D models, so the 2D-to-3D techniques (section 15) matter more there than model replacement. DQMJ uses 3D monster models. A unified look needs the class and upgrade system (section 7) applied consistently so different art styles do not clash.
8. **Performance.** Multiple instances plus the full remake renderer is heavy on mobile. Level 1 is the only one that fits comfortably on a phone.
9. **Rights.** Everything loads from the user's own ROMs and a separate asset pack, and no original code or data is shipped (section 6.7). With several publishers involved, keeping the engine a pure framework matters even more.

#### 23.4 Suggested build order

1. Finish the single-game pipeline first (capture, world scene, camera, classifier). Everything else depends on it.
2. Define the `GameAdapter` interface and refactor the first game behind it.
3. Add the second game of the same family (DQMJ 2) and prove the namespaced assets and the meta world graph with Level 1 handoffs.
4. Add the meta save and the monster transfer within the family.
5. Try Level 2 for the seam between two regions.
6. Only then consider Level 3 for a unified battle system, and a different family such as Pokemon.

---

## Part VIII. Planning

_Roadmap, open decisions and next deep-dives._

### 24. Roadmap

1. G-buffer capture and rendering at parity with the original look; capture model and view matrices separately and build the persistent world scene (section 4).
2. File-load hook plus on-screen asset-ID overlay (so you can see what is what); define the Scene IR and put the renderer behind it (section 3).
3. Material sidecar and tagging system, plus the rule-based classifier and override manifest (section 7).
   Then the entity record and templates (section 7A), prototyped on one monster's leaf chains first.
   Then the remake pack index with fallback to the original draw, skeleton families and the play-trace heat map (section 7B).
   Put melonDS behind a `SourceRuntime` interface and start the independence ladder: importers, table scanner, first lifted rule in shadow mode (section 7C).
   In parallel, build the ROM understanding pipeline: overlay log, function fingerprints, library subtraction, function-as-RPC and the first seam stub (section 7D).
   Add the round-trip scorer and constraint tightness map to the entity pipeline, then family template fitting and motion inversion (section 7E).
4. Tier 1 enhancement: resolution, lighting, shadows, the real sky replacing the skybox (section 12), and the free camera (section 8).
5. Wind field plus vertex sway shader (cheap, visible immediately).
6. Shell mesh plus 3D-texture raymarch on a static wall.
7. XPBD cloth against a plane or sphere, then against an SDF; then water and face targets.
8. Grass instancing and interaction map; then hair.
   Then entity interaction and object integrity (section 17A): contact events, breakable bonds on one chain, field channels and the reaction table.
9. Replace one monster end to end (model, animation map, physics) to prove the pipeline.
10. Battle-state reader and battles rendered through your own scene.
11. 2D layers and UI, including 2D-to-3D conversion (section 15) and the tilt-parallax depth effect (section 9); map-link logger and world graph for the open world (section 10).
12. Compute RT shadows and reflections once the base pipeline is stable (profile on the target phone first and add the quality governor, section 18).
    Then the engine substrate (section 20A): paged memory and caches first, then GPU-driven clusters, then the shared spatial field.
13. Tooling: batch converter and manifest generator so adding the next 200 monsters is not hand work. After the single-game pipeline is proven, add the GameAdapter interface and a second game of the same family (section 23).

### 25. Open questions

| Topic | Question | Section |
|---|---|---|
| Scope | Is the content existing DS games (materials must be inferred) or your own assets (you control the data)? | 1 |
| Scope | What does "64-bit level" mean exactly: big visual/system leap (assumed here), N64-style visuals, or a literal 64-bit port of the emulator? | 1 |
| Emulator hooks | Exact function names for vertex transform and matrix capture vary between melonDS versions, so check the version you build on. | 2 |
| Scene IR | Which platforms must be supported first (desktop only, desktop plus Android, or also a 3DS profile)? | 3 |
| World scene | Which symptom shows now: shadows and AO that flicker when the camera moves, or lights that appear to rotate with the camera? | 4 |
| Free camera | How does the target game's overworld camera behave (fixed angle or following behind the player), and where is its camera struct in RAM? | 8 |
| Depth effect | Should head tracking be offered at all (it needs the front camera and a face detector), or is tilt parallax enough? | 9 |
| Open world | Is the map format of the target game already parsed (headers, collision, links), or does everything start from the dynamic position logger? | 10 |
| Sky | Does the target game paint horizon details (mountains, buildings) on the skybox, and does it move or scale the sky with the player? | 12 |
| Entity rework | Which family comes first: cloth (capes, banners), leaf chains (tails, ears) or jelly bodies? Do the target game's cloth-like assets have a recoverable skeleton? | 7A |
| Remake compiler | Can the target game's battles be replayed from recorded input scripts deterministically enough to build a play-trace heat map? Which events (damage, faint, menu moves) does the RAM map expose? | 7B |
| Remake compiler | Which subsystem should leave the oracle first (camera, movement and collision, battle presentation, UI), and does the target game's code allow patching it cleanly? | 7B |
| Preimage remaster | What round-trip score and tolerance are acceptable, and which degrade operators should be tested for the target game's textures and meshes? | 7E |
| Preimage remaster | Which monster families cover the most monsters, so template authoring starts where it pays most? | 7E |
| ROM understanding | Which SDK and middleware versions does the target game link, and are reference signatures or symbols available for them? | 7D |
| ROM understanding | Which seam should be stubbed first (draw monster, read input, file reads, sound), and does the game call it through one clean function? | 7D |
| ROM understanding | Can the signature corpus be built from the ROMs you own, and how will profiles be shared without sharing code or data? | 7D |
| Native engine | What is the end state: a better layer on the emulator, a native engine with the DS game as importer, or a platform for new content built from the DS game's parts? | 7C |
| Native engine | Which rules are worth lifting first (stats and damage, battle state machine, AI, synthesis), and are the target game's tables and script formats already understood? | 7C |
| Native engine | Should the classic ruleset be bit-exact with the original, or is behavioral equivalence enough? | 7C |
| Remake compiler | Which faithfulness default fits the target audience: faithful, balanced or reimagined? | 7B |
| 2D to 3D | Which 2D parts of the target game matter most (overworld tiles, sprites, or menus and backgrounds)? | 15 |
| Interaction | Should physics stay presentation-only (classic) or be allowed to change game rules in the extended ruleset, and for which events? | 17A |
| Interaction | Which classes get `full` integrity first (props, foliage, cloth, jelly monsters), and which stay `visual`? | 17A |
| Interaction | Which division and multiplication cases make sense for the target game's monsters (jelly, swarms, summons)? | 17A |
| Substrate | Which target tiers get full preload, and how much RAM, VRAM and storage can the pack and its caches use? | 20A |
| Substrate | Is Vulkan (or at least multi-draw indirect on ES) available on the target phone, so GPU-driven drawing is worth it there? | 20A |
| Substrate | Which consumers move to the shared spatial field first (cloth collision, soft shadows, grass), and which keep their own fallback? | 20A |
| Android | Which phone or chip is targeted, and is the current slowdown on the emulation thread or on the GPU? | 18 |
| Multi-game | Which fusion level is the target (one world with one game active at a time, several cores at once, or a unified game), and is the main goal a continuous world or monsters that move between games? | 23 |

### 26. Possible next deep-dives

| Area | Deep-dive | Section |
|---|---|---|
| Scene IR | Define the schema and put the desktop renderer behind it | 3 |
| World scene | Separate capture of model and view matrices with a world-space debug view | 4 |
| World scene | Relevance-region BVH (static plus dynamic) and the world-anchored probe grid | 4 |
| Capture and assets | G-buffer capture hook in the melonDS renderer | 2 |
| Capture and assets | NitroFS load hook and asset-ID overlay | 6 |
| Capture and assets | NSBCA to modern skeletal rig animation mapping | 6 |
| Classifier and tooling | Render-state rule engine for the classifier | 7 |
| Classifier and tooling | Batch scanner tool: ROM scan, texture statistics, `manifest.generated.json` | 7 |
| Classifier and tooling | Debug overlay and live class override hotkey | 7 |
| Entity rework | Leaf-chain prototype: classify, destructure and spring bones on one monster | 7A |
| Entity rework | Cloth sheet pipeline: anchor mask, rectified UVs, grid resampling, fold relief | 7A |
| Entity rework | Family template format and the verify stage with per-stage disable | 7A |
| Remake compiler | Pack index format, content hashes and fallback to the original draw | 7B |
| Remake compiler | Skeleton clustering and label propagation across a monster family | 7B |
| Remake compiler | Play-trace recorder and heat map from deterministic input scripts | 7B |
| Remake compiler | Camera dependence map by perturbation testing (forked instances, patched camera, state diff) | 7B |
| Remake compiler | Shadow-mode porting of one subsystem out of the oracle, with replay comparison | 7B |
| Remake compiler | Event grammar and rules file for one battle | 7B |
| Remake compiler | Pass contracts, dependency graph and incremental rebuild | 7B |
| Remake compiler | Regression by replay with per-frame silhouette scores | 7B |
| Preimage remaster | `degrade` operators and the round-trip scorer through the oracle | 7E |
| Preimage remaster | Color-cell dequantization and the constraint tightness map | 7E |
| Preimage remaster | Shell tolerance per vertex and bounded fold relief | 7E |
| Preimage remaster | Parameter search (grid, CMA-ES) over rework stage parameters | 7E |
| Preimage remaster | Family template format, fitting loop and fallback to 7A | 7E |
| Preimage remaster | Animation-to-physics inversion for leaf chains | 7E |
| ROM understanding | Overlay table parser and overlay-load event log | 7D |
| ROM understanding | Function recovery, normalized fingerprints and cross-ROM diffing for library subtraction | 7D |
| ROM understanding | Hardware-domain attribution and label propagation over the call graph | 7D |
| ROM understanding | Function-as-RPC with save and restore, and purity detection | 7D |
| ROM understanding | Hook registry by fingerprint and the first seam stub | 7D |
| ROM understanding | Dispatch-table scan and struct inference with typed memory views | 7D |
| ROM understanding | Coverage-guided exploration and trace contrast | 7D |
| ROM understanding | ROM profile schema, relocation across regions and the shared signature corpus | 7D |
| Native engine | `SourceRuntime` interface and keeping emulator types out of the engine core | 7C |
| Native engine | Table scanner: stride detection, plausibility checks, typed table export | 7C |
| Native engine | Differential testing of the damage formula, with shadow mode in one battle | 7C |
| Native engine | Battle state machine extraction from traces | 7C |
| Native engine | Monster synthesis through the part grammar | 7C |
| Native engine | Classic versus extended ruleset and save import | 7C |
| Free camera | Matrix logging and view recovery with a debug overlay | 8 |
| Free camera | Retained scene versus static map chunks, and patching the game's culling | 8 |
| Free camera | Stick direction remap and spring-arm collision | 8 |
| Depth effect | Off-axis projection prototype with a fake input on desktop | 9 |
| Depth effect | Tilt input filtering, calibration, idle sway and the depth slider | 9 |
| Open world | Map-link logger, stitch solver and `world_layout.json` | 10 |
| Open world | Boundary handoff with a masked load | 10 |
| Sky | Detection rules for the skybox mesh and removal from the G-buffer and BVH | 12 |
| Sky | Atmosphere LUT shader and clouds with cloud shadows | 12 |
| Ray tracing | Compute BVH plus shadow-ray pass | 13 |
| 2D to 3D | Layer and sprite capture with tile and sprite ID overlay | 15 |
| 2D to 3D | Tile-to-prop manifest and tileset converter | 15 |
| Simulation | XPBD compute shader in full | 14 |
| Simulation | Grass scatter compute shader and interaction map | 17 |
| Simulation | Hair ribbon generation and specular model | 17 |
| Simulation | Interaction events, mass from volume and joint limits from animation range | 17A |
| Simulation | Bond graph with XPBD load measurement, damage and entity fission with caps | 17A |
| Simulation | Field channels and the reaction rule table | 17A |
| Simulation | Pre-fractured fragment sets with an energy-based hierarchy | 17A |
| Simulation | Jelly implicit surface: division, merge and multiplication | 17A |
| Simulation | Damage atlas on the second UV set and the integrity stress suite | 17A |
| Android | Profiling setup and frame budget | 18 |
| Android | Quality governor (render scale, volumetric/RT quality, sim density) | 18 |
| Android | Vulkan migration plan | 18 |
| Substrate | Page table, pool allocator, feedback buffer and IO thread, first for textures | 20A |
| Substrate | Cluster format, offline builder and GPU culling with CPU fallback | 20A |
| Substrate | Sparse brick distance field: build from open DS meshes, query interface, first consumer | 20A |
| Substrate | Radiance cache on the field with visibility term | 20A |
| Substrate | Prefetch from the world graph, events and lifted encounter tables | 20A |
| Substrate | Full-preload mode, precompute caches and per-tier defaults | 20A |
| Ideas | Sort the extra ideas by effort versus visible impact | 21 |
| Multi-game | The GameAdapter interface and refactoring the first game behind it | 23 |
| Multi-game | Namespaced asset IDs and a canonical species table (DQMJ 1 and 2) | 23 |
| Multi-game | Meta world graph, meta save and the masked handoff between games | 23 |

---

## Appendix A. Old to new section numbers

| Original | Original title | New |
|---|---|---|
| 0 | Context and goals | 1 |
| 1 | Core architecture | 2 |
| 2 | Ray tracing | 13 |
| 3 | Volumetric textures and surface "shaping" | 14 |
| 4 | Physics: hair, grass, wind and secondary motion | 17 |
| 5 | Making the engine a live "remake" ("64-bit level" capability) | 6 |
| 6 | Detecting object, texture and asset types and upgrading them automatically (no AI) | 7 |
| 7 | Open world: all map instances in one continuous world | 10 |
| 8 | Android optimization (native, instead of a "bridge" between two systems) | 18 |
| 9 | A real sky instead of a skybox (for correct ray tracing) | 12 |
| 10 | Extra ideas (fun things that only make sense without hardware limits) | 21 |
| 11 | Turning 2D into 3D | 15 |
| 12 | Scene IR: one scene description, several rendering backends | 3 |
| 13 | Native depth effect on a regular smartphone (3DS-style feel, engine only) | 9 |
| 14 | Free camera (with an analogue stick replacing the 8-way d-pad) | 8 |
| 15 | Separating the camera from the game render (world scene for ray tracing and ambient light) | 4 |
| 16 | Fusing several games into one world (DQMJ 1 and 2, or all Pokemon) | 23 |
| 17 | Roadmap | 24 |
| 18 | Open questions | 25 |
| 19 | Possible next deep-dives | 26 |
| 20 | Modernizing flat and poor assets: what is covered and what was missing | 11 |
| 21 | Going past the DS engine's limits and its design assumptions | 5 (merged into one section) |
| 21B | Studying the DS engine, and ideas that go past its limits and its conception | 5 (merged into one section) |
| 22 | More ideas: materials, UI, terrain, tooling, audio and persistence | 16 |
| 23 | Shader warm-up, pipeline cache, texture compression and memory budget | 19 |
| 24 | More ideas against DS technical limits | 5 (merged into one section) |
| (new) | Improvements to existing ideas | 22 |
| (new) | Engine internals | 20 |
| (new) | Entity rework pipeline | 7A |
| (new) | The remake compiler | 7B |
| (new) | The DS as raw material | 7C |
| (new) | Understanding any ROM automatically | 7D |
| (new) | Preimage remaster | 7E |
| (new) | Entity interaction and object integrity | 17A |
| (new) | Engine substrate: spatial field, GPU-driven geometry, virtualized memory | 20A |
