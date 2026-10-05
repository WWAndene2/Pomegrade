#ifndef REMAKE_TOWNSHAPES_H
#define REMAKE_TOWNSHAPES_H

// Rounded shapes from a grid of cells: a zone's mask (paths, lighter grass, the walkable ground) is blurred and cut
// at its half level (marching squares), so its corners are rounded instead of square. The result is the zone's fill
// (triangles) and its outlines (chains of points with their outward normals) from the same cut, so a fill and the
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

// mask[row][column], cells of cellSize world units whose top-left corner is (originX, originZ). Each cell is split
// in two each way; blur: the radius, in those halves, of the two box blurs that round the corners (0: square, as the
// mask). Cells outside the mask repeat its edge, so a zone touching the border runs on.
ZoneShape SmoothZone(const std::vector<std::vector<bool>>& mask, float cellSize, float originX, float originZ, int blur);

}

#endif // REMAKE_TOWNSHAPES_H
