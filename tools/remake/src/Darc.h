#ifndef REMAKE_DARC_H
#define REMAKE_DARC_H

// DARC: the 3DS archive that holds a layout's files (BCLYT, BCLAN, BCLIM) under folders. Header: "darc", u16 BOM 0xFEFF,
// u16 header size 0x1C, u32 version, u32 file size, u32 table offset, u32 table size, u32 data offset. The table: 12-byte
// nodes (u32 name offset in the name table, bit 24 set for a folder; u32 file offset or folder parent; u32 file size or
// the index past the folder's last node), node 0 the root holding the node count, then the UTF-16 names. The layout is
// the community's (Kuriimu, 3dstool); checked here against itself (remake_layout_test).

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct DarcFile { std::string Path; Bytes Data; };

std::vector<DarcFile> ReadDarc(const Bytes& file);
// the files under folders built from their paths, each folder's files and subfolders contiguous (ReadDarc returns them
// folder by folder: a folder's files in their given order, then its subfolders')
Bytes WriteDarc(const std::vector<DarcFile>& files);

}

#endif // REMAKE_DARC_H
