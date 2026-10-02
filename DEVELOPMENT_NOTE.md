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
| 5 | Licences: one notices screen (melonDS GPLv3, Azahar GPLv2+, stb, …) | Required by the GPL when distributing the APK |
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

  Geometry fidelity (local, per edge): curvature reduced where the normals don't match the geometry (e.g. spherical normals painted on a flat cel-shaded face: 28% less bulge), round shapes unchanged. Not solved locally: an angular shape with smoothed normals (box, sharp chin) looks exactly like a rounded one from a single polygon. **Next:** detect hard edges from neighbouring polygons (a box has 90° between faces, a low-poly sphere 36-60°), which means multiplying at the end of the frame with each polygon's lighting state saved.

  Known limits: the Compute renderer ignores all of these; small cracks are possible where a multiplied polygon meets one that isn't; extra polygons aren't kept in savestates (redrawn on the next frame).

  Next:
  - Native texture smoothing in the 3D engine (mipmaps).
  - Automatic texture upscaling at asset level (algorithmic, e.g. xBRZ/ScaleForce applied to each texture when loaded, as Azahar does on 3DS).
  - 3DS: the same enhancements for Azahar's engine.

**Performance and size**
- Measure the release APK library by library and drop what neither core needs (e.g. check whether `libSPIRV-Tools-shared.so`, 5.7 MB, is required at runtime).
- One copy of shared libraries: both cores ship their own zstd, xxhash, fmt, …
- Device profiles: renderer and resolution chosen from the phone's capabilities.

**Build**
- Remove or fix the unused upstream workflows in `melonDS-android/.github/`.
- Run the HD texture desktop tests in CI.

## 3. New features

- **120 fps**: frame interpolation as an option (decision pending).
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

1. **Unified storage**: one tree `Pomegrade/{games, saves, states, textures, system}` instead of melonDS's settings plus Azahar's own folder.
2. **Unified settings**: one settings screen; each core declares its options and the interface displays them.
3. **Core interface**: a common contract, with melonDS and Azahar adapted to it — the largest piece of work. Same idea as RetroArch's libretro, tailored to Android.
4. **Single emulation screen**: same controls, menu and save states for every console; Azahar's and melonDS's own screens go away.
5. **One visual identity**: theme and icons built around the Pomegrade name and logo.

Steps 1–2 take a few work sessions; steps 3–4 are a major rebuild measured in weeks, with full re-testing on a phone after each step.

## Recommended order

1. Section 1, for a stable, tested version.
2. Steps 1–2 of section 4 (storage and settings), the most visible improvement day to day.
3. Then decide on the core interface and single emulation screen, once the base has been tested on a phone.
