# ORAS remaster V1 — offline renders and the live engine

Offline stills of Littleroot (ORAS) from the game's own models and textures, with modern light but the
game's art direction kept: vibrant, warm, anime. Nothing here runs the game. The game data (the dump,
`orig.gltf`, `orig/*.png`, `mat/`, the interiors' glTF) is extracted into a scratch folder and is
**never committed**. Only these scripts are.

The owner validated the offline look below (it became Remaster V1). Owner feedback that sets the direction:
- Keep the game's look. A realistic "Unreal Engine" look was rejected.
- Rejected, do not re-add: painted clouds, cloud shadows not cast by real game clouds, light rays and
  particles, 3D grass, leaf translucency, dappled light, wind.
- Texture outlines are raised, never removed. Seams between two map pieces stay flat (no ridge).
- One render at a time; avoid renders of ten minutes or more.

## Pipeline

1. **Extract** (scratch folder, outside the repository): `remake_tool` exports the map piece to
   `orig.gltf` with its textures in `orig/<i>.png`. `mats.json` maps material name to texture index.
   The interiors are zones 223, 225 and 227 (pieces 506, 508 and 510, area packs 112–114), exported as
   `house_a.gltf`, `house_b.gltf` and `lab.gltf`.
2. **Recognise materials:** `python materials.py 13` writes, per texture, `mat/<i>_{classes,orm,normal,
   height,trans,opacity,outline,volume,layer<k>,layerw<k>}.png`. Per-texel classes come from
   reference profiles measured in ORAS. The relief is high-passed from the texture's own shading;
   outlines are raised and borders are faded to flat.
3. **Raster passes** (headless Chromium, three.js r0.160):
   `node shoot_pass.mjs "interiors=1&pom=1&cam=<cam>&az=160&el=32&w=960&h=540" base,relief,flat,depth,normal,matid,pomshadow`
4. **Path-traced light** (three-gpu-pathtracer 0.0.20, white surfaces, about 8 min at 96 samples):
   `node shoot_pt2.mjs <dir>/pt_white.png "time=afternoon&linear=1&white=1&norelief=1&interiors=1&exposure=0.35&cam=<cam>&w=960&h=540&samples=96"`
5. **Stylised compositing:**
   `python compose.py - 0.35 sky_afternoon.png <dir>/stylised.png <dir>/pt_white.png 0.60 0.78 <dir> <cam>`
   (no extras: clouds, dapple and translucency are off)
6. **Layers:** `python layers.py <dir> <cam>` writes `<dir>/final.png`.

Cameras (`px,py,pz,tx,ty,tz`): wide `0,330,330,0,0,-30`; close `-40,70,150,-40,10,40`.
Sun: azimuth 160°, elevation 32° (afternoon).

## Offline look values

| Part | Value |
|---|---|
| Base colours | The game's unlit look (texture × vertex colours); painted shadow meshes (`shadow1`) hidden |
| Light | Path-traced white surfaces at exposure 0.35, divided into the base |
| Two-tone step | smoothstep 0.60 → 0.78 |
| Shade / sun | Cool lavender shadow tone plus a cool additive lift (so shade shows on saturated green); warm sun |
| Texture relief (normal maps) | `passes.html` relief pass `normalScale` **2.2**; in `compose.py`, `tone *= 1 + (relief - 1) * 1.25 * (0.35 + 0.65 * step)` |
| True relief (parallax occlusion) | `extras.js` `pomMaterial`: depth 0.022, 24 steps, view shift limited at grazing angles; soft self-shadow `1 - smoothstep(0, 0.25, block)` mapped to 0.7–1.0, multiplied as `0.75 + 0.25 * pomshadow` |
| Degenerate tangents | Some roof triangles have collapsed UVs: their tangent is NaN, so the shift is dropped there (v6's dark wedges on the roofs) |
| Vibrance | 1.28 |
| Layers (`layers.py`) | Sky light on up-facing surfaces, warm rim on sunlit silhouettes, fine contour on depth breaks, glow on highlights, aerial perspective, slight far blur, S-curve contrast 0.7 |
| Interiors | Real interiors placed on their door warps, scaled to the façade footprint, overflowing vertices pressed onto the walls, black doorway panels removed (`interiors.js`); windows see through via `mat/<i>_opacity.png` |

## Later offline iterations (folded into V1)

**Iteration 2** = the offline look + texture relief halved (`normalScale` 1.1) + Virtual Volumetric Layered Depth
(`vvld.js`, flag `vvld=1`).
- **What gets volume:** the protruding texels of the classes that really protrude become 4 cut-out
  shells of real geometry, 0.5 units thick in all. Stone weighs 1.0, tile 0.8, wood and foliage 0.6;
  plaster, metal, glass and unknown get 0.
- **Threshold:** each texture's relief is scaled to its own 95th percentile, and anything under 40 %
  of that stays flat. Without this, the window sills and walls turned bumpy.
- **Shading:** the lower layers are darkened to 0.82 (occlusion inside the relief).
- **Decals:** ground decals are lifted above the shells.
- **Light:** iteration 2 reuses the offline look's path-traced light. A full path trace with the shells was stopped: too slow,
  over 20 min for one view.

**Iteration 3** = iteration 2 without the texture relief + ray-traced light work. Everything else is unchanged; how to
run it is under "Commands" below.
- **Texture relief:** off (`norelief` extra).
- **Ambient occlusion, ray-traced in world space:** `ao.html` / `shoot_ao.mjs`, a custom three-mesh-bvh
  shader. three-gpu-pathtracer 0.0.20's `AmbientOcclusionMaterial` does not compile.
  - 4 frames × 16 cosine rays per pixel; radius 8 (close) or 12 (wide). About 3 min for the close view,
    1 min for the wide one.
  - The normal is oriented toward the camera, because some ORAS faces wind backwards.
  - Hits under 1.5 units are ignored: ORAS layers cut-out sheets just in front of the walls, and the
    BVH ignores alpha. Faint stripes remain.
- **Compositing (`compose.py`):**
  - The light's grain is smoothed by a guided filter (He et al.) guided by depth and the bare
    geometry's shading.
  - The AO frames are averaged, median- and guided-filtered, raised to the power 1.3, and applied as
    `tone *= 1 - (1 - ao) * (0.55 - 0.4 * step)`, so it is strong in the shade and weak in the sun.
  - A sky fill (0.010, 0.018, 0.045) is added in the shade where the AO is open.
- **Adaptive contrast (`layers.py <dir> <cam> <sun elevation>`):**
  `CONTRAST = clip(0.7 * 0.2 / std(lum), 0.4, 1.0) + 0.15 * smoothstep(35, 8, elevation)`.
  It came out 1.0 on the afternoon setup; morning and evening are untested.

Commands for iteration 3:
```
node shoot_pass.mjs "interiors=1&pom=1&vvld=1&cam=<cam>&az=160&el=32&w=960&h=540" base,relief,flat,depth,normal,matid,pomshadow
node shoot_ao.mjs <dir> "interiors=1&cam=<cam>&radius=8&frames=4"
python compose.py - 0.35 sky_afternoon.png <dir>/stylised.png <dir>/pt_white.png 0.60 0.78 <dir> <cam> norelief
python layers.py <dir> <cam> 32
```

## Live in the emulator: Remaster (9 October)

Remaster V1 runs live on the 3DS core (OpenGL): Settings → Remaster → "V1" (or "Custom": V1 with its switches). It
path-traces the light every frame (renderer_opengl/gl_pathtracer), shades the textures' surfaces from their recognised
materials, and composes the frame as the offline renders did; the table below says where each technique went.

## The modern engine: every technique live (owner's request, 9 October)

Every technique of the offline renders goes into the 3DS core, each where a modern game puts it: computed once what never changes
(a texture's material, a map's light transport), every frame what depends on the camera, the clock or what moves.

| Technique | In the engine | Each frame | State |
|---|---|---|---|
| Material recognition (`materials.py`) | `video_core/material_recognition`: the same steps in C++, once per texture when the game uploads it | no | **Done**: on Littleroot's 13 textures the classes agree on 99.96 % of the texels with `materials.py`, the outlines are the same, normals, heights and volumes within 2/255 (about 0.1 s a texture) |
| Properties (roughness, metalness, transmission, glass opacity) | from the class, in the material maps | no | done with the recognition |
| Texture relief (normal maps) | the material maps bound beside each texture (`pomegrade_darp_manager`, packed RG normal, B height, A volume), the relief lit in the generated fragment shader (`WritePomegradeSurface`) | yes | **Done** (1.1, kept at the 3DS's size) |
| Parallax occlusion with soft self-shadow | the height map in the fragment shader, tangent-free (the tilt from the texture coordinates' screen footprint), the shift limited at grazing angles | yes | **Done**, same state |
| Volumetric layered depth | 4 layers of the parallax on the classes that protrude, the lower ones darkened to 0.82 | yes | **Done** (V1), same state |
| Path-traced sun and sky light (shadows, bounces) | live path tracer (`renderer_opengl/gl_pathtracer`): each frame's lit draws that write depth captured (software vertex shading), a BVH (`pathtrace_bvh`), a G-buffer in the render target's layout; per pixel a ray to a point of a 3-degree sun disc (penumbra growing with distance) and three hemisphere rays (sky, occlusion, bounce with a second shadow ray). Sun and moon by the game's clock and the owner's table (every 3 hours: azimuth, height, colour, power, sky colours; 15h is the offline renders' afternoon, azimuth 160, 32-36 degrees) | yes | **Done**; OpenGL only, cost on a phone unknown. Headless full render (9 October, Littleroot by day): traced cast shadows of houses, roofs and the player, the painted ones gone; the table itself not yet rendered |
| Ray-traced ambient occlusion | the path tracer's hemisphere rays (hits within 40 units) | yes | **Done** |
| Denoising (median, guided filter) | two a-trous passes guided by the G-buffer's positions and normals, then temporal accumulation (80 % history where the pixel shows the same point) | yes | **Done** |
| The game's painted shadows removed | the shadow decals (shadow1, shadow_a) recognised by their texture (dark, one colour, shaped by alpha) and not drawn while the path tracer runs; inside textures, only broad smooth dark areas lifted (r 4 against r 16), never lines | no | **Done** |
| Two-tone light, coloured bounce, sky fill, vibrance | Remaster (live post-process) | yes | **Done** (the bounce from the light pass) |
| Layers: sky light, warm rim, contours, glow, aerial perspective, far blur, adaptive contrast | Remaster, on the path tracer's G-buffer depth; the rim on the traced sun's side; an unsharp mask keeps the textures' lines | yes | **Done** |
| Interiors: fake interior light behind see-through windows | glass texels (A = 255 in the maps) show a room two texture-widths deep (interior mapping), lit by a ceiling lamp, furniture in silhouette, 80 % seen through, Fresnel sky reflection | yes | **Done**, not yet seen on screen |


### Full render against the reference (9 October)

Littleroot by day, headless (llvmpipe, 400 x 240), base game against V1 with the path tracer, before the hourly table: the painted shadows are replaced by traced ones (the houses' shadows on the ground, the roofs' overhangs on the walls, the player's own), the grass keeps its painted blades, the shade takes the sky's blue-green. Against the owner's reference (the offline render at 1920 x 1080): its shadows are longer and bolder (an afternoon sun, now the table's 15h), its facades brighter, its resolution higher; the windows' rooms did not show (their glass is probably not recognised as glass: unchecked).

Final full render (same day): Littleroot at 15h (the table's afternoon, the reference's sun), internal resolution x3
(1200 x 720 a screen), base game against V1. The houses cast their shadows on the grass toward the lower right in the
reference's deep blue-green, the poles, the sign and the player cast long shadows sharp at their foot, the roofs'
overhangs shade the facades, the sunlit facades are warm, the painted shadows are gone and the grass keeps its painted
blades. A thin bright line along the top was found and fixed (the main pass now reads a texel inside the frame). The
glass class is recognised on some textures (two 128 x 64 textures two-thirds glass), but these houses' windows are
painted opaque blue and show no room, as in the reference. Still differing from the reference: its resolution (1920 x
1080) and its wide camera; not tested on a phone.

Measured against the reference (Littleroot 15h, same view; luminance, saturation, share of shade, shade over sun in R, G,
B): reference 0.367, 0.59, 33 %, 0.34 0.43 0.50; base game 0.497, 0.66, 19 %, 0.48 0.54 0.57; V1 with the path
tracer 0.472, 0.70, 23 %, 0.47 0.43 0.74. The shade's green matches; it stays bluer and its red lighter than the
reference's (cause not found: the bounce tint at 30 % and the sky light left out over the traced light changed nothing
measurable). Sun presets (Settings remaster_sun_hour: game clock, or 00h ... 21h) choose the table's hour. The Android
build of main with them succeeded (GitHub Actions, run 117).

Shade matched (same day, after the measurements above): the bluer shade came from compose.py's shade lift and sky fill,
added in linear light to dark shade (0.016 shows as 0.13), not from the tone; they are left out over the traced light
(it holds the sky) and the shade tone is fitted in linear light ((target / shown)^2.2): (0.31, 0.43, 0.60). Measured on
the same view: shade over sun 0.35, 0.42, 0.49 against the reference's 0.34, 0.43, 0.50; luminance 0.457 (reference
0.367, its wide camera holding more forest shade), saturation 0.75 (0.59).