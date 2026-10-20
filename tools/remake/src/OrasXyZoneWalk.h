#ifndef REMAKE_ORASXYZONEWALK_H
#define REMAKE_ORASXYZONEWALK_H

// X/Y's IsZoneWalk, as an X/Y native for Omega Ruby (OrasXyNatives.h): what a zone script was started with, read by X/Y's
// init scripts (zone 0's). Read 20 October: X's native (0x10251A30) returns the signed byte at +0x14B of the script's run
// object (the AMX's +0x44, its +0x90), which the function making that object (X 0x3DD64C) stores from its caller's stacked
// argument (`ldr r7, [sp, #0x28]` ... `strb r7, [r5, #0x14B]`). Omega Ruby's same function (0x3FEC3C, the same shape and
// the same argument) stores it instead in event work 0x4010 (`strh r7` through FUN_0011fa90, the works' address, of the
// event works at save + 0x13140). So the native reads work 0x4010 as X reads its byte. That both arguments mean the same
// (whether the zone was entered walking) is inferred from the name and the matching code, not checked in the game.

#include "OrasXyNatives.h"

#include <vector>

namespace remake
{

std::vector<XyNative> XyZoneWalkNatives();

}

#endif // REMAKE_ORASXYZONEWALK_H
