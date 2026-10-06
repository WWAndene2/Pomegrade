#ifndef REMAKE_BPS_H
#define REMAKE_BPS_H

// BPS patches (beat format, "BPS1"): a target file described as runs copied
// from the source file and new bytes, with CRC32s of source, target and patch.
// Azahar applies them to RomFS files from load/mods/<program id>/romfs_ext/
// <path>.bps (azahar/src/core/file_sys/layered_fs.cpp, patch.cpp), so a mod
// that changes a few pieces of a 100 MB archive ships as a small file.
//
// Two limits of Azahar's applier: no metadata, and a target shorter than its
// source keeps the source's size (zero-filled past the target's end), so a
// patch for Azahar should not shrink the file. Pomegrade's Azahar cuts such a
// target to its size since 6 October 2026 (patch.cpp, ApplyBpsPatch); the
// workarounds stay for builds older than that.

#include "Bytes.h"

namespace remake
{

uint32_t Crc32(const Bytes& data);
// a patch turning source into target: runs of 32 bytes or more found in the source (at the same
// offset, or anywhere through a hash of its 32-byte blocks), new bytes for the rest
Bytes BpsCreate(const Bytes& source, const Bytes& target);
// target from source and patch; throws FormatError on a bad patch or a checksum mismatch
Bytes BpsApply(const Bytes& source, const Bytes& patch);

}

#endif // REMAKE_BPS_H
