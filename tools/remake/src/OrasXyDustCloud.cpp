#include "OrasXyDustCloud.h"

#include "OrasXyDustBox.h"
#include "OrasXyMoveCodes.h"
#include "Bch.h"
#include "BchUpgrade.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"

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

// X's dust cloud move codes 0x38-0x3A (MoveCodeTsutikemuri*, the mod's 0x3F-0x41: OrasXyMoveCodes.h), their 7 methods
// each (X's vtables 0x103013B4, 0x103013FC, 0x10300D54) and the routines they share, copied (read 25 October). The
// method [3] (the start) hides the cloud's character (X's command 0x2C, VanishON) and sets the work's counters; [4] (the
// update) of 0x38/0x39 walks it a step when the player walks by (X 0x1025B01C), of 0x3A turns it toward the player in its
// range (the range widened by one around a check, X's 0x102DB17C); [5]/[6] remove and make its small swirl effect.
constexpr Block ClassBlocks[] = {
    {0x1024FC88, 0x1024FC90}, // 0x3A's destructors
    {0x10251088, 0x102510A0}, // 0x38's and 0x39's destructors
    {0x1025B014, 0x1025B0C0}, // 0x38's update, its walk a step (X's 0x1025B01C)
    {0x1025B228, 0x1025B38C}, // the swirl made (0x1025B230), removed (0x1025B29C), the start (0x1025B30C)
    {0x102DB160, 0x102DB3E8}, // 0x3A's start, update and code
    {0x102DCCE8, 0x102DCDD0}, // 0x38's and 0x39's start, methods and codes
};
constexpr uint32_t ClassVtables[3][7] = {
    {0x1025108C, 0x10251088, 0x102DCD24, 0x102DCCE8, 0x1025B014, 0x102DCD14, 0x102DCD1C},
    {0x1025109C, 0x10251098, 0x102DCDC8, 0x102DCD84, 0x102DCDB0, 0x102DCDB8, 0x102DCDC0},
    {0x1024FC8C, 0x1024FC88, 0x102DB3E0, 0x102DB160, 0x102DB17C, 0x1025B294, 0x1025B228},
};
constexpr uint32_t ClassDirections = 0x102DB3DC; // the literal naming X's data 0x102EB720: the four directions 0, 1, 2, 3
// X's calls out of the class blocks -> Omega Ruby's same .code functions (X's DllField stub -> X's .code; Omega Ruby's twin
// read side by side, the character's fields +0x4E-+0x60 the same in both: X 0x4AA680/0x3D3488 = 0x4E32D4/0x3F48F8)
const std::map<uint32_t, uint32_t> ClassCodeCalls = {
    {0x101F91C8, 0x2FF8C0}, // the heap's delete
    {0x101F9200, 0x14EA64}, // an assert, empty in both
    {0x101FA098, 0x4DBDBC}, // a character's direction
    {0x101FA1D0, 0x4DBDF4}, // a character's position set
    {0x101FA238, 0x4DD0E0}, // a character's step checked
    {0x101FA2E8, 0x48AD84}, // Util_RandomBelow
    {0x101FA3B8, 0x3FAE50}, // SetCommand (the command numbers translated below)
    {0x101FC1B0, 0x4E394C}, // a character's flag (X's 0x4AAB48)
    {0x101FC1B8, 0x4E39C0}, // a character's flag cleared (X's 0x4AABBC)
    {0x101FC1C0, 0x4DBE10}, // a character's direction (X's 0x4A3FF4)
    {0x101FC1D8, 0x4E32E4}, // a character's + 0x5E
    {0x101FC1E0, 0x4E32EC}, // its + 0x60
    {0x101FC1E8, 0x3F4908}, // + 0x5E set
    {0x101FC1F0, 0x3F4910}, // + 0x60 set
    {0x101FE630, 0x4DDF48}, // a position in the character's range (the same code, the + 0x5A-+0x60 range)
};
// X's DllField functions the class blocks call -> Omega Ruby's (through the getter word)
const std::map<uint32_t, uint32_t> ClassFieldCalls = {
    {0x10272BA4, 0x102C3F74}, // a step's cell checked (Omega Ruby's twin of X's 0x1025B0D0, 0x102A824C, calls it there)
    {0x1027AB84, 0x102CC730}, // a position -> its grid cell
    {0x102832E4, 0x102D1644}, // a grid position
};
// X's .code accessors Omega Ruby has not (its characters' + 0x5A, + 0x5C, read in Omega Ruby's 0x4DDF48 as in X's), and
// the swirl's effect (below), as thunks of the room
constexpr uint32_t ClassThunks[][2] = {
    {0x101FE668, 0xE1D005FA}, // ldrsh r0, [r0, #0x5a]
    {0x101FE670, 0xE1D005FC}, // ldrsh r0, [r0, #0x5c]
    {0x101FE678, 0xE1C015BA}, // strh r1, [r0, #0x5a]
    {0x101FE680, 0xE1C015BC}, // strh r1, [r0, #0x5c]
};

// the clouds' small swirl: X's effect kind 0x1F (field::mmodel::FieldMoveModelEffectTsuchikemuriEncount, read live in X's
// effect manager, run xr3: the 9 records of kind 0x1F are Route 13's clouds), a kind Omega Ruby has not (its 0x29 kinds,
// X's 0x2B). Its methods [10]-[13], [15] and destructors copied, its base's (field::mmodel::FieldMoveModelEffect, the same
// code in both: X's splash and slope dust vtables against Omega Ruby's) Omega Ruby's. Its resources, X's effect pack ids
// 0xBD (the model), 0xBE and 0xBF (its animations), are X's a/0/6/6 "TM" entries 0xBD-0xBF given Omega Ruby's a/0/6/4 at
// the same numbers (XyDustCloudEffectsArchive), the pack grown to them (XyDustCloudFieldPatchAt)
constexpr Block SwirlBlocks[] = {
    {0x10251228, 0x10251230}, // its destructors
    {0x102DCFE0, 0x102DD23C}, // its resource ids [12], model id [11], count [10], placement [15], update [13]
};
constexpr uint32_t SwirlIds = 0x102DCFEC;    // the literal naming X's data 0x102EB7E8: the animations' ids
constexpr uint32_t SwirlIdsData = 0x102EB7E8;
// its vtable: X's methods (in the blocks) or Omega Ruby's base's (0: X's own; Omega Ruby's FieldMoveModelEffect methods,
// X's base's twins one for one: 0x102DC694 = 0x10325B8C ... 0x102DC69C = 0x10325B94)
constexpr uint32_t SwirlVtable[17][2] = {
    {0x1025122C, 0}, {0x10251228, 0}, {0, 0x10325B8C}, {0, 0x10325BA4}, {0, 0x10325AC0}, {0, 0x10325B9C}, {0, 0x103259CC},
    {0, 0x1028770C}, {0, 0x10287714}, {0, 0x10287718}, {0x102DCFF8, 0}, {0x102DCFF0, 0}, {0x102DCFE0, 0}, {0x102DD0F4, 0},
    {0, 0x10325BA0}, {0x102DD000, 0}, {0, 0x10325B94},
};
constexpr uint32_t EffectCreate = 0x102D25A0, EffectRemove = 0x102CFB74; // X's 0x10284434, 0x10282C9C (the same code)
const std::map<uint32_t, uint32_t> SwirlCodeCalls = {
    {0x101F91C8, 0x2FF8C0}, // the heap's delete
    {0x101F91F8, 0x44FA8C}, // a sound played (same)
    {0x101F9BD8, 0x44C710}, // a sound playing (same)
    {0x101FA1B0, 0x4DC690}, // a character's position (same)
    {0x101FAF00, 0x4C9C58}, // a box seen by the camera (same)
    {0x101FA3A8, 0x3964E8}, // a model's position set (X's model + 0x138, Omega Ruby's + 0x130: the same code beside it)
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
    for (const uint32_t to : {FieldCalls.at(0x1027DE54), FieldCalls.at(0x1027F5CC), OrasStateObject, InitContext, SelectTable, ClearContext,
                              ClassFieldCalls.at(0x10272BA4), ClassFieldCalls.at(0x1027AB84), ClassFieldCalls.at(0x102832E4),
                              EffectCreate, EffectRemove, 0x10325B8Cu, 0x10325BA4u, 0x10325AC0u, 0x10325B9Cu, 0x103259CCu, 0x1028770Cu,
                              0x10287714u, 0x10287718u, 0x10325BA0u, 0x10325B94u})
    {
        fieldStub[to] = take(28);
        fieldOrder.push_back(to);
    }
    const uint32_t weatherGet = take(8), weatherSet = take(4), request = take(8), module = take(20), heaps = take(32);
    const uint32_t encounter = take(EncounterTo - EncounterFrom), rockSmash = take(RockSmashTo - RockSmashFrom), step = take(39 * 4);
    std::vector<uint32_t> blockAt;
    for (const Block& b : Blocks) blockAt.push_back(take(b.To - b.From));
    const uint32_t eventVtable = take(8 + 6 * 4) + 8, effectVtable = take(8 + 2 * 4) + 8, desc = take(8);
    std::map<uint32_t, uint32_t> classThunk;
    for (const auto& t : ClassThunks) classThunk[t[0]] = take(8);
    const uint32_t swirlCreate = take(16 * 4);
    std::vector<uint32_t> classAt;
    for (const Block& b : ClassBlocks) classAt.push_back(take(b.To - b.From));
    const uint32_t directions = take(4);
    uint32_t classVtable[3];
    for (uint32_t& v : classVtable) v = take(8 + 7 * 4) + 8;
    std::vector<uint32_t> swirlAt;
    for (const Block& b : SwirlBlocks) swirlAt.push_back(take(b.To - b.From));
    const uint32_t swirlIds = take(16), swirlVtable = take(8 + 17 * 4) + 8, swirlKind = take(4);
    const auto swirlRoom = [&](uint32_t x) -> uint32_t {
        for (size_t k = 0; k < std::size(SwirlBlocks); k++)
            if (x >= SwirlBlocks[k].From && x < SwirlBlocks[k].To) return swirlAt[k] + (x - SwirlBlocks[k].From);
        char m[64];
        snprintf(m, sizeof m, "X's dust cloud swirl: 0x%X is in no copied block", x);
        throw FormatError(m);
    };
    const auto classRoom = [&](uint32_t x) -> uint32_t {
        for (size_t k = 0; k < std::size(ClassBlocks); k++)
            if (x >= ClassBlocks[k].From && x < ClassBlocks[k].To) return classAt[k] + (x - ClassBlocks[k].From);
        char m[64];
        snprintf(m, sizeof m, "X's dust cloud classes: 0x%X is in no copied block", x);
        throw FormatError(m);
    };
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

    // the move codes' classes: the thunks, the swirl's record made, the blocks, X's four directions, the vtables
    for (const auto& t : ClassThunks) { placed(classThunk.at(t[0])); out.insert(out.end(), {t[1], 0xE12FFF1E}); }
    // X's 0x10284434 (the effect manager, X's kind, the character -> the record's handle or 0xFFFF): Omega Ruby's same
    // function given a kind past its 0x29 (its kind 0's object, no check), the record then given X's kind's object (+0xC,
    // the only word of the record the kind gives; records of 0x57 words at the manager's + 0xD8, X's + 0xE0)
    placed(swirlCreate);
    out.insert(out.end(), {
        0xE92D4010,                                                       // 00 push {r4, lr}
        0xE1A04000,                                                       // 04 mov r4, r0
        0xE3A01029,                                                       // 08 mov r1, #0x29
        Branch(0xE, swirlCreate + 0x0C, fieldStub.at(EffectCreate), true), // 0C bl the record made
        0xE59F1020,                                                       // 10 ldr r1, [pc, #0x20]  0xFFFF
        0xE1500001,                                                       // 14 cmp r0, r1
        0x08BD8010,                                                       // 18 popeq {r4, pc}
        0xE3A03F57,                                                       // 1C mov r3, #0x15c
        0xE0020390,                                                       // 20 mul r2, r0, r3
        0xE59410D8,                                                       // 24 ldr r1, [r4, #0xd8]  the records
        0xE59F300C,                                                       // 28 ldr r3, [pc, #0xc]   X's kind's object
        0xE0811002,                                                       // 2C add r1, r1, r2
        0xE581300C,                                                       // 30 str r3, [r1, #0xc]
        0xE8BD8010,                                                       // 34 pop {r4, pc}
        0x0000FFFF,                                                       // 38
        swirlKind,                                                        // 3C
    });
    for (size_t k = 0; k < std::size(ClassBlocks); k++)
    {
        ArmBlockPort p;
        p.From = ClassBlocks[k].From;
        p.To = ClassBlocks[k].To;
        for (const auto& [x, to] : ClassCodeCalls) p.Calls[x] = to;
        for (const auto& [x, to] : ClassFieldCalls) p.Calls[x] = fieldStub.at(to);
        for (const auto& [x, to] : classThunk) p.Calls[x] = to;
        p.Calls[0x10284434] = swirlCreate;                  // the swirl's record made (above)
        p.Calls[0x10282C9C] = fieldStub.at(EffectRemove);   // and removed
        for (const Block& b : ClassBlocks)
            for (uint32_t a = b.From; a < b.To; a += 4) p.Calls[a] = classRoom(a);
        const auto word = [&](uint32_t a, uint32_t expected, uint32_t value) {
            classRoom(a);
            if (a >= p.From && a < p.To) p.Words[a] = {expected, value};
        };
        // a character's manager + 0x8AC -> + 0x8B4, the manager's map + 0x14 -> + 0x10, the map's player + 0x158 -> + 0x164
        // (as the event's above and XyRoute13's)
        for (const uint32_t a : {0x1025B044u, 0x102DB210u, 0x102DB26Cu, 0x102DB2C8u}) word(a, 0xE59508AC, 0xE59508B4);
        word(0x1025B24C, 0xE59258AC, 0xE59258B4);
        word(0x1025B2C0, 0xE59068AC, 0xE59068B4);
        word(0x1025B340, 0xE59678AC, 0xE59678B4);
        word(0x102DCD54, 0xE59608AC, 0xE59608B4);
        for (const uint32_t a : {0x102DCD68u, 0x102DCD74u}) word(a, 0xE59108AC, 0xE59108B4);
        for (const uint32_t a : {0x1025B048u, 0x102DB214u, 0x102DB274u}) word(a, 0xE5900014, 0xE5900010);
        for (const uint32_t a : {0x1025B04Cu, 0x102DB218u, 0x102DB27Cu, 0x102DB364u}) word(a, 0xE5900158, 0xE5900164);
        word(0x102DB2CC, 0xE590201C, 0xE5902018); // the character manager's + 0x1C -> + 0x18 (XyRoute13's)
        // the player's position, its + 0xC64 -> + 0x6F8: the player (field::PlayerGrid) read live in both games' heaps (X
        // run xr3 0x082BF3E4, Omega Ruby run or1 0x082C8030), its part at X's + 0xC04 (a base's vtable) at Omega Ruby's
        // + 0x698, the same words from there (the position + 0x60 into it)
        word(0x102DB228, 0xE2801B03, 0xE2801C06); // add r1, r0, #0x600
        word(0x102DB22C, 0xE2811064, 0xE28110F8); // add r1, r1, #0xf8
        word(0x102DB280, 0xE2804B03, 0xE2804C06); // add r4, r0, #0x600
        word(0x102DB288, 0xE2844064, 0xE28440F8); // add r4, r4, #0xf8
        // the effect manager: the character manager's + 0x14B4 -> + 0x36C0 (Omega Ruby's callers of its 0x102D25A0 read it
        // there, 0x10282868, 0x10287474)
        for (const uint32_t a : {0x1025B290u, 0x1025B308u, 0x1025B388u}) word(a, 0x14B4, 0x36C0);
        // X's action commands -> Omega Ruby's same-named ones (ORAS_ENGINE.md 0.0, "Action commands"): Walk_7F 0x12 ->
        // 0x13, Wait8F 0x28 -> 0x2C, VanishON 0x2C -> 0x30
        word(0x1025B0A8, 0xE3A01012, 0xE3A01013);
        word(0x102DB3CC, 0xE3A01012, 0xE3A01013);
        word(0x102DB2C0, 0xE3A01028, 0xE3A0102C);
        word(0x1025B334, 0xE3A0102C, 0xE3A01030);
        // each class's code (its method [2]): the mod's numbers
        word(0x102DCD24, 0xE3A00038, 0xE3A00000u | (uint32_t)XyMoveCodeToOras(0x38));
        word(0x102DCDC8, 0xE3A00039, 0xE3A00000u | (uint32_t)XyMoveCodeToOras(0x39));
        word(0x102DB3E0, 0xE3A0003A, 0xE3A00000u | (uint32_t)XyMoveCodeToOras(0x3A));
        word(ClassDirections, 0, directions);
        placed(classAt[k]);
        const std::vector<uint32_t> w = PortArmBlock(xField, XFieldBase, p, classAt[k]);
        out.insert(out.end(), w.begin(), w.end());
    }
    placed(directions);
    out.push_back(Word(xField, 0x102EB720 - XFieldBase));
    if (out.back() != 0x03020100) throw FormatError("X's dust cloud classes: not X's four directions at 0x102EB720");
    for (size_t c = 0; c < 3; c++)
    {
        placed(classVtable[c] - 8);
        out.insert(out.end(), {0u, 0u}); // the offset to the top, no type info (a DllField address would not be relocated)
        for (const uint32_t m : ClassVtables[c]) out.push_back(classRoom(m));
        r.Vtables.push_back(classVtable[c]);
    }

    // the swirl: its blocks, X's animation ids, its vtable, its kind's object (a vtable word, as the game's kinds are)
    for (size_t k = 0; k < std::size(SwirlBlocks); k++)
    {
        ArmBlockPort p;
        p.From = SwirlBlocks[k].From;
        p.To = SwirlBlocks[k].To;
        for (const auto& [x, to] : SwirlCodeCalls) p.Calls[x] = to;
        const auto word = [&](uint32_t a, uint32_t expected, uint32_t value) {
            swirlRoom(a);
            if (a >= p.From && a < p.To) p.Words[a] = {expected, value};
        };
        word(SwirlIds, 0, swirlIds);
        // the character's + 0x87F -> + 0x887 (both bases' starts read it, 0x102DC6EC / 0x10325BE8), its manager + 0x8AC ->
        // + 0x8B4, the manager's map + 0x14 -> + 0x10, the map's + 0xF0 -> + 0x100, its camera + 0xAB8 -> + 0xAA4 (that
        // object's four fields X reads, + 0xAB4 0xAB8 0xAF8 0xAFC, Omega Ruby's + 0xAA0 0xAA4 0xAE4 0xAE8: the same code at
        // X's + 0xAB4 and Omega Ruby's + 0xAA0, 0x10240004 / 0x102797BC), the map's player + 0x158 -> + 0x164
        word(0x102DD014, 0xE5D0187F, 0xE5D01887);
        word(0x102DD020, 0xE59008AC, 0xE59008B4);
        word(0x102DD024, 0xE5900014, 0xE5900010);
        word(0x102DD028, 0xE59000F0, 0xE5900100);
        word(0x102DD02C, 0xE5900AB8, 0xE5900AA4);
        word(0x102DD10C, 0xE59008AC, 0xE59008B4);
        word(0x102DD110, 0xE5900014, 0xE5900010);
        word(0x102DD114, 0xE5900158, 0xE5900164);
        // the model's (xy_system::H3dShaderModel, both games') animation frame: its virtual + 0xB8 -> + 0xD0 (the same
        // code, the vtables X 0x59B278 and Omega Ruby 0x5E0F9C side by side: from + 0xB0 on 0x18 further; + 0x40 the same)
        word(0x102DD1C0, 0xE59120B8, 0xE59120D0);
        if (Word(xField, 0x102DD238 - XFieldBase) != 0xA00C7) throw FormatError("X's dust cloud swirl: not its sound 0xA00C7");
        word(0x102DD238, 0xA00C7, sound(0xA00C7));
        placed(swirlAt[k]);
        const std::vector<uint32_t> w = PortArmBlock(xField, XFieldBase, p, swirlAt[k]);
        out.insert(out.end(), w.begin(), w.end());
    }
    placed(swirlIds);
    for (uint32_t k = 0; k < 4; k++) out.push_back(Word(xField, SwirlIdsData + 4 * k - XFieldBase));
    if (out[out.size() - 4] != 0xBE || out[out.size() - 3] != 0xBF) throw FormatError("X's dust cloud swirl: not its animation ids 0xBE, 0xBF");
    placed(swirlVtable - 8);
    out.insert(out.end(), {0u, 0u});
    for (const auto& m : SwirlVtable) out.push_back(m[0] ? swirlRoom(m[0]) : fieldStub.at(m[1]));
    placed(swirlKind);
    out.push_back(swirlVtable);
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
    // the field's effect pack (its builder 0x1026D628, its lookup 0x102AA3A8): ids 8-0xAC (0xA5 entries of 0x4C bytes, the
    // TM a/0/6/4's entries at their own numbers) -> 8-0xBF, for X's swirl's 0xBD-0xBF (XyDustCloudEffectsArchive)
    w.push_back({0x1026D680, 0xE3A040A5, 0xE3A04000 | XyEffectPackEntries, "the effect pack: its entries, 0xA5 -> 0xB8"});
    w.push_back({0x1026D7D4, 0x3104, 8 + 0x4C * XyEffectPackEntries, "the effect pack: its entries' allocation"});
    w.push_back({0x102AA3AC, 0xE35200A4, 0xE3520000 | (XyEffectPackEntries - 1), "the effect pack's lookup: its last id"});
    // its builder skips Omega Ruby's own entries past the game's (0xAD-0xBC: 7 models, 4 visibility, 2 skeletal and 2
    // material animations the game never loads as effects): loaded, they left two of the scene's animations without a
    // resource, read near address 0 every frame on X/Y's zones (PCs 0x38A28C, 0x4CCDF4: about 5,000 reads a run; bisected
    // 26 October, runs sc1x-sc3x: none without the pack grown). Their entries stay empty, as the lookup found them before
    // (+ 0x3C 0: none). The loop's `add r0, r4, #8` (the id) calls the routine, which goes on to the next entry for these
    constexpr uint32_t BuilderId = 0x1026D6E8, BuilderNext = 0x1026D79C;
    constexpr uint32_t SkipFirst = 8 + 0xA5, SkipLast = XySwirlFirstId - 1;
    static_assert(SkipLast - SkipFirst < 0x100, "the routine compares with an 8-bit immediate");
    const uint32_t skip = cave + 4 * (uint32_t)std::size(code);
    w.push_back({BuilderId, 0xE2840008, Branch(0xE, BuilderId, skip, true), "the effect pack's builder: Omega Ruby's ids 0xAD-0xBC skipped"});
    const uint32_t skipCode[] = {
        0xE2840008,                                     // 00 add r0, r4, #8        the id (the game's word)
        0xE240C000 | SkipFirst,                         // 04 sub ip, r0, #0xAD
        0xE35C0000 | (SkipLast - SkipFirst),            // 08 cmp ip, #0xF
        0x812FFF1E,                                     // 0C bxhi lr               another id: loaded
        Branch(0xE, skip + 0x10, BuilderNext, false),   // 10 b the next entry
    };
    for (uint32_t k = 0; k < std::size(skipCode); k++) w.push_back({skip + 4 * k, 0, skipCode[k], "the effect pack's skipped ids (cave)"});
    return w;
}

Bytes XyDustCloudEffectsArchive(const Bytes& orasGarc, const Bytes& xGarc)
{
    // both games' archives hold one TM of the field's effect files (X's a/0/6/6: 204, Omega Ruby's a/0/6/4: 189), the
    // effect pack's id the entry's number (the swirl's three BCH found in X's by their bytes as X loaded them, run xd2)
    Garc o(orasGarc);
    const Garc x(xGarc);
    const auto plain = [](const Bytes& m) { return !m.empty() && m[0] == 0x11 ? LzDecompress(m) : m; };
    const bool compressed = !o.Sub(0).empty() && o.Sub(0)[0] == 0x11;
    BinLinker ot = BinLinker::Read(plain(o.Sub(0)), "TM");
    const BinLinker xt = BinLinker::Read(plain(x.Sub(0)), "TM");
    if (o.Count() != 1 || ot.Files.size() != XySwirlFirstId || xt.Files.size() != 204)
        throw FormatError("the effects' archives: not Omega Ruby's 189 files and X's 204");
    for (uint32_t id = XySwirlFirstId; id < XySwirlFirstId + 3; id++)
    {
        Bytes f = xt.Files.at(id);
        if (!Bch::Is(f) || f[4] != 0x07) throw FormatError("the effects' archives: X's swirl file not a BCH 0x07");
        ot.Files.push_back(BchUpgradeTo21(f));
    }
    const Bytes tm = ot.Write();
    o.Set(0, compressed ? Lz11Compress(tm) : tm);
    return o.Write();
}

}
