#ifndef REMAKE_ORASXYDUSTCLOUD_H
#define REMAKE_ORASXYDUSTCLOUD_H

// X/Y's dust clouds (the swirls of Route 13 a wild Pokemon jumps out of when the player walks into one) given to Omega
// Ruby, which has none (ORAS_ENGINE.md 0.0, step 1 B2; read 23-24 October, static). Ported, not rewritten (CLAUDE.md 8):
//  - a dust cloud is a character of the zone (no model, its move code 0x38, 0x39 or 0x3A, the mod's 0x3F-0x41: OrasXyMoveCodes.h; X's zone 272 has 11): X's Route
//    13 module DllUSR13 lists them, shows 9 at random, and moves one away once it is used. The module is ported with the
//    gyms' tooling (XyRoute13, OrasXyGymModules.h: its imports pointed at Omega Ruby's same functions, its field offsets
//    made Omega Ruby's) and loaded for X's area kind 0x18, the mod's 18 (a kind Omega Ruby loads nothing for);
//  - X's step check (X DllField 0x102265DC = Omega Ruby's 0x10263014, 97%) starts field::EventTsuchikemuriEncount when
//    the character walked into has one of those move codes: Omega Ruby's copy lacks the branch, so its `ldr r0, [fp,
//    #0x1c]` at 0x10263304 goes to a DllField cave calling the room's routine (XyDustCloudFieldPatchAt), which creates
//    the event as X's branch does;
//  - the event (6 methods), the cloud's effect (X DllField 0x10250CC4-0x10250E6C, its update 0x10277324, its end
//    0x102709C4) and the getter 0x1027F5AC are copied whole into the room (DllField is mapped where the heap has room, so
//    their vtables and jump tables, absolute addresses, lie in the .code's room): their calls pointed at Omega Ruby's same
//    functions (the effect's resource class is Omega Ruby's .code one, its object 0x40 bytes for X's 0x3C), DllField's
//    through the getter word the loader writes (OrasXyNatives.h), and X's field offsets made Omega Ruby's (the map
//    object's +0x2C, +0xF0, +0xF4, +0x158, +0x160, +0x198: +0x38, +0x100, +0x104, +0x164, +0x168, +0x1A0; the 0x4C-byte
//    object of state 6 Omega Ruby's 0x48-byte one; the records + 0x1B814: + 0x1C8D4);
//  - its battle is Omega Ruby's Rock Smash (0x102D9F04, copied) with X's: the land table 4 (the mod's kind 12,
//    OrasXyEncounters.h), battle type 0x16 (the mod's 95, --xy-battle-types) and flags 3, the event returned; the
//    encounter event made as X's 0x3BC140 makes it, not pushed on the event list (Omega Ruby's Field_StartEncounterEvent
//    copied but its push): X's update pushes it at its state 9, once the effect is over;
//  - its effect's models, X's archive a/1/7/5 (one "TM" of 4 CGFX), get a number past the game's (the module's port, its
//    Archive), and its sound X's 0xA012A is translated (OrasXySound.h).
// Run headless (24 October, runs dc1-dcj, X's zone 272 as mod zone 538): the module lists 11 spots and shows 8-9, the
// player walking into one starts the event, the cloud's character goes, X's swirl plays, the field is torn down and the
// battle starts (a wild Dugtrio, Route 13's desert). Seen there, not fixed here:
//  - the cloud's character wears X's model 0 (a woman, standing): X hides it and plays a small swirl from time to time
//    through its move controller of codes 0x38-0x3A (the counter at the controller's + 0xC the module sets), which Omega
//    Ruby's move code 0x3A does not do (not read: where the controllers are made);
//  - the battle's command menu took no input once (run dcj) with X's type 0x16 whole (words 0 and 3: 7 and 9); not
//    reproduced since: run dcx (24 October, the same type) plays the battle's turn. Omega Ruby's battle reads word 3 as
//    X's does (ORAS_ENGINE.md 0.0, "Word 3 read live").
// Not ported now: the weather byte X sets (its +0x70 to 3, X's 0x3E018C / 0x3E0194: X/Y's weather is step 6) and the
// module's lamps (15 props switched by a word of X's [[game system + 0x2C] + 0x14], an object Omega Ruby does not have:
// X's 0x4B3EA4 answered 0, the lamps left as placed). X's script native CallTsuchikemuriEncountEvent (0x102914A0) is not
// given. Azahar only (the room runs: OrasXyDustBox.h).

#include "CodePatch.h"
#include "OrasXyGymModules.h"

#include <functional>
#include <vector>

namespace remake
{

constexpr uint32_t XyDustCloudBattleType = 0x16;                              // X's battle type of a dust cloud's Pokemon
constexpr uint32_t XyDustCloudBattleTypeNumber = 73 + XyDustCloudBattleType;   // the mod's (X/Y's after the game's 73)
constexpr uint32_t XyDustCloudKind = 12;                                       // the mod's encounter kind of X/Y's table 4
constexpr uint32_t XyDustCloudPointer = 0x579DC8;                              // the .code's free word holding the step routine's address
constexpr uint32_t XyDustCloudArchive = 0xAF;                                  // X's archive of the effect's models (a/1/7/5)

// X's Route 13 module, ported (its area kind 0x18)
const XyModulePort& XyRoute13();

// the room's part: the code copied from X's DllField (xField, its file) and Omega Ruby's (orasField; orasCode its .code),
// as bytes at `at`;
// Code: the .code word naming the step routine. archive: the mod's number of X's a/1/7/5; sound: X/Y's sound id -> the mod's
struct XyDustCloudRoom
{
    Bytes Room;
    std::vector<CodeWord> Code;
};
XyDustCloudRoom XyDustCloudRoomAt(const Bytes& xField, const Bytes& orasField, const Bytes& orasCode, uint32_t at, uint32_t archive,
                                  const std::function<uint32_t(uint32_t)>& sound);

// DllField's words (linked addresses): the step check's hook and its cave
std::vector<CodeWord> XyDustCloudFieldPatchAt(uint32_t cave);

}

#endif // REMAKE_ORASXYDUSTCLOUD_H
