#ifndef REMAKE_ORASXYDUSTBOX_H
#define REMAKE_ORASXYDUSTBOX_H

// X/Y's dust boxes (the trash cans a wild Pokemon or an item hides in) given to Omega Ruby, which has none (ORAS_ENGINE.md
// 0.0, step 1 B1; read 23 October, static). Ported, not rewritten (CLAUDE.md 8, the owner's rule of 22 October):
//  - the table: 10 entries of 0x1C bytes (byte +0 the state: 2 not drawn, 1 an item, 0 a Pokemon; +4 species, +8 form,
//    +0xC level; +0x14 the item), then the flags +0x118-+0x11B and 8 drawn spots from +0x11C, X's game data + 0x18C (0x13C
//    bytes, set by X's 0x112A48 at boot). Omega Ruby's game data has no such member: the table lies in the room past the
//    .bss (OrasEngine.cpp), written with X's initial values, the natives finding it through a word of the .code's free
//    bytes (TablePointer), wherever the room places it;
//  - X's filler, its helper and its reset (X .code 0x3B4C14-0x3B5208, one block) copied into the room, their calls
//    pointed at Omega Ruby's same functions (prototype/xy_import_match.py, each read: the random 0x45A908 is
//    Util_RandomBelow 0x48AD84, the date constructor 0x13A158 is 0x139404), their tables (the spots X 0x5432A4, the
//    Pokemon 0x5449EC, the items 0x544B6C) copied after the table with X's flags moved as the scripts' are (XyFirstFlag +
//    f: 0xC50-0xC59 become 0x1950-0x1959, oras-engine --story large), and X's offsets in the game data's first word (the
//    save) made Omega Ruby's: the event work + 0x13148 -> + 0x13140, the date + 0xEA8 -> + 0xEB0;
//  - the hooks where X calls them: the zone change (X's 0x3B843C calls the reset and the filler after 0x3CF184 = Omega
//    Ruby's Zone_ChangeLoadStep 0x3D5698 after 0x3F0FBC: its next word 0x3D5858 a call of a stub doing both), the new day
//    (X's 0x3CEB48 sets +0x11A, +0x11B, and +0x118 on day 4 or +0x119 on day 2 of the date's weekday 0x479B84 = 0x4AA6E0;
//    Omega Ruby's 0x3F0AD0 has no such lines: its `mov r0, r0` at 0x3F0C34, inside the new-day block, a call of a stub
//    doing them), a placed prop (X's 0x1027D3BC, after each prop of id 0xC50-0xCE5, plays the prop's searched animation
//    when its flag is set: X's 0x3B4BC0 through two DllField functions, 0x102D030C and 0x102141AC = Omega Ruby's
//    0x1031BEAC and 0x10257E34; in Omega Ruby's Map_PlacePropsFromContainer 0x102607A4, `add r5, r5, #1` at 0x102608AC, its
//    loop's end, a call of a routine in DllField's cave doing it, the prop list in r8, the game data through X's map
//    + 0x50, Omega Ruby's + 0x60);
//  - the natives (OrasXyNatives.h): CallDustBoxEncount (X 0x1028CFD4: the entry's Pokemon fought as X's scripted wild
//    battle 0x1029A650 sets it, X's setter 0x1026DE30 written out with Omega Ruby's encounter context: X's +0x7C-+0xA4
//    are Omega Ruby's +0x78-+0xA0, the battle type X's +0x8E Omega Ruby's +0x8A; X's 0x15 is the mod's X/Y type 73 + 0x15),
//    CheckTypeOfDustBox (X 0x1028D7EC), GetItemIdOfDustBox (X 0x1028D8F0).
// Not ported yet: the native's diary line (X's 0x448E08 = Omega Ruby's 0x478964, event 0x204, the zone at X's game data +
// 0x124: Omega Ruby keeps events 0x10B-0x242 in its second list, game data + 0x1C49C, 50 records, so 0x204 is kept; the
// text its diary shows for 0x204 is not read), X's shared script calling the natives (X's a/0/3/1 member 39, its ids
// 10480-10529 Omega Ruby's own). Not run.
// Azahar only: the room's code runs because Azahar fetches code from any mapped page (DynarmicUserCallbacks::
// MemoryReadCode, azahar/src/core/arm/dynarmic/arm_dynarmic.cpp: read, not run), and DllField is patched.

#include "CodePatch.h"
#include "OrasXyNatives.h"

#include <map>
#include <vector>

namespace remake
{

constexpr uint32_t XyDustBoxTablePointer = 0x579DC4; // the .code's free word holding the table's address
constexpr uint32_t XyDustBoxBattleType = 0x15;      // X's battle type of a dust box's Pokemon
constexpr uint32_t XyDustBoxBattleTypeNumber = 73 + XyDustBoxBattleType; // the mod's (--xy-battle-types: X/Y's after the game's 73)

// an ARM block copied to another address: every `b` / `bl` leaving it pointed at calls[its target] (throws if a target is
// neither inside nor mapped), every word equal to a key of literals (a literal pool's) given its value, every word at an
// address of `words` checked against the first value and given the second
struct ArmBlockPort
{
    uint32_t From = 0, To = 0;                                     // the block, as the source code maps it
    std::map<uint32_t, uint32_t> Calls;                           // a source function -> the target's
    std::map<uint32_t, uint32_t> Literals;                        // a source literal word -> its value
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> Words;      // a source address -> (its word, the new word)
};
std::vector<uint32_t> PortArmBlock(const Bytes& code, uint32_t codeBase, const ArmBlockPort& port, uint32_t at);

// the room's part (OrasEngine.cpp places it): the table, X's tables, the ported block and the two stubs, as bytes at
// `at`; Code: the .code words calling them and the table pointer
struct XyDustBoxRoom
{
    Bytes Room;
    uint32_t Table = 0; // its address (at)
    std::vector<CodeWord> Code;
};
XyDustBoxRoom XyDustBoxRoomAt(const Bytes& xCode, const Bytes& orasCode, uint32_t at);

// DllField's words (linked addresses): the placed prop's routine at cave and the call of it
std::vector<CodeWord> XyDustBoxFieldPatchAt(uint32_t cave);

// the three natives (OrasXyNatives.h)
std::vector<XyNative> XyDustBoxNatives();

}

#endif // REMAKE_ORASXYDUSTBOX_H
