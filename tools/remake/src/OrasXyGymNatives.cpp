#include "OrasXyGymNatives.h"

#include "OrasXyGymModules.h"

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

// Olympia's GymEsperWarpEvent (X's 0x10294528, named XyGymEsperWarpEvent in X's scripts: OrasXyEvents.cpp): X's event
// object (0x3C bytes, from the field's heap: X's [[field + 0x50] + 0x18], Omega Ruby's + 0x60) built as X does, its vtable
// the one DllXyGymEsper builds (X's 0x102FE5A4, at 0x27A0 in the ported module) and the event registered (X's 0x108CF0 =
// Omega Ruby's 0x108CB0), then its init (X's 0x1021EFEC, copied at 0x2248) with the field. The two offsets are where
// XyPortModule places them for XyGymEsper(): xy-gym-module prints them, and they must be updated with its copies. As X,
// no test of the allocation (X's would crash the same way on a null one)
std::vector<uint32_t> EsperWarpEvent()
{
    XyNativeAsm a;
    a.Op(0xE92D41F0);           // push {r4, r5, r6, r7, r8, lr}
    a.Call(ScriptContext);
    a.Op(0xE590000C);           // ldr r0, [r0, #0xc]
    a.Op(0xE5905048);           // ldr r5, [r0, #0x48]          the field
    a.Op(0xE5950060);           // ldr r0, [r5, #0x60]
    a.Op(0xE5906018);           // ldr r6, [r0, #0x18]          its heap
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x12BB54);           // the heap's allocator
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(0xE3A0003C);           // mov r0, #0x3c
    a.Call(0x123480);           // new
    a.Op(0xE1A04000);           // mov r4, r0
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x12BB54);
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(0xE1A00004);           // mov r0, r4
    a.Call(0x109434);           // the event's base constructor
    a.Op(0xE59571A4);           // ldr r7, [r5, #0x1a4]         the module's handle
    a.Literal(2, 0x27A0);       // ldr r2, =the vtable's offset
    a.Op(0xE0872002);           // add r2, r7, r2
    a.Op(0xE3A01000);           // mov r1, #0
    a.Op(0xE5801018);           // str r1, [r0, #0x18]
    a.Op(0xE5802000);           // str r2, [r0]
    a.Op(0xE580101C);           // str r1, [r0, #0x1c]
    a.Op(0xE5801020);           // str r1, [r0, #0x20]
    a.Op(0xE5801024);           // str r1, [r0, #0x24]
    a.Op(0xE1A04000);           // mov r4, r0
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x108CB0);           // the event registered
    a.Op(0xE1A00004);           // mov r0, r4
    a.Op(0xE1A01005);           // mov r1, r5
    a.Literal(12, 0x2248);      // ldr ip, =the init's offset
    a.Op(0xE087C00C);           // add ip, r7, ip
    a.Op(0xE12FFF3C);           // blx ip
    a.Op(0xE3A00000);           // mov r0, #0
    a.Op(0xE8BD81F0);           // pop {r4, r5, r6, r7, r8, pc}
    return a.Finish();
}

// Clemont's event natives (2 November), as X's 0x10293F08 (_GymElecLiftUp), 0x10294660 (TopLiftUp), 0x10294418 (LiftDown),
// 0x102949B4 (TopLiftDown), 0x102945B8 (AnswerAnm) and 0x10294394 (BlackAnm): an event object from the field's heap, built
// by the event's base constructor, its vtable the one DllXyGymElec builds for X's class (at `vtable` in the module:
// OrasXyGymModules.h), registered, then the field at + 0x1C. The lifts (0x28 bytes): + 0x18 and + 0x1C 0, + 0x20 a float
// (-18.0 up, 0 down: `value`), + 0x24 a byte (1 for the top floor's: `flag`). The answer (0x28): the script's first
// argument made 0 or 1 at + 0x20, its second (a float) at + 0x24. The black (0x20): nothing more. As X's, no test of the
// allocation in the registration (X's would crash the same way on a null one)
enum class ElecEvent { Lift, Answer, Black };
std::vector<uint32_t> ElecEventNative(ElecEvent kind, uint32_t vtable, uint32_t value, int flag)
{
    XyNativeAsm a;
    a.Op(0xE92D41F0);           // push {r4, r5, r6, r7, r8, lr}
    if (kind == ElecEvent::Answer)
    {
        a.Op(0xE5918004);       // ldr r8, [r1, #4]                the first argument
        a.Op(0xE3580000);       // cmp r8, #0
        a.Op(0x13A08001);       // movne r8, #1
        a.Op(0xE5917008);       // ldr r7, [r1, #8]                the second (a float's bits)
    }
    a.Call(ScriptContext);
    a.Op(0xE590000C);           // ldr r0, [r0, #0xc]
    a.Op(0xE5905048);           // ldr r5, [r0, #0x48]          the field
    a.Op(0xE5950060);           // ldr r0, [r5, #0x60]          (X's + 0x50)
    a.Op(0xE5906018);           // ldr r6, [r0, #0x18]          its heap
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x12BB54);           // the heap's allocator
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(kind == ElecEvent::Black ? 0xE3A00020 : 0xE3A00028); // mov r0, #size
    a.Call(0x123480);           // new
    a.Op(0xE1A04000);           // mov r4, r0
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x12BB54);
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(0xE1A00004);           // mov r0, r4
    a.Call(0x109434);           // the event's base constructor
    a.Op(0xE59521A4);           // ldr r2, [r5, #0x1a4]         the module's handle
    a.Literal(3, vtable);       // ldr r3, =the vtable's offset
    a.Op(0xE0822003);           // add r2, r2, r3
    a.Op(0xE3A01000);           // mov r1, #0
    a.Op(0xE5802000);           // str r2, [r0]
    a.Op(0xE5801018);           // str r1, [r0, #0x18]
    a.Op(0xE580101C);           // str r1, [r0, #0x1c]
    if (kind == ElecEvent::Lift)
    {
        a.Literal(2, value);    // ldr r2, =the float
        a.Op(0xE5802020);       // str r2, [r0, #0x20]
        a.Op(flag ? 0xE3A02001 : 0xE3A02000); // mov r2, #flag
        a.Op(0xE5C02024);       // strb r2, [r0, #0x24]
    }
    if (kind == ElecEvent::Answer)
    {
        a.Op(0xE5C08020);       // strb r8, [r0, #0x20]
        a.Op(0xE5807024);       // str r7, [r0, #0x24]          the float
    }
    a.Op(0xE1A04000);           // mov r4, r0
    a.Op(0xE1A01000);           // mov r1, r0
    a.Op(0xE1A00006);           // mov r0, r6
    a.Call(0x108CB0);           // the event registered
    a.Op(0xE584501C);           // str r5, [r4, #0x1c]          the field
    a.Op(0xE3A00000);           // mov r0, #0
    a.Op(0xE8BD81F0);           // pop {r4, r5, r6, r7, r8, pc}
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
        {"_GymElecLiftUp", ElecEventNative(ElecEvent::Lift, XyElecLiftUpVtable, 0xC1900000, 0)},       // -18.0
        {"_GymElecTopLiftUp", ElecEventNative(ElecEvent::Lift, XyElecLiftUpVtable, 0xC1900000, 1)},
        {"_GymElecLiftDown", ElecEventNative(ElecEvent::Lift, XyElecLiftDownVtable, 0, 0)},
        {"_GymElecTopLiftDown", ElecEventNative(ElecEvent::Lift, XyElecLiftDownVtable, 0, 1)},
        {"_GymElecAnswerAnm", ElecEventNative(ElecEvent::Answer, XyElecAnswerAnmVtable, 0, 0)},
        {"_GymElecBlackAnm", ElecEventNative(ElecEvent::Black, XyElecBlackAnmVtable, 0, 0)},
        // Ramos's (31 October): X's 0x10294B4C and 0x10294CCC, each a call of a function of DllUSGymGrass with the
        // module's object (X's DllField stubs 0x101FD3D8 and 0x101FD3E0: its 0x69C, then 0 returned; its 0x780, its
        // answer returned)
        {"_GymGrassPlayGardAnm", ModuleCall(0x69C, 0, 0, false, false)},
        {"_GymGrassIsEndGardAnm", ModuleCall(0x780, 0, 0, false, true)},
        {"XyGymEsperWarpEvent", EsperWarpEvent()},
    };
}

}
