#ifndef REMAKE_ROOMBUILDER_H
#define REMAKE_ROOMBUILDER_H

// An interior's shell (SINNOH_BUILD.md R4): a Platinum room's floor, walls, door and windows, computed as ORAS draws them
// and in ORAS's measures (Littleroot's first house, t101r0101, measured on 9 October; RoomBuilder.cpp has the numbers). The
// room is the tiles Platinum gives terrain. Its walls are flat rectangles standing on the room's edges (north: the back wall;
// west and east: the side walls; the south edge stays open, the camera looking from there), 54 units high with ORAS's
// skirting, wallpaper and moulding bands, topped by ORAS's dark cap one tile deep; the floor is ORAS's wood, darkened along the
// walls as ORAS's is; the south edge is a lip going down; the door is ORAS's way out (the floor one tile further out, its mat
// and its light) on Platinum's exit mat; each Platinum window (building model window01) is ORAS's window (an opening in the
// wall with its recess, sill, frame and glass) at the same place; the stairs are computed on Platinum's stairs tiles (steps in
// ORAS's measures; going down, an opening in the floor). The tiles a wall stands on are solid (outside the room).
// Furniture: each Platinum piece of furniture with an ORAS equivalent gets ORAS's own, cut out of Littleroot's house (its ground
// floor's kitchen, table, chairs, TV and plant; its bedroom's bed, desk, console and stool) at Platinum's place, its tiles solid
// (RoomBuilder.cpp, Furniture).

#include "TownLayout.h"

namespace remake
{

// an ORAS room giving the materials (its terrain model is the room's): its map piece (a/0/3/9) and the area pack holding its
// textures
struct RoomDonor
{
    size_t Piece = 0, Pack = 0;
    size_t Bedroom = 0; // the same house's bedroom (a/0/3/9), its textures in the same pack: the furniture the ground floor lacks
};

// Littleroot's first house, its ground floor (t101r0101, zone 223, area pack 112) and its bedroom (t101r0102)
extern const RoomDonor GroundFloorRoom;

// the piece townPiece with its terrain model replaced by the room (named modelName, as long as the donor's); stairsC/R: the
// tile of the warp to another floor (-1: none), stairsUp: whether it goes up
Bytes BuildRoom(const TownLayout& layout, const Bytes& townPiece, const Bytes& donorPiece, const Bytes& bedroomPiece, const RoomDonor& donor,
                int stairsC, int stairsR, bool stairsUp, const std::string& modelName, std::vector<std::string>& log);

}

#endif // REMAKE_ROOMBUILDER_H
