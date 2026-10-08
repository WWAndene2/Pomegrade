#include "OrasSandbox.h"
#include "BinLinker.h"
#include "Bps.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasTown.h"
#include "OrasZone.h"
#include "TownLayout.h"

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
    zone.Header[2] = (uint16_t)matrixIndex;
    zone.Header[13] = (uint16_t)d.Zone;
    for (int at : {22, 25}) { zone.Header[at] = (uint16_t)(d.SpawnX * 18); zone.Header[at + 2] = (uint16_t)(d.SpawnZ * 18); }
    BinLinker zc = BinLinker::Read(zone.Write(templateData), "ZO");
    const Bytes encounter = d.Encounters >= 0 ? BinLinker::Read(Plain(zoneArchive.Sub((size_t)d.Encounters)), "ZO").Files.at(3) : Bytes{};
    zc.Files.at(3) = encounter;
    const Bytes zoneData = zc.Write();
    const OrasZone check = OrasZone::Read(zoneData);
    if (check.Number() != d.Zone || check.Matrix() != (int)matrixIndex || !check.Characters.empty() || !check.Doors.empty())
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
    std::filesystem::create_directories(out);
    WriteFile((out / "sandbox_layout.txt").string(), Bytes(layouts.begin(), layouts.end()));
    return log;
}

}
