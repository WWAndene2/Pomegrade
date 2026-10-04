#ifndef REMAKE_NITROCOMPRESSION_H
#define REMAKE_NITROCOMPRESSION_H

// The DS BIOS's LZ77 variants, which Pokemon DS games use on many files:
// LZ10 (header byte 0x10) and LZ11 (0x11, longer matches). Both: a 24-bit
// decompressed size after the type byte (LZ11: a further 32-bit size when 0),
// then blocks of 8 tokens, each flag bit (MSB first) a literal byte (0) or a
// back-reference (1).

#include "Bytes.h"

namespace remake
{

// whether data looks LZ-compressed: type byte 0x10/0x11 and a plausible size
bool IsLzCompressed(const Bytes& data);
Bytes LzDecompress(const Bytes& data);

}

#endif // REMAKE_NITROCOMPRESSION_H
