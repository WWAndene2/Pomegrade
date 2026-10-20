#include "OrasXyZoneWalk.h"

namespace remake
{

namespace
{
constexpr uint32_t ScriptContext = 0x3FE5C8; // the script context
constexpr uint32_t WorkAddress = 0x11FA90;   // an event work's address (works, number), 0 past 0x4177

// IsZoneWalk(): work 0x4010's low byte, signed, as X reads its byte (0 if there is no such work)
std::vector<uint32_t> IsZoneWalk()
{
    XyNativeAsm a;
    a.Op(0xE92D4010);      // push {r4, lr}
    a.Call(ScriptContext);
    a.Op(0xE590000C);      // ldr r0, [r0, #0xC]
    a.Op(0xE590001C);      // ldr r0, [r0, #0x1C]
    a.Op(0xE5900000);      // ldr r0, [r0]               the save
    a.Op(0xE2800801);      // add r0, r0, #0x10000
    a.Op(0xE2800DC5);      // add r0, r0, #0x3140        the event works
    a.Op(0xE3A01901);      // mov r1, #0x4000
    a.Op(0xE3811010);      // orr r1, r1, #0x10          work 0x4010
    a.Call(WorkAddress);
    a.Op(0xE3500000);      // cmp r0, #0
    a.Op(0x11D000D0);      // ldrsbne r0, [r0]
    a.Op(0xE8BD8010);      // pop {r4, pc}
    return a.Finish();
}
}

std::vector<XyNative> XyZoneWalkNatives()
{
    return {{"IsZoneWalk", IsZoneWalk()}};
}

}
