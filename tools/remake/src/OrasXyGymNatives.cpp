#include "OrasXyGymNatives.h"

namespace remake
{

namespace
{
constexpr uint32_t ScriptContext = 0x3FE5C8; // the script context

// a call of the zone's gym module's function at offset with its object: the script's first arguments (count of them) in
// r1, r2, argument asBool (1 or 2; 0 none) made 0 or 1 (as X's natives do), r1 = 1 when one is set and count is 0; the
// function's result returned when keep, else 0
std::vector<uint32_t> ModuleCall(uint32_t offset, int count, int asBool, bool one, bool keep)
{
    XyNativeAsm a;
    a.Op(0xE92D4070);                       // push {r4, r5, r6, lr}
    if (count >= 1) a.Op(0xE5914004);       // ldr r4, [r1, #4]
    if (count >= 2) a.Op(0xE5915008);       // ldr r5, [r1, #8]
    a.Call(ScriptContext);
    a.Op(0xE590000C);                       // ldr r0, [r0, #0xc]
    a.Op(0xE5900048);                       // ldr r0, [r0, #0x48]      the field
    a.Op(0xE59061A4);                       // ldr r6, [r0, #0x1a4]     the module's handle
    a.Op(0xE59001A0);                       // ldr r0, [r0, #0x1a0]     its object
    for (int k = 1; k <= count; k++)
    {
        const uint32_t from = k == 1 ? 4 : 5, to = (uint32_t)k; // r4 -> r1, r5 -> r2
        if (k == asBool)
        {
            a.Op(0xE3500000 | from << 16);          // cmp rFrom, #0
            a.Op(0x13A00001 | to << 12);            // movne rTo, #1
            a.Op(0x03A00000 | to << 12);            // moveq rTo, #0
        }
        else a.Op(0xE1A00000 | to << 12 | from);    // mov rTo, rFrom
    }
    if (count == 0 && one) a.Op(0xE3A01001); // mov r1, #1
    a.Literal(12, offset);                  // ldr ip, =offset
    a.Op(0xE086C00C);                       // add ip, r6, ip
    a.Op(0xE12FFF3C);                       // blx ip
    if (!keep) a.Op(0xE3A00000);            // mov r0, #0
    a.Op(0xE8BD8070);                       // pop {r4, r5, r6, pc}
    return a.Finish();
}
}

std::vector<XyNative> XyGymNatives()
{
    // Clemont's: X's natives (0x10294950 ...) and the module functions they call (X's DllField stubs 0x101FD3B8 ...)
    return {
        {"_GymElecPlayDoorAnm", ModuleCall(0x2B4, 2, 2, false, false)},
        {"_GymElecBtmPlayDoorAnm", ModuleCall(0x1494, 1, 1, false, false)},
        {"_GymElecPlayMissAnm", ModuleCall(0x324, 1, 0, false, false)},
        {"_GymElecIsEndDoorAnm", ModuleCall(0x96C, 1, 0, false, true)},
        {"_GymElecIsEndMissAnm", ModuleCall(0x994, 0, 0, false, true)},
        {"_GymElecSetBlackFace", ModuleCall(0x2A0, 0, 0, true, false)},
        {"_GymElecBtmIsEndDoorAnm", ModuleCall(0x1630, 0, 0, false, true)},
    };
}

}
