#include "OrasXyActionCommands.h"

namespace remake
{

const std::array<uint8_t, 212>& XyActionCommandsToOras()
{
    // prototype/xy_action_commands.py's output (2 November, from both games' DllField builders X 0x102446F0 and Omega
    // Ruby 0x1027E180): the same class name, else an animation by its DrawStatus block (inferred), else X's number
    // 108 by name, 75 animations by DrawStatus block, 29 kept
    static const std::array<uint8_t, 212> table = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 26, 27, 28, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 62, 63,
    68, 69, 70, 67, 68, 69, 74, 71, 72, 73, 74, 79, 76, 77, 78, 102,
    103, 104, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 110, 111,
    112, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
    149, 150, 151, 152, 153, 154, 155, 135, 136, 137, 138, 156, 174, 175, 176, 157,
    144, 145, 162, 151, 153, 166, 167, 168, 169, 170, 171, 172, 156, 157, 158, 174,
    175, 176, 177, 180, 181, 182, 183, 186, 187, 188, 189, 192, 193, 194, 195, 198,
    199, 200, 201, 179, 178, 184, 182, 179, 185, 190, 196, 187, 191, 197, 190, 229,
    230, 231, 232, 233, 234, 235, 236, 237, 238, 201, 239, 240, 241, 243, 244, 245,
    246, 247, 248, 249,
    };
    return table;
}

XyNative XyMdlAcmdSetNative()
{
    // the table first (jumped over), then: the script's first argument (the command) translated in place when under 212,
    // and Omega Ruby's MdlAcmdSet called with the same arguments, its answer returned
    XyNativeAsm a;
    const int start = a.NewLabel(), call = a.NewLabel();
    a.Branch(0xE, start);
    const auto& t = XyActionCommandsToOras();
    for (size_t k = 0; k < t.size(); k += 4)
        a.Op((uint32_t)t[k] | (uint32_t)t[k + 1] << 8 | (uint32_t)t[k + 2] << 16 | (uint32_t)t[k + 3] << 24);
    a.Set(start);                           // word 54
    a.Op(0xE92D4010);                       // push {r4, lr}
    a.Op(0xE5912004);                       // ldr r2, [r1, #4]          the command
    a.Op(0xE35200D4);                       // cmp r2, #212
    a.Branch(0x2, call);                    // bhs: none of X's
    a.Op(0xE24F30EC);                       // sub r3, pc, #236          the table (word 58: pc 240, the table at 4)
    a.Op(0xE7D32002);                       // ldrb r2, [r3, r2]
    a.Op(0xE5812004);                       // str r2, [r1, #4]
    a.Set(call);
    a.CallField(0x10293B44);                // Script_Native_MdlAcmdSet
    a.Op(0xE8BD8010);                       // pop {r4, pc}
    return {"XyMdlAcmdSet", a.Finish()};
}

}
