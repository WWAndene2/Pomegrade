#ifndef REMAKE_ORASXYMOVECODES_H
#define REMAKE_ORASXYMOVECODES_H

// X/Y's move codes in Omega Ruby (ORAS_ENGINE.md 0.0, "Move codes, X against Omega Ruby", 25 October). A zone character's
// move code (its record's word 2) picks the class that moves it, from a table of one object per code that DllField's
// builder fills (X 0x1024C138, Omega Ruby 0x10286208, the same code). By the classes' RTTI names the two games agree on
// codes 0x00-0x37; X's 0x38-0x3E (MoveCodeTsutikemuri* the dust clouds, MoveCodeYagi* Skiddo, MoveCodeTravelingPoke) are
// X-only, X's 0x3F/0x40 (MoveCodeSpinRight/Left) are Omega Ruby's 0x38/0x39, and Omega Ruby's 0x3A-0x3E are its own
// (Mimicry*, Seiza, SecretBaseTrainer, Fishing2). So:
//  - the zone port gives X/Y's characters Omega Ruby's numbers (XyMoveCodeToOras), X's seven X-only codes 0x3F-0x45;
//  - Omega Ruby's table (0x10385720, 63 codes, read by the builder, its teardown 0x10286F18 and its getter 0x10286FBC,
//    both bounded by 0x3F) moves into the room with 70 slots: the three loads go through DllField caves reading the
//    table's address from a free .code word, both bounds become 0x46, and the builder's end allocates the 7 new codes'
//    objects as it does the others. Each new code's class is the room's (its vtable in the list after the table), or,
//    until X's class is ported, Omega Ruby's MoveCodeNone (slot 0's vtable, read at run time: a cave word holding a
//    DllField address would not be relocated).
// Azahar only (the room runs: OrasXyDustBox.h).

#include "CodePatch.h"

#include <vector>

namespace remake
{

constexpr int OrasMoveCodes = 0x3F;                 // Omega Ruby's codes 0x00-0x3E
constexpr int XyMoveCodesAdded = 7;                 // X's 0x38-0x3E, as 0x3F-0x45
constexpr int XyMoveCodeSlots = OrasMoveCodes + XyMoveCodesAdded;
constexpr uint32_t XyMoveCodesPointer = 0x579DCC;   // the .code's free word holding the room table's address

// X/Y's move code -> Omega Ruby's (throws past X/Y's 0x40)
int XyMoveCodeToOras(int xCode);

// the room's part at `at`: the table (zero; the builder fills it), then the new codes' vtables (zero: Omega Ruby's
// MoveCodeNone, until X's classes are ported); Code: the .code word naming the table
struct XyMoveCodesRoom
{
    Bytes Room;
    uint32_t Table = 0;
    std::vector<CodeWord> Code;
};
XyMoveCodesRoom XyMoveCodesRoomAt(uint32_t at);

// DllField's words (linked addresses): the builder's, teardown's and getter's table and bounds, and their cave
std::vector<CodeWord> XyMoveCodesFieldPatchAt(uint32_t cave);

}

#endif // REMAKE_ORASXYMOVECODES_H
