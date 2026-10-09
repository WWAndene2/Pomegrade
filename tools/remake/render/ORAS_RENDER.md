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

## After v7 (in progress, not validated)

- **Texture relief halved:** `normalScale` 2.2 → **1.1** (owner's request). It is already applied in
  `passes.html`; set it back to 2.2 to reproduce v7 exactly.
- **Virtual Volumetric Layered Depth** (`vvld.js`, URL flag `vvld=1`, off by default). The protruding
  texels of the classes that really protrude (stone 1.0, tile 0.8, wood 0.6, foliage 0.6; plaster,
  metal, glass and unknown get 0) become 4 cut-out shells of real geometry, 0.5 units thick in all.
  Being real geometry, they also exist for the path tracer.
  - Each texture's relief is scaled to its own 95th percentile (grass rises about 50 times less than
    stone). Anything under 40 % of that stays flat, so soft painted shading, such as the window sills,
    gets no volume.
  - Ground decals (`chip_grass_decolate`) are lifted above the shells.
  - Raster passes: checked. A path-traced render with the shells takes well over 8 min (over 20 min
    for the wide view, unfinished); its final result has not been seen yet.
- **Known issue:** without the shells, the window sills and walls show bumps from the relief, because
  painted shading is read as height. Halving the relief and the volume floor are the fixes in progress.
