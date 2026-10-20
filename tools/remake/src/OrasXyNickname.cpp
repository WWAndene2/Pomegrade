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
constexpr uint32_t GetName = 0x4EA7A8;        // the status block's name (+0x50) into a buffer (status, buffer) (X 0x4AFB6C)
constexpr uint32_t Language = 0x1285D8;       // the game's language, 1 for Japanese (X 0x127AE8)
// the same as X's but for their branches' offsets (compared with those masked, 20 October)
constexpr uint32_t SetString = 0x3A5C74;      // a buffer from an array (buffer, array) (X 0x38E478)
constexpr uint32_t SetWord = 0x13D4AC;        // a word of the script's words (words, slot, buffer, grammar, [sp] 0) (X 0x13E004)
// the typed nickname (X 0x1028DC58 Load, 0x10290ACC CallInput, 0x10290250 InputFix, 0x1028EFA4 Unload)
constexpr uint32_t Holder = 0x9C;             // the keyboard's slot in the context's +0x1C object (X's +0x90, with the +0xC of
                                              // its message holder; X uses +0x90 for these natives only, Omega Ruby nothing
                                              // there that was found: inferred)
constexpr uint32_t EventManager = 0x102C0798; // DllField: the field's event (process call) object of [game + 0x18] (X 0x1026F15C)
constexpr uint32_t CallProcess = 0x3CFD88;    // call an application with a caller's parameter block (manager, id, params, 0,
                                              // [sp] 0.0) (X 0x3B2A00, the same start)
constexpr uint32_t SetCallMode = 0x3CFD78;    // `strb r1, [r0, #0x1d]` (manager, 4) (X 0x3B29F8)
constexpr uint32_t Free = 0x1397B8;           // the parameter block's free (X 0x13A50C, the same but for branch offsets)
constexpr uint32_t Delete = 0x2FF8C0;         // operator delete (X 0x2FF520, the same but for branch offsets)

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

// WordSetPlayerNickNamePrefix(slot): X's 0x1029144C and the function it calls, 0x427974: the player's name's first letter
// (two characters in Japanese) as word slot, with the status's grammar (+0xD: 0-2)
std::vector<uint32_t> Prefix()
{
    XyNativeAsm a;
    a.Op(0xE92D41F0);      // push {r4-r8, lr}
    a.Op(0xE24DD010);      // sub sp, sp, #0x10
    a.Op(0xE5917004);      // ldr r7, [r1, #4]           the slot
    a.Call(ScriptContext);
    a.Op(0xE590101C);      // ldr r1, [r0, #0x1C]
    a.Op(0xE5914000);      // ldr r4, [r1]               the words
    a.Op(0xE590000C);      // ldr r0, [r0, #0xC]
    a.Op(0xE590001C);      // ldr r0, [r0, #0x1C]
    a.Op(0xE5906000);      // ldr r6, [r0]               the save
    a.Op(0xE2866B4A);      // add r6, r6, #0x12800
    a.Op(0xE2866F68);      // add r6, r6, #0x1A0         the status block
    a.Op(0xE5941000);      // ldr r1, [r4]               the words' buffer
    a.Op(0xE1A00006);      // mov r0, r6
    a.Call(GetName);
    a.Op(0xE5940000);      // ldr r0, [r4]
    a.Op(0xE5905004);      // ldr r5, [r0, #4]           its characters
    a.Call(Language);
    a.Op(0xE3500001);      // cmp r0, #1
    a.Op(0xE1D500B0);      // ldrh r0, [r5]
    a.Op(0xE3A08000);      // mov r8, #0
    a.Op(0xE1CD00B4);      // strh r0, [sp, #4]          the first character
    a.Op(0x01D500B2);      // ldrheq r0, [r5, #2]
    a.Op(0x01CD80B8);      // strheq r8, [sp, #8]
    a.Op(0x01CD00B6);      // strheq r0, [sp, #6]        Japanese: the second
    a.Op(0x11CD80B6);      // strhne r8, [sp, #6]
    a.Op(0xE5940000);      // ldr r0, [r4]
    a.Op(0xE28D1004);      // add r1, sp, #4
    a.Call(SetString);
    a.Op(0xE5D6300D);      // ldrb r3, [r6, #0xD]
    a.Op(0xE2033003);      // and r3, r3, #3             the grammar
    a.Op(0xE58D8000);      // str r8, [sp]
    a.Op(0xE5942000);      // ldr r2, [r4]
    a.Op(0xE1A01007);      // mov r1, r7
    a.Op(0xE1A00004);      // mov r0, r4
    a.Call(SetWord);
    a.Op(0xE28DD010);      // add sp, sp, #0x10
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD81F0);      // pop {r4-r8, pc}
    return a.Finish();
}

// PlayerNickNameLoad(): a 4-byte holder of the keyboard's parameters, in the context's slot, unless one is there
std::vector<uint32_t> Load()
{
    XyNativeAsm a;
    const int out = a.NewLabel();
    a.Op(0xE92D4010);      // push {r4, lr}
    a.Call(ScriptContext);
    a.Op(0xE590401C);      // ldr r4, [r0, #0x1C]
    a.Op(0xE594009C);      // ldr r0, [r4, #Holder]
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x1, out);    // bne out
    a.Op(0xE3A00008);      // mov r0, #8
    a.Call(HeapOf);
    a.Op(0xE1A01000);      // mov r1, r0
    a.Op(0xE3A00004);      // mov r0, #4
    a.Call(HeapNew);
    a.Op(0xE3500000);      // cmp r0, #0
    a.Op(0x13A01000);      // movne r1, #0
    a.Op(0x15801000);      // strne r1, [r0]
    a.Op(0xE584009C);      // str r0, [r4, #Holder]
    a.Set(out);
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD8010);      // pop {r4, pc}
    return a.Finish();
}

// PlayerNickNameCallInput(): the name keyboard in mode 5 (the player's nickname) over a 13-character buffer, as X calls it:
// a 0x20-byte block of heap 8 (the save's +0x3418, as Omega Ruby's CallPokePartyNameInput gives the keyboard; the mode; the
// buffer at +0x14), called as application 2 through the field's event manager, kept in the holder for InputFix and Unload.
// That Omega Ruby's keyboard knows mode 5 is inferred (its DllStrInput and X's are the same code, read 20 October)
std::vector<uint32_t> CallInput()
{
    XyNativeAsm a;
    const int out = a.NewLabel(), noBuffer = a.NewLabel();
    a.Op(0xE92D41F0);      // push {r4-r8, lr}
    a.Op(0xE24DD008);      // sub sp, sp, #8
    a.Call(ScriptContext);
    a.Op(0xE1A05000);      // mov r5, r0                 the context
    a.Op(0xE3A00008);      // mov r0, #8
    a.Call(HeapOf);
    a.Op(0xE1A07000);      // mov r7, r0                 heap 8
    a.Op(0xE1A01007);      // mov r1, r7
    a.Op(0xE3A00020);      // mov r0, #0x20
    a.Call(HeapNew);
    a.Op(0xE1A04000);      // mov r4, r0                 the parameters
    a.Op(0xE3540000);      // cmp r4, #0
    a.Branch(0x0, out);
    a.Op(0xE3A00000);      // mov r0, #0
    for (uint32_t k = 0; k < 0x20; k += 4) a.Op(0xE5840000 | k); // str r0, [r4, #k]
    a.Op(0xE595000C);      // ldr r0, [r5, #0xC]
    a.Op(0xE590001C);      // ldr r0, [r0, #0x1C]
    a.Op(0xE5900000);      // ldr r0, [r0]               the save
    a.Op(0xE2800B0D);      // add r0, r0, #0x3400
    a.Op(0xE2800018);      // add r0, r0, #0x18
    a.Op(0xE5840000);      // str r0, [r4]
    a.Op(0xE3A00005);      // mov r0, #5
    a.Op(0xE5840004);      // str r0, [r4, #4]           the mode
    a.Op(0xE1A01007);      // mov r1, r7
    a.Op(0xE3A00010);      // mov r0, #0x10
    a.Call(HeapNew);
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, noBuffer);
    a.Op(0xE1A06000);      // mov r6, r0
    a.Op(0xE1A02007);      // mov r2, r7
    a.Op(0xE3A0100D);      // mov r1, #13
    a.Call(StringInit);
    a.Op(0xE5846014);      // str r6, [r4, #0x14]        the buffer
    a.Set(noBuffer);
    a.Op(0xE595000C);      // ldr r0, [r5, #0xC]
    a.Op(0xE5900018);      // ldr r0, [r0, #0x18]
    a.CallField(EventManager);
    a.Op(0xE1A06000);      // mov r6, r0                 the manager
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE58D0000);      // str r0, [sp]
    a.Op(0xE3A03000);      // mov r3, #0
    a.Op(0xE1A02004);      // mov r2, r4
    a.Op(0xE3A01002);      // mov r1, #2                 the keyboard
    a.Op(0xE1A00006);      // mov r0, r6
    a.Call(CallProcess);
    a.Op(0xE3A01004);      // mov r1, #4
    a.Op(0xE1A00006);      // mov r0, r6
    a.Call(SetCallMode);
    a.Op(0xE595001C);      // ldr r0, [r5, #0x1C]
    a.Op(0xE590009C);      // ldr r0, [r0, #Holder]
    a.Op(0xE3500000);      // cmp r0, #0
    a.Op(0x15804000);      // strne r4, [r0]             kept for InputFix and Unload
    a.Set(out);
    a.Op(0xE28DD008);      // add sp, sp, #8
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD81F0);      // pop {r4-r8, pc}
    return a.Finish();
}

// PlayerNickNameInputFix(): the keyboard's buffer into the nickname
std::vector<uint32_t> InputFix()
{
    XyNativeAsm a;
    const int out = a.NewLabel();
    a.Op(0xE92D4070);      // push {r4-r6, lr}
    a.Call(ScriptContext);
    a.Op(0xE590100C);      // ldr r1, [r0, #0xC]
    a.Op(0xE591101C);      // ldr r1, [r1, #0x1C]
    a.Op(0xE5916000);      // ldr r6, [r1]               the save
    a.Op(0xE2866B4A);      // add r6, r6, #0x12800
    a.Op(0xE2866F82);      // add r6, r6, #0x208
    a.Op(0xE2866002);      // add r6, r6, #2             the nickname
    a.Op(0xE590001C);      // ldr r0, [r0, #0x1C]
    a.Op(0xE590009C);      // ldr r0, [r0, #Holder]
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, out);
    a.Op(0xE5900000);      // ldr r0, [r0]               the parameters
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, out);
    a.Op(0xE5900014);      // ldr r0, [r0, #0x14]        the buffer
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, out);
    a.Op(0xE1A01006);      // mov r1, r6
    a.Op(0xE3A0200D);      // mov r2, #13
    a.Call(ToArray);
    a.Set(out);
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD8070);      // pop {r4-r6, pc}
    return a.Finish();
}

// PlayerNickNameUnload(): the buffer, the parameters and the holder freed, the slot emptied
std::vector<uint32_t> Unload()
{
    XyNativeAsm a;
    const int out = a.NewLabel(), freeParams = a.NewLabel(), freeHolder = a.NewLabel();
    a.Op(0xE92D4070);      // push {r4-r6, lr}
    a.Call(ScriptContext);
    a.Op(0xE590401C);      // ldr r4, [r0, #0x1C]
    a.Op(0xE594509C);      // ldr r5, [r4, #Holder]
    a.Op(0xE3550000);      // cmp r5, #0
    a.Branch(0x0, out);
    a.Op(0xE5956000);      // ldr r6, [r5]               the parameters
    a.Op(0xE3560000);      // cmp r6, #0
    a.Branch(0x0, freeHolder);
    a.Op(0xE5960014);      // ldr r0, [r6, #0x14]        the buffer
    a.Op(0xE3500000);      // cmp r0, #0
    a.Branch(0x0, freeParams);
    a.Op(0xE5901000);      // ldr r1, [r0]
    a.Op(0xE5911004);      // ldr r1, [r1, #4]
    a.Op(0xE12FFF31);      // blx r1                     its virtual destructor
    a.Set(freeParams);
    a.Op(0xE1A00006);      // mov r0, r6
    a.Call(Free);
    a.Set(freeHolder);
    a.Op(0xE1A00005);      // mov r0, r5
    a.Call(Delete);
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE584009C);      // str r0, [r4, #Holder]
    a.Set(out);
    a.Op(0xE3A00000);      // mov r0, #0
    a.Op(0xE8BD8070);      // pop {r4-r6, pc}
    return a.Finish();
}
}

std::vector<XyNative> XyNicknameNatives()
{
    return {{"PlayerNickNameSelectFix", SelectFix()}, {"WordSetPlayerNickNamePrefix", Prefix()},
            {"PlayerNickNameLoad", Load()}, {"PlayerNickNameCallInput", CallInput()},
            {"PlayerNickNameInputFix", InputFix()}, {"PlayerNickNameUnload", Unload()}};
}

}
