#ifndef REMAKE_PNG_H
#define REMAKE_PNG_H

// A minimal PNG writer (RGBA8, zlib "stored" blocks: no compression, no
// dependency): decoded game textures, written for viewing and for glTF.

#include "Bytes.h"

namespace remake
{

// rgba: width * height * 4 bytes
Bytes EncodePng(uint32_t width, uint32_t height, const Bytes& rgba);

}

#endif // REMAKE_PNG_H
