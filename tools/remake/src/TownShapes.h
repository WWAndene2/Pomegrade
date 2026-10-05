#ifndef REMAKE_TOWNSHAPES_H
#define REMAKE_TOWNSHAPES_H

// Zone shapes from a grid of cells, as ORAS lays them: a zone's mask (paths, lighter grass, the walkable ground) becomes its
// fill (triangles) and its outlines (chains of points with their outward normals) from the same lattice, so a fill and the
// strip laid along its border always agree.

#include <array>
#include <vector>

namespace remake
{

struct ShapePoint { float X = 0, Z = 0; };

struct ShapeChain
{
    std::vector<ShapePoint> Points;
    std::vector<ShapePoint> Normals; // unit, pointing out of the zone
    bool Closed = false;
};

struct ZoneShape
{
    std::vector<std::array<ShapePoint, 3>> Fill; // counter-clockwise in (X, Z) as the ground's quads
    std::vector<ShapeChain> Chains;
};

// mask[row][column], cells of cellSize world units whose top-left corner is (originX, originZ). The boundary runs along the cell
// lattice, one point at every lattice point, so a zone is made of whole cells and its corners are square; each lattice point
// is moved by a small fixed jitter, up to jitter/2 of a cell either way (a hash of the lattice point: the same zone, the same shape; the fill
// and the chains share it). Over the whole game the norm is no jitter at all (mean 0.007 tile on light grass; ORAS_LITTLEROOT.md 9e): it is Littleroot's style
// (mean 0.056), the one the town is cut like.
// roundTips: where three of the four cells around a lattice point are in the zone (a tip of the *outside* pokes into it), the
// point is replaced by an arc's middle vertex, 0.36 cell towards that outside cell on each axis, so the border cuts the tip with
// a radius of one cell (the forest's edge); a tip of the zone itself stays square. Cells outside the mask repeat its edge.
ZoneShape StairZone(const std::vector<std::vector<bool>>& mask, float cellSize, float originX, float originZ, bool roundTips, float jitter = 0.18f);

}

#endif // REMAKE_TOWNSHAPES_H
