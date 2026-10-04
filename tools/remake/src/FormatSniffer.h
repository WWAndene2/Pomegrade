#ifndef REMAKE_FORMATSNIFFER_H
#define REMAKE_FORMATSNIFFER_H

// What a file is, from its first bytes: the Nitro (DS) formats Pokemon games
// use, by their 4-character signature, and the containers and compression.

#include "Bytes.h"

#include <string>

namespace remake
{

struct FileKind
{
    std::string Id;           // "nsbmd", "narc", "lz10"... ("unknown" when not recognised)
    std::string Description;  // "3D models (NSBMD)"
};

FileKind Sniff(const Bytes& data);

}

#endif // REMAKE_FORMATSNIFFER_H
