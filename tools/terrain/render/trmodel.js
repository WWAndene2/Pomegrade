// Loads a model exported by sv/model.py (positions, normals, uvs, index groups, textures) as a three.js geometry
// with one material per group: albedo with alpha cut-out, normal map.
import * as THREE from 'three';
const dec = (s, T) => { const b = Uint8Array.from(atob(s), c => c.charCodeAt(0)); return new T(b.buffer); };
export async function loadModel(name, tint) {
  const js = await (await fetch('models/' + name + '.json')).json();
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(dec(js.position, Float32Array), 3));
  if (js.normal) { const n = dec(js.normal, Float32Array), d = js.dims.normal, m = new Float32Array(n.length / d * 3);
    for (let i = 0; i < n.length / d; i++) { m[i*3] = n[i*d]; m[i*3+1] = n[i*d+1]; m[i*3+2] = n[i*d+2]; }
    g.setAttribute('normal', new THREE.BufferAttribute(m, 3)); }
  if (js.uv) g.setAttribute('uv', new THREE.BufferAttribute(dec(js.uv, Float32Array), 2));
  g.setIndex(new THREE.BufferAttribute(dec(js.index, Uint32Array), 1));
  const L = new THREE.TextureLoader(); const mats = [];
  js.groups.forEach((gr, k) => {
    g.addGroup(gr.start, gr.count, k);
    const alb = Object.keys(js.textures).find(t => t.startsWith(gr.material) && t.endsWith('_alb'));
    const nrm = Object.keys(js.textures).find(t => t.startsWith(gr.material) && t.endsWith('_nrm'));
    const m = new THREE.MeshLambertMaterial({ side: THREE.DoubleSide, alphaTest: 0.45 });
    if (alb) { const t = L.load('models/' + js.textures[alb]); t.colorSpace = THREE.SRGBColorSpace; t.flipY = false; m.map = t; }
    if (tint) m.color = new THREE.Color(tint);
    mats.push(m);
  });
  return { geometry: g, materials: mats };
}
