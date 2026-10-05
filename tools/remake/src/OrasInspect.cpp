#include "OrasInspect.h"
#include "Amx.h"
#include "Bch.h"
#include "BchTextureFile.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasZone.h"

#include <algorithm>
#include <cstdarg>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <set>
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
    for (const ZoneDoor& d : z.Doors)
    {
        s += F("    warp to zone %d at tile (%.1f, %.1f), words:", d.DestZone(), d.TileX(), d.TileZ());
        for (uint16_t w : d.Raw) s += F(" %u", w);
        s += "\n";
    }
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
            s += F("      mesh %2zu: layer %u %s material %-24s texture %-22s%s%s  %5zu vertices, %5zu triangles\n", i, (unsigned)mesh.Layer,
                   ((mat.Flags >> 24) & 4) ? "blended" : "opaque ", mat.Name.c_str(), mat.Texture[0].c_str(),
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


std::string InspectMatrix(N3dsRom& game, size_t matrix)
{
    Bytes data;
    std::string s = Member(game, "a/0/4/0", matrix, data);
    if (!s.empty()) return s;
    s += F("map matrix %zu (a/0/4/0 member %zu, %zu bytes decompressed), starts %c%c\n", matrix, matrix, data.size(), data.size() > 1 ? data[0] : '?', data.size() > 1 ? data[1] : '?');
    std::vector<Bytes> files;
    try { files = BinLinker::Read(data, std::string(data.begin(), data.begin() + 2)).Files; }
    catch (const FormatError& e) { s += F("  not a container (%s): read as one file\n", e.what()); files = {data}; }
    for (size_t i = 0; i < files.size(); i++)
    {
        const Bytes& f = files[i];
        s += F("  file %zu: %zu bytes\n", i, f.size());
        // every u16 word, 16 per line, with its offset: the layout is unknown, so nothing is interpreted
        for (size_t at = 0; at + 1 < f.size() && at < 8192; at += 32)
        {
            s += F("    %04zx:", at);
            for (size_t k = at; k + 1 < f.size() && k < at + 32; k += 2) s += F(" %04x", U16(f, k));
            s += "\n";
        }
    }
    return s;
}

std::string InspectPieceNames(N3dsRom& game)
{
    const Garc g(game.Read("a/0/3/9"));
    std::string s;
    for (size_t i = 0; i < g.Count(); i++)
    {
        std::string name = "?";
        try
        {
            const Bytes raw = Plain(g.Sub(i));
            const Bch bch = Bch::Read(BinLinker::Read(raw, "GR").Files.at(1));
            if (!bch.Models.empty()) name = bch.Models[0].Name;
        }
        catch (const FormatError&) {}
        s += F("%zu %s\n", i, name.c_str());
    }
    return s;
}

std::string InspectZones(N3dsRom& game)
{
    const Garc g(game.Read("a/0/1/3"));
    std::string s = "zone matrix area | furniture characters warps triggers others | warps: dest@tileX,tileZ | triggers: tileX,tileZ\n";
    for (size_t i = 0; i < g.Count(); i++)
    {
        try
        {
            const OrasZone z = OrasZone::Read(Plain(g.Sub(i)));
            s += F("%zu %d %d | %zu %zu %zu %zu %zu |", i, z.Matrix(), z.AreaPack(), z.Furniture.size(), z.Characters.size(), z.Doors.size(), z.Triggers.size(), z.Others.size());
            for (const ZoneDoor& d : z.Doors) s += F(" %d@%.1f,%.1f", d.DestZone(), d.TileX(), d.TileZ());
            s += " |";
            for (const ZoneTrigger& t : z.Triggers) s += F(" %d,%d", t.TileX(), t.TileZ());
            s += "\n";
        }
        catch (const FormatError& e) { s += F("%zu not read: %s\n", i, e.what()); }
    }
    return s;
}

std::string VerifyGame(N3dsRom& game, bool& ok)
{
    ok = true;
    std::string s;
    // texture files
    {
        const Garc areas(game.Read("a/0/1/4"));
        int files = 0, same = 0, placeholders = 0;
        for (size_t j = 0; j < areas.Count(); j++)
        {
            BinLinker ad;
            try { ad = BinLinker::Read(Plain(areas.Sub(j)), "AD"); } catch (const FormatError&) { continue; }
            for (size_t slot : {(size_t)1, (size_t)11})
            {
                if (ad.Files.size() <= slot || ad.Files[slot].size() < 0x44 || !Bch::Is(ad.Files[slot])) continue;
                const Bytes& orig = ad.Files[slot];
                if (U32(orig, 0x2C) == 0) { placeholders++; continue; } // one texture, no data
                files++;
                try
                {
                    std::vector<BchTextureSource> sources;
                    for (const BchTexture& t : Bch::Read(orig).Textures) sources.push_back({t.Name, t.Width, t.Height, t.Format, t.Data});
                    const Bytes w = BchWriteTextureFile(sources);
                    const uint32_t at = U32(orig, 0x18), length = U32(orig, 0x34);
                    auto words = [](const Bytes& f, uint32_t from, uint32_t n) { std::multiset<uint32_t> m; for (uint32_t k = 0; k < n; k += 4) m.insert(U32(f, from + k)); return m; };
                    if (U32(w, 0x18) == at && U32(w, 0x34) == length && std::equal(w.begin(), w.begin() + at, orig.begin()) && words(w, at, length) == words(orig, at, length)) same++;
                }
                catch (const FormatError&) {}
            }
        }
        s += F("texture files: %d of %d rewritten identically (sections before the relocations byte for byte, relocations as a set); %d empty placeholders skipped\n", same, files, placeholders);
        ok = ok && same == files;
    }
    // zones
    {
        const Garc zones(game.Read("a/0/1/3"));
        int total = 0, read = 0, scripts = 0, notZone = 0;
        for (size_t i = 0; i < zones.Count(); i++)
        {
            if (!zones.Has(i)) continue;
            const Bytes data = Plain(zones.Sub(i));
            if (data.size() < 2 || data[0] != 'Z' || data[1] != 'O') { notZone++; continue; }
            total++;
            try
            {
                const OrasZone z = OrasZone::Read(data);
                read++;
                AmxInfo::Read(z.Script); AmxInfo::Read(z.InitScript);
                scripts += 2;
            }
            catch (const FormatError&) {}
        }
        s += F("zones: %d of %d ZO containers read exactly (%d scripts with a valid header); %d members are not ZO\n", read, total, scripts, notZone);
        ok = ok && read == total && scripts == 2 * read;
    }
    // map pieces
    {
        const Garc pieces(game.Read("a/0/3/9"));
        int total = 0, read = 0;
        for (size_t i = 0; i < pieces.Count(); i++)
        {
            if (!pieces.Has(i)) continue;
            const Bytes data = Plain(pieces.Sub(i));
            if (data.size() < 2 || data[0] != 'G' || data[1] != 'R') continue;
            total++;
            try { read += !Bch::Read(BinLinker::Read(data, "GR").Files.at(1)).Models.empty(); } catch (const std::exception&) {}
        }
        s += F("map pieces: %d of %d GR containers have a readable terrain model\n", read, total);
        ok = ok && read == total;
    }
    s += ok ? "all checks passed\n" : "SOME CHECKS FAILED\n";
    return s;
}

std::string CatalogGame(N3dsRom& game, const std::string& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream packs(directory + "/packs.tsv"), pieces(directory + "/pieces.tsv");
    packs << "pack\ttextures\tnames\n";
    pieces << "piece\tmodel\tfile_bytes\tvertices\ttriangles\tmeshes\tmaterials (name:texture0|texture1|texture2:triangles)\n";
    size_t packCount = 0, pieceCount = 0;
    struct TileUse { size_t Pieces = 0, Tiles = 0; };
    std::map<uint32_t, TileUse> tileUse; // tile value -> pieces and tiles using it (the source of TownCheck's established set)
    const Garc areas(game.Read("a/0/1/4"));
    for (size_t j = 0; j < areas.Count(); j++)
    {
        try
        {
            const BinLinker ad = BinLinker::Read(Plain(areas.Sub(j)), "AD");
            std::set<std::string> names;
            for (const Bytes& f : ad.Files) if (!f.empty() && Bch::Is(f)) for (const BchTexture& t : Bch::Read(f).Textures) names.insert(t.Name);
            packs << j << '\t' << names.size() << '\t';
            bool first = true;
            for (const std::string& n : names) { packs << (first ? "" : ",") << n; first = false; }
            packs << '\n';
            packCount++;
        }
        catch (const std::exception&) {}
    }
    const Garc grs(game.Read("a/0/3/9"));
    for (size_t i = 0; i < grs.Count(); i++)
    {
        try
        {
            const Bytes raw = Plain(grs.Sub(i));
            if (raw.size() < 2 || raw[0] != 'G' || raw[1] != 'R') continue;
            const BinLinker gr = BinLinker::Read(raw, "GR");
            const Bch b = Bch::Read(gr.Files.at(1));
            if (b.Models.empty()) continue;
            const BchModel& m = b.Models[0];
            const Bytes& tileBlock = gr.Files.at(0);
            if (tileBlock.size() >= 4 + 1600 * 4)
            {
                std::set<uint32_t> seen;
                for (int t = 0; t < 1600; t++) { const uint32_t v = U32(tileBlock, 4 + t * 4); tileUse[v].Tiles++; seen.insert(v); }
                for (uint32_t v : seen) tileUse[v].Pieces++;
            }
            size_t vertices = 0, triangles = 0;
            for (const BchMesh& me : m.Meshes) { vertices += me.Vertices.size(); triangles += me.Triangles.size() / 3; }
            pieces << i << '\t' << m.Name << '\t' << raw.size() << '\t' << vertices << '\t' << triangles << '\t' << m.Meshes.size() << '\t';
            for (size_t k = 0; k < m.Meshes.size(); k++)
            {
                const BchMaterial& mat = m.Materials[m.Meshes[k].Material];
                pieces << (k ? ";" : "") << mat.Name << ':' << mat.Texture[0] << '|' << mat.Texture[1] << '|' << mat.Texture[2] << ':' << m.Meshes[k].Triangles.size() / 3;
            }
            pieces << '\n';
            pieceCount++;
        }
        catch (const std::exception&) {}
    }
    std::ofstream tiles(directory + "/tiles.tsv");
    tiles << "value\tpieces\ttiles\n";
    for (const auto& [value, use] : tileUse) tiles << F("%08X", value) << '\t' << use.Pieces << '\t' << use.Tiles << '\n';
    return F("%zu area packs and %zu map pieces indexed in %s (packs.tsv, pieces.tsv, tiles.tsv)\n", packCount, pieceCount, directory.c_str());
}

}
