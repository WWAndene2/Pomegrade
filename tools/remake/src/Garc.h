#ifndef REMAKE_GARC_H
#define REMAKE_GARC_H

// GARC: the archive Pokemon 3DS games (X/Y, Omega Ruby / Alpha Sapphire,
// Sun/Moon) pack their RomFS data in. Little-endian, magic "CRAG" ("GARC"
// reversed). Header (0x1C bytes in version 4: X/Y and ORAS; 0x24 in
// version 6: Sun/Moon), then three sections:
// - FATO ("OTAF"): one offset per entry into FATB;
// - FATB ("BTAF"): per entry, a bit mask of which sub-files exist, then for
//   each a start, end and length, relative to the data;
// - FIMB ("BMIF"): the data.
// Checked on ORAS's map archives (a/0/3/9, a/0/4/0): version 4, header
// 0x1C bytes (magic, header size, 0xFEFF, version, 4 sections, data start,
// file size, the largest sub-file's length); FATO's count is followed by
// 0xFFFF; each sub-file starts 4-byte aligned, padded with 0xFF, its FATB
// end at the padded end and its length unpadded. Most entries have one
// sub-file (bit 0); some (a Pokemon's several forms) have more.

#include "Bytes.h"

#include <vector>

namespace remake
{

class Garc
{
public:
    explicit Garc(const Bytes& data);
    static bool Is(const Bytes& data);

    uint16_t Version() const { return VersionNumber; }
    size_t Count() const { return Entries.size(); }
    // the sub-files of entry i, by their bit (0-31); absent ones are empty
    size_t SubCount(size_t i) const { return Entries.at(i).size(); }
    const Bytes& Sub(size_t i, size_t sub = 0) const { return Entries.at(i).at(sub); }
    bool Has(size_t i, size_t sub = 0) const { return sub < Entries.at(i).size() && Present.at(i) >> sub & 1; }

    // replaces (or adds) entry i's sub-file; i == Count() appends an entry
    void Set(size_t i, Bytes data, size_t sub = 0);
    // the archive as the game reads it (version 4 only: X/Y, ORAS); byte-identical to the file read
    // when nothing was changed
    Bytes Write() const;

private:
    uint16_t VersionNumber = 0;
    std::vector<std::vector<Bytes>> Entries;
    std::vector<uint32_t> Present; // per entry, its bit mask
};

}

#endif // REMAKE_GARC_H
