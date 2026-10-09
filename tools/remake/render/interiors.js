// The real interiors of Littleroot (ORAS zones 223, 225, 227: map pieces 506, 508, 510), placed
// inside the outdoor houses: each door aligned with the outdoor door (the zones' warps), scaled to
// fit the outdoor shell, and the black panels that closed the doorways removed.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

// file; the interior's door; the outdoor door (its x) and facade (z); the outdoor walls' footprint
// (the front facade's width and the side walls' depth, measured on the model); the horizontal scale front to back (the lab is shallow outside, deep inside)
export const INTERIORS = [
  { file: 'house_a.gltf', door: [-81, 0, -18], outdoor: [-99.5, 1, -142], box: [-164, -58, -213, -139], s: 0.42, sz: 0.29 },
  { file: 'house_b.gltf', door: [-81, 0, -18], outdoor: [117, 1, -142], box: [76, 181, -213, -139], s: 0.42, sz: 0.29 },
  { file: 'lab.gltf', door: [-117, 0, 54], outdoor: [-63, 1, 77], box: [-131, -2, -16, 80], s: 0.52, sz: 0.32 },
];
const INSET = 3;

/// Removes the doorways' black panels from the outdoor model (triangles whose three vertex colours are black).
export function openDoorways(mesh) {
  const col = mesh.geometry.attributes.color, index = mesh.geometry.index;
  if (!col || !index) return 0;
  const dark = (v) => (col.getX(v) + col.getY(v) + col.getZ(v)) / 3 < 0.06;
  const kept = [];
  let removed = 0;
  for (let t = 0; t < index.count; t += 3) {
    const a = index.getX(t), b = index.getX(t + 1), c = index.getX(t + 2);
    if (dark(a) && dark(b) && dark(c)) { removed++; continue; }
    kept.push(a, b, c);
  }
  mesh.geometry.setIndex(kept);
  return removed;
}

export async function loadInteriors() {
  const groups = [];
  for (const it of INTERIORS) {
    const gltf = await new Promise((res, rej) => new GLTFLoader().load(it.file, res, undefined, rej));
    const g = gltf.scene;
    g.updateMatrixWorld(true);
    // into the house: the door on the outdoor door, the front wall just behind the facade...
    const m = new THREE.Matrix4().makeTranslation(it.outdoor[0], it.outdoor[1], it.outdoor[2])
      .multiply(new THREE.Matrix4().makeScale(it.s, it.s, it.sz))
      .multiply(new THREE.Matrix4().makeTranslation(-it.door[0], -it.door[1], -it.door[2]));
    // ...then whatever sticks out of the outdoor walls is pressed back onto them (hidden behind them)
    const [x0, x1, z0, z1] = it.box;
    const p = new THREE.Vector3();
    g.traverse((o) => {
      if (!o.isMesh) return;
      const geo = o.geometry.clone(); o.geometry = geo;
      const pos = geo.attributes.position;
      const toWorld = new THREE.Matrix4().multiplyMatrices(m, o.matrixWorld);
      for (let v = 0; v < pos.count; v++) {
        p.fromBufferAttribute(pos, v).applyMatrix4(toWorld);
        p.x = Math.min(Math.max(p.x, x0 + INSET), x1 - INSET);
        p.z = Math.min(Math.max(p.z, z0 + INSET), z1 - INSET);
        pos.setXYZ(v, p.x, p.y, p.z);
      }
      pos.needsUpdate = true; geo.computeVertexNormals();
      o.position.set(0, 0, 0); o.rotation.set(0, 0, 0); o.scale.set(1, 1, 1);
    });
    // the meshes are now in world space: flatten the hierarchy
    const flat = new THREE.Group(); const list = []; g.traverse((o) => o.isMesh && list.push(o));
    for (const o of list) flat.add(o);
    groups.push(flat);
  }
  return groups;
}
