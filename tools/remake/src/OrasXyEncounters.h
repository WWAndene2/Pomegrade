#ifndef REMAKE_ORASXYENCOUNTERS_H
#define REMAKE_ORASXYENCOUNTERS_H

// An X/Y zone's wild encounters (its file 3, 392 bytes in the 53 zones that have any, none in the other 307) made Omega Ruby's
// (file 3 and its member 537 entry: ORAS_ENGINE.md 6, Wild encounter selection), for oras-engine --wild-tables large. Both
// are a header of one byte per table, then 4-byte slots (species bits 0-10, form 11-15; minimum level; maximum level;
// 00 00 01 01 an empty slot).
//   X/Y     read in its DllField (20 October: the table pick at 0x74D5C, the step check at 0x833D4): header 16 bytes, byte
//           t of table t; tables 0-4 the land tables (12 slots each, +0x10), 5 surf (5, +0x100), 6 rock smash (5, +0x114),
//           7-9 the rods (3 each, +0x128), 10 hordes (3 x 5, +0x14C). On a step on an encounter tile (bit 2): the water flag
//           table 5; tile kind 0x20, 0x24, 0x25 table 0; 0x21, 0x22, 0x23 tables 1-3; any other kind table 4. The table's
//           header byte, when not 0, is 1 + the entry of the encounter effect table (0xEF7D0, 6 bytes an entry) it uses
//   Omega   header 14 bytes, byte k of kind k; kinds 0 and 1 (12 slots, +0xE, +0x3E), 2 (3), 3 surf (5), 4 rock smash (5),
//           5-7 the rods (3 each), 8 hordes (3 x 5, +0xC6), to +0x102; its effect table (DllField 0xF308C) has the same
//           layout and entries in the same order (grass 0, cave 5, water 8, its last two bytes differing)
// Written: 0x194 bytes, the game's 0x104 then X/Y's tables 1-3 at +0x102, +0x132, +0x162 (kinds 9-11) and their header
// bytes at 9-11; kind 0 X/Y's table 0; kind 1 and kind 12 (byte 12) X/Y's table 4; the other tables in the same order.
// X/Y's header bytes are kept as they are: X/Y's 1 (grass), 6 (caves), 12 (0x25) and 5 (0x27) are the entries Omega Ruby
// gives those tile kinds itself (0, 5, 11 and 4).

#include "Bytes.h"

#include <string>

namespace remake
{

// X/Y's wild battle types (read 22 October, ORAS_ENGINE.md 0.1): a step encounter's type is its effect entry's byte +4 (+5 a
// horde), and X/Y's 12 entries differ from Omega Ruby's (their types, rates, entry 4). With xyEffects, the land and surf
// header bytes (0, 1, 3, 9-12) name X/Y's entries appended after Omega Ruby's 12 (+12; oras-engine --xy-wild-effects reads
// them, Map_GetAttributeEffectEntry taking a byte of 13 and up first) and byte 13 is 1, so fishing and Rock Smash give X/Y's
// types in the zone. The rods' bytes (5-7) are rates, kept; byte 8, the horde chance, kept
constexpr uint8_t XyEffectsAfter = 12;

// X/Y's file 3 made Omega Ruby's (empty for an empty one)
Bytes XyEncountersToOras(const Bytes& xFile, int xZone, bool xyEffects);

}

#endif // REMAKE_ORASXYENCOUNTERS_H
