#ifndef REMAKE_ORASXYTRAINERS_H
#define REMAKE_ORASXYTRAINERS_H

// X/Y's trainers in Omega Ruby (oras-xy-world, read 20 October): X/Y's trainer n becomes Omega Ruby's trainer 950 + n, after
// the game's 950 (ids 950-1734 for X/Y's 785, its trainer 0 a copy of Omega Ruby's empty trainer 0), with its team and
// name; oras-engine --xy-trainers lets the game reach them.
//
// Records: X/Y's a/0/3/8 member n (20 bytes) as Omega Ruby's a/0/3/6 (24 bytes), field by field: X/Y's +0 format, +1
// class (u8), +2 battle type, +3 count, +4 four items (u16), +0xC AI (u32), +0x10 healer, +0x11 money, +0x12 prize (u16)
// at Omega Ruby's +0, +2 (u16), +6, +7, +8, +0x10, +0x14, +0x15, +0x16 (the layout read on both games' records: each byte's
// values over the 785 and 950 records; Omega Ruby's +1, +4 and +5 are 0 in all of its records). Omega Ruby's own
// records never set the healer and prize bytes: whether its code reads them there is not checked. Teams: X/Y's a/0/4/0
// member n as it is (the same entries as Omega Ruby's a/0/3/8: 8, 16, 10 or 18 bytes by the format, OrasSandbox.cpp).
// Names: X/Y's a/0/7/2-9 member 21 line n (614 lines: X/Y's trainers 614-784 have none, an empty line here) appended to
// Omega Ruby's a/0/7/1-8 member 22, language for language (X/Y's archives are Omega Ruby's + 1, checked on the class names).
//
// Classes: no class is added (the game reads its 280 class records at boot into a fixed table, GameData_LoadBootTables
// 0x10EAB4, and Trainer_ReadClass reads class 0 past 279). Omega Ruby's classes 0-108 are X/Y's 0-108 and its 109-126
// X/Y's 160-177 (read on the names), none of 0-126 used by Omega Ruby's own trainers; a class record's u16 +4 (the
// trainer's battle picture, inferred) is X/Y's + 113 in all of them. An X/Y class takes the Omega Ruby class of its name and
// picture (124 of 178); the other 54 the first class of its picture, logged: X/Y's 51 Battle Chateau titles (109-159: Baron,
// Duchess... none in Omega Ruby), the two Ace Surfers (Ace Trainer) and one Team Flare (Team Flare Boss's picture).
//
// Beaten trainers: both games keep trainer n's "beaten" flag at 0x6CC + n (Event_IsWarpEnabledByFlag 0x4DE594, DllField
// 0x21328, 0xDFC34, 0xDFDEC; X/Y's DllField the same at 0x2D980, 0xDFE68, 0xE0020), room for 980 in Omega Ruby
// (0x6CC-0xA9F; 0xAA0 is another flag). X/Y's trainer n's flag is 0x2900 + n here (in the story's large range, oras-engine
// --story large), and an X/Y script's flag 0x6CC + n (n < 785) is that one (XyNumbersFor).
//
// Checked headless (run hBat2, 20 October: X/Y zone 56 alone, --story off, its character 8, script 3040, talked to):
// "Donnie le Gamin veut se battre !", then "Donnie le Gamin envoie un Pérégrain !": X/Y's trainer 40 (a Youngster with
// a Spewpa at 10) as Omega Ruby's 990, its name, class and team; the battle runs, a loss sends the player home (run hBat1).
// Not seen yet: the trainer's line before the battle (its balloon showed empty: X/Y's battle lines are not brought in), a
// win and its flag 0x2928, a trainer walking up on sight.

#include "OrasWorkspace.h"

#include <string>
#include <vector>

namespace remake
{

constexpr int XyTrainerFirst = 950;       // Omega Ruby's id of X/Y's trainer 0
constexpr int XyTrainerCount = 785;       // X/Y's trainers (a/0/3/8)
constexpr int XyTrainerFlagBase = 0x2900; // X/Y's trainer n's "beaten" flag: 0x2900 + n
constexpr int GameTrainerFlags = 0x6CC;   // both games' trainer n's flag: 0x6CC + n

// Omega Ruby's trainer id of X/Y's (0 stays 0: no trainer)
inline int XyTrainerToOras(int xTrainer) { return xTrainer > 0 ? XyTrainerFirst + xTrainer : xTrainer; }

// appends X/Y's trainers (ws.XyRom) to the workspace's a/0/3/6, a/0/3/8 and a/0/7/1-8 member 22; returns the log
std::vector<std::string> AddXyTrainers(OrasWorkspace& ws);

}

#endif // REMAKE_ORASXYTRAINERS_H
