// Virtual Volumetric Layered Depth: the protruding parts recognised in each texture (materials.py,
// mat/<i>_layer<k>.png) become a few thin shells of real geometry above the surface, each cut out to
// the texels at least that high. Real triangles, so a path tracer shades, shadows and occludes them
// like the rest of the scene; light (4 layers, 0.5 units in all) so the game's shapes stay as drawn.
import * as THREE from 'three';

export const LAYERS = 4, THICKNESS = 0.5;
const DECALS = ['chip_grass_decolate'];

// shells for every mesh of a recognised material; makeMaterial(k, mesh) gives layer k's material
export async function addVolumeLayers(root, mats, makeMaterial) {
  const bases = [];
  root.traverse((o) => { if (o.isMesh && o.visible && !o.userData.vvld && mats[o.userData.matName ?? o.material.name] !== undefined && !DECALS.includes(o.userData.matName ?? o.material.name) && !(o.userData.transparent ?? o.material.transparent)) bases.push(o); });
  // decals laid flat on the ground (dirt patches, flowers) go above the shells, or the grass's
  // layers would cover them
  root.traverse((o) => {
    if (!o.isMesh || !DECALS.includes(o.userData.matName ?? o.material.name)) return;
    if (!o.geometry.attributes.normal) o.geometry.computeVertexNormals();
    const pos = o.geometry.attributes.position, nrm = o.geometry.attributes.normal, s = new THREE.Vector3(); o.getWorldScale(s);
    const h = (THICKNESS + 0.05) / Math.max(Math.abs(s.x), Math.abs(s.y), Math.abs(s.z), 1e-6);
    for (let i = 0; i < pos.count; i++)
      pos.setXYZ(i, pos.getX(i) + nrm.getX(i) * h, pos.getY(i) + nrm.getY(i) * h, pos.getZ(i) + nrm.getZ(i) * h);
    pos.needsUpdate = true;
  });
  let count = 0;
  for (const o of bases) {
    if (!o.geometry.attributes.normal) o.geometry.computeVertexNormals();
    for (let k = 1; k <= LAYERS; k++) {
      const g = o.geometry.clone();
      const pos = g.attributes.position, nrm = g.attributes.normal;
      // the offset in world units, whatever the mesh's own scale
      const s = new THREE.Vector3(); o.getWorldScale(s);
      const h = THICKNESS * k / LAYERS / Math.max(Math.abs(s.x), Math.abs(s.y), Math.abs(s.z), 1e-6);
      for (let i = 0; i < pos.count; i++)
        pos.setXYZ(i, pos.getX(i) + nrm.getX(i) * h, pos.getY(i) + nrm.getY(i) * h, pos.getZ(i) + nrm.getZ(i) * h);
      pos.needsUpdate = true;
      g.userData = {};
      const shell = new THREE.Mesh(g, await makeMaterial(k, o));
      shell.userData.vvld = k;
      shell.position.copy(o.position); shell.quaternion.copy(o.quaternion); shell.scale.copy(o.scale);
      o.parent.add(shell); count++;
    }
  }
  return count;
}
