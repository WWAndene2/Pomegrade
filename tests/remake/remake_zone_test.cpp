// The remake tooling's reading of an ORAS zone (OrasZone) and of the Pawn script header (AmxInfo), on a zone built here to the layout
// measured on the real file (536 zones, Littleroot's among them): header words, the four counted arrays and the fifth, the
// size rule, and the scripts after the arrays.
#include "Amx.h"
#include "BinLinker.h"
#include "OrasZone.h"
#include "synthetic_files.h"

#include <cstdio>
#include <string>

using namespace remake;
using namespace synthetic;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void P32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }
static void P16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }

// a minimal AMX: the 56-byte header with the standard fields, a public table, a native table, nothing else
static Bytes Amx(uint32_t publics, uint32_t natives)
{
    Bytes b;
    P32(b, 0); // size, set below
    P16(b, 0xF1E0); b.push_back(10); b.push_back(10); P16(b, 0x1C); P16(b, 8);
    const uint32_t pub = 56, nat = pub + 8 * publics, lib = nat + 8 * natives;
    P32(b, lib); P32(b, lib + 0x20); P32(b, lib + 0x30); P32(b, lib + 0x1030); P32(b, 0);
    P32(b, pub); P32(b, nat); P32(b, lib); P32(b, lib); P32(b, lib); P32(b, lib);
    while (b.size() < lib) b.push_back(0);
    const uint32_t size = (uint32_t)b.size();
    for (int i = 0; i < 4; i++) b[i] = (size >> (8 * i)) & 0xFF;
    return b;
}

static Bytes MakeZone(int furniture, int characters, int warps, int triggers, int fifth, bool breakSize = false)
{
    Bytes header;
    for (int i = 0; i < 28; i++) P16(header, i == 1 ? 8 : i == 2 ? 1 : i == 13 ? 6 : i == 22 ? 1809 : i == 24 ? 3105 : 0);
    Bytes events;
    const uint32_t arrays = 12 + furniture * 0x14 + characters * 0x30 + (warps + triggers + fifth) * 0x18;
    P32(events, arrays - 4 + (breakSize ? 8 : 0));
    events.push_back(furniture); events.push_back(characters); events.push_back(warps); events.push_back(triggers);
    P32(events, fifth);
    for (int i = 0; i < furniture; i++) { Bytes f(0x14, 0); f[8] = 106; f[10] = 179; events.insert(events.end(), f.begin(), f.end()); }
    for (int i = 0; i < characters; i++) { Bytes c(0x30, 0); c[0] = i; c[2] = 0x21; c[3] = 0x01; c[40] = 104; c[42] = 179; events.insert(events.end(), c.begin(), c.end()); }
    for (int i = 0; i < warps; i++) { Bytes w(0x18, 0); w[0] = 223; w[8] = 0xA5; w[9] = 0x06; w[12] = 0x0F; w[13] = 0x0C; events.insert(events.end(), w.begin(), w.end()); }
    for (int i = 0; i < triggers + fifth; i++) { Bytes t(0x18, 0); t[12] = 99; t[14] = 162; events.insert(events.end(), t.begin(), t.end()); }
    const Bytes init = Amx(0, 2);
    events.insert(events.end(), init.begin(), init.end());
    while (events.size() % 4) events.push_back(0);
    BinLinker zo;
    zo.Tag = "ZO";
    zo.Files = {header, events, Amx(1, 3), {}, Bytes(12, 0)};
    return zo.Write();
}

int main()
{
    const OrasZone z = OrasZone::Read(MakeZone(2, 3, 2, 1, 0));
    check(z.AreaPack() == 8 && z.Matrix() == 1 && z.Number() == 6, "the header's area pack, matrix and number");
    check(std::abs(z.SpawnTileX() - 100.5f) < 0.01f && std::abs(z.SpawnTileZ() - 172.5f) < 0.01f, "the spawn position, in tiles (a pixel is 1/18)");
    check(z.Furniture.size() == 2 && z.Characters.size() == 3 && z.Doors.size() == 2 && z.Triggers.size() == 1 && z.Others.empty(), "the four counted arrays");
    check(z.Furniture[0].TileX() == 106 && z.Furniture[0].TileZ() == 179, "a furniture's tile");
    check(z.Characters[2].Raw[0] == 2 && z.Characters[2].Model() == 289, "a character's index and model");
    check(z.Characters[1].TileX() == 104 && z.Characters[1].TileZ() == 179, "a character's tile");
    check(z.Doors[0].DestZone() == 223 && std::abs(z.Doors[0].TileX() - 1701.0f / 18) < 0.01f && std::abs(z.Doors[0].TileZ() - 3087.0f / 18) < 0.01f, "a warp's destination and position in tiles");
    check(z.Triggers[0].TileX() == 99 && z.Triggers[0].TileZ() == 162, "a trigger's tile");
    check(!z.InitScript.empty() && !z.Script.empty() && z.Trailer.size() == 12, "both scripts and the trailer kept");
    const AmxInfo a = AmxInfo::Read(z.Script);
    check(a.PublicCount() == 1 && a.NativeCount() == 3 && a.Compact() && a.Magic == 0xF1E0, "the script's header: publics, natives, packed");

    const OrasZone five = OrasZone::Read(MakeZone(0, 1, 1, 0, 3));
    check(five.Others.size() == 3 && five.Doors.size() == 1, "entries of the fifth kind are read, not mistaken for another array");

    bool refused = false;
    try { OrasZone::Read(MakeZone(1, 1, 1, 1, 0, true)); } catch (const FormatError&) { refused = true; }
    check(refused, "a size that does not match the arrays is refused");
    bool notScript = false;
    try { AmxInfo::Read(Bytes(64, 0)); } catch (const FormatError&) { notScript = true; }
    check(notScript, "bytes that are not a Pawn script are refused");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
