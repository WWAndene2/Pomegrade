# ORAS offline render — the v7 preset

Offline stills of Littleroot (ORAS) from the game's own models and textures, with modern light but the
game's art direction kept: vibrant, warm, anime. Nothing here runs the game. The game data (the dump,
`orig.gltf`, `orig/*.png`, `mat/`, the interiors' glTF) is extracted into a scratch folder and is
**never committed**. Only these scripts are.

The owner validated the v7 preset. Owner feedback that sets the direction:
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

## v7 values

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

## v8 and v9 (current scripts; not validated by the owner yet)

**v8** = v7 + texture relief halved (`normalScale` 1.1) + Virtual Volumetric Layered Depth
(`vvld.js`, flag `vvld=1`).
- **What gets volume:** the protruding texels of the classes that really protrude become 4 cut-out
  shells of real geometry, 0.5 units thick in all. Stone weighs 1.0, tile 0.8, wood and foliage 0.6;
  plaster, metal, glass and unknown get 0.
- **Threshold:** each texture's relief is scaled to its own 95th percentile, and anything under 40 %
  of that stays flat. Without this, the window sills and walls turned bumpy.
- **Shading:** the lower layers are darkened to 0.82 (occlusion inside the relief).
- **Decals:** ground decals are lifted above the shells.
- **Light:** v8 reuses v7's path-traced light. A full path trace with the shells was stopped: too slow,
  over 20 min for one view.

**v9** = v8 without the texture relief + ray-traced light work. Everything else is unchanged; how to
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

Commands for v9:
```
node shoot_pass.mjs "interiors=1&pom=1&vvld=1&cam=<cam>&az=160&el=32&w=960&h=540" base,relief,flat,depth,normal,matid,pomshadow
node shoot_ao.mjs <dir> "interiors=1&cam=<cam>&radius=8&frames=4"
python compose.py - 0.35 sky_afternoon.png <dir>/stylised.png <dir>/pt_white.png 0.60 0.78 <dir> <cam> norelief
python layers.py <dir> <cam> 32
```

## Live in the emulator: Remaster (9 October)

The presets also run live on the 3DS core's top screen: Settings → Remaster (OpenGL), "v7 preset", "v9 preset" or
"Custom" with one switch per effect (`azahar/src/video_core/remaster.h`, `renderer_opengl/gl_remaster.cpp`). A frame of
the emulator has only its colours and its depth buffer, so each technique is done from those two, once per emulated
frame: the two-tone step on the frame's luminance, screen-space ambient occlusion applied as v9 applies its ray-traced one,
v9's sky fill, sky light on surfaces facing up (normals from the depth), contours on depth breaks, glow, aerial
perspective, far blur, vibrance 1.28, the S-curve (0.7 in v7, adaptive in v9 without the sun-elevation term). Not live:
the path-traced light, the texture relief, parallax occlusion, volumetric layered depth, the material recognition and the
interiors through the windows. Checked: the shaders compile for OpenGL ES 3.2 and desktop OpenGL (glslangValidator) and
the C++ builds with warnings as errors; not yet run on a phone.
