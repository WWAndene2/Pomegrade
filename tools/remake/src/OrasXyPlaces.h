#ifndef REMAKE_ORASXYPLACES_H
#define REMAKE_ORASXYPLACES_H

// Kalos's places given to Omega Ruby's places table, the first step of flying between the regions (owner, 28 October:
// one fly screen, L/R between Hoenn and Kalos; ORAS_ENGINE.md 0.00). Read 28 October (static):
//  - Omega Ruby's table (.code 0x5862E0, 78 entries of 10 bytes): +0 the place's visited flag, +2 its attributes (bit 1 a
//    fly point: Zone_SetVisitedFlags 0x3E7F38 sets the flag on entering its town), +4 its Pokemon Center's zone and +6
//    tile (low byte x, high z: FUN_003D4260, the last healing place's warp, Teleport and a blackout), +8 its town's zone.
//    Six functions read it through one literal each and bound it by 0x4E (two loops go two entries at a time: the count
//    is even).
//  - X's (.code 0x544E4C, 19 entries of 20 bytes, its twin functions 0x3B6A38-0x3B6BE4): its first 10 bytes the same
//    fields, Kalos's places; the other 10 bytes are not read here.
// Each X entry whose zones the build holds is made Omega Ruby's: its zones the mod's, its flag moved as X/Y's flags are
// (XyFirstFlag + f, OrasXyScript.h), the rest kept. oras-engine --xy-places copies Omega Ruby's table into the room with
// them after its 78 (Kalos's place n is the place 79 + n), pointing the six literals and bounds at the copy.

#include "Bytes.h"
#include "CodePatch.h"
#include "OrasXyScript.h"

#include <set>
#include <string>
#include <vector>

namespace remake
{

constexpr uint32_t OrasPlacesTable = 0x5862E0, OrasPlaceCount = 78, XyPlacesTable = 0x544E4C, XyPlaceCount = 19;
constexpr uint32_t PlaceBytes = 10;

// X's places' visited flags (X's numbers), for XyNumbersFor's extra flags
std::set<int> XyPlaceFlags(const Bytes& xCode);

// X's places whose two zones `numbers` holds, as Omega Ruby's 10-byte entries; the others are logged and left out
Bytes XyPlacesToOras(const Bytes& xCode, const XyNumbers& numbers, std::vector<std::string>& log);

// the .code words of oras-engine --xy-places: the six literals pointing at `table` (the copy in the room) and the eight
// bounds made `count` (even, an ARM immediate)
std::vector<CodeWord> XyPlacesPatch(uint32_t table, uint32_t count);

}

#endif // REMAKE_ORASXYPLACES_H
