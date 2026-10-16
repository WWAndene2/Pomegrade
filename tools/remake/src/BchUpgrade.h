#ifndef REMAKE_BCHUPGRADE_H
#define REMAKE_BCHUPGRADE_H

// A Pokemon X/Y BCH (version 0x07, converter 38669) rewritten as Omega Ruby's (version 0x21, converter 42607), which
// ORAS's engine draws (run xchar, 16 October: an X character's model left as 0x07 in a/0/2/1 loads, but only its shadow
// shows). The differences, after SPICA (its IfVersion fields and GetLegacyRelocDiff, which needs nothing for 0x07):
// - the header: from 0x21 a RawExt section's address and length (left empty, at the relocation table), then a u16 of
//   flags (1, "from the new converter", as ORAS's files have it) and a u16 count of the pointers into the raw data,
//   the uninitialised data length 4 bytes each (ORAS's character 284: 14 and 56);
// - a material: before 0x21 its 3 texture mappers (16 bytes each) inline at +0x18, the material 0x58 bytes; from 0x21
//   a pointer there to the 3 mappers, kept elsewhere in the contents, the material 0x2C bytes. The mappers are moved to
//   the end of the contents and every pointer into or from the contents follows its data;
// - a texture's address in the command lists: relocated into section 4 (raw data) in X's files, 5 (raw data,
//   textures) in ORAS's (seen in both games' character files: three words a texture, the commands of texture units
//   0-2, registers 0x85, 0x95, 0x9D); so moved into section 5.
// The raw data (vertices, indices, textures) is kept byte for byte, at a 0x80-aligned address as before.
// Not converted: animations inside the file (their key frame groups changed at 0x20, SPICA), refused; X's motion
// packs (a character member's other files) are another format, not checked.

#include "Bytes.h"

namespace remake
{

// bch (version 0x07) as version 0x21; throws FormatError for another version, or a file this conversion can't carry
Bytes BchUpgradeTo21(const Bytes& bch);

}

#endif // REMAKE_BCHUPGRADE_H
