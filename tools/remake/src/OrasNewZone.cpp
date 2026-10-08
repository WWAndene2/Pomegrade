#include "OrasNewZone.h"
#include "BinLinker.h"

namespace remake
{

std::array<uint16_t, 28> NewZoneHeader(const NewZoneFields& f)
{
    if (f.Number < 0 || f.AreaPack < 0 || f.Matrix < 0 || f.Text < 0)
        throw FormatError("new zone: its number, area pack, matrix and text file are required");
    const bool inside = f.Kind == NewZoneKind::Interior;
    if (inside && f.Overworld < 0) throw FormatError("new zone " + std::to_string(f.Number) + ": an interior belongs to an outdoor zone");
    if (f.NameLine > 0x3FF) throw FormatError("new zone " + std::to_string(f.Number) + ": a place name line past 10 bits");
    std::array<uint16_t, 28> h{};
    h[0] = inside ? 3 : 0;
    h[1] = (uint16_t)f.AreaPack;
    h[2] = (uint16_t)f.Matrix;
    h[3] = (uint16_t)f.Text;
    h[4] = (uint16_t)(f.Music >= 0 ? f.Music : inside ? 65 : 5);
    h[5] = 1;
    const uint16_t constant[6] = {5, 1, 5, 1, 14, 1}; // words 6-11, the same in every zone
    for (int k = 0; k < 6; k++) h[6 + k] = constant[k];
    h[12] = (uint16_t)f.Number; // unread; past the game's 0-537 for a new zone
    h[13] = (uint16_t)(inside ? f.Overworld : f.Number);
    // the name, and the 6 bits above it as the reference headers hold them (1 outdoors, 0 inside)
    h[14] = (uint16_t)((f.NameLine >= 0 ? f.NameLine : 0) | (inside ? 0 : 1 << 10));
    // words 15, 16, 20, 21 (camera mode, location type and flags): the reference headers', zone 6's outdoors and zone 223's inside
    h[15] = inside ? 0x0389 : 0x04E0;
    h[16] = inside ? 0x4802 : 0x6C01;
    h[19] = 0xFFFF;
    h[20] = inside ? 0x4824 : 0x6814;
    h[21] = inside ? 0x1BE8 : 0x00C8;
    for (int at : {22, 25})
    {
        h[at] = (uint16_t)(f.SpawnX * 18);
        h[at + 1] = 2; // the height both reference headers hold
        h[at + 2] = (uint16_t)(f.SpawnZ * 18);
    }
    return h;
}

ZoneCharacter NewCharacter(int id, int model, int tileX, int tileZ, int facing, int script, int movement, int kind, int sight)
{
    ZoneCharacter c;
    c.Raw[0] = (uint16_t)id; c.Raw[1] = (uint16_t)model; c.Raw[2] = (uint16_t)movement; c.Raw[3] = (uint16_t)kind;
    c.Raw[5] = (uint16_t)script; c.Raw[6] = (uint16_t)facing; c.Raw[7] = (uint16_t)sight;
    c.Raw[12] = 1; c.Raw[13] = 1;
    c.Raw[14] = c.Raw[15] = c.Raw[16] = 0xFFFF;
    c.Raw[20] = (uint16_t)tileX; c.Raw[21] = (uint16_t)tileZ;
    return c;
}

static ZoneDoor Warp(int toZone, int toWarp, uint16_t kind, int pixelX, int pixelZ, int width, int height)
{
    ZoneDoor w;
    w.Raw[0] = (uint16_t)toZone; w.Raw[1] = (uint16_t)toWarp; w.Raw[2] = kind;
    w.Raw[4] = (uint16_t)pixelX; w.Raw[6] = (uint16_t)pixelZ; w.Raw[7] = (uint16_t)width; w.Raw[8] = (uint16_t)height;
    return w;
}

ZoneDoor NewDoorWarp(int toZone, int toWarp, int tileX, int tileZ) { return Warp(toZone, toWarp, 0x0301, tileX * 18 + 9, tileZ * 18 + 9, 1, 1); }
ZoneDoor NewExitWarp(int toZone, int toWarp, int pixelX, int pixelZ) { return Warp(toZone, toWarp, 0x0200, pixelX, pixelZ, 3, 1); }

const char* const EmptyZoneScript =
    "; a zone script that does nothing (OrasNewZone.h)\n"
    "pubvar g_ai_flag 8\npubvar g_interactive_flag 4\npubvar g_mode 0\n"
    "code\n    halt.p 0\nmain:\n    proc\n    zero.pri\n    retn\n"
    "data\n    cell -1 0 0\n";
const char* const EmptyInitScript = EmptyZoneScript;

Bytes WriteNewZone(const OrasZone& zone, const Bytes& encounters)
{
    if (zone.Script.empty() || zone.InitScript.empty()) throw FormatError("new zone: both scripts are required");
    BinLinker c;
    c.Tag = "ZO";
    Bytes h;
    for (uint16_t v : zone.Header) { h.push_back((uint8_t)v); h.push_back((uint8_t)(v >> 8)); }
    c.Files = {h, zone.EventsFile(), zone.Script, encounters, Bytes(12, 0)};
    const Bytes out = c.Write();
    const OrasZone back = OrasZone::Read(out);
    if (back.Header != zone.Header || back.Characters.size() != zone.Characters.size() || back.Doors.size() != zone.Doors.size()
        || back.InitScript != zone.InitScript)
        throw FormatError("new zone: does not read back as written");
    return out;
}

}
