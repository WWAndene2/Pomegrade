#ifndef REMAKE_BCHWRITER_H
#define REMAKE_BCHWRITER_H

// New geometry in an existing BCH model: a mesh's vertices and triangles
// replaced, everything else (its material, shader, vertex format, the
// model's other meshes, textures) kept as the game made it. So a new map
// piece's terrain is an ORAS piece's terrain BCH with its meshes rebuilt.
//
// The new vertex and index buffers are appended to the raw data section,
// after its padding to 0x80 (before any RawExt data and the relocation
// table, which move down; the header's addresses and lengths follow). The mesh's command words
// are repointed: the vertex buffer's address (0x203) and the first
// sub-mesh's index buffer (0x227, now 16-bit: its relocation names the
// 16-bit index section) and count (0x228). A mesh's other sub-meshes draw
// one degenerate triangle. Vertices are written in the mesh's own format
// (each attribute divided by its scale, the position less its offset; a
// second texture coordinate set gets the first's), so a vertex format the
// writer can't fill (bone indices and weights) is refused.
//
// Not changed: the model's and meshes' bounding data (the game may cull a
// mesh by its old bounds; a map piece's terrain keeps the piece's extent).
// The old buffers stay in the file, unused: it grows by the new geometry
// (whether the game has room for much larger pieces is not known yet).

#include "Bch.h"

namespace remake
{

struct BchGeometry
{
    size_t Mesh = 0;                  // index in the model's meshes
    std::vector<BchVertex> Vertices;  // at most 65536
    std::vector<uint32_t> Triangles;  // three indices each; empty draws nothing
};

// bch with the geometry of the given meshes of its model replaced; throws FormatError when the
// file's layout or a mesh's vertex format doesn't allow it
Bytes BchReplaceGeometry(const Bytes& bch, size_t model, const std::vector<BchGeometry>& meshes);

}

#endif // REMAKE_BCHWRITER_H
