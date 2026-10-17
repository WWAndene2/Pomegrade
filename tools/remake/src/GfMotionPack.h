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
//   X 140      -> ORAS 156      (40 frames: the slot sets of whole characters, below)
// Checked against the characters both games share (5 pairs, the same skeleton byte for byte, 17 October): their
// motions sit in slots 1, 3, 26/40, 80/94, 97/178, as above; slots 0, 1, 3 and 80/94 also byte for byte.
// The slots whole characters fill (17 October), the commonest kinds the same in both games once moved: X {1, 3, 80}
// (90 characters) / ORAS {1, 3, 94} (70); X {1, 3, 72, 80} (17) / ORAS {1, 3, 86, 94} (14); X {1, 3, 80, 140} (3) /
// ORAS {1, 3, 94, 156} (3). ORAS's 157 and 158 (8 frames) and 170 (1 frame) are motions X's characters do not have:
// ORAS's 70 characters of {1, 3, 94} have none of them either, so a character without them is one ORAS has too.
// X's other slots (4-71 but 26, 90-113 but 97, 139-172 but 140: a few special characters each, no run to align them
// on and no shared character holding them) are not carried. The slot numbers the game asks for were not read in its
// code: the movement steps play "animations" 0x9D-0xA0 (ghidra/function_names.tsv), not slot numbers.

#include "Bytes.h"

namespace remake
{

// whether data looks like a motion pack of `slots` slots (offsets in order, within the data)
bool IsGfMotionPack(const Bytes& data, uint32_t slots);

// an X/Y pack (173 slots) as an Omega Ruby one (239), its slots moved as above; throws FormatError for another pack
Bytes XyMotionPackToOras(const Bytes& pack);

}

#endif // REMAKE_GF_MOTION_PACK_H
