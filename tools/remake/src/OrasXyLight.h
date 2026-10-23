#ifndef REMAKE_ORASXYLIGHT_H
#define REMAKE_ORASXYLIGHT_H

// An X/Y area pack's light (its file 4, 9728 bytes in all 170 packs) made Omega Ruby's (2944 bytes in all 228), read in both
// games' code on 20 October: Omega Ruby's terrain light FUN_0013D908 and X's at 0x13E5F0 are the same function, the
// planes 0x30 bytes apart in Omega Ruby's (12 floats: 3 kinds x 4 times of day) and 0x40 in X's (16: 4 kinds x 4), the
// element kind x 4 + time in both; 15 planes a light set (ambient red, green, blue; diffuse; two speculars; the direction
// x, y, z as planes 12-14: Omega Ruby's 0x240/0x270/0x2A0, X's 0x300/0x340/0x380, the characters' readers FUN_0013DC4C
// and X's 0x13EB30 too), a set 0x2D0 bytes in Omega Ruby's file (4 sets, then 0x40 bytes) and 0x3C0 in X's (10, then
// 0x80). Omega Ruby's 0x12DDF4 lights the terrain with set 0 and its two character slots with sets 2 and 1 (the
// literal pair 0x168, 0xB4 floats from the file's start, 0x596188), each at kind x 4 + time blended between two times
// (FUN_0013D908, FUN_0013DC4C; X's FUN_0013E5F0 and FUN_0013E934 read set 0 the same way). The kind is the weather: Omega
// Ruby's 7 -> 2, 8 -> 1, else 0; X's 0xD -> 1, 0xE-0x10 (rain) -> 2, 0x11-0x14 (snow, hail) -> 3, else 0 (both read 23
// October), so kind 0, fair weather, is the same in both; X's other kinds are copied to Omega Ruby's of their number,
// not matched to its weathers (X/Y's zones have no weather yet). The sets, compared plane by plane (23 October, X's
// packs 5 (zone 264, Route 5) and 74 (zone 2) against Omega Ruby's 9 (zone 24) and 112 (zone 223)): X's set 1 is empty
// (all 0, direction -Y), X's set 2 has the form of Omega Ruby's set 1 (an ambient curve by time, diffuse and speculars
// 1 outdoors) and X's set 3 that of Omega Ruby's set 2 (an ambient, speculars 1 and the same four directions,
// (0,-0.7,-1) ... (0.7,-0.3,-1), value for value outdoors); Omega Ruby's set 3 is no light (its "directions" 800 and
// 4000 outdoors, -10 and 1500 indoors: distances, inferred). So Omega Ruby's sets 0, 1, 2 take X's sets 0, 2, 3, plane
// by plane, kinds 0-2 (X's fourth kind left out), and its set 3 and last 0x40 bytes stay the donor's. Two earlier
// versions were wrong, seen on the phone (23 October): sets 1-3 from X's 1-3 lit X/Y's people blue indoors by day, and
// sets 1-2 from X's set 0 at time 0 made them dark outdoors (Route 5). Not read: what X's sets 4-9 light; X's
// characters' own per-object values (FUN_0013E934's param_1 + kind x 0x30 + 0x60 and + 0x120, not in the file).

#include "Bytes.h"

namespace remake
{

Bytes XyLightToOras(const Bytes& xLight, const Bytes& orasLight);

}

#endif // REMAKE_ORASXYLIGHT_H
