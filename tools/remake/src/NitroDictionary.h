#ifndef REMAKE_NITRODICTIONARY_H
#define REMAKE_NITRODICTIONARY_H

// The name dictionary the Nitro 3D files (NSBMD, NSBTX, animations) index
// their contents with: a count, a search tree (not needed to list), then a
// data block of fixed-size entries and their 16-character names.
//   +0 u8 0, +1 u8 count, +2 u16 size; +4 the tree block (+6: its size);
//   then the data block: u16 entry size, u16 block size, the entries, the names.
// Layout from the community documentation of the Nitro formats; to be
// checked on a real file.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct DictEntry
{
    std::string Name;
    size_t Data = 0; // offset of this entry's data in the file
};

std::vector<DictEntry> ReadDictionary(const Bytes& file, size_t at);

}

#endif // REMAKE_NITRODICTIONARY_H
