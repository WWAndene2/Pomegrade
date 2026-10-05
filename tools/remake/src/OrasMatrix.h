#ifndef REMAKE_ORASMATRIX_H
#define REMAKE_ORASMATRIX_H

// One map matrix of Omega Ruby / Alpha Sapphire (a member of a/0/4/0, an "MM" container of two files), as
// ORAS_LITTLEROOT.md section 2b reads it, checked on all 431 matrices (`oras-inspect matrices`, `oras-verify`):
//
//   file 0   u16 1, u16 0, u16 width, u16 height, then width x height u16 map piece numbers (a/0/3/9 members), row by row,
//            0xFFFF for no piece. Then either nothing more (padded to 4 bytes: interiors and small places, the whole matrix
//            one zone) or a (4 width) x (4 height) grid of u16 zone numbers (a/0/1/3 members), one per 10 x 10 tiles,
//            0xFFFF outside the map, and a third width x height u16 grid (0xFFFF in 8 of the 15 such matrices, data in 7:
//            meaning unknown).
//   file 1   kept raw (324 bytes in matrix 1: a count, then groups of words that read as floats; meaning unknown).

#include "Bytes.h"

#include <vector>

namespace remake
{

struct OrasMatrix
{
    static constexpr uint16_t None = 0xFFFF;
    static constexpr int BlocksPerPiece = 4; // a piece's 40 tiles are 4 zone blocks of 10 across

    uint16_t Width = 0, Height = 0;
    std::vector<uint16_t> Pieces;   // Width x Height
    std::vector<uint16_t> Zones;    // (4 Width) x (4 Height), empty for a matrix with the piece grid only
    std::vector<uint16_t> Third;    // Width x Height, present with Zones
    Bytes File1;
    // as read, so a game matrix is written back byte for byte: the container's alignment (BinLinker measures it from the offsets,
    // 4 to 0x80) and the bytes after the piece grid of a matrix without a zone grid (its padding)
    uint32_t Align = 4;
    Bytes Padding;

    uint16_t& Piece(int x, int y) { return Pieces.at((size_t)y * Width + x); }
    uint16_t& Zone(int bx, int bz) { return Zones.at((size_t)bz * Width * BlocksPerPiece + bx); }

    // throws FormatError when file 0 fits neither layout
    static OrasMatrix Read(const Bytes& mm);
    Bytes Write() const;
};

}

#endif // REMAKE_ORASMATRIX_H
