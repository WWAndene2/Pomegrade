#ifndef REMAKE_ROOMBUILDER_H
#define REMAKE_ROOMBUILDER_H

// An interior's look (SINNOH_BUILD.md R4): a Platinum room rebuilt with an ORAS room's own models. The room is the tiles
// Platinum gives terrain (outside them a Platinum room map has none); its floor and walls are one-tile slices of the donor
// room's own floor, back wall and side walls (triangles clipped to the tile, texture coordinates carried on), laid on every
// room tile and along every edge the room ends on (north: the back wall; west and east: the side walls; the south edge stays
// open, as the camera looks from there); the stairs are the donor's own stairs, placed on Platinum's stairs tiles. No
// furniture: the owner's order (9 October) is correct, solid walls with their door and windows first; the furniture placed
// from Platinum's material names came out misplaced, overlapping and cut (removed).

#include "TownLayout.h"

namespace remake
{

// an ORAS room used as the donor: its map piece (a/0/3/9), the area pack holding its textures, the tiles its plain floor and
// plain walls are sliced from (donor tile coordinates: column, row), and its stairs (a box of tiles and the warp tile on them)
struct RoomDonor
{
    size_t Piece = 0, Pack = 0;
    int FloorC = 0, FloorR = 0;         // a tile of plain floor
    float BackZ = 0; int BackC = 0;     // the back wall's line (row) and a column where it is plain
    float LeftX = 0; int LeftR = 0;     // the west wall's line (column) and a row where it is plain
    float RightX = 0; int RightR = 0;   // the east wall's line and a row where it is plain
    float StairsBox[4] = {};            // columns c0-c1, rows r0-r1 holding the stairs, their walls included
    int StairsC = 0, StairsR = 0;       // the stairs warp's tile, which the Platinum stairs warp's tile is put on
};

// Littleroot's first house, its ground floor (t101r0101, zone 223) and its upstairs (t101r0102, zone 224): both in area pack 112
extern const RoomDonor GroundFloorRoom, UpstairsRoom;

// the piece townPiece with its terrain model replaced by the room (named modelName, 15 characters as the game's interiors);
// stairs: the tile of the warp to another floor, or (-1, -1)
Bytes BuildRoom(const TownLayout& layout, const Bytes& townPiece, const Bytes& donorPiece, const RoomDonor& donor, int stairsC, int stairsR,
                const std::string& modelName, std::vector<std::string>& log);

}

#endif // REMAKE_ROOMBUILDER_H
