#ifndef REMAKE_ORASXYEFFECTPACK_H
#define REMAKE_ORASXYEFFECTPACK_H

// The field's effect pack given X/Y's effect files Omega Ruby has not: its archive a/0/6/4 (one "TM" of the effect files,
// an effect's id its entry's number) and the pack DllField builds from it, grown to the new ids. Two users:
//  - X/Y's dust clouds' swirl (OrasXyDustCloud.h): X's a/0/6/6 entries 0xBD-0xBF at the same numbers;
//  - X/Y's flower beds: the shaken tall grass drawn on a tile walked into. Both games pick it the same way (Omega Ruby's
//    DllField 0x1031E414, read 28 October): the tile's kind 0x20, 0x21, 0x22, 0x23, 0x27 or 0x1F (the .code's 0x403628-
//    0x40368C) is a grass kind 0-5, the model and animation ids of the kind from two tables (DllField + 0xF5C64, + 0xF5C7C).
//    X's (its DllField + 0xF3678, + 0xF368C) name grass00-grass03 for kinds 0-3, each its own corner of the enhana atlas:
//    the plain grass and the yellow, lavender and red flower beds X's tiles 0x21-0x23 are; Omega Ruby's name grass00 for
//    all four (it has no flower bed: none of its 857 pieces holds a tile 0x21-0x23, checked), so on X/Y's flower beds the
//    flowers turned to plain grass where the player walked (owner's phone, 28 October; run r5s: X's Route 5, mod zone 539).
//    X's grass01-03 and their animations (X's ids 0x31-0x32, 0x3F-0x40, 0x4D-0x4E) are given after the swirl's, and
//    Omega Ruby's table names them for kinds 1-3, as X's does. Kinds 0, 4 and 5 keep Omega Ruby's.

#include "Bytes.h"
#include "CodePatch.h"

#include <vector>

namespace remake
{

constexpr uint32_t XySwirlFirstId = 0xBD;        // X's swirl's effect files 0xBD-0xBF: Omega Ruby's a/0/6/4 entries 0xBD-0xBF
constexpr uint32_t XyFlowerGrassFirstId = 0xC0;  // X's grass01-03, model and animation each: entries 0xC0-0xC5

// Omega Ruby's a/0/6/4 with X's a/0/6/6 swirl files appended (BCH 0x07 -> 0x21), and X's flower grass after them when
// flowerGrass is set
Bytes XyEffectsArchive(const Bytes& orasGarc, const Bytes& xGarc, bool flowerGrass);

// DllField's words (linked addresses): the effect pack grown to the last id XyEffectsArchive gives (its builder, its
// allocation, its lookup; Omega Ruby's unused ids skipped in a cave at `cave`)
std::vector<CodeWord> XyEffectPackPatchAt(uint32_t cave, bool flowerGrass);

// DllField's grass tables' kinds 1-3 naming X's grass01-03: words of its read-only segment (file 0xF1000 on), which the
// bytes its code was grown by (room, oras-engine --field-room) move by as much in the file
std::vector<CodeWord> XyFlowerGrassTableWords(uint32_t room);

}

#endif // REMAKE_ORASXYEFFECTPACK_H
