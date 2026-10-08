#include "OrasSandbox.h"
#include "BinLinker.h"
#include "GameText.h"
#include "Bps.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasTown.h"
#include "OrasZone.h"
#include "TownLayout.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }

struct SandboxDescription
{
    int Zone = -1, Template = -1, Encounters = -1, Width = 0, Height = 0;
    float SpawnX = -1, SpawnZ = -1;
    std::string Name;
    std::vector<std::array<int, 8>> Characters; // model, x, z, facing, script, movement, kind, sight
    std::vector<std::string> Map;
};

static SandboxDescription ReadDescription(const std::string& text)
{
    SandboxDescription d;
    std::istringstream in(text);
    std::string line;
    bool map = false;
    int number = 0;
    while (std::getline(in, line))
    {
        number++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (map) { if (!line.empty()) d.Map.push_back(line); continue; }
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        std::string what;
        if (!(words >> what)) continue;
        bool ok = true;
        if (what == "zone") ok = (bool)(words >> d.Zone);
        else if (what == "template") ok = (bool)(words >> d.Template);
        else if (what == "encounters") ok = (bool)(words >> d.Encounters);
        else if (what == "spawn") ok = (bool)(words >> d.SpawnX >> d.SpawnZ);
        else if (what == "pieces") ok = (bool)(words >> d.Width >> d.Height);
        else if (what == "name") { std::getline(words >> std::ws, d.Name); ok = !d.Name.empty(); }
        else if (what == "character" || what == "trainer")
        {
            // character MODEL X Z [face F] [script S] [move M]; trainer ID MODEL X Z [face F] [sight N] [move M]
            int id = 0;
            std::array<int, 8> c{0, 0, 0, 0, 0, 0, 0, 0};
            if (what == "trainer") { ok = (bool)(words >> id); c[4] = 3000 + id; c[6] = 1; c[7] = 4; }
            ok = ok && (bool)(words >> c[0] >> c[1] >> c[2]);
            std::string key;
            while (ok && words >> key)
            {
                int v = 0;
                ok = (bool)(words >> v);
                if (key == "face") c[3] = v;
                else if (key == "script" && what == "character") c[4] = v;
                else if (key == "move") c[5] = v;
                else if (key == "sight" && what == "trainer") c[7] = v;
                else throw FormatError("sandbox description, line " + std::to_string(number) + ": " + what + " has no " + key);
            }
            if (what == "trainer" && (id < 1 || id > 949)) throw FormatError("sandbox description, line " + std::to_string(number) + ": trainer ids are 1-949 (a/0/3/6)");
            d.Characters.push_back(c);
        }
        else if (what == "map") map = true;
        else throw FormatError("sandbox description, line " + std::to_string(number) + ": unknown statement " + what);
        if (!ok) throw FormatError("sandbox description, line " + std::to_string(number) + ": " + what + " needs its numbers");
    }
    if (d.Zone < 0 || d.Template < 0 || d.SpawnX < 0 || d.Width <= 0 || d.Height <= 0)
        throw FormatError("sandbox description: zone, template, spawn and pieces are required");
    if ((int)d.Map.size() != d.Height * TownTiles)
        throw FormatError("sandbox description: the map has " + std::to_string(d.Map.size()) + " rows, pieces asks for " + std::to_string(d.Height * TownTiles));
    for (size_t r = 0; r < d.Map.size(); r++)
    {
        if ((int)d.Map[r].size() != d.Width * TownTiles)
            throw FormatError("sandbox description: map row " + std::to_string(r) + " has " + std::to_string(d.Map[r].size()) + " letters, pieces asks for " +
                              std::to_string(d.Width * TownTiles));
        for (char c : d.Map[r])
            if (std::string(".g:s*~tT").find(c) == std::string::npos)
                throw FormatError(std::string("sandbox description: map row ") + std::to_string(r) + ": letter '" + c + "' is not built yet (. g : s * ~ t T)");
    }
    if (d.SpawnX >= d.Width * TownTiles || d.SpawnZ >= d.Height * TownTiles) throw FormatError("sandbox description: the spawn tile is outside the map");
    const char under = d.Map[(int)d.SpawnZ][(int)d.SpawnX];
    if (under == '~' || under == 't' || under == 'T') throw FormatError("sandbox description: the spawn tile is solid");
    return d;
}

// one piece's layout from the hand-drawn roles: paths and water at half-tile precision as whole tiles, no snow, collision from the role
static TownLayout PieceLayout(const SandboxDescription& d, int px, int py)
{
    TownLayout l;
    for (int r = 0; r < TownTiles; r++) l.Vis.push_back(d.Map[(size_t)py * TownTiles + r].substr((size_t)px * TownTiles, TownTiles));
    for (int r = 0; r < 2 * TownTiles; r++)
    {
        std::string path, water;
        for (int c = 0; c < 2 * TownTiles; c++)
        {
            const char v = l.Vis[r / 2][c / 2];
            path += v == ':' ? ':' : '.';
            water += v == '~' ? '~' : '.';
        }
        l.Path2.push_back(path);
        l.Water2.push_back(water);
        l.Snow2.push_back(std::string(2 * TownTiles, '.'));
    }
    for (const std::string& row : l.Vis)
    {
        std::string c;
        for (char v : row) c += v == '~' ? '~' : v == 't' || v == 'T' ? '#' : v == 'g' ? 'g' : '.';
        l.Collision.push_back(c);
    }
    return l;
}

std::vector<std::string> BuildOrasSandbox(N3dsRom& oras, const std::string& description, const std::string& outDir)
{
    const SandboxDescription d = ReadDescription(description);
    std::vector<std::string> log;
    char line[256];

    const Bytes pieces = oras.Read("a/0/3/9"), matrices = oras.Read("a/0/4/0"), zones = oras.Read("a/0/1/3"), areas = oras.Read("a/0/1/4");
    const Garc pieceArchive(pieces), matrixArchive(matrices), zoneArchive(zones), areaArchive(areas);
    Garc newPieces(pieces), newMatrices(matrices), newZones(zones), newAreas(areas);
    if ((size_t)d.Zone != zoneArchive.Count())
        throw FormatError("sandbox: zone " + std::to_string(d.Zone) + " is not the next free member of a/0/1/3 (" + std::to_string(zoneArchive.Count()) + ")");
    if ((size_t)d.Template >= 536 || (d.Encounters >= 0 && d.Encounters >= 536)) throw FormatError("sandbox: template and encounters are game zones, 0-535");

    const Bytes templateData = Plain(zoneArchive.Sub((size_t)d.Template));
    const OrasZone model = OrasZone::Read(templateData);
    const int pack = model.AreaPack();
    Bytes packData = Plain(areaArchive.Sub((size_t)pack));
    const Bytes originalPack = packData;

    // the matrix: one rectangle over the whole map (the player must stand in one of file 1's: OrasRegion), every block the zone
    const size_t matrixTemplate = 1;
    const OrasMatrix mm = OrasMatrix::Read(Plain(matrixArchive.Sub(matrixTemplate)));
    OrasMatrix matrix;
    matrix.Width = (uint16_t)d.Width; matrix.Height = (uint16_t)d.Height;
    matrix.Pieces.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.Zones.assign((size_t)d.Width * d.Height * 16, (uint16_t)d.Zone);
    matrix.Third.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.File1 = mm.File1;
    std::fill(matrix.File1.begin(), matrix.File1.end(), 0);
    const float whole[4] = {0.0f, d.Width * 720.0f, 0.0f, d.Height * 720.0f};
    matrix.File1[0] = 1;
    std::memcpy(&matrix.File1[4], whole, sizeof whole);
    matrix.Lead[0] = mm.Lead[0]; matrix.Lead[1] = mm.Lead[1];

    OrasTownOptions to;
    to.CloseEdges = false;
    to.AreaPack = (size_t)pack;
    const PieceBudget budget = GamePieceBudget(pieceArchive);
    std::string layouts;
    for (int y = 0; y < d.Height; y++)
        for (int x = 0; x < d.Width; x++)
        {
            const TownLayout layout = PieceLayout(d, x, y);
            snprintf(line, sizeof line, "world16_%02d_%02d", x, y);
            const Bytes piece = BuildTownPiece(layout, to, pieceArchive, areaArchive, budget, x, y, line, packData, log);
            const size_t index = AppendMember(newPieces, pieceArchive, to.TargetPiece, piece, "GR");
            matrix.Piece(x, y) = (uint16_t)index;
            snprintf(line, sizeof line, "piece (%d, %d): a/0/3/9 member %zu, %zu bytes", x, y, index, piece.size());
            log.push_back(line);
            layouts += "piece " + std::to_string(x) + " " + std::to_string(y) + "\n" + layout.Text();
        }
    const Bytes matrixData = matrix.Write();
    if (OrasMatrix::Read(matrixData).Write() != matrixData) throw FormatError("the new matrix does not read back identical");
    const size_t matrixIndex = AppendMember(newMatrices, matrixArchive, matrixTemplate, matrixData, "MM");
    snprintf(line, sizeof line, "matrix: a/0/4/0 member %zu, %d x %d pieces, every block zone %d", matrixIndex, d.Width, d.Height, d.Zone);
    log.push_back(line);

    // the zone: the template's header and scripts, its events emptied, its own number, the new matrix and the spawn tile
    OrasZone zone = model;
    zone.Furniture.clear(); zone.Characters.clear(); zone.Doors.clear(); zone.Triggers.clear(); zone.Others.clear();
    // characters: each a copy of a game character's 24 words (zone 24's: character 2 standing, kind 0; character 5 its trainer 10)
    // with the known words set (OrasZone.h): 0 the id, 1 the model, 2 the movement, 3 the kind, 5 the script (3000 + id for a
    // trainer, inferred), 6 the facing, 7 the sight, 20-21 the tile; the rest as the copied one
    if (!d.Characters.empty())
    {
        const OrasZone route = OrasZone::Read(Plain(zoneArchive.Sub(24)));
        for (const auto& c : d.Characters)
        {
            if (c[1] < 0 || c[2] < 0 || c[1] >= d.Width * TownTiles || c[2] >= d.Height * TownTiles) throw FormatError("sandbox: a character stands outside the map");
            const char under = d.Map[(size_t)c[2]][(size_t)c[1]];
            if (under == '~' || under == 't' || under == 'T') throw FormatError("sandbox: a character stands on a solid tile");
            ZoneCharacter k = route.Characters.at(c[6] == 1 ? 5 : 2);
            k.Raw[0] = (uint16_t)zone.Characters.size();
            k.Raw[1] = (uint16_t)c[0]; k.Raw[2] = (uint16_t)c[5]; k.Raw[3] = (uint16_t)c[6]; k.Raw[5] = (uint16_t)c[4];
            k.Raw[6] = (uint16_t)c[3]; k.Raw[7] = (uint16_t)(c[6] == 1 ? c[7] : 0); k.Raw[20] = (uint16_t)c[1]; k.Raw[21] = (uint16_t)c[2];
            zone.Characters.push_back(k);
        }
        log.push_back("characters: " + std::to_string(zone.Characters.size()));
    }
    zone.Header[2] = (uint16_t)matrixIndex;
    zone.Header[13] = (uint16_t)d.Zone;
    for (int at : {22, 25}) { zone.Header[at] = (uint16_t)(d.SpawnX * 18); zone.Header[at + 2] = (uint16_t)(d.SpawnZ * 18); }
    // the place name (Navi-Map, the name shown on entering): header word 14 & 0x3FF is a line of text file 90 (Zone_GetLocationNameId
    // 0x4D86EC; checked: zone 6 and its house 223 both 170, line 170 "Bourg-en-Vol" in a/0/7/4, "Littleroot Town" in a/0/7/3).
    // The name is added as a new line in the eight languages' files (a/0/7/1-a/0/7/8), the same text in all
    std::vector<std::tuple<std::string, Bytes, Garc>> texts; // archive, original, new
    if (!d.Name.empty())
    {
        size_t line = 0;
        for (int a = 1; a <= 8; a++)
        {
            const std::string path = "a/0/7/" + std::to_string(a);
            const Bytes data = oras.Read(path);
            const Garc g(data);
            Garc n(data);
            const Bytes file = Plain(g.Sub(90));
            const std::vector<std::string> lines = ReadGameText(file);
            if (a == 1) line = lines.size();
            else if (line != lines.size()) throw FormatError(path + " member 90: not as many place names as a/0/7/1's");
            const Bytes added = AppendGameTextLine(file, d.Name);
            std::vector<std::string> back = ReadGameText(added);
            if (back.size() != lines.size() + 1 || back.back() != d.Name || !std::equal(lines.begin(), lines.end(), back.begin()))
                throw FormatError(path + " member 90: the added place name does not read back");
            ReplaceMember(n, g, 90, added, "");
            texts.emplace_back(path, data, std::move(n));
        }
        if (line > 0x3FF) throw FormatError("sandbox: no place name number left (10 bits)");
        zone.Header[14] = (uint16_t)((zone.Header[14] & ~0x3FF) | line);
        log.push_back("place name \"" + d.Name + "\": line " + std::to_string(line) + " of text file 90 in a/0/7/1-a/0/7/8");
    }
    BinLinker zc = BinLinker::Read(zone.Write(templateData), "ZO");
    const Bytes encounter = d.Encounters >= 0 ? BinLinker::Read(Plain(zoneArchive.Sub((size_t)d.Encounters)), "ZO").Files.at(3) : Bytes{};
    zc.Files.at(3) = encounter;
    const Bytes zoneData = zc.Write();
    const OrasZone check = OrasZone::Read(zoneData);
    if (check.Number() != d.Zone || check.Matrix() != (int)matrixIndex || check.Characters.size() != zone.Characters.size() || !check.Doors.empty())
        throw FormatError("the new zone does not read back as written");
    const size_t zoneIndex = AppendMember(newZones, zoneArchive, (size_t)d.Template, zoneData, "ZO");
    snprintf(line, sizeof line, "zone %zu: template %d's header and scripts, area pack %d, no events, spawn (%.1f, %.1f), encounters %s", zoneIndex, d.Template,
             pack, d.SpawnX, d.SpawnZ, d.Encounters >= 0 ? ("of zone " + std::to_string(d.Encounters)).c_str() : "none");
    log.push_back(line);

    // the zone tables (ORAS_ENGINE.md 2): the header table (member 536) one 56-byte row per zone number, the zone's header in its
    // row and the template's in the rows between (536, 537: the table members, never zones); the encounter container (member 537)
    // one file per zone number, the zone's own file 3 in its
    const size_t rowBytes = 56;
    Bytes table = Plain(zoneArchive.Sub(536));
    if (table.size() != 536 * rowBytes) throw FormatError("member 536 is not the 536-row zone header table");
    const Bytes header = zc.Files.at(0);
    const Bytes filler(table.begin() + d.Template * rowBytes, table.begin() + (d.Template + 1) * rowBytes);
    while (table.size() < zoneIndex * rowBytes) table.insert(table.end(), filler.begin(), filler.end());
    table.insert(table.end(), header.begin(), header.end());
    ReplaceMember(newZones, zoneArchive, 536, table, "");
    BinLinker en = BinLinker::Read(Plain(zoneArchive.Sub(537)), "EN");
    if (en.Files.size() != 536) throw FormatError("member 537 is not the 536-file encounter container");
    while (en.Files.size() < zoneIndex) en.Files.push_back(Bytes{});
    en.Files.push_back(encounter);
    ReplaceMember(newZones, zoneArchive, 537, en.Write(), "EN");
    snprintf(line, sizeof line, "zone tables: %zu header rows (oras-engine --zone-rows %zu), %zu encounter files", table.size() / rowBytes, table.size() / rowBytes, en.Files.size());
    log.push_back(line);
    if (packData != originalPack) ReplaceMember(newAreas, areaArchive, (size_t)pack, packData, "AD");

    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path out(outDir), root = out / "load" / "mods" / id / "romfs_ext";
    for (const auto& [path, archive, original] : {std::tuple<const char*, Garc*, const Bytes*>{"a/0/3/9", &newPieces, &pieces}, {"a/0/4/0", &newMatrices, &matrices},
                                                  {"a/0/1/3", &newZones, &zones}, {"a/0/1/4", &newAreas, &areas}})
    {
        const Bytes data = archive->Write();
        if (data == *original) continue;
        if (data.size() < original->size()) throw FormatError(std::string(path) + ": shorter than the game's, a patch would keep the old tail (Bps.h)");
        const Bytes bps = BpsCreate(*original, data);
        if (BpsApply(*original, bps) != data) throw FormatError(std::string(path) + ": the patch does not rebuild the file");
        std::filesystem::create_directories((root / path).parent_path());
        WriteFile((root / (std::string(path) + ".bps")).string(), bps);
        snprintf(line, sizeof line, "%s: %zu members, patch %zu bytes, checked", path, Garc(data).Count(), bps.size());
        log.push_back(line);
    }
    for (auto& [path, original, archive] : texts)
    {
        const Bytes data = archive.Write();
        const Bytes bps = BpsCreate(original, data);
        if (BpsApply(original, bps) != data) throw FormatError(path + ": the patch does not rebuild the file");
        std::filesystem::create_directories((root / path).parent_path());
        WriteFile((root / (path + ".bps")).string(), bps);
    }
    if (!texts.empty()) log.push_back("a/0/7/1-a/0/7/8: place names patched, checked");
    std::filesystem::create_directories(out);
    WriteFile((out / "sandbox_layout.txt").string(), Bytes(layouts.begin(), layouts.end()));
    return log;
}

}
