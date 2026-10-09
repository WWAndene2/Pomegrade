// Vegetation with volume and true relief for the Littleroot renders.
//  - makeGrass: short 3D blades on the normal grass (ORAS's dark green; the light green is the path),
//    denser around houses, cliffs and edges. A fixed seed: every pass sees the same blades.
//  - pomMaterial: parallax occlusion on the textures' height maps (materials.py), with self-shadowing
//    toward the sun (mode 1 outputs that shadow alone, for the compositing).
import * as THREE from 'three';

function rng(seed) { // mulberry32: deterministic
  return () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
}

export function makeGrass(root, { density = 0.025, nearBoost = 0.2, seed = 7, exclude = [] } = {}) {
  const ground = [], structures = [], decals = [];
  root.updateMatrixWorld(true);
  root.traverse((o) => {
    if (!o.isMesh) return;
    const n = o.material.name;
    if (n === 'chip_kusa_') ground.push(o);
    else if (n === 'chip_grass_decolate') decals.push(o); // dirt patches and flowers laid over the grass
    else if (/t01_0|chip_gake|chip_wood_a/.test(n)) structures.push(o);
  });
  // where structures stand (house walls, cliffs, fences): a coarse 2D grid of their footprint
  const cell = 10, occ = new Map();
  const v = new THREE.Vector3();
  for (const o of structures) {
    const pos = o.geometry.attributes.position;
    for (let i = 0; i < pos.count; i++) {
      v.fromBufferAttribute(pos, i).applyMatrix4(o.matrixWorld);
      if (v.y < 3) continue;
      occ.set(`${Math.floor(v.x / cell)},${Math.floor(v.z / cell)}`, true);
    }
  }
  // where decals lie: no blades there (cells of 6 units)
  const dcell = 6, decal = new Set();
  const q0 = new THREE.Vector3(), q1 = new THREE.Vector3(), q2 = new THREE.Vector3();
  for (const o of decals) {
    const pos = o.geometry.attributes.position, idx = o.geometry.index;
    for (let t = 0; t < idx.count; t += 3) {
      // the whole triangle, sampled every 2 units, not only its corners
      q0.fromBufferAttribute(pos, idx.getX(t)).applyMatrix4(o.matrixWorld);
      q1.fromBufferAttribute(pos, idx.getX(t + 1)).applyMatrix4(o.matrixWorld);
      q2.fromBufferAttribute(pos, idx.getX(t + 2)).applyMatrix4(o.matrixWorld);
      const n = Math.ceil(Math.max(q0.distanceTo(q1), q0.distanceTo(q2), q1.distanceTo(q2)) / 2) + 1;
      for (let i = 0; i <= n; i++) for (let j = 0; j <= n - i; j++) {
        const u = i / n, w = j / n;
        const x = q0.x + (q1.x - q0.x) * u + (q2.x - q0.x) * w, z = q0.z + (q1.z - q0.z) * u + (q2.z - q0.z) * w;
        decal.add(`${Math.floor(x / dcell)},${Math.floor(z / dcell)}`);
      }
    }
  }
  const nearStructure = (x, z) => { // distance to the nearest structure cell, up to 4 cells
    const cx = Math.floor(x / cell), cz = Math.floor(z / cell);
    for (let r = 0; r <= 4; r++)
      for (let dx = -r; dx <= r; dx++) for (let dz = -r; dz <= r; dz++)
        if (Math.max(Math.abs(dx), Math.abs(dz)) === r && occ.has(`${cx + dx},${cz + dz}`)) return r * cell;
    return Infinity;
  };
  const rand = rng(seed);
  const P = [], C = [], N = [];
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
  const wind = new THREE.Vector3(0.35, 0, 0.2); // a static lean for stills (wind, in the animated version)
  for (const o of ground) {
    const geo = o.geometry, pos = geo.attributes.position, col = geo.attributes.color, idx = geo.index;
    for (let t = 0; t < idx.count; t += 3) {
      const ia = idx.getX(t), ib = idx.getX(t + 1), ic = idx.getX(t + 2);
      a.fromBufferAttribute(pos, ia).applyMatrix4(o.matrixWorld);
      b.fromBufferAttribute(pos, ib).applyMatrix4(o.matrixWorld);
      c.fromBufferAttribute(pos, ic).applyMatrix4(o.matrixWorld);
      const area = new THREE.Vector3().subVectors(b, a).cross(new THREE.Vector3().subVectors(c, a)).length() / 2;
      // the triangle's grass tint: its vertex colours (ORAS's baked shading), lightened
      const tint = col ? [(col.getX(ia) + col.getX(ib) + col.getX(ic)) / 3, (col.getY(ia) + col.getY(ib) + col.getY(ic)) / 3, (col.getZ(ia) + col.getZ(ib) + col.getZ(ic)) / 3] : [1, 1, 1];
      const mid = new THREE.Vector3().add(a).add(b).add(c).multiplyScalar(1 / 3);
      // no grass inside or under the houses (the outdoor ground continues under them)
      if (exclude.some(([x0, x1, z0, z1]) => mid.x > x0 - 4 && mid.x < x1 + 4 && mid.z > z0 - 4 && mid.z < z1 + 4)) continue;
      const d = nearStructure(mid.x, mid.z);
      const boost = d < 40 ? nearBoost * (1 - d / 40) : 0;
      let count = area * (density + boost);
      while (count > 0) {
        if (count < 1 && rand() > count) break;
        count -= 1;
        let u = rand(), w = rand(); if (u + w > 1) { u = 1 - u; w = 1 - w; }
        const p = new THREE.Vector3().copy(a).addScaledVector(new THREE.Vector3().subVectors(b, a), u).addScaledVector(new THREE.Vector3().subVectors(c, a), w);
        if (decal.has(`${Math.floor(p.x / dcell)},${Math.floor(p.z / dcell)}`)) continue;
        const h = 1.8 + rand() * 1.8 + (d < 20 ? 0.8 : 0), half = 0.3 + rand() * 0.2;
        const ang = rand() * Math.PI;
        const side = new THREE.Vector3(Math.cos(ang), 0, Math.sin(ang)).multiplyScalar(half);
        const tip = new THREE.Vector3().copy(p).add(new THREE.Vector3(0, h, 0)).addScaledVector(wind, h * (0.6 + rand() * 0.6));
        P.push(p.x - side.x, p.y, p.z - side.z, p.x + side.x, p.y, p.z + side.z, tip.x, tip.y, tip.z);
        const nrm = new THREE.Vector3().subVectors(tip, p).cross(side).normalize();
        if (nrm.y < 0) nrm.negate();
        for (let k = 0; k < 3; k++) N.push(nrm.x * 0.5, 0.85, nrm.z * 0.5);
        // dark at the root, lighter at the tip: the base's grass green, as ORAS paints it
        // the grass texture's own colour (linear: dark p20 at the root, light p80 at the tip) x ORAS's shading
        const base = [0.012 * tint[0], 0.24 * tint[1], 0.03 * tint[2]], top = [0.05 * tint[0], 0.46 * tint[1], 0.085 * tint[2]];
        C.push(...base, ...base, ...top);
      }
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(P, 3));
  g.setAttribute('normal', new THREE.Float32BufferAttribute(N, 3));
  g.setAttribute('color', new THREE.Float32BufferAttribute(C, 3));
  const mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide }));
  mesh.name = 'grass_blades';
  console.log(`grass: ${P.length / 9} blades`);
  return mesh;
}

/// Parallax occlusion on map + height map; mode 0: colour (map x vertex colour), 1: relief self-shadow.
export function pomMaterial({ map, heightMap, vertexColors, sunDir, mode, depth = 0.022, transparent = false }) {
  return new THREE.ShaderMaterial({
    side: THREE.DoubleSide, transparent,
    defines: vertexColors ? { USE_VC: '' } : {},
    uniforms: { map: { value: map }, heightMap: { value: heightMap }, depthScale: { value: depth },
                sunDir: { value: sunDir }, mode: { value: mode } },
    vertexShader: `
      attribute vec4 tangent;
      #ifdef USE_VC
      attribute vec3 color;
      varying vec3 vColor;
      #endif
      varying vec2 vUv; varying vec3 vViewT; varying vec3 vSunT;
      uniform vec3 sunDir;
      void main() {
        vUv = uv;
        #ifdef USE_VC
        vColor = color;
        #endif
        vec3 n = normalize(mat3(modelMatrix) * normal);
        vec3 t = normalize(mat3(modelMatrix) * tangent.xyz);
        vec3 b = cross(n, t) * tangent.w;
        vec4 wp = modelMatrix * vec4(position, 1.0);
        vec3 toCam = cameraPosition - wp.xyz;
        vViewT = vec3(dot(toCam, t), dot(toCam, b), dot(toCam, n));
        vSunT = vec3(dot(sunDir, t), dot(sunDir, b), dot(sunDir, n));
        gl_Position = projectionMatrix * viewMatrix * wp;
      }`,
    fragmentShader: `
      uniform sampler2D map; uniform sampler2D heightMap; uniform float depthScale; uniform int mode;
      varying vec2 vUv; varying vec3 vViewT; varying vec3 vSunT;
      #ifdef USE_VC
      varying vec3 vColor;
      #endif
      float depthAt(vec2 uv) { return 1.0 - texture2D(heightMap, uv).r; }
      void main() {
        vec3 v = normalize(vViewT);
        // steep parallax: march into the relief along the view ray until under the surface
        const int STEPS = 24;
        float layer = 1.0 / float(STEPS);
        vec2 shift = v.xy / max(v.z, 0.55) * depthScale * layer; // limited at grazing angles (no smearing)
        vec2 uv = vUv; float d = 0.0; float h = depthAt(uv);
        for (int i = 0; i < STEPS; i++) { if (d >= h) break; uv -= shift; d += layer; h = depthAt(uv); }
        // refine between the last two steps
        vec2 prev = uv + shift; float after = h - d, before = depthAt(prev) - d + layer;
        float w = after / (after - before + 1e-5);
        uv = mix(uv, prev, clamp(w, 0.0, 1.0));
        // at most 2 texels of shift: further, the march reads another part of the atlas (dark wedges on roofs)
        vec2 off = (uv - vUv) * vec2(textureSize(map, 0));
        float len = length(off);
        // a degenerate tangent (UVs collapsed on some roof triangles) gives NaN: no relief there
        if (!(len <= 2.0)) uv = (len > 2.0) ? vUv + off * (2.0 / len) / vec2(textureSize(map, 0)) : vUv;
        if (mode == 1) {
          // self-shadow: march toward the sun; anything higher in between shades this texel
          vec3 s = normalize(vSunT);
          // soft self-shadow: how far above the ray toward the sun the relief rises, accumulated
          float sh = 1.0;
          if (s.z > 0.0) {
            float d0 = depthAt(uv); vec2 st = s.xy / max(s.z, 0.3) * depthScale / 16.0; vec2 u2 = uv; float dd = d0;
            float block = 0.0;
            for (int i = 0; i < 16; i++) { u2 += st; dd -= 1.0 / 16.0; if (dd <= 0.0) break;
              block = max(block, (dd - depthAt(u2)) * (1.0 - float(i) / 16.0)); }
            sh = 1.0 - smoothstep(0.0, 0.25, block);
          }
          gl_FragColor = vec4(vec3(mix(0.7, 1.0, sh)), 1.0);
          return;
        }
        vec4 c = texture2D(map, uv);
        if (c.a < 0.5) discard;
        #ifdef USE_VC
        c.rgb *= vColor;
        #endif
        gl_FragColor = c;
        #include <colorspace_fragment>
      }`,
  });
}
