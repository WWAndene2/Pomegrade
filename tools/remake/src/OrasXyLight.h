#ifndef REMAKE_ORASXYLIGHT_H
#define REMAKE_ORASXYLIGHT_H

// An X/Y area pack's light (its file 4, 9728 bytes in all 170 packs) made Omega Ruby's (2944 bytes in all 228), read in both
// games' code on 20 October: Omega Ruby's terrain light FUN_0013D908 and X's at 0x13E5F0 are the same function, the
// planes 0x30 bytes apart in Omega Ruby's (12 floats: 3 kinds x 4 times of day) and 0x40 in X's (16: 4 kinds x 4), the
// element kind x 4 + time in both; 15 planes a light set (ambient red, green, blue; diffuse; two speculars; the direction
// x, y, z as planes 12-14: Omega Ruby's 0x240/0x270/0x2A0, X's 0x300/0x340/0x380, the characters' readers FUN_0013DC4C
// and X's 0x13EB30 too), a set 0x2D0 bytes in Omega Ruby's file (4 sets, then 0x40 bytes) and 0x3C0 in X's (10, then
// 0x80). So Omega Ruby's sets 0 and 3 take X's set of their number, plane by plane, X's kinds 0-2 as Omega Ruby's 0-2
// (inferred: what a kind is, in either game, is not read), X's fourth kind and sets 4-9 left out, the donor's last 0x40
// bytes kept. The characters differ (read 23 October, after the owner saw X/Y's people lit blue by day indoors, zones
// 2 and 318 on the phone): Omega Ruby's 0x12DDF4 lights its two character slots with sets 2 and 1 (its literal pair
// 0x168, 0xB4 floats from the file's start: 0x596188), at kind x 4 + time blended between two times (FUN_0013DC4C),
// while X's 0x12D1E8 lights them with set 0 at kind x 4 alone, time 0 (FUN_0013E934 reads no time), plus each object's
// own values (not in the file). X's sets 1 and 2 are no character light (set 1 zero, set 2 an ambient curve by time,
// blue by day, its diffuse zero, in zone 2's pack 74): copied to Omega Ruby's character sets they lit the people blue.
// So Omega Ruby's sets 1 and 2 take X's set 0 at time 0, the same for every time.

#include "Bytes.h"

namespace remake
{

Bytes XyLightToOras(const Bytes& xLight, const Bytes& orasLight);

}

#endif // REMAKE_ORASXYLIGHT_H
