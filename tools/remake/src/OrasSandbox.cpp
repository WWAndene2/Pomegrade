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
#include <map>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

struct SandboxZone
{
    int Number = -1, Template = -1, Encounters = -1, Lighting = -1, Camera = -1;
    float Pitch = -1000; // a test of the area pack's camera table (file 6): preset 0's pitch, degrees
    float SpawnX = -1, SpawnZ = -1;
    std::string Name;
    std::vector<std::array<int, 8>> Characters; // model, x, z, facing, script, movement, kind, sight
    std::vector<std::array<int, 3>> Doors;      // x, z, the game interior zone copied as the house's inside
};

struct SandboxDescription
{
    std::vector<SandboxZone> Zones;
    int Width = 0, Height = 0;
    std::vector<std::string> Blocks; // 4 Height rows of 4 Width digits: the zone (its place in Zones) of each 10 x 10 block
    std::vector<std::string> Map;
};

static constexpr size_t rowBytesOf536 = 56; // a zone header row (member 536 of a/0/1/3)
static constexpr int BlockTiles = 10; // a zone block of the matrix (OrasMatrix.h)

static bool Solid(char c) { return c == '~' || c == 't' || c == 'T' || c == 'H' || c == 'F' || c == 'L'; }

static SandboxDescription ReadDescription(const std::string& text)
{
    SandboxDescription d;
    std::istringstream in(text);
    std::string line;
    enum { Statements, Blocks, Map } part = Statements;
    int number = 0;
    auto fail = [&](const std::string& what) { throw FormatError("sandbox description, line " + std::to_string(number) + ": " + what); };
    while (std::getline(in, line))
    {
        number++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        std::string what;
        if (!(words >> what)) continue;
        if (part == Map) { d.Map.push_back(what); continue; }
        if (what == "map") { part = Map; continue; }
        if (part == Blocks && what.find_first_not_of("0123456789") == std::string::npos) { d.Blocks.push_back(what); continue; }
        part = Statements;
        bool ok = true;
        if (what == "zone") { d.Zones.emplace_back(); ok = (bool)(words >> d.Zones.back().Number); continue; }
        if (what == "pieces") { ok = (bool)(words >> d.Width >> d.Height); if (!ok) fail("pieces needs its numbers"); continue; }
        if (what == "blocks") { part = Blocks; continue; }
        if (d.Zones.empty()) fail(what + " before any zone");
        SandboxZone& z = d.Zones.back();
        if (what == "template") ok = (bool)(words >> z.Template);
        else if (what == "encounters") ok = (bool)(words >> z.Encounters);
        else if (what == "lighting") ok = (bool)(words >> z.Lighting);
        else if (what == "camera") ok = (bool)(words >> z.Camera);
        else if (what == "camera-pitch") ok = (bool)(words >> z.Pitch);
        else if (what == "spawn") ok = (bool)(words >> z.SpawnX >> z.SpawnZ);
        else if (what == "name") { std::getline(words >> std::ws, z.Name); ok = !z.Name.empty(); }
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
                else fail(what + " has no " + key);
            }
            if (what == "trainer" && (id < 1 || id > 949)) fail("trainer ids are 1-949 (a/0/3/6)");
            z.Characters.push_back(c);
        }
        else if (what == "door")
        {
            // door X Z house Z2: the door tile ('D' on the map) and the game interior its house's inside is copied from
            std::array<int, 3> door{0, 0, 0};
            std::string house;
            ok = (bool)(words >> door[0] >> door[1] >> house >> door[2]) && house == "house";
            z.Doors.push_back(door);
        }
        else fail("unknown statement " + what);
        if (!ok) fail(what + " needs its numbers");
    }
    if (d.Zones.empty() || d.Width <= 0 || d.Height <= 0) throw FormatError("sandbox description: at least one zone, and pieces, are required");
    if ((int)d.Map.size() != d.Height * TownTiles)
        throw FormatError("sandbox description: the map has " + std::to_string(d.Map.size()) + " rows, pieces asks for " + std::to_string(d.Height * TownTiles));
    for (size_t r = 0; r < d.Map.size(); r++)
    {
        if ((int)d.Map[r].size() != d.Width * TownTiles)
            throw FormatError("sandbox description: map row " + std::to_string(r) + " has " + std::to_string(d.Map[r].size()) + " letters, pieces asks for " +
                              std::to_string(d.Width * TownTiles));
        for (char c : d.Map[r])
            if (std::string(".g:s*~tTHDFL").find(c) == std::string::npos)
                throw FormatError(std::string("sandbox description: map row ") + std::to_string(r) + ": letter '" + c + "' is not built yet (. g : s * ~ t T H D F L)");
    }
    const int bw = d.Width * OrasMatrix::BlocksPerPiece, bh = d.Height * OrasMatrix::BlocksPerPiece;
    if (d.Blocks.empty())
    {
        if (d.Zones.size() > 1) throw FormatError("sandbox description: several zones need a blocks grid");
        d.Blocks.assign((size_t)bh, std::string((size_t)bw, '0'));
    }
    if ((int)d.Blocks.size() != bh) throw FormatError("sandbox description: blocks needs " + std::to_string(bh) + " rows (4 a piece)");
    std::vector<bool> used(d.Zones.size(), false);
    for (const std::string& row : d.Blocks)
    {
        if ((int)row.size() != bw) throw FormatError("sandbox description: a blocks row needs " + std::to_string(bw) + " digits (4 a piece)");
        for (char c : row)
        {
            if ((size_t)(c - '0') >= d.Zones.size()) throw FormatError(std::string("sandbox description: blocks names zone ") + c + ", there are " + std::to_string(d.Zones.size()));
            used[(size_t)(c - '0')] = true;
        }
    }
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        const std::string which = "sandbox description, zone " + std::to_string(z.Number);
        if (z.Number < 0 || z.Template < 0 || z.SpawnX < 0) throw FormatError(which + ": its number, template and spawn are required");
        if (!used[k]) throw FormatError(which + ": no block of the blocks grid");
        if (k && z.Number != d.Zones[k - 1].Number + 1) throw FormatError(which + ": zones are numbered one after the other");
        auto check = [&](float x, float zz, const std::string& what) {
            if (x < 0 || zz < 0 || x >= d.Width * TownTiles || zz >= d.Height * TownTiles) throw FormatError(which + ": " + what + " outside the map");
            if (Solid(d.Map[(size_t)zz][(size_t)x])) throw FormatError(which + ": " + what + " on a solid tile");
            if ((size_t)(d.Blocks[(size_t)zz / BlockTiles][(size_t)x / BlockTiles] - '0') != k) throw FormatError(which + ": " + what + " on another zone's block");
        };
        check(z.SpawnX, z.SpawnZ, "the spawn tile");
        for (const auto& c : z.Characters) check((float)c[1], (float)c[2], "a character");
        for (const auto& door : z.Doors)
        {
            check((float)door[0], (float)door[1], "a door");
            if (d.Map[(size_t)door[1]][(size_t)door[0]] != 'D') throw FormatError(which + ": a door is not on a 'D' of the map");
            if (door[2] < 0 || door[2] >= 536) throw FormatError(which + ": a house's inside is a game zone, 0-535");
        }
    }
    return d;
}

// one piece's layout from the hand-drawn roles: paths and water at half-tile precision as whole tiles, no snow, collision from the role
static TownLayout PieceLayout(const SandboxDescription& d, int px, int py)
{
    TownLayout l;
    for (int r = 0; r < TownTiles; r++) l.Vis.push_back(d.Map[(size_t)py * TownTiles + r].substr((size_t)px * TownTiles, TownTiles));
    // a door: a house tile to the builder, which puts Petalburg's house on it, as wide as the solid tiles beside it on its row
    // (BuildTown), and its door model; solid, as Littleroot's door tiles are (0x01000021 in piece 6: the player
    // enters by walking into it from the tile below, where the warp stands at the door tile)
    for (int r = 0; r < TownTiles; r++)
        for (int c = 0; c < TownTiles; c++)
            if (l.Vis[r][c] == 'D') { l.Vis[r][c] = 'H'; TownDoor door; door.Column = c; door.Row = r; l.Doors.push_back(door); }
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
        for (char v : row) c += v == '~' ? '~' : v == 't' || v == 'T' || v == 'H' || v == 'F' || v == 'D' ? '#' : v == 'g' ? 'g' : '.';
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
    if ((size_t)d.Zones[0].Number != zoneArchive.Count())
        throw FormatError("sandbox: zone " + std::to_string(d.Zones[0].Number) + " is not the next free member of a/0/1/3 (" + std::to_string(zoneArchive.Count()) + ")");
    for (const SandboxZone& z : d.Zones)
        if (z.Template >= 536 || z.Encounters >= 536 || z.Lighting >= 536 || z.Camera >= 536) throw FormatError("sandbox: template, encounters and lighting are game zones, 0-535");

    // every zone draws its textures from the first template's area pack (header word 1), which the pieces are built into
    const int pack = OrasZone::Read(Plain(zoneArchive.Sub((size_t)d.Zones[0].Template))).AreaPack();
    Bytes packData = Plain(areaArchive.Sub((size_t)pack));

    // the matrix: one rectangle over the whole map (the player must stand in one of file 1's: OrasRegion), each block its zone
    const size_t matrixTemplate = 1;
    const OrasMatrix mm = OrasMatrix::Read(Plain(matrixArchive.Sub(matrixTemplate)));
    OrasMatrix matrix;
    matrix.Width = (uint16_t)d.Width; matrix.Height = (uint16_t)d.Height;
    matrix.Pieces.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.Zones.assign((size_t)d.Width * d.Height * 16, OrasMatrix::None);
    for (size_t r = 0; r < d.Blocks.size(); r++)
        for (size_t c = 0; c < d.Blocks[r].size(); c++) matrix.Zone((int)c, (int)r) = (uint16_t)d.Zones[(size_t)(d.Blocks[r][c] - '0')].Number;
    matrix.Third.assign((size_t)d.Width * d.Height, OrasMatrix::None);
    matrix.File1 = mm.File1;
    std::fill(matrix.File1.begin(), matrix.File1.end(), 0);
    const float whole[4] = {0.0f, d.Width * 720.0f, 0.0f, d.Height * 720.0f};
    matrix.File1[0] = 1;
    std::memcpy(&matrix.File1[4], whole, sizeof whole);
    matrix.Lead[0] = mm.Lead[0]; matrix.Lead[1] = mm.Lead[1];

    OrasTownOptions to;
    to.CloseEdges = false;
    to.Ledges = true;
    to.SnowClumps = false; // no snow in a sandbox, and its clumps take the snow-band mesh the ledges use
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
    snprintf(line, sizeof line, "matrix: a/0/4/0 member %zu, %d x %d pieces, %zu zone(s) on its blocks", matrixIndex, d.Width, d.Height, d.Zones.size());
    log.push_back(line);

    // the place names (Navi-Map, the name shown on entering): header word 14 & 0x3FF is a line of text file 90 (Zone_GetLocationNameId
    // 0x4D86EC; checked: zone 6 and its house 223 both 170, line 170 "Bourg-en-Vol" in a/0/7/4, "Littleroot Town" in a/0/7/3).
    // Each name is added as a new line in the eight languages' files (a/0/7/1-a/0/7/8), the same text in all
    std::vector<std::tuple<std::string, Bytes, Garc>> texts; // archive, original, new
    std::vector<int> nameLine(d.Zones.size(), -1);
    for (int a = 1; a <= 8; a++)
    {
        const std::string path = "a/0/7/" + std::to_string(a);
        const Bytes data = oras.Read(path);
        const Garc g(data);
        Bytes file = Plain(g.Sub(90));
        const std::vector<std::string> lines = ReadGameText(file);
        size_t next = lines.size();
        std::vector<std::string> added;
        for (size_t k = 0; k < d.Zones.size(); k++)
        {
            if (d.Zones[k].Name.empty()) continue;
            if (a == 1) nameLine[k] = (int)next;
            else if (nameLine[k] != (int)next) throw FormatError(path + " member 90: not as many place names as a/0/7/1's");
            file = AppendGameTextLine(file, d.Zones[k].Name);
            added.push_back(d.Zones[k].Name);
            next++;
        }
        if (added.empty()) break;
        const std::vector<std::string> back = ReadGameText(file);
        if (back.size() != lines.size() + added.size() || !std::equal(lines.begin(), lines.end(), back.begin()) || !std::equal(added.begin(), added.end(), back.begin() + lines.size()))
            throw FormatError(path + " member 90: the added place names do not read back");
        Garc n(data);
        ReplaceMember(n, g, 90, file, "");
        texts.emplace_back(path, data, std::move(n));
    }

    // the sandbox's own area packs: the game's 9 placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195: twelve small files each)
    // are filled, never a pack a game zone draws from, so no Hoenn place changes: the first with the shared pack the pieces were
    // built for (the template's pack and the textures they show), the next ones with that pack's copy under another light.
    // A pack appended past the game's 229 is refused: its zone sent the game into its fatal-error loop even with a/1/3/7 grown
    // to match (runs light1-light4; a/1/3/7 is one cause, read under the debugger in runs apk1-apk4, another is not found)
    const Bytes perArea = oras.Read("a/1/3/7");
    const Garc perAreaArchive(perArea);
    Garc newPerArea(perArea);
    std::vector<uint16_t> freePacks;
    {
        const Bytes headers = Plain(zoneArchive.Sub(536));
        std::vector<bool> used(areaArchive.Count(), false);
        for (size_t z = 0; z * rowBytesOf536 + 4 <= headers.size(); z++) { const uint16_t p = (uint16_t)(headers[z * rowBytesOf536 + 2] | headers[z * rowBytesOf536 + 3] << 8); if (p < used.size()) used[p] = true; }
        for (size_t p = 0; p < areaArchive.Count() && p < perAreaArchive.Count(); p++) if (!used[p]) freePacks.push_back((uint16_t)p);
    }
    size_t nextFree = 0;
    auto takeFree = [&](const Bytes& data, const std::string& what) {
        if (nextFree >= freePacks.size()) throw FormatError("sandbox: no free area pack left for " + what + " (" + std::to_string(freePacks.size()) + " in the game)");
        const uint16_t slot = freePacks[nextFree++];
        ReplaceMember(newAreas, areaArchive, slot, data, "AD");
        newPerArea.Set(slot, perAreaArchive.Sub((size_t)pack)); // its characters' list: the template pack's
        snprintf(line, sizeof line, "area pack %u (unused by the game): %s", slot, what.c_str());
        log.push_back(line);
        return slot;
    };
    Bytes ownData = packData;
    for (const SandboxZone& z : d.Zones)
        if (z.Pitch > -1000)
        {
            // file 6: u32 1, u32 0, then presets of 17 floats (68 bytes) from offset 8, the first word of each its pitch (15.85
            // in preset 0 of every pack): preset 0's pitch set, to see whether the field's camera follows it
            BinLinker c = BinLinker::Read(ownData, "AD");
            Bytes& f6 = c.Files.at(6);
            uint32_t bits; std::memcpy(&bits, &z.Pitch, 4);
            for (int k = 0; k < 4; k++) f6.at(8 + k) = (uint8_t)(bits >> (8 * k));
            ownData = c.Write();
        }
    const uint16_t ownPack = takeFree(ownData, "the sandbox's pack, a copy of pack " + std::to_string(pack) + " with the pieces' textures");
    // `lighting`: an area pack's file 4 (2,944 bytes of RGBA colours, 6 different ones among the game's 229 packs: outdoors,
    // interiors and a few places) taken from the pack of game zone `lighting`. Taken for the light and fog colours, but an
    // interior's (zone 216's, run light5: zone 538 on the copy in pack 1) changed nothing seen outdoors, terrain or player:
    // what file 4 does is not known; the scene's light is elsewhere
    std::map<int, uint16_t> lightingPack; // game zone -> the sandbox pack carrying its light
    for (const SandboxZone& z : d.Zones)
    {
        if (z.Lighting < 0 || lightingPack.count(z.Lighting)) continue;
        const int from = OrasZone::Read(Plain(zoneArchive.Sub((size_t)z.Lighting))).AreaPack();
        BinLinker copy = BinLinker::Read(packData, "AD");
        const Bytes light = BinLinker::Read(Plain(areaArchive.Sub((size_t)from)), "AD").Files.at(4);
        if (copy.Files.size() < 5 || light.size() != copy.Files[4].size()) throw FormatError("sandbox: an area pack's file 4 is not the size of the shared pack's");
        copy.Files[4] = light;
        lightingPack[z.Lighting] = takeFree(copy.Write(), "the light of zone " + std::to_string(z.Lighting) + " (pack " + std::to_string(from) + "'s file 4)");
    }

    // the zones: each its template's header and scripts, its events emptied but for its characters, its own number, the new
    // matrix, the shared area pack, the spawn tile and the name
    const size_t rowBytes = 56;
    Bytes table = Plain(zoneArchive.Sub(536));
    if (table.size() != 536 * rowBytes) throw FormatError("member 536 is not the 536-row zone header table");
    BinLinker en = BinLinker::Read(Plain(zoneArchive.Sub(537)), "EN");
    if (en.Files.size() != 536) throw FormatError("member 537 is not the 536-file encounter container");
    const OrasZone route = OrasZone::Read(Plain(zoneArchive.Sub(24)));
    // every 'D' of the map is one zone's door; each door's house inside is a new zone after the described ones, a copy of a game
    // interior: its warp 0 (an interior's way out, kind 0: zone 223, Littleroot's first house) leads back to the door's warp
    size_t doorsGiven = 0, doorsDrawn = 0;
    for (const SandboxZone& z : d.Zones) doorsGiven += z.Doors.size();
    for (const std::string& row : d.Map) doorsDrawn += (size_t)std::count(row.begin(), row.end(), 'D');
    if (doorsGiven != doorsDrawn) throw FormatError("sandbox: the map has " + std::to_string(doorsDrawn) + " doors ('D'), the zones give " + std::to_string(doorsGiven));
    struct Inside { int Zone, Template, Outside, Warp; };
    std::vector<Inside> insides;
    int nextZone = d.Zones.back().Number + 1;
    for (const SandboxZone& z : d.Zones)
        for (size_t w = 0; w < z.Doors.size(); w++) insides.push_back({nextZone++, z.Doors[w][2], z.Number, (int)w});
    // a door's warp: a copy of Littleroot's warp 0 (kind 1, its first house's door) with the destination and the tile set
    const ZoneDoor doorWarp = OrasZone::Read(Plain(zoneArchive.Sub(6))).Doors.at(0);
    if (doorWarp.Kind() != 1) throw FormatError("zone 6's warp 0 is not a door (kind 1)");
    size_t insideAt = 0;
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        const Bytes templateData = Plain(zoneArchive.Sub((size_t)z.Template));
        OrasZone zone = OrasZone::Read(templateData);
        zone.Furniture.clear(); zone.Characters.clear(); zone.Doors.clear(); zone.Triggers.clear(); zone.Others.clear();
        // characters: each a copy of a game character's 24 words (zone 24's: character 2 standing, kind 0; character 5 its trainer
        // 10) with the known words set (OrasZone.h): 0 the id, 1 the model, 2 the movement, 3 the kind, 5 the script (3000 + id for
        // a trainer, inferred), 6 the facing, 7 the sight, 20-21 the tile; the rest as the copied one
        for (const auto& c : z.Characters)
        {
            ZoneCharacter ch = route.Characters.at(c[6] == 1 ? 5 : 2);
            ch.Raw[0] = (uint16_t)zone.Characters.size();
            ch.Raw[1] = (uint16_t)c[0]; ch.Raw[2] = (uint16_t)c[5]; ch.Raw[3] = (uint16_t)c[6]; ch.Raw[5] = (uint16_t)c[4];
            ch.Raw[6] = (uint16_t)c[3]; ch.Raw[7] = (uint16_t)(c[6] == 1 ? c[7] : 0); ch.Raw[20] = (uint16_t)c[1]; ch.Raw[21] = (uint16_t)c[2];
            zone.Characters.push_back(ch);
        }
        for (const auto& door : z.Doors)
        {
            ZoneDoor w = doorWarp;
            w.Raw[0] = (uint16_t)insides.at(insideAt++).Zone; w.Raw[1] = 0;
            w.Raw[4] = (uint16_t)(door[0] * 18 + 9); w.Raw[6] = (uint16_t)(door[1] * 18 + 9);
            zone.Doors.push_back(w);
        }
        zone.Header[1] = z.Lighting >= 0 ? lightingPack.at(z.Lighting) : ownPack;
        zone.Header[2] = (uint16_t)matrixIndex;
        zone.Header[13] = (uint16_t)z.Number;
        for (int at : {22, 25}) { zone.Header[at] = (uint16_t)(z.SpawnX * 18); zone.Header[at + 2] = (uint16_t)(z.SpawnZ * 18); }
        if (nameLine[k] >= 0)
        {
            if (nameLine[k] > 0x3FF) throw FormatError("sandbox: no place name number left (10 bits)");
            zone.Header[14] = (uint16_t)((zone.Header[14] & ~0x3FF) | nameLine[k]);
        }
        BinLinker zc = BinLinker::Read(zone.Write(templateData), "ZO");
        const Bytes encounter = z.Encounters >= 0 ? BinLinker::Read(Plain(zoneArchive.Sub((size_t)z.Encounters)), "ZO").Files.at(3) : Bytes{};
        zc.Files.at(3) = encounter;
        // camera: a zone's file 4 (12 bytes) is zero in 511 of the game's 536 zones; in the others byte 0 is 1 and small signed
        // u16 words follow (-6, -5, -2, 45, 90, 135...): read as the zone's camera turn (to check live). Copied from game zone `camera`
        if (z.Camera >= 0) zc.Files.at(4) = BinLinker::Read(Plain(zoneArchive.Sub((size_t)z.Camera)), "ZO").Files.at(4);
        const Bytes zoneData = zc.Write();
        const OrasZone check = OrasZone::Read(zoneData);
        if (check.Number() != z.Number || check.Matrix() != (int)matrixIndex || check.Characters.size() != zone.Characters.size() || check.Doors.size() != zone.Doors.size())
            throw FormatError("zone " + std::to_string(z.Number) + " does not read back as written");
        const size_t zoneIndex = AppendMember(newZones, zoneArchive, (size_t)z.Template, zoneData, "ZO");
        if ((int)zoneIndex != z.Number) throw FormatError("zone " + std::to_string(z.Number) + " was appended as member " + std::to_string(zoneIndex));
        // the zone tables (ORAS_ENGINE.md 2): the header table (member 536) one 56-byte row per zone number, the zone's header in its
        // row and the template's in the rows between (536, 537: the table members, never zones); the encounter container (member 537)
        // one file per zone number, the zone's own file 3 in its
        const Bytes filler(table.begin() + z.Template * rowBytes, table.begin() + (z.Template + 1) * rowBytes);
        while (table.size() < zoneIndex * rowBytes) table.insert(table.end(), filler.begin(), filler.end());
        const Bytes& header = zc.Files.at(0);
        table.insert(table.end(), header.begin(), header.end());
        while (en.Files.size() < zoneIndex) en.Files.push_back(Bytes{});
        en.Files.push_back(encounter);
        snprintf(line, sizeof line, "zone %zu: template %d's header and scripts, area pack %u, %zu character(s), spawn (%.1f, %.1f), name %s, encounters %s", zoneIndex,
                 z.Template, (unsigned)zone.Header[1], zone.Characters.size(), z.SpawnX, z.SpawnZ, nameLine[k] >= 0 ? ("\"" + z.Name + "\" (line " + std::to_string(nameLine[k]) + ")").c_str() : "the template's",
                 z.Encounters >= 0 ? ("of zone " + std::to_string(z.Encounters)).c_str() : "none");
        log.push_back(line);
    }
    for (const Inside& in : insides)
    {
        // the inside: the game interior as it is (its own matrix, area pack, furniture, scripts), its own number, no characters
        // or triggers, warp 0 leading out to the door's warp
        const Bytes templateData = Plain(zoneArchive.Sub((size_t)in.Template));
        OrasZone zone = OrasZone::Read(templateData);
        if (zone.Doors.empty() || zone.Doors[0].Kind() != 0)
            throw FormatError("sandbox: zone " + std::to_string(in.Template) + " is not a house's inside (its warp 0 is not a way out, kind 0)");
        zone.Characters.clear(); zone.Triggers.clear(); zone.Others.clear();
        zone.Doors.resize(1);
        zone.Doors[0].Raw[0] = (uint16_t)in.Outside; zone.Doors[0].Raw[1] = (uint16_t)in.Warp;
        zone.Header[13] = (uint16_t)in.Zone;
        BinLinker zc = BinLinker::Read(zone.Write(templateData), "ZO");
        zc.Files.at(3) = Bytes{};
        const Bytes zoneData = zc.Write();
        const OrasZone check = OrasZone::Read(zoneData);
        if (check.Number() != in.Zone || check.Doors.size() != 1 || check.Doors[0].DestZone() != in.Outside || check.Doors[0].DestWarp() != in.Warp)
            throw FormatError("the inside zone " + std::to_string(in.Zone) + " does not read back as written");
        const size_t zoneIndex = AppendMember(newZones, zoneArchive, (size_t)in.Template, zoneData, "ZO");
        if ((int)zoneIndex != in.Zone) throw FormatError("the inside zone " + std::to_string(in.Zone) + " was appended as member " + std::to_string(zoneIndex));
        while (table.size() < zoneIndex * rowBytes) table.insert(table.end(), rowBytes, 0);
        table.insert(table.end(), zc.Files.at(0).begin(), zc.Files.at(0).end());
        while (en.Files.size() < zoneIndex) en.Files.push_back(Bytes{});
        en.Files.push_back(Bytes{});
        snprintf(line, sizeof line, "zone %zu: the inside of zone %d's door %d, a copy of game zone %d (its matrix %d, area pack %d), warp 0 back out", zoneIndex,
                 in.Outside, in.Warp, in.Template, zone.Matrix(), zone.AreaPack());
        log.push_back(line);
    }
    ReplaceMember(newZones, zoneArchive, 536, table, "");
    ReplaceMember(newZones, zoneArchive, 537, en.Write(), "EN");
    snprintf(line, sizeof line, "zone tables: %zu header rows (oras-engine --zone-rows %zu), %zu encounter files", table.size() / rowBytes, table.size() / rowBytes, en.Files.size());
    log.push_back(line);

    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path out(outDir), root = out / "load" / "mods" / id / "romfs_ext";
    for (const auto& [path, archive, original] : {std::tuple<const char*, Garc*, const Bytes*>{"a/0/3/9", &newPieces, &pieces}, {"a/0/4/0", &newMatrices, &matrices},
                                                  {"a/0/1/3", &newZones, &zones}, {"a/0/1/4", &newAreas, &areas},
                                                  {"a/1/3/7", &newPerArea, &perArea}})
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
