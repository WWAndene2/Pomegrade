#include "NitroDictionary.h"

namespace remake
{

std::vector<DictEntry> ReadDictionary(const Bytes& file, size_t at)
{
    const uint8_t count = U8(file, at + 1);
    // the tree block's size field counts from the dictionary's start (its 4-byte header
    // included): the data block starts right there
    const size_t data = at + U16(file, at + 6);
    const uint16_t unit = U16(file, data);
    if (unit == 0 && count) throw FormatError("dictionary: zero-sized entries");
    if (U16(file, data + 2) != 4 + (size_t)count * unit) throw FormatError("dictionary: data block size doesn't match its entries");
    std::vector<DictEntry> out;
    for (uint8_t i = 0; i < count; i++)
    {
        DictEntry e;
        e.Data = data + 4 + (size_t)i * unit;
        e.Name = Text(file, data + 4 + (size_t)count * unit + i * 16, 16);
        out.push_back(e);
    }
    return out;
}

}
