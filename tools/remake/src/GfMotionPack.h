#ifndef REMAKE_GF_MOTION_PACK_H
#define REMAKE_GF_MOTION_PACK_H

// A character's motion pack (the second file of an a/0/2/1 member, X/Y and Omega Ruby alike): u32 slot count, then
// count + 1 u32 offsets (0 an empty slot; the last one the pack's end), then the slots' data, each 4-aligned. Slot 0
// is the character's skeleton, the others its motions (GfMotion.h), which the game asks for by slot number.
//
// X/Y's packs have 173 slots, Omega Ruby's 239, numbered otherwise. Read 16 October from the frame counts of every
// character's motions (the count most characters have at each slot, X's 159 packs of 173 slots against ORAS's 326 of
// 239, aligned in order): runs of equal frame counts, so the same motions inferred (not read in the game's code):
//   X 0-3      -> ORAS 0-3      (skeleton; 1 frame; 10; 14, the walk)
//   X 72-89    -> ORAS 86-103   (18 slots: 9 equal counts in a row from X 72, among them X 80 -> ORAS 94, the
//                                36-frame motion of 276 ORAS characters and 143 X ones)
//   X 114-138  -> ORAS 124-148  (20 equal in a row)
//   X 26       -> ORAS 40       (50 frames: X 270 and ORAS 511, the same skeleton byte for byte)
//   X 97       -> ORAS 178      (1 frame, the motion byte for byte the same in both games)
// Checked against the characters both games share (5 pairs, the same skeleton byte for byte, 17 October): their
// motions sit in slots 1, 3, 26/40, 80/94, 97/178, as above; slots 0, 1, 3 and 80/94 also byte for byte.
// X's other slots (4-71 but 26, 90-113 but 97, 139-172: too few characters, no run to align them on and no shared
// character holding them) are not carried; ORAS's
// 157 and 158 (8 frames, half its characters) have no X counterpart.

#include "Bytes.h"

namespace remake
{

// whether data looks like a motion pack of `slots` slots (offsets in order, within the data)
bool IsGfMotionPack(const Bytes& data, uint32_t slots);

// an X/Y pack (173 slots) as an Omega Ruby one (239), its slots moved as above; throws FormatError for another pack
Bytes XyMotionPackToOras(const Bytes& pack);

}

#endif // REMAKE_GF_MOTION_PACK_H
