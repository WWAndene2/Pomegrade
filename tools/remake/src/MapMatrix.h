#ifndef REMAKE_MAPMATRIX_H
#define REMAKE_MAPMATRIX_H

// A Pokemon Diamond/Pearl/Platinum map matrix (a member of
// fielddata/mapmatrix/map_matrix.narc): the grid that lays the world's map
// pieces side by side. u8 width, u8 height, u8 has-headers, u8 has-heights,
// u8 name length, the name, then per cell (row-major): the map header id
// (u16, if present), the height layer (u8, if present), the land data id
// (u16, 0xFFFF: no map). Layout from the community documentation (Pokemon
// DS Map Studio); to be checked on a real file.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct MapMatrix
{
    std::string Name;
    uint32_t Width = 0, Height = 0;
    std::vector<int> Headers;   // per cell, -1 when the matrix has none
    std::vector<int> Heights;   // per cell, 0 when the matrix has none
    std::vector<int> LandData;  // per cell, -1: empty

    static MapMatrix Read(const Bytes& data);
    size_t Cell(uint32_t x, uint32_t y) const { return (size_t)y * Width + x; }
};

}

#endif // REMAKE_MAPMATRIX_H
