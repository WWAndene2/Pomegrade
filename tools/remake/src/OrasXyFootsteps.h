#ifndef REMAKE_ORASXYFOOTSTEPS_H
#define REMAKE_ORASXYFOOTSTEPS_H

// X/Y's footsteps in X/Y's zones (29 October, the owner's rule: X/Y's sounds where they differ, ported). Both games play a
// step's sound from a table by the ground under the foot, the kind of move and the foot (read 29 October):
//  - Omega Ruby: DllField 0x102E5DC8, the ground FUN_00402FCC (tile bits 12-16) below 13, the kind 0-8 (its caller
//    FUN_102E93EC), table 0x10334B10: 13 grounds x 9 kinds x 2 feet of sound ids (0x48 bytes a ground);
//  - X: DllField 0x1026DAF8, the ground FUN_003E0A34 (tile bits 11-15) below 17, the kind 0-22 by the player's motion
//    (table 0x1026DCDC), table 0x102E7804: 17 grounds x 23 kinds x 2 feet (0xB8 bytes a ground).
// X/Y's zones keep X/Y's ground numbers (OrasXyPiece.h XyTileValue moves the field to Omega Ruby's bits), so Omega Ruby's
// code reads X's grounds there, its own kinds. Each Omega Ruby kind is X's of the same sounds on the grounds both games
// number alike (2-12; X's own id there wherever Omega Ruby's is, both feet) and of the same move class (Omega Ruby's
// 0x103333AC, X's 0x102A5B78): 0 walk = X's 0, 2 run = 1, 3, 7, 8 = 2, 4, 6 = 3, 5 = 21. Kind 1 (Omega Ruby's alone, 6
// grounds, of class 0) keeps Omega Ruby's sounds, X having no such kind. The ids are X's made Omega Ruby's (OrasXySound.h:
// a sound of the same data by Omega Ruby's id, the others ported); X's id 0 is silence. X's ground 1 has no step sound
// (Omega Ruby's has one): X's kept.
// The table (17 x 9 x 2, Omega Ruby's layout grown to X's grounds) goes into DllField's room; two hooks pick it in X/Y's
// zones (the zone read as OrasXyPlayerScale.h reads it): the ground bound `cmp r6, #0xD` (0x102E5E90) made 17 there, and
// after the table's load (0x102E5EE4) the table replaced. Not checked: the kinds' meaning in game (inferred from the
// sounds and classes); X's kinds Omega Ruby's player never takes (the bicycle's, 4-10) are not reached.

#include "Bytes.h"
#include "CodePatch.h"
#include "OrasXySound.h"

#include <vector>

namespace remake
{

// the file oras-xy-world writes for oras-engine --xy-footsteps: u16 the first and last of X/Y's zones, then the table
// (17 x 9 x 2 u32 sound ids, Omega Ruby's); from X's and Omega Ruby's DllField.cro
Bytes XyFootstepsFile(const Bytes& xDllField, const Bytes& orasDllField, const XySounds& sounds, int firstZone, int lastZone);

// DllField's words (linked addresses): the two hooks and the cave at `cave` (DllField's room), from that file
std::vector<CodeWord> XyFootstepsFieldPatchAt(uint32_t cave, const Bytes& file);

}

#endif // REMAKE_ORASXYFOOTSTEPS_H
