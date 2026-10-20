#ifndef REMAKE_ORASXYLIGHT_H
#define REMAKE_ORASXYLIGHT_H

// An X/Y area pack's light (its file 4, 9728 bytes in all 170 packs) made Omega Ruby's (2944 bytes in all 228), read in both
// games' code on 20 October: Omega Ruby's terrain light FUN_0013D908 and X's at 0x13E5F0 are the same function, the
// planes 0x30 bytes apart in Omega Ruby's (12 floats: 3 kinds x 4 times of day) and 0x40 in X's (16: 4 kinds x 4), the
// element kind x 4 + time in both; 15 planes a light set (ambient red, green, blue; diffuse; two speculars; the direction
// x, y, z as planes 12-14: Omega Ruby's 0x240/0x270/0x2A0, X's 0x300/0x340/0x380, the characters' readers FUN_0013DC4C
// and X's 0x13EB30 too), a set 0x2D0 bytes in Omega Ruby's file (4 sets, then 0x40 bytes) and 0x3C0 in X's (10, then
// 0x80). So each of Omega Ruby's 4 sets takes X's set of its number, plane by plane, X's kinds 0-2 as Omega Ruby's 0-2
// (inferred: what a kind is, in either game, is not read), X's fourth kind and sets 4-9 left out, the donor's last 0x40
// bytes kept.

#include "Bytes.h"

namespace remake
{

Bytes XyLightToOras(const Bytes& xLight, const Bytes& orasLight);

}

#endif // REMAKE_ORASXYLIGHT_H
