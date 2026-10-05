// The remake tooling's map matrix (OrasMatrix), on matrices built here to the layout read on all 431 of the game's
// (ORAS_LITTLEROOT.md 2b): the piece grid alone (interiors) and the piece, zone and third grids (the overworld); each read
// and written back byte for byte, a matrix made from nothing written in that layout, and sizes fitting neither refused.
#include "BinLinker.h"
#include "OrasMatrix.h"

#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void P16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }

static Bytes Container(const Bytes& file0, const Bytes& file1)
{
    BinLinker c;
    c.Tag = "MM";
    c.Align = 4;
    c.Files = {file0, file1};
    return c.Write();
}

int main()
{
    const Bytes file1 = {1, 0, 0, 0, 0, 0xA0, 0xB9, 0x44};

    // an interior: 1 x 3 pieces, padded to 4 bytes
    {
        Bytes f;
        for (uint16_t v : {1, 0, 1, 3, 200, 201, 0xFFFF}) P16(f, v);
        f.push_back(0); f.push_back(0);
        const Bytes mm = Container(f, file1);
        const OrasMatrix m = OrasMatrix::Read(mm);
        check(m.Width == 1 && m.Height == 3 && m.Pieces.size() == 3 && m.Pieces[1] == 201 && m.Pieces[2] == OrasMatrix::None, "the piece grid is read");
        check(m.Zones.empty() && m.Third.empty() && m.File1 == file1, "a matrix with the piece grid only has no zone grid; file 1 kept");
        check(m.Write() == mm, "it is written back byte for byte");
    }

    // an overworld section: 3 x 2 pieces, a 12 x 8 zone grid, the third grid
    {
        Bytes f;
        for (uint16_t v : {1, 0, 3, 2}) P16(f, v);
        for (int k = 0; k < 6; k++) P16(f, k == 4 ? 0xFFFF : (uint16_t)(10 + k));
        for (int k = 0; k < 96; k++) P16(f, (uint16_t)(k < 48 ? 6 : 23));
        for (int k = 0; k < 6; k++) P16(f, k == 0 ? 7 : 0xFFFF);
        const Bytes mm = Container(f, file1);
        OrasMatrix m = OrasMatrix::Read(mm);
        check(m.Zones.size() == 96 && m.Zone(0, 0) == 6 && m.Zone(11, 7) == 23 && m.Zone(5, 3) == 6 && m.Zone(5, 4) == 23, "the zone grid is 4 blocks a piece, row by row");
        check(m.Piece(1, 1) == OrasMatrix::None && m.Piece(2, 0) == 12 && m.Third[0] == 7, "pieces by cell, the third grid kept");
        check(m.Write() == mm, "it is written back byte for byte");
        m.Zone(3, 2) = 538;
        check(OrasMatrix::Read(m.Write()).Zone(3, 2) == 538, "a changed block reads back");
    }

    // made from nothing, as oras-region makes its matrix
    {
        OrasMatrix m;
        m.Width = 2; m.Height = 3;
        m.Pieces.assign(6, OrasMatrix::None);
        m.Zones.assign(96, OrasMatrix::None);
        m.Third.assign(6, OrasMatrix::None);
        m.File1 = file1;
        m.Piece(1, 2) = 857;
        m.Zone(7, 11) = 6;
        const Bytes mm = m.Write();
        const Bytes f = BinLinker::Read(mm, "MM").Files[0];
        check(f.size() == 8 + 36 * 6 && U16(f, 4) == 2 && U16(f, 6) == 3, "file 0 is 8 + 36 wh bytes, width and height in its header");
        check(U16(f, 8 + 2 * 5) == 857 && U16(f, 8 + 12 + 2 * 95) == 6, "the last cell and the last block are where the layout puts them");
        bool refused = false;
        m.Third.clear();
        try { m.Write(); } catch (const FormatError&) { refused = true; }
        check(refused, "a zone grid without the third grid is refused");
    }

    // sizes that fit neither layout
    {
        Bytes f;
        for (uint16_t v : {1, 0, 2, 2, 1, 2, 3, 4, 5, 6}) P16(f, v);
        bool refused = false;
        try { OrasMatrix::Read(Container(f, file1)); } catch (const FormatError&) { refused = true; }
        check(refused, "a file 0 longer than the piece grid but short of the zone grids is refused");
    }

    printf("%s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
