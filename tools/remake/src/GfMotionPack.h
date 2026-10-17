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
//   X 4-6      -> ORAS 5-7      (16 frames: the rivals' X 4 against ORAS's player models' 5 and 6; 3 equal in a row)
//   X 7-9, 10  -> ORAS 12-14, 16 (an object's 1, 15, 16, 31 frames against ORAS object 536's 1, 15, 16, 30)
//   X 11-13, 15, 16 -> ORAS 25-27, 29, 30 (characters tr0108 and tr0109, so named in both games: 40, 40, 40, 30, 18)
//   X 26       -> ORAS 40       (50 frames: X 270 and ORAS 511, the same skeleton byte for byte)
//   X 72-89    -> ORAS 86-103   (9 equal counts in a row from X 72, among them X 80 -> ORAS 94, the 36-frame motion of
//                                276 ORAS characters and 143 X ones)
//   X 97       -> ORAS 178      (1 frame, the motion byte for byte the same in both games)
//   X 100-138  -> ORAS 110-148  (20 equal in a row from 114; the rivals' 5 scene motions X 104-108 against ORAS's
//                                player models' 114-118; tr0103, so named in both games, X 101 / ORAS 111)
//   X 140      -> ORAS 156      (40 frames: the slot sets of whole characters, below)
// Checked against the characters both games share (5 pairs, the same skeleton byte for byte): their motions sit in
// slots 1, 3, 26/40, 80/94, 97/178, as above; slots 0, 1, 3 and 80/94 also byte for byte. The slots whole characters
// fill, the commonest kinds the same in both games once moved: X {1, 3, 80} (90 characters) / ORAS {1, 3, 94} (70);
// X {1, 3, 72, 80} (17) / ORAS {1, 3, 86, 94} (14); X {1, 3, 80, 140} (3) / ORAS {1, 3, 94, 156} (3). ORAS's 157 and
// 158 (8 frames) and 170 (1 frame) are motions X's characters do not have: ORAS's 70 characters of {1, 3, 94} have none
// of them either.
// Every X character keeps all its motions but X's two players (members 1 and 2, tr0001 and tr0002): 77 slots of theirs
// (14, 17-25, 27-71, 90-96, 98, 99, 139, 141-151, 172), the player's own motions, have no ORAS slot found; an NPC made of
// them stands, walks and turns. The slot numbers the game asks for were not read in its code: the movement steps play
// "animations" 0x9D-0xA0 (ghidra/function_names.tsv), not slot numbers.

#include "Bytes.h"

namespace remake
{

// whether data looks like a motion pack of `slots` slots (offsets in order, within the data)
bool IsGfMotionPack(const Bytes& data, uint32_t slots);

// an X/Y pack (173 slots) as an Omega Ruby one (239), its slots moved as above; throws FormatError for another pack
Bytes XyMotionPackToOras(const Bytes& pack);

}

#endif // REMAKE_GF_MOTION_PACK_H
