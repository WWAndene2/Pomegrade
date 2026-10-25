#include "OrasXyDustCloud.h"

#include "OrasXyDustBox.h"
#include "OrasXyMoveCodes.h"

#include <cstdio>
#include <iterator>
#include <map>
#include <set>

namespace remake
{

namespace
{
constexpr uint32_t XFieldBase = 0x101F9000;    // X's DllField in the linked program (OrasEngine.cpp's XyGameModules)
constexpr uint32_t OrasFieldBase = 0x10242000; // Omega Ruby's (OrasEngine.cpp's DllFieldBase)
// the .code word the loader fills with a DllField function's address, and that function's linked address (OrasXyNatives.cpp)
constexpr uint32_t GetterAddress = 0x19C8D4, Getter = 0x1029F1E0;
constexpr uint32_t Nop = 0xE320F000;

uint32_t Word(const Bytes& b, size_t at) { return (uint32_t)(b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24); }
void Put(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }
uint32_t Branch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}
// an ARM data-processing immediate (8 bits rotated right by an even amount)
uint32_t Immediate(uint32_t v)
{
    for (uint32_t r = 0; r < 16; r++)
    {
        const uint32_t imm = r ? (v << (2 * r) | v >> (32 - 2 * r)) : v;
        if (imm <= 0xFF) return r << 8 | imm;
    }
    char m[64];
    snprintf(m, sizeof m, "0x%X is not an ARM immediate", v);
    throw FormatError(m);
}

// X's DllField copied: the event, the effect, the effect's update and end, the getter of the field's +0x160 object
struct Block { uint32_t From, To; };
constexpr Block Blocks[] = {
    {0x10239668, 0x10239B94}, // the event: 0x10239668 (returns 1), 0x10239670 (end), 0x1023969C (start), 0x10239704 (update), destructors 0x10239B0C, 0x10239B54
    {0x10250CC4, 0x10250E6C}, // the effect: its six state functions, constructor 0x10250D68, destructors 0x10250DF0, 0x10250E2C
    {0x10277324, 0x1027779C}, // the effect's update (states 1-9)
    {0x102709C4, 0x10270A4C}, // the effect's end (its models and resource freed)
    {0x1027F5AC, 0x1027F5CC}, // the field's +0x160 object: its part stopped
};

// X's calls out of the blocks -> Omega Ruby's same .code functions (X's DllField stub, or X's function; xy_import_match.py,
// "near" ones read side by side, 23-24 October; ORAS_ENGINE.md 0.0 B2's map)
const std::map<uint32_t, uint32_t> CodeCalls = {
    {0x101F91C8, 0x2FF8C0}, // the heap's delete
    {0x101F91F8, 0x44FA8C}, // a sound played (same)
    {0x101F9200, 0x14EA64}, // an assert, empty in both
    {0x101F9218, 0x123480}, // an allocation from a heap
    {0x101F94F8, 0x1E8294}, // the event base class's destructor (empty in both)
    {0x101F9528, 0x11EF4C}, // the fatal error (the same loop)
    {0x101F9950, 0x16A7A4}, // a record incremented
    {0x101F9A80, 0x44FB44}, // a sound stopped (same)
    {0x101F9BD8, 0x44C710}, // a sound playing (same)
    {0x101F9C18, 0x371C04}, // a model part's flag (the same three getters/setters in turn)
    {0x101F9CA0, 0x12BB54}, // a heap's parent (same)
    {0x101F9CB0, 0x108CB0}, // an event pushed on the task list (same)
    {0x101FA370, 0x3F57B8}, // the fade's start (Omega Ruby's twin of X's 0x1021B51C calls it at the same place)
    {0x101FA390, 0x3F6088}, // the fade's end (same but its offsets)
    {0x101FA8E8, 0x3F11B8}, // a player flag cleared (same)
    {0x101FAB00, 0x4E3234}, // a character's id (+0xC: the step checks' calls side by side)
    {0x101FB338, 0x395C70}, // a model's reset (same)
    {0x101FB348, 0x39181C}, // a model bound to its animation set (same)
    {0x101FB350, 0x3878F8}, // a model's position (read side by side)
    {0x101FB418, 0x4DC498}, // a player flag tested (same)
    {0x101FB420, 0x3F4918}, // a player flag set (same)
    {0x101FB428, 0x4E435C}, // the field's talk slots ready (field + 0x54 in both)
    {0x101FB430, 0x1DBCD8}, // a battle type's six transition words (the longer table, --xy-battle-types)
    {0x101FC1F8, 0x4DCF74}, // a character's position (same)
    {0x101FCA08, 0x38F934}, // a model's end (read side by side)
    {0x10277D18, 0x4521E4}, // the resource's models (Omega Ruby's .code copy: its fields from +0x2C 4 bytes on)
    {0x10277E64, 0x4CE280}, // a model's animation from the resource (Omega Ruby's effects call it in its place)
    {0x1027E618, 0x452A00}, // the resource's members bound
    {0x1027E964, 0x4525BC}, // the resource's load
    {0x1027EDAC, 0x452E7C}, // the resource's constructor (0x40 bytes in Omega Ruby)
    {0x1027EF88, 0x452EE8}, // the resource's end
};
// Thumb helpers (X's stub -> Omega Ruby's, the same bytes): through a stub of the room, a `bl` cannot reach Thumb code
const std::map<uint32_t, uint32_t> ThumbCalls = {
    {0x101F9188, 0x100295}, // __cxa_vec_dtor
    {0x101F93E0, 0x10018D}, // __cxa_vec_ctor
};
// X's DllField functions -> Omega Ruby's (through the getter word: DllField moves)
const std::map<uint32_t, uint32_t> FieldCalls = {
    {0x1027DE54, 0x1032AC3C}, // the player's animation done (same)
    {0x1027F5CC, 0x102CDFD0}, // the zone's character list (called at the same place, X 0x1021B580 = 0x1025E270)
};

// the jump tables (relocated words, zero in the file): X's targets
constexpr uint32_t UpdateTable = 0x10239734, EffectTable = 0x10277350;
constexpr uint32_t UpdateTargets[] = {0x10239B00, 0x10239764, 0x102397C4, 0x10239828, 0x10239868, 0x1023994C,
                                      0x10239964, 0x10239A18, 0x10239A48, 0x10239AA4, 0x10239AE8, 0x10239AF0};
constexpr uint32_t EffectTargets[] = {0x1027773C, 0x10277378, 0x10277390, 0x102773B0, 0x1027773C, 0x10277588,
                                      0x102776D8, 0x1027773C, 0x10277748, 0x10277788};

// Omega Ruby's functions the room calls in DllField
constexpr uint32_t OrasStateObject = 0x1026BCD0;   // the 0x48-byte object of state 6: base, vtable 0x10349664, fields
constexpr uint32_t InitContext = 0x102BFDD4, SelectTable = 0x102BFBA4, ClearContext = 0x102C00C0;
constexpr uint32_t RockSmashFrom = 0x102D9F04, RockSmashTo = 0x102D9FD0;
constexpr uint32_t EncounterFrom = 0x3DADE8, EncounterTo = 0x3DAE6C; // Field_StartEncounterEvent
constexpr uint32_t StepCheckHook = 0x10263304, StepCheckBack = 0x10263308, StepCheckExit = 0x10263498;
constexpr uint32_t EncounterEventVtable = 0x5DED9C; // Omega Ruby's encounter event's, for its type info word
}

const XyModulePort& XyRoute13()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSR13";
        p.Name = "DllXyR13";
        // X's .code -> Omega Ruby's (xy_import_match.py; "near" ones read side by side, 24 October)
        p.Static = {
            {0x2FEB7C, 0x2FF8C0}, // a thunk of the heap's delete
            {0x2FF520, 0x2FF8C0}, // the heap's delete
            {0x14F220, 0x1F9544}, // an assert, empty -> Util_Nop_001F9544
            {0x3D41D8, 0x3F54E8}, // a character created from its zone record (its character manager's offsets moved)
            {0x3D5818, 0x3F6C70}, // a character removed (the same, offsets moved)
            {0x42E728, 0x45BB00}, // the area work set to a kind (the same function; the game data + 0x12E4 -> 0x12EC, below)
            {0x42E7A0, 0x45BB78}, // the area work
            {0x45A908, 0x48AD84}, // Util_RandomBelow
            {0x1228EC, 0x12344C}, // an allocation from a heap's tail
            {0x4B7FC0, 0x4F2B9C}, // a byte array allocated (same)
            {0x4A3FD8, 0x4DBDF4}, // a character's position (same)
            {0x4AAA70, 0x4E3674}, // a character by its id (its 0xAB0-byte records for X's)
            {0x4B3EA4, 0x1F9544}, // (a thunk below)
            {0x58E8AC, 0x5D4458}, {0x58E8BC, 0x5D4468}, // the C++ type infos' vtables
        };
        p.Field = {
            {0x102140D8, 0x10257D60}, // a prop's animation stopped (the same code, the field's offsets moved)
            {0x102141AC, 0x10257E34}, // a prop's animation started (same)
            {0x102832E4, 0x102D1644}, // a grid position (Omega Ruby's gym modules import it)
            {0x102CDEEC, 0x1031AFC0}, // a prop's animation frame (the same code, offsets moved)
            {0x10276FF4, 0x102C9194}, // a prop by its tile (same)
            {0x1027AB84, 0x102CC730}, // a position -> its grid cell
            {0x102DDD60, 0x10327D20}, // the field's player
            {0x1027F5CC, 0x102CDFD0}, // the zone's character list
            {0x1026F3A8, 0x102D3F94}, {0x1024FC30, 0}, {0x1024FC40, 0}, {0x1024FC54, 0}, // (thunks below)
        };
        // X's functions Omega Ruby has no copy of, as thunks: a prop's animation n + 3 (n below 4, X's 0x1026F3A8 calling
        // its 0x10285C3C = Omega Ruby's 0x102D3F94); the move controller's counter at +0xC (X's 0x1024FC30, 0x1024FC40,
        // 0x1024FC54, their own code); X's 0x4B3EA4 (the word of the object Omega Ruby lacks) answering 0: the lamps off
        p.Thunks = {
            {0x1026F3A8, 0x102D3F94, {0xE3510004, 0x212FFF1E, 0xE2811003}, "a prop's animation n + 3: cmp r1, #4; bxhs lr; add r1, r1, #3"},
            {0x1024FC30, 0x1F9544, {0xE280000C, 0xE1D000B0, 0xE12FFF1E}, "the move controller's counter"},
            {0x1024FC40, 0x1F9544, {0xE280000C, 0xE1C010B0, 0xE12FFF1E}, "the move controller's counter set"},
            {0x1024FC54, 0x1F9544, {0xE280000C, 0xE1D010B0, 0xE3510000, 0x03A00000, 0x012FFF1E, 0xE2411001, 0xE1A01801,
                                    0xE1B01821, 0xE1C010B0, 0x13A00001, 0x03A00000, 0xE12FFF1E}, "the move controller's counter counted down"},
            {0x4B3EA4, 0x1F9544, {0xE3A00000, 0xE1C100B0, 0xE12FFF1E}, "X's word of [[game system + 0x2C] + 0x14]: 0 (Omega Ruby has no such object)"},
        };
        // its words holding X's layouts (file offsets), each made Omega Ruby's
        p.Words = {
            {0x2D8, 0xE28000B8, 0xE28000C0, "a character's move controller: + 0x8B8 -> + 0x8C0 (Omega Ruby's 0x3F835C)"},
            {0x61C, 0xE28000B8, 0xE28000C0, "a character's move controller: + 0x8B8 -> + 0x8C0"},
            {0x400, 0xE5900050, 0xE5900060, "the map's + 0x50 -> + 0x60"},
            {0x6FC, 0xE5902050, 0xE5902060, "the map's + 0x50 -> + 0x60"},
            {0x8C4, 0xE5900050, 0xE5900060, "the map's + 0x50 -> + 0x60"},
            {0x964, 0xE5913050, 0xE5913060, "the map's + 0x50 -> + 0x60"},
            {0x968, 0xE593302C, 0xE3A03000, "X's object of the lamps' word: none (mov r3, #0)"},
            {0x96C, 0xE5933014, Nop, "X's object of the lamps' word: none"},
            {0x958, 0xE5913168, 0xE5913170, "the map's + 0x168 -> + 0x170"},
            {0x5EC, 0xE1D12EB0, 0xE1D12FB0, "the map's + 0xE0 -> + 0xF0"},
            {0x710, 0xE1D02EB0, 0xE1D02FB0, "the map's + 0xE0 -> + 0xF0"},
            {0x730, 0xE59608AC, 0xE59608B4, "a character's manager: + 0x8AC -> + 0x8B4"},
            {0x738, 0xE28990AC, 0xE28990B4, "a character's manager: + 0x8AC -> + 0x8B4"},
            {0x73C, 0xE590501C, 0xE5905018, "the character manager's + 0x1C -> + 0x18 (the step checks side by side)"},
            {0x770, 0xE5900014, 0xE5900010, "the character manager's map: + 0x14 -> + 0x10 (read in a run: the manager's words 4 nearer from + 0x10)"},
            {0x8D4, 0xE2800FB9, 0xE2800FBB, "the game data's area work: + 0x12E4 -> + 0x12EC"},
            {0x90C, 0xE2800FB9, 0xE2800FBB, "the game data's area work: + 0x12E4 -> + 0x12EC"},
            // the zone's clouds: characters of X's move codes 0x38-0x3A, Omega Ruby's numbers of them (OrasXyMoveCodes.h)
            {0x428, 0xE3500038, 0xE3500000u | (uint32_t)XyMoveCodeToOras(0x38), "a cloud's move code: X's 0x38 -> the mod's"},
            {0x42C, 0x13500039, 0x13500000u | (uint32_t)XyMoveCodeToOras(0x39), "a cloud's move code: X's 0x39 -> the mod's"},
            {0x430, 0x1350003A, 0x13500000u | (uint32_t)XyMoveCodeToOras(0x3A), "a cloud's move code: X's 0x3A -> the mod's"},
        };
        p.Vtables = {0x10A8};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0x938, 0x70}};
        p.Archive = XyDustCloudArchive; // loaded by the room's effect, not the module
        return p;
    }();
    return port;
}

XyDustCloudRoom XyDustCloudRoomAt(const Bytes& xField, const Bytes& orasField, const Bytes& orasCode, uint32_t at,
                                  uint32_t archive, const std::function<uint32_t(uint32_t)>& sound)
{
    // the layout: the stubs, the routines, the blocks, the data
    uint32_t next = at;
    const auto take = [&](uint32_t bytes) { const uint32_t a = next; next += bytes; return a; };
    // (each stub written in the order it is placed: thumbOrder, fieldOrder)
    std::map<uint32_t, uint32_t> thumbStub, fieldStub;
    std::vector<uint32_t> thumbOrder, fieldOrder;
    for (const auto& [x, to] : ThumbCalls) { thumbStub[to] = take(8); thumbOrder.push_back(to); }
    for (const uint32_t to : {FieldCalls.at(0x1027DE54), FieldCalls.at(0x1027F5CC), OrasStateObject, InitContext, SelectTable, ClearContext})
    {
        fieldStub[to] = take(28);
        fieldOrder.push_back(to);
    }
    const uint32_t weatherGet = take(8), weatherSet = take(4), request = take(8), module = take(20), heaps = take(32);
    const uint32_t encounter = take(EncounterTo - EncounterFrom), rockSmash = take(RockSmashTo - RockSmashFrom), step = take(39 * 4);
    std::vector<uint32_t> blockAt;
    for (const Block& b : Blocks) blockAt.push_back(take(b.To - b.From));
    const uint32_t eventVtable = take(8 + 6 * 4) + 8, effectVtable = take(8 + 2 * 4) + 8, desc = take(8);
    const auto room = [&](uint32_t x) -> uint32_t {
        for (size_t k = 0; k < std::size(Blocks); k++)
            if (x >= Blocks[k].From && x < Blocks[k].To) return blockAt[k] + (x - Blocks[k].From);
        char m[64];
        snprintf(m, sizeof m, "X's dust cloud code: 0x%X is in no copied block", x);
        throw FormatError(m);
    };

    XyDustCloudRoom r;
    std::vector<uint32_t> out;
    // the Thumb stubs
    const auto placed = [&](uint32_t address) {
        if (at + 4 * (uint32_t)out.size() != address) throw FormatError("X's dust cloud code: a part not where its layout puts it");
    };
    for (const uint32_t to : thumbOrder) { placed(thumbStub.at(to)); out.insert(out.end(), {0xE51FF004, to}); }
    // a DllField function: its address the getter word's plus the distance between them (ip only, then a jump)
    for (const uint32_t to : fieldOrder)
    {
        placed(fieldStub.at(to));
        std::vector<uint32_t> s = {0xE59FC010, 0xE59CC000}; // ldr ip, [pc, #0x10] (the getter word); ldr ip, [ip]
        const bool down = to < Getter;
        uint32_t d = down ? Getter - to : to - Getter;
        while (d)
        {
            uint32_t low = 0;
            while (!(d & 3u << low)) low += 2;
            const uint32_t chunk = d & 0xFFu << low;
            s.push_back((down ? 0xE24CC000 : 0xE28CC000) | Immediate(chunk)); // sub / add ip, ip, #chunk
            d -= chunk;
        }
        if (s.size() > 5) throw FormatError("X's dust cloud code: a DllField function too far from the getter");
        while (s.size() < 5) s.push_back(Nop);
        s.push_back(0xE12FFF1C); // bx ip
        s.push_back(GetterAddress);
        out.insert(out.end(), s.begin(), s.end());
    }
    // X's weather byte, not given (step 6): its getter answers 0, so the setter is never reached
    placed(weatherGet);
    out.insert(out.end(), {0xE3A00000, 0xE12FFF1E, 0xE12FFF1E});
    // X's request of the resource's archive: Omega Ruby's takes a third word, how many members (0: all)
    placed(request);
    out.push_back(0xE3A02000);
    out.push_back(Branch(0xE, request + 4, 0x452E5C, false));
    placed(module);
    // DllUSR13's 0x107AF818 (the spot used): the module's code from its handle (the map's + 0x1A4, OrasXyGymModules.cpp's
    // loader), r5 the field in the event's update
    out.insert(out.end(), {0xE595C048, 0xE59CC1A4, 0xE28CCB02, 0xE28CC018, 0xE12FFF1C});
    // the effect's resource's two heaps, as the class's other users take them in both games (X 0x10236BF4 = Omega Ruby's
    // 0x10270378: heaps 8 and 9 by their ids), for X's map + 4 and + 0x10: r6 and r5 for the constructor
    placed(heaps);
    out.insert(out.end(), {0xE52DE004, 0xE3A00009, Branch(0xE, heaps + 8, 0x12BB5C, true), 0xE1A05000,
                           0xE3A00008, Branch(0xE, heaps + 20, 0x12BB5C, true), 0xE1A06000, 0xE49DF004});

    // the encounter event made as X's 0x3BC140 makes it: Omega Ruby's Field_StartEncounterEvent but its push on the event
    // list (0x108CB0); X's update pushes it at its state 9, once the effect is over (pushed at once, its battle tore the field
    // down under the effect: runs dce, 24 October)
    {
        ArmBlockPort p;
        p.From = EncounterFrom;
        p.To = EncounterTo;
        p.Calls = {{0x12BB54, 0x12BB54}, {0x123480, 0x123480}, {0x3DB524, 0x3DB524}, {0x3DA1E8, 0x3DA1E8}};
        p.Words = {{0x3DAE40, {0xEBF4B79A, Nop}}};
        placed(encounter);
        const std::vector<uint32_t> w = PortArmBlock(orasCode, CodeBase, p, encounter);
        out.insert(out.end(), w.begin(), w.end());
    }

    // Omega Ruby's Rock Smash with X's dust cloud's: the land table 4 (the mod's kind 12), battle type 95, flags 3, the
    // event returned (X's 0x1029B1E4 returns 0x3BC140's event, the update keeps it)
    {
        ArmBlockPort p;
        p.From = RockSmashFrom;
        p.To = RockSmashTo;
        p.Calls = {{0x10242BF8, 0x4DBE1C}, {0x10246278, 0x3D9BBC}, {0x102450B0, 0x3D9888}, {0x10246620, encounter},
                   {ClearContext, fieldStub.at(ClearContext)}, {InitContext, fieldStub.at(InitContext)}, {SelectTable, fieldStub.at(SelectTable)}};
        p.Words = {{0x102D9F68, {0xE3A00004, 0xE3A00000 | XyDustCloudKind}},
                   {0x102D9F88, {0xE3A02009, 0xE3A02000 | Immediate(XyDustCloudBattleTypeNumber)}},
                   {0x102D9FA4, {0xE3A03000, 0xE3A03003}},
                   {0x102D9FB8, {0xE3A00001, Nop}}};
        placed(rockSmash);
        const std::vector<uint32_t> w = PortArmBlock(orasField, OrasFieldBase, p, rockSmash);
        out.insert(out.end(), w.begin(), w.end());
    }

    // the step routine (the character walked into, the field): X's branch at 0x10226878, the event or 0
    {
        const uint32_t s = step;
        placed(step);
        const uint32_t code[] = {
            0xE92D4070,                               // 00 push {r4, r5, r6, lr}
            0xE1A05000,                               // 04 mov r5, r0           the character
            0xE1A06001,                               // 08 mov r6, r1           the field
            Branch(0xE, s + 0x0C, 0x4DC198, true),    // 0C bl its move code
            0xE3500000u | (uint32_t)XyMoveCodeToOras(0x38), // 10 cmp r0, #code   X's 0x38-0x3A, the mod's numbers
            0x13500000u | (uint32_t)XyMoveCodeToOras(0x39), // 14 cmpne r0, #code
            0x13500000u | (uint32_t)XyMoveCodeToOras(0x3A), // 18 cmpne r0, #code
            0x13A00000,                               // 1C movne r0, #0
            0x18BD8070,                               // 20 popne {r4, r5, r6, pc}
            0xE5966018,                               // 24 ldr r6, [r6, #0x18]  the heap
            0xE1A00006,                               // 28 mov r0, r6
            Branch(0xE, s + 0x2C, 0x12BB54, true),    // 2C bl its parent
            0xE1A01000,                               // 30 mov r1, r0
            0xE3A00034,                               // 34 mov r0, #0x34
            Branch(0xE, s + 0x38, 0x123480, true),    // 38 bl the allocation
            0xE1B04000,                               // 3C movs r4, r0
            Branch(0x0, s + 0x40, s + 0x80, false),   // 40 beq 80
            0xE1A00006,                               // 44 mov r0, r6
            Branch(0xE, s + 0x48, 0x12BB54, true),    // 48 bl its parent
            0xE1A01000,                               // 4C mov r1, r0
            0xE1A00004,                               // 50 mov r0, r4
            Branch(0xE, s + 0x54, 0x109434, true),    // 54 bl the event's base constructor
            0xE1A04000,                               // 58 mov r4, r0
            0xE59F1034,                               // 5C ldr r1, [pc, #0x34]  the event's vtable
            0xE5841000,                               // 60 str r1, [r4]
            0xE3A02000,                               // 64 mov r2, #0
            0xE5842018,                               // 68 str r2, [r4, #0x18]  its state
            0xE584201C,                               // 6C str r2, [r4, #0x1c]
            0xE5842020,                               // 70 str r2, [r4, #0x20]  its effect
            0xE3E01000,                               // 74 mvn r1, #0
            0xE5841024,                               // 78 str r1, [r4, #0x24]
            0xE5842030,                               // 7C str r2, [r4, #0x30]  its battle
            0xE1A00006,                               // 80 mov r0, r6
            0xE1A01004,                               // 84 mov r1, r4
            Branch(0xE, s + 0x88, 0x108CB0, true),    // 88 bl the event pushed
            0xE584501C,                               // 8C str r5, [r4, #0x1c]  the character, as X's
            0xE1A00004,                               // 90 mov r0, r4
            0xE8BD8070,                               // 94 pop {r4, r5, r6, pc}
            eventVtable,                              // 98
        };
        static_assert(sizeof code / 4 == 39, "the step routine's size");
        out.insert(out.end(), std::begin(code), std::end(code));
    }

    // the blocks (every word to change placed once: `intended` and `changed` the same at the end)
    std::set<uint32_t> intended, changed;
    for (size_t k = 0; k < std::size(Blocks); k++)
    {
        ArmBlockPort p;
        p.From = Blocks[k].From;
        p.To = Blocks[k].To;
        for (const auto& [x, to] : CodeCalls) p.Calls[x] = to;
        for (const auto& [x, to] : ThumbCalls) p.Calls[x] = thumbStub.at(to);
        for (const auto& [x, to] : FieldCalls) p.Calls[x] = fieldStub.at(to);
        p.Calls[0x101FA5D8] = weatherGet;
        p.Calls[0x101FA5E0] = weatherSet;
        p.Calls[0x1027ED9C] = request;
        p.Calls[0x101FB410] = module;
        p.Calls[0x1029B1E4] = rockSmash;
        p.Calls[0x101F9CA8] = fieldStub.at(OrasStateObject); // X's base constructor, called by state 6 alone: Omega Ruby's whole one
        for (const Block& b : Blocks)
            for (uint32_t a = b.From; a < b.To; a += 4) p.Calls[a] = room(a);
        const auto word = [&](uint32_t a, uint32_t expected, uint32_t value) {
            room(a); // inside a copied block
            if (a >= p.From && a < p.To) { p.Words[a] = {expected, value}; changed.insert(a); }
            intended.insert(a);
        };
        for (size_t t = 0; t < std::size(UpdateTargets); t++) word(UpdateTable + 4 * (uint32_t)t, 0, room(UpdateTargets[t]));
        for (size_t t = 0; t < std::size(EffectTargets); t++) word(EffectTable + 4 * (uint32_t)t, 0, room(EffectTargets[t]));
        // the event: the map object's offsets; the effect allocated on the map's heap +4, as both games' twin events do (X
        // 0x102351C4 = 0x1026F164), for X's + 0x10, which no load of Omega Ruby's DllField reads (its word there points
        // into the linear heap; what it is there not read)
        word(0x102396B0, 0xE5951010, 0xE5951004);
        word(0x102396DC, 0xE5900158, 0xE5900164); // the player: + 0x158 -> + 0x164
        word(0x102396F8, 0xE5950198, 0xE59501A0); // the area module: + 0x198 -> + 0x1A0
        word(0x10239808, 0xE59000F4, 0xE5900104); // + 0xF4 -> + 0x104 (Rock Smash's first argument)
        word(0x1023986C, 0xE5900158, 0xE5900164); // the player
        word(0x1023990C, 0xE5900160, 0xE5900168); // + 0x160 -> + 0x168 (built in turn: X 0x10245D00 = 0x1027F7CC)
        // state 6: Omega Ruby's 0x48-byte object (its constructor 0x1026BCD0 for X's base constructor, vtable and fields)
        word(0x10239990, 0xE3A0004C, 0xE3A00048);
        word(0x102399BC, 0xE59F1120, Nop);
        word(0x102399C4, 0xE5801000, Nop);
        word(0x102399C8, 0xE5C08018, Nop);
        word(0x102399CC, 0xE5C08034, Nop);
        word(0x102399D0, 0xE5C0803C, Nop);
        word(0x102399D4, 0xE5808044, Nop);
        word(0x102399E8, 0xE3A00016, 0xE3A00000 | Immediate(XyDustCloudBattleTypeNumber)); // its transition: X/Y's type 0x16
        word(0x102399F4, 0xE5C52018, Nop);        // X's +0x18 byte, which Omega Ruby's object has not
        word(0x102399F8, 0xE285301C, 0xE2853018); // the six words: + 0x1C -> + 0x18
        word(0x10239A08, 0xE5C52034, 0xE5C52030);
        word(0x10239A0C, 0xE5C50035, 0xE5C50031);
        word(0x10239A10, 0xE5852044, 0xE5852040);
        // the record (0x77, Sweet Scent encounters in both games' one list): the records + 0x1B814 -> + 0x1C8D4
        word(0x10239AC4, 0xE2800B6E, 0xE2800B72);
        word(0x10239AC8, 0xE2800014, 0xE28000D4);
        word(0x10239B50, 0, eventVtable);
        word(0x10239B90, 0, eventVtable);
        // the effect: its vtable, its parts' constructor and destructor, the resource's size
        for (const uint32_t a : {0x10250DE8u, 0x10250E24u, 0x10250E60u}) word(a, 0, effectVtable);
        word(0x10250DEC, 0, 0x395C80);
        for (const uint32_t a : {0x10250E28u, 0x10250E64u}) word(a, 0, 0x395D2C);
        word(0x10250DAC, 0xE3A0003C, 0xE3A00040);
        word(0x10250D9C, 0xE5100010, Branch(0xE, room(0x10250D9C), heaps, true)); // the heaps (r5, r6) from the routine
        word(0x10250DA0, 0xE5B06004, Nop);
        word(0x10250DA4, 0xE590500C, Nop);
        // the effect's update: the map object's offsets, its archive, its members, its sound
        word(0x10277338, 0xE59000F0, 0xE5900100); // + 0xF0 -> + 0x100
        word(0x1027737C, 0xE3A010AF, 0xE3A01000 | Immediate(archive));
        word(0x102773C0, 0xE590202C, 0xE5902038); // + 0x2C -> + 0x38
        word(0x10277434, 0xE5913060, 0xE3A03002); // X's word at its data 0x102EB72C, 2
        word(0x1027744C, 0xE591102C, 0xE5911038); // + 0x2C -> + 0x38
        word(0x10277770, 0, desc);
        if (Word(xField, 0x10277784 - XFieldBase) != 0xA012A) throw FormatError("X's dust cloud effect: not its sound 0xA012A");
        word(0x10277784, 0xA012A, sound(0xA012A));
        placed(blockAt[k]);
        const std::vector<uint32_t> w = PortArmBlock(xField, XFieldBase, p, blockAt[k]);
        out.insert(out.end(), w.begin(), w.end());
    }

    if (intended != changed) throw FormatError("X's dust cloud code: a word to change outside the copied blocks");
    // the data: the event's vtable (X's six methods; the offset to the top and the type info before, Omega Ruby's
    // encounter event's), the effect's (its two destructors), X's member list of the resource (4 pairs)
    const uint32_t typeInfo = Word(orasCode, EncounterEventVtable - 4 - CodeBase);
    out.insert(out.end(), {0u, typeInfo, room(0x10239B54), room(0x10239B0C), room(0x10239668), room(0x1023969C), room(0x10239704), room(0x10239670)});
    out.insert(out.end(), {0u, typeInfo, room(0x10250E2C), room(0x10250DF0)});
    out.push_back(Word(xField, 0x102EB7C8 - XFieldBase));
    out.push_back(Word(xField, 0x102EB7CC - XFieldBase));
    if (at + 4 * (uint32_t)out.size() != next) throw FormatError("X's dust cloud code: its layout and words differ");
    for (const uint32_t w : out) Put(r.Room, w);
    r.Code.push_back({XyDustCloudPointer, 0, step, "X/Y's dust clouds: the step routine's address"});
    return r;
}

std::vector<CodeWord> XyDustCloudFieldPatchAt(uint32_t cave)
{
    // Omega Ruby's step check (0x10263014) at its `ldr r0, [fp, #0x1c]` past the guard: the room's routine (the character
    // r5, the field fp) returns the event (out through the function's exit, as X's branch returns it) or 0 (back)
    cave = (cave + 3) & ~3u;
    std::vector<CodeWord> w;
    w.push_back({StepCheckHook, 0xE59B001C, Branch(0xE, StepCheckHook, cave, false), "the step check: to X/Y's dust clouds' cave"});
    const uint32_t code[] = {
        0xE1A00005,                                     // 00 mov r0, r5
        0xE1A0100B,                                     // 04 mov r1, fp
        0xE59FC014,                                     // 08 ldr ip, [pc, #0x14]   the room word
        0xE59CC000,                                     // 0C ldr ip, [ip]
        0xE12FFF3C,                                     // 10 blx ip
        0xE3500000,                                     // 14 cmp r0, #0
        Branch(0x1, cave + 0x18, StepCheckExit, false), // 18 bne the exit (r0 the event)
        0xE59B001C,                                     // 1C ldr r0, [fp, #0x1c]   the game's word
        Branch(0xE, cave + 0x20, StepCheckBack, false), // 20 b back
        XyDustCloudPointer,                             // 24
    };
    for (uint32_t k = 0; k < std::size(code); k++) w.push_back({cave + 4 * k, 0, code[k], "X/Y's dust clouds' step hook (cave)"});
    return w;
}

}
