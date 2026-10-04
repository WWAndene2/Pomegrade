# Development Note

Roadmap of Pomegrade: what is left before a first usable version, what can be improved, what can be developed, and the plan for turning the merged apps into one product. Update it as items are done; the git history records when and how.

## Where things stand

Pomegrade is currently **two apps side by side in one APK**: melonDS-android (DS/DSi, with Pomegrade's HD texture replacement) and Azahar (3DS), joined by a single game list.

| Done | Verified by |
|---|---|
| HD texture replacement for the OpenGL renderer | Desktop tests (`tests/hd-textures/`) |
| Azahar (3DS) merged as the `:azahar` module | Local build |
| Unified game list: DS, DSi and 3DS games, each launched in its own core | Local build, `RomPlatformTest` |
| App renamed Pomegrade, own application ID (`io.github.wwandene2.pomegrade`) | APK inspection |
| Pomegrade icon (adaptive, themed, legacy) | APK inspection |
| CI: builds only on `main` and manual runs, ccache, only the needed 3DS target, optimized native code | Local clean build with the CI settings (27 min); no CI run yet |

**Nothing has been run on a phone yet.** Every user-facing item above is untested on a device.

## 1. Before a first usable version

| # | Task | Why |
|---|---|---|
| 1 | Test on a real phone: DS, HD textures, 3DS | Only compilation and desktop tests are verified so far |
| 2 | Release build: shrunk, signed | The CI debug APK (optimized native code) is 136.5 MB; release should be smaller, to be measured. Needs a signing key kept in GitHub Secrets, and R8 rules checked for Azahar's JNI classes |
| 3 | Merge into `main` and run the GitHub build | CI only builds `main` |
| 4 | README: 3DS section (formats, Android 10+, 64-bit only, encrypted games unsupported) | Documentation is out of date |
| 5 | Licences: one notices screen (melonDS GPLv3, Azahar GPLv2+, stb, …) | Required by the GPL when distributing the APK. Partly done: the about screen names each project and licence and links the licence texts; the texts themselves are not bundled yet |
| 6 | Nightly flavor: still has melonDS icons | Consistency (not used for now) |
| 7 | Purge the git history of files that must not be public (the owner has the list) | The repository is public; planned once the APK works |

## 2. Improvements to what exists

**Unified list**
- Real title and icon of 3DS games, read in the background and cached (today: file name only, since reading them needs the 3DS core).
- Filter by console (DS / DSi / 3DS).
- Home-screen shortcuts for 3DS games (the shortcut picker offers DS games only).
- `.cia` installation from the main list (today: Azahar's "3DS games" screen).

**HD textures**
- Background loading, to remove the hitch the first time a texture appears.
- 2D graphics replacement (menus, sprites): a separate engine in melonDS.
- Optional bilinear filtering of HD textures.
- Easier pack installation (folder picker): `Android/data` is hard to reach on Android 11+.
- HD textures for 3DS: Azahar already supports custom textures; both could share one pack format.
- "DS engine on steroids": native enhancements done by the emulator itself (no AI, no hand-made assets, no post-processing of the final image). The game always sees the real DS behaviour; only what is drawn changes. All in Settings → Video, off by default, verified pixel-identical to the original engine when off. Desktop tests in `tests/polygon-multiplier/`. **None tried on real games or a phone yet.**

  | Enhancement | Renderers | Status, measured on the desktop |
  |---|---|---|
  | Polygon multiplier (×4 to ×64): lit polygons subdivided on a curved surface and relit with the DS lighting | Software, OpenGL | Done. "PNhong" curves: per level, the measured best blend of Phong and circular PN (17% closer than either alone at ×9 and ×16, over six test models). No fixed cap (memory guard: 262144 extra polygons per frame) |
  | Remove polygon limit: polygons the DS would drop are drawn | Software, OpenGL | Done. 3000-polygon scene: 1536 drawn → 3000 |
  | High-precision geometry: sub-pixel vertex positions | OpenGL | Done. Frame-to-frame jitter of a rotating triangle 27.2 px → 3.0 px |
  | High colour: 8-bit colour instead of the DS's 6 | OpenGL | Done. Lit sphere shading 50 → 184 colour levels |
  | Better polygons (melonDS option, fewer seams at high resolution) | OpenGL | Exposed in the settings |
  | OLED deep blacks: each scene's near-black shades become true black (threshold follows the scene's black level) | OpenGL | Done. Dark grey background → 000000, mid tones unchanged |
  | Adaptive colours: levels and saturation adjusted to each scene, smoothed over ~0.5 s | OpenGL | Done. Washed-out scene: contrast ×1.25-1.5 depending on its black level; full-range scenes left alone |
  | Texture upscaling ×2 to ×16: each texture magnified when loaded (MMPX pixel-art rules, same size on screen, no new colours) | OpenGL | Done. Diagonal line edge 7.5 px from the ideal → 3.5 (×2) → 1.5 (×4) → 0.67 (×8) → 0.28 (×16); identical to the MMPX reference code on 400 test images away from clamped texture edges, where the pattern is continued instead of repeated (the line's first texels used to stay blocky) |
  | Texture filtering: colour filtered without mipmaps ("sharp bilinear" up close: texels keep their colour, only their edges are blended over one pixel; up to 4 samples along the footprint where a pixel spans several texels); the DS's nearest texel keeps deciding alpha | OpenGL | Done. Test texture: texel middles unchanged, each texel edge blended over one pixel (nearest: none), transparent texels cover the same pixels, 4 texels per pixel 9x closer to each pixel's texels than nearest (0.25 vs 2.25). Shimmer in motion not measured (stills only) |
  | Pseudo ray tracing, step 1 - ambient occlusion: soft shadows in creases and under objects, from each opaque pixel's view-space position (screen-space, SAO estimator) | OpenGL | Done. Test scene: floor-wall crease 19% darker, contact shadow under a sphere 12-15% darker, open surfaces and 2D (orthographic) geometry unchanged, same at internal resolution ×2/×4 and with the multiplier. Display only: the game's display capture keeps the DS render |
  | Pseudo ray tracing, step 2 - light bounce: lit surfaces light their surroundings with their colour (one diffuse bounce, from what is on screen) | OpenGL | Done. Grey floor in front of a lit red sphere 3-6% redder, floor-wall crease 3% brighter, open surfaces and 2D geometry unchanged. Screen-space: only surfaces the camera sees can send light |
  | Pseudo ray tracing, step 3 - real-time shadows: opaque 3D geometry casts shadows from the scene's main light (shadow map from the light, fitted to the scene) | OpenGL | Done. Floor in a sphere's shadow x0.66, lit surfaces and 2D geometry unchanged. In the shadow, only the main light's share of the colour (its cosine on the surface, 60%) is taken away. Contact-hardening (PCSS): sharp where a shadow meets its caster, softer further away (light ~1 degree across, penumbra up to 12 shadow map texels); bar 0.1 above the floor: edge 0 pixels, 2.4 above: 3 (fixed 3x3 filter: 0 and 0) |
  | Pseudo ray tracing, step 4 - reflections: surfaces reflect what is on screen (ray marched in view space, Fresnel; the game's specular colour makes a material shiny) | OpenGL | Done. Shiny floor in front of a red sphere: red/green x1.41; matte floor: x1.006; HUD untouched. Screen-space: what is off screen or hidden can't be reflected; reflections use facet normals (low-poly curved objects reflect in facets) |
  | Frame generation (120 fps): an extra image between two DS frames, the 3D re-rendered with each polygon halfway between the frame shown and the next (which the DS has already rendered: no added latency); the display is asked for 120 Hz | OpenGL | Done (desktop tests; pacing untested on a phone). Moving sphere: generated frame within 0.35 px of halfway, 33x closer to a real halfway render than either frame. Limits: faces that turn towards or away from the camera between two frames shift silhouettes by about a pixel; 2D layers stay at 60; a game running at 30 fps gets no in-between images; scene cuts show the frame as is |

  Geometry fidelity (local, per edge): curvature reduced where the normals don't match the geometry (e.g. spherical normals painted on a flat cel-shaded face: 28% less bulge), round shapes unchanged. Geometry fidelity from neighbours: an angular shape with smoothed normals (box, sharp chin) looks exactly like a rounded one from a single polygon, so the angle between the two faces of each edge decides (kept up to 55°, flat from 80°: a box's 90° edges stay sharp, a low-poly sphere's 36-45° keep their curve). Edges are recognised from frame to frame by their model positions; the angles found in a frame apply from the next one (a model's very first frame curves as before). Box with smoothed normals at ×16: bulge 67% → 0.1% of its half size, sphere unchanged, no cracks.

  Lighting effects and how games draw: they light the opaque 3D only; translucent polygons (2D dialog boxes, water, smoke), fog and edge marking are drawn again over the lit image with the DS's own rules (dialog box within one 6-bit colour step of the scene lit behind it, full fog unchanged). Unlit polygons (painted-in lighting, skies) cast no shadow; reflections ignore surfaces facing away from the ray (no more floor reflecting itself in bands). The effects pause while a game captures its 3D every frame (3D on both screens, motion blur), which would otherwise flicker; they come back 30 frames after. Limits: cut-out sprites (A3I5/A5I3 textures) get no effects; a sprite very close in front of a flat backdrop gets a faint AO halo; a shiny floor can faintly reflect an object hidden in fog.

  Known limits: the Compute renderer ignores all of these; small cracks are possible where a multiplied polygon meets one that isn't; extra polygons aren't kept in savestates (redrawn on the next frame).

  Next:
  - Native texture smoothing in the 3D engine (mipmaps).
  - 3DS: the same enhancements for Azahar's engine.

**Performance and size**
- Measure the release APK library by library and drop what neither core needs (e.g. check whether `libSPIRV-Tools-shared.so`, 5.7 MB, is required at runtime).
- One copy of shared libraries: both cores ship their own zstd, xxhash, fmt, …
- Device profiles: renderer and resolution chosen from the phone's capabilities.

**Build**
- Remove or fix the unused upstream workflows in `melonDS-android/.github/`.
- Run the HD texture desktop tests in CI.

## 3. New features

- **120 fps** (DS, OpenGL renderer): done as "Frame generation", see the enhancement table. Not yet for the 3DS core.
- **Analogue movement** (DS, per game): a game's D-pad movement code is patched to read the stick's exact direction (Pomegrade-only register, patch written only over the game's original instructions); the touch D-pad becomes a joystick and gamepad sticks are read as analogue (setting "Analogue movement"). Done for Dragon Quest Monsters: Joker (Europe, AJRP): direction only, normal walking speed; checked in the game on desktop, untested on a phone. Next: tilt for walking speed; other games (each needs that game's code reverse-engineered).
- **Rewind and save states** shared by all consoles.
- **Online play**: melonDS has early support; Azahar has none on Android.
- **Cloud saves** (Google Drive) for all consoles.
- **Cheats**: Action Replay (DS) and Gateway (3DS) in one format and one screen.
- **RetroAchievements** for 3DS (melonDS already supports it).
- **Controllers**: one mapping for all consoles.
- **GBA**: an mGBA core would complete the Nintendo handheld family.

## 4. One coherent app

Today each core keeps its own settings, screens, visual style and file storage. Target architecture:

```
Pomegrade
├── Shared interface
│   ├── Library: every game, one list, one style (Compose)
│   ├── Settings: general + per console + per game
│   ├── Emulation screen: shared on-screen controls, menu, save states
│   └── Controls, cheats, achievements, saves: one module each
├── Core interface: one contract
│   └── load game / run a frame / video / audio / input / save state
└── Cores (plugins)
    ├── melonDS (DS / DSi)
    ├── Azahar (3DS)
    └── later: mGBA (GBA), …
```

Steps, from least to most work:

1. **Unified storage**: done (untested on a phone). One folder picked once, `Pomegrade/{Roms/<game>/, BIOS, 3DS}`: each game moved into its own folder with its DS save and save states, DS/DSi BIOS (when none was set elsewhere), Azahar's folder. Asks for "All files access" (moving by path; Azahar's standard build needs it too). Not moved yet: HD textures (still in `Android/data/<app>/files/textures`), an Azahar folder set up before elsewhere.
2. **Unified settings**: first part done (untested on a phone). One settings screen in three sections: Pomegrade (the Pomegrade folder, settings saved in it, General), DS (melonDS's categories), 3DS (each entry opens that section of Azahar's settings, which keep their own look and their config file in Pomegrade/3DS). Settings, controls, layouts and the game list (each game's settings, play time) are saved automatically in Pomegrade/Settings and offered back when a Pomegrade folder used before is set up again (after a reinstall). Next: Azahar's settings drawn as Pomegrade settings.
3. **Core interface**: a common contract, with melonDS and Azahar adapted to it — the largest piece of work. Same idea as RetroArch's libretro, tailored to Android.
4. **Single emulation screen**: first part done (not compiled, untested on a phone). Both screens use the `:emulator-ui` module: Azahar's in-game menu (a drawer, same entries, order, icons and Pomegrade colours on both consoles; DS opens it by pausing), one save state dialog (quick slot + slots 1-10 on both; DS shows screenshots), and the DS on-screen buttons drawn with Azahar's artwork. Each core still runs in its own activity. Next: the core interface (step 3), then one activity; DS on-screen control options in the menu; the menu's titles translated (English only, Azahar's were translated).
5. **One visual identity**: done (not compiled, untested on a phone). Pomegranate palette from the icon on the app, the 3DS screens (Azahar's red theme, now its default) and the in-game menu; splash and notification icon; about screen crediting melonDS, melonDS-android, Azahar and the libraries, with their licences.

Steps 1–2 take a few work sessions; steps 3–4 are a major rebuild measured in weeks, with full re-testing on a phone after each step.

## Recommended order

1. Section 1, for a stable, tested version.
2. Steps 1–2 of section 4 (storage and settings), the most visible improvement day to day.
3. Then decide on the core interface and single emulation screen, once the base has been tested on a phone.
