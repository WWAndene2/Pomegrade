#include "OrasXyNatives.h"

#include "Amx.h"
#include "Bytes.h"
#include "OrasXyFashion.h"
#include "OrasXyNickname.h"
#include "OrasXyZoneWalk.h"

#include <set>

namespace remake
{

namespace
{
constexpr uint32_t MaskTest = 0x3FBDA0;      // `tst r4, #0x400` in Script_RegisterNativeTablesByMask (r4 the mask, r5 the list)
constexpr uint32_t MaskTestWord = 0xE3140B01;
constexpr uint32_t Routine = 0x579E80;       // to 0x579EC7 (0x579E80-0x579EFB is this file's)
constexpr uint32_t LinkTail = 0x506178;      // Script_LinkNativeImports' `bl` of the bit-0x400 table getter (r4 the list)
constexpr uint32_t Getter400 = 0x19C910;     // that getter's .code stub
// the .code's import stub to DllField's bit-2 table getter is `ldr pc, [pc, #-4]` and the getter's address, which the
// loader writes at 0x19C8D4; the getter (`ldr r0, [pc]; bx lr`) is in DllField's code segment, at 0x1029F1E0 linked. The
// anchor is the getter, not its table: a module's data segment (the table's) is loaded apart from its code
constexpr uint32_t GetterAddress = 0x19C8D4;
constexpr uint32_t Getter = 0x1029F1E0;
constexpr uint32_t LinkImports = 0x506190;   // Sys_LinkImportsByNameHash(list, table, count; -1: to a null name)

uint32_t ArmBranch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}
}

void XyNativeAsm::Call(uint32_t address)
{
    Calls.push_back({Words.size(), address});
    Words.push_back(0);          // ldr ip, [pc, #k]
    Words.push_back(0xE12FFF3C); // blx ip
}

int XyNativeAsm::NewLabel()
{
    Labels.push_back(-1);
    return (int)Labels.size() - 1;
}

void XyNativeAsm::Set(int label) { Labels.at((size_t)label) = (int)Words.size(); }

void XyNativeAsm::Branch(uint32_t cond, int label)
{
    Branches.push_back({Words.size(), label});
    Words.push_back(cond << 28);
}

std::vector<uint32_t> XyNativeAsm::Finish()
{
    std::vector<uint32_t> out = Words;
    for (const auto& [at, label] : Branches)
    {
        const int to = Labels.at((size_t)label);
        if (to < 0) throw FormatError("an X/Y native branches to a label never set");
        out[at] = ArmBranch(out[at] >> 28, 4 * (uint32_t)at, 4 * (uint32_t)to, false);
    }
    for (const auto& [at, address] : Calls)
    {
        const uint32_t offset = 4 * (uint32_t)(out.size() - at) - 8;
        if (offset >= 0x1000) throw FormatError("an X/Y native's call is too far from its pool");
        out[at] = 0xE59FC000 | offset;
        out.push_back(address);
    }
    return out;
}

const std::vector<XyNative>& XyNatives()
{
    static const std::vector<XyNative> all = [] {
        std::vector<XyNative> v;
        for (const XyNative& n : XyNicknameNatives()) v.push_back(n);
        for (const XyNative& n : XyZoneWalkNatives()) v.push_back(n);
        for (const XyNative& n : XyFashionNatives()) v.push_back(n);
        return v;
    }();
    return all;
}

std::function<std::string(const std::string&)> XyNativeCheck(const std::string& namesFile, uint32_t mask)
{
    const auto game = AmxNativeCheck(namesFile, mask);
    std::set<std::string> ours;
    for (const XyNative& n : XyNatives()) ours.insert(n.Name);
    return [game, ours, mask](const std::string& name) -> std::string {
        if (!ours.count(name)) return game(name);
        if (mask & XyNativesBit) return "";
        char why[96];
        snprintf(why, sizeof why, "registered under bit 0x%X, not in the zone's native mask 0x%X", XyNativesBit, mask);
        return why;
    };
}

XyNativesPatch XyNativesPatchAt(uint32_t fieldCave)
{
    const std::vector<XyNative>& natives = XyNatives();
    XyNativesPatch p;
    // DllField: the table (a count, then each native's name and function as offsets from the table), the names, the code,
    // then the subroutine registering them
    const uint32_t table = (fieldCave + 3) & ~3u;
    uint32_t at = table + 4 + 8 * (uint32_t)natives.size();
    std::vector<uint32_t> names, functions;
    Bytes text;
    for (const XyNative& n : natives)
    {
        names.push_back(at + (uint32_t)text.size());
        text.insert(text.end(), n.Name.begin(), n.Name.end());
        text.push_back(0);
    }
    while (text.size() % 4) text.push_back(0);
    for (size_t k = 0; k < text.size(); k += 4)
        p.Field.push_back({at + (uint32_t)k, 0, (uint32_t)(text[k] | text[k + 1] << 8 | text[k + 2] << 16 | (uint32_t)text[k + 3] << 24), "X/Y natives: their names"});
    at += (uint32_t)text.size();
    for (const XyNative& n : natives)
    {
        functions.push_back(at);
        for (const uint32_t w : n.Code) { p.Field.push_back({at, 0, w, "X/Y native " + n.Name}); at += 4; }
    }
    p.Field.push_back({table, 0, (uint32_t)natives.size(), "X/Y natives: their count"});
    for (size_t k = 0; k < natives.size(); k++)
    {
        p.Field.push_back({table + 4 + 8 * (uint32_t)k, 0, names[k] - table, "X/Y native " + natives[k].Name + ": its name"});
        p.Field.push_back({table + 8 + 8 * (uint32_t)k, 0, functions[k] - table, "X/Y native " + natives[k].Name + ": its function"});
    }
    // the subroutine (r0 the script's list): the table copied onto the stack with its own address added to each offset,
    // ended by a null entry, and given to Sys_LinkImportsByNameHash; it finds the table from its own pc
    const uint32_t sub = at;
    const uint32_t subroutine[] = {
        0xE92D4070,                                 // 00 push {r4, r5, r6, lr}
        0xE1A05000,                                 // 04 mov r5, r0            the list
        0xE59F1064,                                 // 08 ldr r1, [pc, #0x64]   the table - (here + 0x14)
        0xE08F1001,                                 // 0C add r1, pc, r1        the table
        0xE1A00001,                                 // 10 mov r0, r1
        0xE4912004,                                 // 14 ldr r2, [r1], #4      the count
        0xE1A0600D,                                 // 18 mov r6, sp
        0xE04DD182,                                 // 1C sub sp, sp, r2, lsl #3
        0xE24DD008,                                 // 20 sub sp, sp, #8        the null entry
        0xE1A0300D,                                 // 24 mov r3, sp
        0xE2522001,                                 // 28 subs r2, r2, #1
        ArmBranch(0xB, 0x2C, 0x4C, false),          // 2C blt the end
        0xE491C004,                                 // 30 ldr ip, [r1], #4      a name
        0xE08CC000,                                 // 34 add ip, ip, r0
        0xE483C004,                                 // 38 str ip, [r3], #4
        0xE491C004,                                 // 3C ldr ip, [r1], #4      its function
        0xE08CC000,                                 // 40 add ip, ip, r0
        0xE483C004,                                 // 44 str ip, [r3], #4
        ArmBranch(0xE, 0x48, 0x28, false),          // 48 b the next
        0xE3A0C000,                                 // 4C mov ip, #0
        0xE583C000,                                 // 50 str ip, [r3]
        0xE583C004,                                 // 54 str ip, [r3, #4]
        0xE1A0100D,                                 // 58 mov r1, sp
        0xE3E02000,                                 // 5C mvn r2, #0
        0xE1A00005,                                 // 60 mov r0, r5
        0xE59FC00C,                                 // 64 ldr ip, [pc, #0xC]
        0xE12FFF3C,                                 // 68 blx ip                Sys_LinkImportsByNameHash
        0xE1A0D006,                                 // 6C mov sp, r6
        0xE8BD8070,                                 // 70 pop {r4, r5, r6, pc}
        table - (sub + 0x14),                       // 74
        LinkImports,                                // 78
    };
    for (uint32_t k = 0; k < sizeof subroutine / 4; k++) p.Field.push_back({sub + 4 * k, 0, subroutine[k], "the X/Y natives' registration"});

    // the .code: both ways a script's natives are linked call it before their last table: Script_RegisterNativeTables
    // ByMask's bit-0x400 test (r5 the list) and Script_LinkNativeImports' call of the bit-0x400 table getter (r4 the list);
    // each stub saves lr (with r3, for the stack's 8-byte alignment) and enters the subroutine through `call`, which finds
    // DllField from the getter's address in the .code's import stub
    const uint32_t r = Routine, call = r + 0x2C;
    p.Code.push_back({MaskTest, MaskTestWord, ArmBranch(0xE, MaskTest, r, true), "Script_RegisterNativeTablesByMask's bit 0x400 test: bl the X/Y natives' stub"});
    p.Code.push_back({LinkTail, ArmBranch(0xE, LinkTail, Getter400, true), ArmBranch(0xE, LinkTail, r + 0x18, true), "Script_LinkNativeImports' bit-0x400 getter call: bl the X/Y natives' stub"});
    const uint32_t routine[] = {
        0xE92D4008,                                 // 00 push {r3, lr}         from Script_RegisterNativeTablesByMask
        0xE1A00005,                                 // 04 mov r0, r5
        ArmBranch(0xE, r + 0x08, call, true),       // 08 bl call
        0xE8BD4008,                                 // 0C pop {r3, lr}
        MaskTestWord,                               // 10 tst r4, #0x400        the game's test
        0xE12FFF1E,                                 // 14 bx lr
        0xE92D4008,                                 // 18 push {r3, lr}         from Script_LinkNativeImports
        0xE1A00004,                                 // 1C mov r0, r4
        ArmBranch(0xE, r + 0x20, call, true),       // 20 bl call
        0xE8BD4008,                                 // 24 pop {r3, lr}
        ArmBranch(0xE, r + 0x28, Getter400, false), // 28 b the game's getter, which returns to the caller
        0xE59FC00C,                                 // 2C ldr ip, [pc, #0xC]    call: 0x19C8D4
        0xE59CC000,                                 // 30 ldr ip, [ip]          the getter, in DllField's code
        0xE59F1008,                                 // 34 ldr r1, [pc, #8]
        0xE08CC001,                                 // 38 add ip, ip, r1        the subroutine
        0xE12FFF1C,                                 // 3C bx ip
        GetterAddress,                              // 40
        sub - Getter,                               // 44 the subroutine, from the getter
    };
    for (uint32_t k = 0; k < sizeof routine / 4; k++) p.Code.push_back({r + 4 * k, 0, routine[k], "the X/Y natives' stubs"});
    return p;
}

}
