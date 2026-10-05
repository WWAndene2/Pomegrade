#ifndef REMAKE_BINLINKER_H
#define REMAKE_BINLINKER_H

// The small container ORAS packs several files of one thing in (pk3DS calls
// it "mini"): a 2-letter tag ("GR": a map piece, "MM": a map matrix), u16
// count, count + 1 u32 offsets (the last one is the container's size), then
// the files, zero-padded to the container's alignment (the padding is kept
// with the file before it). The alignment depends on the kind: GR files
// start at 0x80 and every part is a multiple of 0x80; MM only aligns to 4.
// It is measured when reading (the largest power of two up to 0x80 dividing
// every offset) and kept when writing. Checked on ORAS: all 857 GR and 431
// MM files read and written back byte-identical.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct BinLinker
{
    std::string Tag;
    std::vector<Bytes> Files;
    uint32_t Align = 4;

    static BinLinker Read(const Bytes& data, const std::string& tag);
    Bytes Write() const;
};

}

#endif // REMAKE_BINLINKER_H
