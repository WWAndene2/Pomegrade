#include "OrasXyNickname.h"

namespace remake
{

namespace
{
// Omega Ruby's .code, each byte-identical to the X function in brackets
constexpr uint32_t ScriptContext = 0x3FE5C8;  // the script context (X 0x3DCFC8)
constexpr uint32_t HeapOf = 0x12BB5C;         // a heap by its id (X 0x12B284)
constexpr uint32_t HeapNew = 0x123480;        // new(size, heap) (X 0x122920)
constexpr uint32_t MessageInit = 0x3A736C;    // a message reader over a loaded file (this, file, heap) (X 0x38FB68)
constexpr uint32_t StringInit = 0x12B5B0;     // Text_WideStringBuffer_Init(this, length, heap) (X 0x12ACD8)
constexpr uint32_t StringFree = 0x12B624;     // its destructor (X 0x12AD4C)
constexpr uint32_t GetLine = 0x139448;        // Text_GetMessageString(reader, line, buffer) (X 0x13A19C)
constexpr uint32_t ExpandTags = 0x13CCEC;     // Text_ExpandControlTags(words, out, in) (X 0x13D844)
constexpr uint32_t ToArray = 0x4CF164;        // a buffer's characters into an array (buffer, array, length) (X 0x49D97C)

// PlayerNickNameSelectFix(line): X's 0x10290BA4 (its assertion on the context left out)
std::vector<uint32_t> SelectFix()
{
    XyNativeAsm a;
    const int out = a.NewLabel();
    a.Op(0xE92D41F0);      // push {r4-r8, lr}
    a.Op(0xE24DD020);      // sub sp, sp, #0x20          two string buffers
    a.Op(0xE5915004);      // ldr r5, [r1, #4]           the line
    a.Op(0xE6FF5075);      // uxth r5, r5
    a.Call(ScriptContext);
    a.Op(0xE590100C);      // ldr r1, [r0, #0xC]
    a.Op(0xE591101C);      // ldr r1, [r1, #0x1C]
    a.Op(0xE5916000);      // ldr r6, [r1]               the save
    a.Op(0xE2866B4A);      // add r6, r6, #0x12800
    a.Op(0xE2866F82);      // add r6, r6, #0x208         the status block (+0x129A0) ...
    a.Op(0xE2866002);      // add r6, r6, #2             ... + 0x6A, the nickname
    a.Op(0xE590001C);      // ldr r0, [r0, #0x1C]
    a.Op(0xE5907000);      // ldr r7, [r0]               the words
    a.Op(0xE5904088);      // ldr r4, [r0, #0x88]        the message holder
    a.Op(0xE3540000);      // cmp r4, #0
    a.Branch(0x0, out);    // beq out                    no message loaded
    a.Op(0xE5944004);      // ldr r4, [r4, #4]           the loaded file
    a.Op(0xE3540000);      // cmp r4, #0
    a.Branch(0x0, out);
    a.Op(0xE3A00008);      // mov r0, #8
    a.Call(HeapOf);
    a.Op(0xE1A08000);      // mov r8, r0                 heap 8
    a.Op(0xE1A01008);      // mov r1, r8
    a.Op(0xE3A00028);      // mov r0, #0x28
    a.Call(HeapNew);
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, out);
    a.Op(0xE1A01004);      // mov r1, r4
    a.Op(0xE1A02008);      // mov r2, r8
    a.Call(MessageInit);
    a.Op(0xE1A04000);      // mov r4, r0                 the reader
    a.Op(0xE1A02008);      // mov r2, r8
    a.Op(0xE3A01020);      // mov r1, #0x20
    a.Op(0xE1A0000D);      // mov r0, sp
    a.Call(StringInit);
    a.Op(0xE1A02008);      // mov r2, r8
    a.Op(0xE3A01020);      // mov r1, #0x20
    a.Op(0xE28D0010);      // add r0, sp, #0x10
    a.Call(StringInit);
    a.Op(0xE28D2010);      // add r2, sp, #0x10
    a.Op(0xE1A01005);      // mov r1, r5
    a.Op(0xE1A00004);      // mov r0, r4
    a.Call(GetLine);
    a.Op(0xE28D2010);      // add r2, sp, #0x10
    a.Op(0xE1A0100D);      // mov r1, sp
    a.Op(0xE1A00007);      // mov r0, r7
    a.Call(ExpandTags);
    a.Op(0xE1A0000D);      // mov r0, sp
    a.Op(0xE1A01006);      // mov r1, r6
    a.Op(0xE3A0200D);      // mov r2, #13
    a.Call(ToArray);
    a.Op(0xE5940000);      // ldr r0, [r4]               the reader's virtual destructor
    a.Op(0xE5901004);      // ldr r1, [r0, #4]
    a.Op(0xE1A00004);      // mov r0, r4
    a.Op(0xE12FFF31);      // blx r1
    a.Op(0xE28D0010);      // add r0, sp, #0x10
    a.Call(StringFree);
    a.Op(0xE1A0000D);      // mov r0, sp
    a.Call(StringFree);
    a.Set(out);
    a.Op(0xE28DD020);      // add sp, sp, #0x20
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD81F0);      // pop {r4-r8, pc}
    return a.Finish();
}
}

std::vector<XyNative> XyNicknameNatives()
{
    return {{"PlayerNickNameSelectFix", SelectFix()}};
}

}
