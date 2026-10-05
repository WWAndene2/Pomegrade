#ifndef REMAKE_BCHTEXTUREFILE_H
#define REMAKE_BCHTEXTUREFILE_H

// A BCH file that holds textures only: what slots 1 and 11 of every ORAS area pack (a/0/1/4 "AD" members) are. Written from a list of
// decoded textures, so a pack can be given textures from any other pack (the pack's own slot-11 file is replaced by one holding the
// union that a map piece needs).
//
// Layout, measured on all 439 texture files of the game's 228 packs that hold data (the 17 others are empty placeholders: one texture,
// no data): header 0x44 bytes ("BCH\0", 21 21 6F A6, six section addresses, six lengths, u32 12 * count, u32 0, u16 1, u16 3 * count),
// then contents, strings, commands, raw data, relocations.
//   contents  15 dictionary descriptors (values pointer, count, name tree pointer; the textures are the fourth), the roots of the three
//             first (empty) trees, the textures' name tree (a Patricia tree, count + 1 nodes of 12 bytes), the textures' pointer list,
//             the roots of the eleven last (empty) trees, then one 32-byte object per texture: three command-list pointers and counts
//             (12 words each), u8 format, u8 1 (mipmaps), the name pointer.
//   strings   the names, NUL-ended, in order.
//   commands  per texture 144 bytes: three texture units of 12 words writing the size, the data's address and the format.
//   raw       the textures' data one after the other, the section padded to 0x80.
//   relocations  9 * count + 16 words naming each pointer (section, offset in words, or in bytes for strings, and the section it points into).
// The name tree: bit i of a name is (name[i / 8] >> (i % 8)) & 1 (0 past its end); a lookup walks from the root's left child while the
// reference bit decreases. Names are inserted in order: find the closest, take the highest bit where it differs, insert there.

#include "Bch.h"

namespace remake
{

struct BchTextureSource
{
    std::string Name;
    uint32_t Width = 0, Height = 0;
    uint8_t Format = 0;
    Bytes Data;   // as stored (PicaTextureLength bytes)
};

// the file for these textures, in this order; at least one
Bytes BchWriteTextureFile(const std::vector<BchTextureSource>& textures);

}

#endif // REMAKE_BCHTEXTUREFILE_H
