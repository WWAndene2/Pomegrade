#ifndef REMAKE_BLZ_H
#define REMAKE_BLZ_H

// The 3DS's "backward LZ" (BLZ), which packs a game's ExeFS .code: decompressed from the end towards the start. The packed file
// ends with two u32: the packed region's length (low 24 bits) and the footer's length (top 8), then how many bytes decompressing
// adds. Each control byte (read backwards) gives 8 flags, high bit first: 0 a literal byte, 1 a u16 (length - 3 in bits 12-15,
// distance - 2 in bits 0-11, counted from the output position before each byte is written). Layout as ctrtool and 3dstool decode it; checked on Omega Ruby's
// .code by its decompressed size (0x530000) and the facts ORAS_LITTLEROOT.md section 7 read from it.

#include "Bytes.h"

namespace remake
{

// throws FormatError on a stream that runs out of bounds
Bytes BlzDecompress(const Bytes& packed);

}

#endif // REMAKE_BLZ_H
