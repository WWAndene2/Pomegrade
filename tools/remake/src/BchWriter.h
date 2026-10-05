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
// The replaced meshes' old buffers are removed, in whole 0x80 blocks (the
// data left keeps its alignment) and only where no pointer points into them,
// and every pointer into the raw data after them moves down.

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

// bch with one material's texture (slot 0-2, one that names a texture already) named otherwise: the
// game binds a material's textures by name, from the zone's area pack. The new name is appended to the
// string section (padded to 0x80, so the sections after it keep their alignment and, their pointers
// being relative to their own section, only the header's addresses move) and the slot repointed to it.
Bytes BchSetTextureName(const Bytes& bch, size_t model, size_t material, int slot, const std::string& name);

// bch with every string equal to `from` in its string section, and every "<name>@<from>" (a material
// qualified by its model), made `to` (the same length: nothing moves).
// For a map piece's terrain model, whose name tells its place (world<matrix>_<x>_<y>: Littleroot's
// world01_02_04): a model taken from another piece is renamed for its new place. Throws when the
// lengths differ or no string matches.
Bytes BchReplaceString(const Bytes& bch, const std::string& from, const std::string& to);

}

#endif // REMAKE_BCHWRITER_H
