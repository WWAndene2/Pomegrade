#ifndef REMAKE_GXDISPLAYLIST_H
#define REMAKE_GXDISPLAYLIST_H

// The DS geometry engine's command lists (what NSBMD shapes hold, as the
// game DMAs them to the GPU): packed words of four command bytes, lowest
// first, followed by their parameters. Decoded into triangles: positions
// (4.12 fixed point, from VTX_16/10/XY/XZ/YZ/DIFF), texture coordinates
// (12.4, in texels), normals (1.0.9), vertex colours (BGR555) and, per
// vertex, the matrix-stack slot last restored (MTX_RESTORE: the joint a
// skinned vertex follows). Matrix arithmetic and lighting are not applied:
// the caller places the vertices.

#include "Bytes.h"

#include <vector>

namespace remake
{

struct GxVertex
{
    float Position[3] = {};   // model units (4.12 / 4096)
    float TexCoord[2] = {};   // texels
    float Normal[3] = {0, 0, 1};
    uint16_t Colour = 0x7FFF; // BGR555
    int Joint = -1;           // matrix-stack slot restored before it, -1: none
};

struct GxMesh
{
    std::vector<GxVertex> Vertices;
    std::vector<uint32_t> Triangles; // three indices each, counter-clockwise as drawn
    uint32_t Commands = 0;           // commands read
};

// throws FormatError on an unknown command or a list cut short
GxMesh DecodeDisplayList(const Bytes& list);
// parameter words of a geometry command, -1 when it is not one
int GxParamCount(uint8_t command);

}

#endif // REMAKE_GXDISPLAYLIST_H
