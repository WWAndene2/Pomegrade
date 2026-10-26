#include "OrasXyDustBox.h"

#include "OrasXyScript.h"

namespace remake
{

namespace
{
uint32_t ArmBranch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

uint32_t Word(const Bytes& b, size_t at) { return (uint32_t)(b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24); }
void Put(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }

// X's block: the filler 0x3B4C14 (table, game data), its helper 0x3B5000 (table, entry, group), the reset 0x3B513C (table,
// game data), and their literal pools
constexpr uint32_t XBlockFrom = 0x3B4C14, XBlockTo = 0x3B5208, XFiller = 0x3B4C14, XReset = 0x3B513C;
// X's tables: the spots (12 bytes: the entry, its flag, its group; four lists: 1, 1, 4 and 4 spots), the Pokemon (4 groups
// of 6, 16 bytes: species, form, level, cumulative percent), the items (4 groups of 7, 8 bytes: item, cumulative percent)
constexpr uint32_t XSpots = 0x5432A4, XSpotsSize = 10 * 12, XPokemon = 0x5449EC, XPokemonSize = 4 * 0x60, XItems = 0x544B6C, XItemsSize = 4 * 0x38;
constexpr uint32_t TableSize = 0x13C;

// Omega Ruby's same functions (prototype/xy_import_match.py, 23 October)
const std::map<uint32_t, uint32_t> XyCalls = {
    {0x140170, 0x12FD48}, // a flag's state (same)
    {0x3E3250, 0x406B30}, // Save_SetEventFlag (same)
    {0x11F034, 0x11FA10}, // Save_ClearEventFlag_0011FA10 (same)
    {0x45A908, 0x48AD84}, // Util_RandomBelow (read side by side: its generator's two calls differ only by their offsets)
    {0x13A158, 0x139404}, // the date's constructor (the call before each date read, 9 copies: read at every site)
    {0x1765F0, 0x177300}, // the date read from the save (same)
    {0x479B84, 0x4AA6E0}, // the date's weekday (same)
};
}

std::vector<uint32_t> PortArmBlock(const Bytes& code, uint32_t codeBase, const ArmBlockPort& port, uint32_t at)
{
    if (port.From < codeBase || port.To <= port.From || port.To - codeBase > code.size() || (port.From | port.To) & 3)
        throw FormatError("a ported block outside its code");
    std::vector<uint32_t> out;
    size_t matched = 0;
    for (uint32_t a = port.From; a < port.To; a += 4)
    {
        uint32_t w = Word(code, a - codeBase);
        const auto fixed = port.Words.find(a);
        if (fixed != port.Words.end())
        {
            if (w != fixed->second.first)
            {
                char m[96];
                snprintf(m, sizeof m, "a ported block: 0x%X holds 0x%08X, not 0x%08X", a, w, fixed->second.first);
                throw FormatError(m);
            }
            out.push_back(fixed->second.second);
            matched++;
            continue;
        }
        const auto literal = port.Literals.find(w);
        if ((w & 0x0E000000) == 0x0A000000 && w >> 28 != 0xF)
        {
            // b / bl: its target, kept inside the block or mapped
            const uint32_t target = a + 8 + (uint32_t)((int32_t)(w << 8) >> 6);
            const uint32_t from = at + (a - port.From);
            if (target >= port.From && target < port.To) w = (w & 0xFF000000) | (((at + (target - port.From) - from - 8) >> 2) & 0xFFFFFF);
            else
            {
                const auto call = port.Calls.find(target);
                if (call == port.Calls.end())
                {
                    char m[96];
                    snprintf(m, sizeof m, "a ported block: 0x%X branches to 0x%X, not mapped", a, target);
                    throw FormatError(m);
                }
                w = (w & 0xFF000000) | (((call->second - from - 8) >> 2) & 0xFFFFFF);
            }
        }
        else if (literal != port.Literals.end()) w = literal->second;
        out.push_back(w);
    }
    if (matched != port.Words.size()) throw FormatError("a ported block: a word to change is outside it");
    return out;
}

XyDustBoxRoom XyDustBoxRoomAt(const Bytes& xCode, const Bytes& orasCode, uint32_t at)
{
    XyDustBoxRoom r;
    // the table, as X's 0x112A48 leaves it: each entry's state (its byte +0) 2, the drawn spots -1
    const uint32_t table = r.Table = at;
    r.Room.assign(TableSize, 0);
    for (uint32_t k = 0; k < 10; k++) r.Room[0x1C * k] = 2;
    for (uint32_t k = 0x11C; k < TableSize; k++) r.Room[k] = 0xFF;
    while (r.Room.size() % 0x10) r.Room.push_back(0);
    // X's tables, the spots' flags moved as X/Y's scripts' are
    const auto copy = [&](uint32_t from, uint32_t size) {
        const uint32_t placed = at + (uint32_t)r.Room.size();
        r.Room.insert(r.Room.end(), xCode.begin() + (from - CodeBase), xCode.begin() + (from - CodeBase) + size);
        return placed;
    };
    const uint32_t spots = copy(XSpots, XSpotsSize), pokemon = copy(XPokemon, XPokemonSize), items = copy(XItems, XItemsSize);
    for (uint32_t k = 0; k < 10; k++)
    {
        const size_t flag = spots - at + 12 * k + 4;
        const uint32_t f = Word(r.Room, flag);
        if (f < 0xC50 || f > 0xC59) throw FormatError("X's dust box spots: a flag not 0xC50-0xC59 (not X's table)");
        const uint32_t moved = (uint32_t)XyFirstFlag + f;
        for (int b = 0; b < 4; b++) r.Room[flag + b] = (uint8_t)(moved >> (8 * b));
    }
    // X's block, its literals pointed at the copies and its save offsets made Omega Ruby's
    ArmBlockPort port;
    port.From = XBlockFrom;
    port.To = XBlockTo;
    port.Calls = XyCalls;
    port.Literals = {{XSpots, spots}, {XSpots + 12, spots + 12}, {XSpots + 24, spots + 24}, {XSpots + 72, spots + 72},
                     {XPokemon, pokemon}, {XItems, items}};
    // the event work: `add rd, rn, #0x13000` then `add r0, r0, #0x148` (10 places: 8 in the filler, 2 in the reset), made #0x140
    for (uint32_t a = XBlockFrom; a + 4 < XBlockTo; a += 4)
    {
        const uint32_t w = Word(xCode, a - CodeBase), next = Word(xCode, a + 4 - CodeBase);
        if ((w & 0xFFF00FFF) == 0xE2800A13 && next == 0xE2800F52) port.Words[a + 4] = {next, 0xE2800F50};
    }
    if (port.Words.size() != 10) throw FormatError("X's dust box code: not 10 event work offsets (not X's code)");
    port.Words[0x3B5150] = {0xE2844FAA, 0xE2844FAC}; // the date: add r4, r4, #0x2A8 (+ 0xC00) made #0x2B0
    const uint32_t block = at + (uint32_t)r.Room.size();
    for (const uint32_t w : PortArmBlock(xCode, CodeBase, port, block)) Put(r.Room, w);
    const uint32_t filler = block + (XFiller - XBlockFrom), reset = block + (XReset - XBlockFrom);

    // the zone change's stub (r4 the field, its + 0x1C the game data; called in place of `ldrh r0, [sb, #4]`)
    constexpr uint32_t zoneHook = 0x3D5858, zoneWord = 0xE1D900B4, dayHook = 0x3F0C34, dayWord = 0xE1A00000;
    if (Word(orasCode, zoneHook - CodeBase) != zoneWord || Word(orasCode, dayHook - CodeBase) != dayWord)
        throw FormatError("the dust box's hooks: not Omega Ruby's code");
    const uint32_t zone = at + (uint32_t)r.Room.size();
    const uint32_t zoneStub[] = {
        0xE92D506F,                                 // 00 push {r0-r3, r5, r6, ip, lr}
        0xE594101C,                                 // 04 ldr r1, [r4, #0x1C]   the game data
        0xE59F0018,                                 // 08 ldr r0, [pc, #0x18]   the table
        ArmBranch(0xE, zone + 0x0C, reset, true),   // 0C bl the reset
        0xE594101C,                                 // 10 ldr r1, [r4, #0x1C]
        0xE59F000C,                                 // 14 ldr r0, [pc, #0xC]
        ArmBranch(0xE, zone + 0x18, filler, true),  // 18 bl the filler
        0xE8BD506F,                                 // 1C pop {r0-r3, r5, r6, ip, lr}
        zoneWord,                                   // 20 ldrh r0, [sb, #4]     the game's
        0xE12FFF1E,                                 // 24 bx lr
        table,                                      // 28
    };
    for (const uint32_t w : zoneStub) Put(r.Room, w);
    // the new day's stub (r4 the game data; in place of a nop): X's 0x3CEB48's flag lines
    const uint32_t day = at + (uint32_t)r.Room.size();
    const uint32_t dayStub[] = {
        0xE92D503F,                                 // 00 push {r0-r5, ip, lr}
        0xE24DD008,                                 // 04 sub sp, sp, #8        the date
        0xE59F505C,                                 // 08 ldr r5, [pc, #0x5C]   the table
        0xE3A00001,                                 // 0C mov r0, #1
        0xE5C5011A,                                 // 10 strb r0, [r5, #0x11A]
        0xE5C5011B,                                 // 14 strb r0, [r5, #0x11B]
        0xE1A0000D,                                 // 18 mov r0, sp
        ArmBranch(0xE, day + 0x1C, 0x139404, true), // 1C bl the date's constructor
        0xE5940000,                                 // 20 ldr r0, [r4]          the save
        0xE2800C0E,                                 // 24 add r0, r0, #0xE00
        0xE28000B0,                                 // 28 add r0, r0, #0xB0     its date
        0xE1A0100D,                                 // 2C mov r1, sp
        ArmBranch(0xE, day + 0x30, 0x177300, true), // 30 bl the date read
        0xE1A0000D,                                 // 34 mov r0, sp
        ArmBranch(0xE, day + 0x38, 0x4AA6E0, true), // 38 bl the weekday
        0xE3500004,                                 // 3C cmp r0, #4
        0x03A00001,                                 // 40 moveq r0, #1
        0x05C50118,                                 // 44 strbeq r0, [r5, #0x118]
        ArmBranch(0x0, day + 0x48, day + 0x60, false), // 48 beq the end
        0xE1A0000D,                                 // 4C mov r0, sp
        ArmBranch(0xE, day + 0x50, 0x4AA6E0, true), // 50 bl the weekday
        0xE3500002,                                 // 54 cmp r0, #2
        0x03A00001,                                 // 58 moveq r0, #1
        0x05C50119,                                 // 5C strbeq r0, [r5, #0x119]
        0xE28DD008,                                 // 60 add sp, sp, #8
        0xE8BD503F,                                 // 64 pop {r0-r5, ip, lr}
        0xE12FFF1E,                                 // 68 bx lr                 (the game's word was a nop)
        table,                                      // 6C
    };
    for (const uint32_t w : dayStub) Put(r.Room, w);

    r.Code.push_back({zoneHook, zoneWord, ArmBranch(0xE, zoneHook, zone, true), "Zone_ChangeLoadStep: the dust boxes' reset and filler (X's 0x3B843C)"});
    r.Code.push_back({dayHook, dayWord, ArmBranch(0xE, dayHook, day, true), "the new day (0x3F0AD0): the dust boxes' flags (X's 0x3CEB48)"});
    r.Code.push_back({XyDustBoxTablePointer, 0, table, "the dust box table's address (its natives)"});
    return r;
}

std::vector<CodeWord> XyDustBoxFieldPatchAt(uint32_t cave)
{
    // the placed prop (Map_PlacePropsFromContainer's loop, r4 its row + 4, r8 the prop list, the caller's r1 at sp + 0x40)
    constexpr uint32_t hook = 0x102608AC, hookWord = 0xE2855001, propOf = 0x1031BEAC, searched = 0x10257E34, flagState = 0x12FD48;
    const uint32_t done = cave + 0x6C;
    const uint32_t routine[] = {
        0xE92D404F,                                 // 00 push {r0-r3, r6, lr}
        0xE5946028,                                 // 04 ldr r6, [r4, #0x28]   the prop's id
        0xE2460EC5,                                 // 08 sub r0, r6, #0xC50
        0xE3500095,                                 // 0C cmp r0, #0x95         0xC50-0xCE5
        ArmBranch(0x8, cave + 0x10, done, false),   // 10 bhi done
        0xE59D0058,                                 // 14 ldr r0, [sp, #0x58]   the caller's r1
        0xE59000AC,                                 // 18 ldr r0, [r0, #0xAC]
        0xE5900000,                                 // 1C ldr r0, [r0]          the map
        0xE5900060,                                 // 20 ldr r0, [r0, #0x60]   (X's + 0x50)
        0xE590001C,                                 // 24 ldr r0, [r0, #0x1C]   the game data
        0xE5900000,                                 // 28 ldr r0, [r0]          the save
        0xE2800801,                                 // 2C add r0, r0, #0x10000
        0xE2800DC5,                                 // 30 add r0, r0, #0x3140   the event work
        0xE6FF1076,                                 // 34 uxth r1, r6
        0xE2811C0D,                                 // 38 add r1, r1, #0xD00    X's flag moved (XyFirstFlag)
        0xE59FC034,                                 // 3C ldr ip, [pc, #0x34]
        0xE12FFF3C,                                 // 40 blx ip                the flag's state
        0xE3500000,                                 // 44 cmp r0, #0
        ArmBranch(0x0, cave + 0x48, done, false),   // 48 beq done
        0xE1A00008,                                 // 4C mov r0, r8
        0xE6FF1076,                                 // 50 uxth r1, r6
        ArmBranch(0xE, cave + 0x54, propOf, true),  // 54 bl the prop of the id
        0xE3500000,                                 // 58 cmp r0, #0
        ArmBranch(0x0, cave + 0x5C, done, false),   // 5C beq done
        0xE3A01001,                                 // 60 mov r1, #1
        // the animation's float in s0, 1.0 as X's 0x3B4BC0 gives it (`vldr s0` at 0x3B4C04): without it the call took
        // whatever s0 held. No difference seen either way (runs sc9, sc10, 26 October: the can of a drawn spot looks and moves
        // as an empty one's, as in X itself, run xv3)
        0xEEB70A00,                                 // 64 vmov.f32 s0, #1.0
        ArmBranch(0xE, cave + 0x68, searched, true),// 68 bl its animation slot 1 + 3 (searched)
        0xE8BD404F,                                 // 6C done: pop {r0-r3, r6, lr}
        hookWord,                                   // 70 add r5, r5, #1        the game's
        0xE12FFF1E,                                 // 74 bx lr
        flagState,                                  // 78
    };
    static_assert(XyFirstFlag == 0xD00, "the routine adds 0xD00");
    std::vector<CodeWord> words;
    words.push_back({hook, hookWord, ArmBranch(0xE, hook, cave, true), "Map_PlacePropsFromContainer: a dust box's searched look (X's 0x3B4BC0)"});
    for (uint32_t k = 0; k < sizeof routine / 4; k++) words.push_back({cave + 4 * k, 0, routine[k], "a dust box's searched look (cave)"});
    return words;
}

std::vector<XyNative> XyDustBoxNatives()
{
    constexpr uint32_t ScriptContext = 0x3FE5C8, Getter = 0x4DBE1C, StartEncounter = 0x3DADE8, ClearTemp = 0x3FFF30, PokeDiarySet = 0x478964;
    constexpr uint32_t ClearContext = 0x102C00C0, InitContext = 0x102BFDD4;
    // CallDustBoxEncount(n): X's 0x1028CFD4 and the battle it sets (0x1029A650), on Omega Ruby's encounter context (0xA4
    // bytes at sp + 8)
    XyNativeAsm battle;
    for (const uint32_t w : {0xE92D41F0u,   // push {r4-r8, lr}
                             0xE24DD0B0u,   // sub sp, sp, #0xB0
                             0xE5910004u,   // ldr r0, [r1, #4]
                             0xE6FF4070u})  // uxth r4, r0            n
        battle.Op(w);
    battle.Call(ScriptContext);
    battle.Op(0xE1A05000);                  // mov r5, r0
    battle.Op(0xE595600C);                  // ldr r6, [r5, #0xC]     the field
    battle.Literal(7, XyDustBoxTablePointer);
    for (const uint32_t w : {0xE5977000u,   // ldr r7, [r7]           the table
                             0xE0640184u,   // rsb r0, r4, r4, lsl #3
                             0xE0877100u,   // add r7, r7, r0, lsl #2 entry n
                             0xE5960048u,   // ldr r0, [r6, #0x48]
                             0xE5900104u,   // ldr r0, [r0, #0x104]   (X's + 0xF4)
                             0xE5900004u,   // ldr r0, [r0, #4]
                             0xE590000Cu})  // ldr r0, [r0, #0xC]
        battle.Op(w);
    battle.Call(Getter);
    battle.Op(0xE1A08000);                  // mov r8, r0
    battle.Op(0xE28D0008);                  // add r0, sp, #8
    battle.CallField(ClearContext);
    for (const uint32_t w : {0xE596002Cu,   // ldr r0, [r6, #0x2C]
                             0xE5D0001Cu,   // ldrb r0, [r0, #0x1C]   (X's + 0x34)
                             0xE58D0004u,   // str r0, [sp, #4]
                             0xE3A000FFu,   // mov r0, #0xFF
                             0xE58D0000u,   // str r0, [sp]
                             0xE28D0008u,   // add r0, sp, #8
                             0xE1A01006u,   // mov r1, r6
                             0xE3A02001u,   // mov r2, #1
                             0xE1A03008u})  // mov r3, r8
        battle.Op(w);
    battle.CallField(InitContext);
    // X's setter 0x1026DE30 (context, species, form, level, type)
    for (const uint32_t w : {0xE28D3008u,   // add r3, sp, #8
                             0xE5970004u,   // ldr r0, [r7, #4]       species
                             0xE6FF0070u,   // uxth r0, r0
                             0xE5830000u,   // str r0, [r3]
                             0xE5971008u,   // ldr r1, [r7, #8]       form
                             0xE597200Cu,   // ldr r2, [r7, #0xC]     level
                             0xE20110FFu,   // and r1, r1, #0xFF
                             0xE20220FFu,   // and r2, r2, #0xFF
                             0xE1821401u,   // orr r1, r2, r1, lsl #8
                             0xE5831004u,   // str r1, [r3, #4]       level, form
                             0xE3A00000u,   // mov r0, #0
                             0xE5830008u,   // str r0, [r3, #8]
                             0xE583000Cu,   // str r0, [r3, #0xC]
                             0xE1C301B0u,   // strh r0, [r3, #0x10]
                             0xE3A00001u,   // mov r0, #1
                             0xE583006Cu,   // str r0, [r3, #0x6C]
                             0xE5830070u,   // str r0, [r3, #0x70]
                             0xE5830074u,   // str r0, [r3, #0x74]
                             0xE3A00000u | (XyDustBoxBattleTypeNumber & 0xFF), // mov r0, #type
                             0xE5C3008Au,   // strb r0, [r3, #0x8A]   (X's + 0x8E)
                             0xE3A00000u,   // mov r0, #0
                             0xE58D0000u,   // str r0, [sp]           Omega Ruby's fifth argument
                             0xE5960018u,   // ldr r0, [r6, #0x18]
                             0xE1A01006u,   // mov r1, r6
                             0xE28D2008u,   // add r2, sp, #8
                             0xE3A03000u})  // mov r3, #0
        battle.Op(w);
    battle.Call(StartEncounter);
    // X's diary line: PokeDiarySet's (X 0x448E08 = Omega Ruby's 0x478964) event 0x204, the zone, the species
    for (const uint32_t w : {0xE3A00F81u,   // mov r0, #0x204
                             0xE596101Cu,   // ldr r1, [r6, #0x1C]   the game data
                             0xE2811E12u,   // add r1, r1, #0x120
                             0xE1D110B4u,   // ldrh r1, [r1, #4]      the zone (PlayerGetZoneID's, both games)
                             0xE5972004u,   // ldr r2, [r7, #4]
                             0xE6FF2072u,   // uxth r2, r2            the species
                             0xE3A03000u})  // mov r3, #0
        battle.Op(w);
    battle.Call(PokeDiarySet);
    battle.Op(0xE1A00005);                  // mov r0, r5
    battle.Call(ClearTemp);
    for (const uint32_t w : {0xE3A00000u,   // mov r0, #0
                             0xE28DD0B0u,   // add sp, sp, #0xB0
                             0xE8BD81F0u})  // pop {r4-r8, pc}
        battle.Op(w);

    // CheckTypeOfDustBox(n): entry n's state, 2 above 1; GetItemIdOfDustBox(n): its item
    const auto entry = [](XyNativeAsm& a) {
        a.Op(0xE5910004);                   // ldr r0, [r1, #4]
        a.Op(0xE6FF0070);                   // uxth r0, r0
        a.Literal(1, XyDustBoxTablePointer);
        a.Op(0xE5911000);                   // ldr r1, [r1]
        a.Op(0xE0600180);                   // rsb r0, r0, r0, lsl #3
    };
    XyNativeAsm type;
    entry(type);
    type.Op(0xE7D10100);                    // ldrb r0, [r1, r0, lsl #2]
    type.Op(0xE3500001);                    // cmp r0, #1
    type.Op(0x83A00002);                    // movhi r0, #2
    type.Op(0xE12FFF1E);                    // bx lr
    XyNativeAsm item;
    entry(item);
    item.Op(0xE0810100);                    // add r0, r1, r0, lsl #2
    item.Op(0xE5900014);                    // ldr r0, [r0, #0x14]
    item.Op(0xE12FFF1E);                    // bx lr
    return {{"CallDustBoxEncount", battle.Finish()}, {"CheckTypeOfDustBox", type.Finish()}, {"GetItemIdOfDustBox", item.Finish()}};
}

}
