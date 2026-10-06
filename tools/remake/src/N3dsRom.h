#ifndef REMAKE_N3DSROM_H
#define REMAKE_N3DSROM_H

// A decrypted 3DS game image (.3ds: NCSD, or a single NCCH: .cxi), read for
// its RomFS (the game's file system: a/0/3/9 and so on). Read from the file
// on demand: an image is up to several GB. Layout: NCSD partition 0, or the
// NCCH itself; NCCH flags[7] bit 2 "no crypto" (an encrypted image is
// refused: its RomFS can't be read without the console's keys); RomFS at the
// NCCH's 0x1B0 (in 0x200-byte units): an IVFC header whose level 3 starts at
// 0x60 + the master hash size, aligned to its block size, holding the
// directory and file tables. Checked on Omega Ruby (Europe): 655 files, the
// map archives' content identical to the same files read separately.

#include "Bytes.h"

#include <fstream>
#include <map>
#include <string>

namespace remake
{

class N3dsRom
{
public:
    explicit N3dsRom(const std::string& path);

    uint64_t ProgramId() const { return Program; } // 000400000011C400: Omega Ruby
    const std::string& ProductCode() const { return Product; }
    // RomFS paths ("a/0/3/9") to their offset and size in the image
    const std::map<std::string, std::pair<uint64_t, uint64_t>>& Files() const { return FileList; }
    bool Has(const std::string& path) const { return FileList.count(path) != 0; }
    Bytes Read(const std::string& path);
    // the game's code (ExeFS ".code"), decompressed when the extended header says it is packed (BLZ, BlzDecompress): ARM code
    // loaded at 0x100000 (ORAS_LITTLEROOT.md 7)
    Bytes Code();

private:
    uint64_t Ncch = 0;
    std::ifstream File;
    uint64_t ImageSize = 0, Program = 0;
    std::string Product;
    std::map<std::string, std::pair<uint64_t, uint64_t>> FileList;
    Bytes ReadAt(uint64_t offset, uint64_t size);
};

}

#endif // REMAKE_N3DSROM_H
