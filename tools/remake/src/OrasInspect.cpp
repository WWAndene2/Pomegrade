#include "OrasInspect.h"
#include "Amx.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasZone.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>

namespace remake
{

static Bytes Plain(const Bytes& d) { return IsLzCompressed(d) ? LzDecompress(d) : d; }

static std::string F(const char* format, ...) __attribute__((format(printf, 1, 2)));
static std::string F(const char* format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    return line;
}

static std::string Member(N3dsRom& game, const char* archive, size_t index, Bytes& out)
{
    const Garc g(game.Read(archive));
    if (index >= g.Count() || !g.Has(index)) return F("%s has no member %zu (it has %zu)\n", archive, index, g.Count());
    out = Plain(g.Sub(index));
    return "";
}

static std::string Script(const char* what, const Bytes& b)
{
    if (b.empty()) return F("  %s: none\n", what);
    try
    {
        const AmxInfo a = AmxInfo::Read(b);
        return F("  %s: Pawn script, %u bytes (version %u/%u, flags 0x%X%s), %u public(s), %u native call(s), %u bytes of code, %u of data\n", what, a.Size,
                 a.FileVersion, a.AmxVersion, a.Flags, a.Compact() ? ", packed" : "", a.PublicCount(), a.NativeCount(), a.CodeBytes(), a.DataBytes());
    }
    catch (const FormatError& e) { return F("  %s: %zu bytes, not a script: %s\n", what, b.size(), e.what()); }
}

std::string InspectZone(N3dsRom& game, size_t zone)
{
    Bytes data;
    std::string s = Member(game, "a/0/1/3", zone, data);
    if (!s.empty()) return s;
    const OrasZone z = OrasZone::Read(data);
    s += F("zone %zu (a/0/1/3 member %zu, %zu bytes decompressed)\n", zone, zone, data.size());
    s += F("  area pack %d (a/0/1/4), map matrix %d, own number %d, spawn tile (%.1f, %.1f)\n", z.AreaPack(), z.Matrix(), z.Number(), z.SpawnTileX(), z.SpawnTileZ());
    s += "  header words:";
    for (size_t i = 0; i < z.Header.size(); i++) s += F(" %u", z.Header[i]);
    s += "\n";
    s += F("  %zu furniture, %zu characters, %zu warps, %zu triggers, %zu of the fifth kind\n", z.Furniture.size(), z.Characters.size(), z.Doors.size(), z.Triggers.size(), z.Others.size());
    for (const ZoneFurniture& f : z.Furniture) s += F("    furniture at tile (%d, %d)\n", f.TileX(), f.TileZ());
    for (const ZoneCharacter& c : z.Characters) s += F("    character %u: model %d at tile (%d, %d)\n", c.Raw[0], c.Model(), c.TileX(), c.TileZ());
    for (const ZoneDoor& d : z.Doors) s += F("    warp to zone %d at tile (%.1f, %.1f)\n", d.DestZone(), d.TileX(), d.TileZ());
    for (const ZoneTrigger& t : z.Triggers) s += F("    trigger at tile (%d, %d)\n", t.TileX(), t.TileZ());
    s += Script("initialisation script", z.InitScript);
    s += Script("zone script", z.Script);
    return s;
}

std::string InspectPiece(N3dsRom& game, size_t piece)
{
    Bytes data;
    std::string s = Member(game, "a/0/3/9", piece, data);
    if (!s.empty()) return s;
    const BinLinker gr = BinLinker::Read(data, "GR");
    s += F("map piece %zu (a/0/3/9 member %zu, %zu bytes decompressed), %zu parts\n", piece, piece, data.size(), gr.Files.size());
    const Bytes& tiles = gr.Files.at(0);
    const unsigned w = U16(tiles, 0), h = U16(tiles, 2);
    s += F("  part 0: tile block %ux%u, %zu bytes (%zu after the tiles)\n", w, h, tiles.size(), tiles.size() - 4 - (size_t)w * h * 4);
    std::map<uint32_t, int> values;
    for (size_t i = 0; i < (size_t)w * h; i++) values[U32(tiles, 4 + i * 4)]++;
    for (const auto& [value, n] : values) s += F("    tile value 0x%08X: %d tiles\n", value, n);
    const Bch bch = Bch::Read(gr.Files.at(1));
    s += F("  part 1: terrain model, %zu bytes\n", gr.Files[1].size());
    for (const BchModel& m : bch.Models)
    {
        size_t vertices = 0, triangles = 0;
        for (const BchMesh& mesh : m.Meshes) { vertices += mesh.Vertices.size(); triangles += mesh.Triangles.size() / 3; }
        s += F("    model '%s': %zu meshes, %zu vertices, %zu triangles\n", m.Name.c_str(), m.Meshes.size(), vertices, triangles);
        for (size_t i = 0; i < m.Meshes.size(); i++)
        {
            const BchMesh& mesh = m.Meshes[i];
            const BchMaterial& mat = m.Materials[mesh.Material];
            s += F("      mesh %2zu: material %-24s texture %-22s%s%s  %5zu vertices, %5zu triangles\n", i, mat.Name.c_str(), mat.Texture[0].c_str(),
                   mat.Texture[1].empty() ? "" : " + ", mat.Texture[1].c_str(), mesh.Vertices.size(), mesh.Triangles.size() / 3);
        }
    }
    static const char* known[] = {"tile block", "terrain model", "collision geometry", "door models", "(unknown, 128 bytes in Littleroot's)", "(unknown, zeros in Littleroot's)", "shadow block"};
    for (size_t i = 2; i < gr.Files.size(); i++)
    {
        const Bytes& f = gr.Files[i];
        std::string head;
        for (size_t k = 0; k < 4 && k < f.size(); k++) head += f[k] >= 32 && f[k] < 127 ? (char)f[k] : '.';
        s += F("  part %zu: %s, %zu bytes, starts \"%s\"\n", i, i < 7 ? known[i] : "(unknown)", f.size(), head.c_str());
        if (i == 3 && f.size() >= 4) s += F("    %u door model(s), 44 bytes each: type, scale x y z, rotation, position (x, y, z in pixels)\n", U32(f, 0));
    }
    return s;
}

std::string InspectArea(N3dsRom& game, size_t area)
{
    Bytes data;
    std::string s = Member(game, "a/0/1/4", area, data);
    if (!s.empty()) return s;
    const BinLinker ad = BinLinker::Read(data, "AD");
    s += F("area pack %zu (a/0/1/4 member %zu, %zu bytes decompressed), %zu files\n", area, area, data.size(), ad.Files.size());
    for (size_t i = 0; i < ad.Files.size(); i++)
    {
        const Bytes& f = ad.Files[i];
        s += F("  file %2zu: %zu bytes", i, f.size());
        if (!f.empty() && Bch::Is(f))
        {
            const Bch b = Bch::Read(f);
            s += F(", BCH with %zu model(s), %zu texture(s)", b.Models.size(), b.Textures.size());
            std::string names;
            for (const BchTexture& t : b.Textures) names += (names.empty() ? "" : ", ") + t.Name;
            if (!names.empty()) s += ": " + names;
        }
        s += "\n";
    }
    return s;
}

}
