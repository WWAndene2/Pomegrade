#include "OrasSandbox.h"
#include "Amx.h"
#include "BinLinker.h"
#include "GameText.h"
#include "Bps.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasTown.h"
#include "OrasNewZone.h"
#include "OrasWorkspace.h"
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
    int Number = -1, Pack = -1, Music = -1, Encounters = -1, Lighting = -1;
    std::vector<std::string> Lines; // its text file's lines (the zone's own, header word 3)
    std::map<uint32_t, float> Camera; // the camera settings given: byte offset in preset 0 of the area pack's file 6 -> value
    std::map<uint32_t, float> Light;  // the light colours given: byte offset in the area pack's file 4 -> value
    float SpawnX = -1, SpawnZ = -1;
    std::string Name;
    std::vector<std::array<int, 8>> Characters; // model, x, z, facing, script, movement, kind, sight
    std::vector<std::array<int, 3>> Doors;      // x, z, the game interior zone copied as the house's inside
    std::string Script, InitScript;             // assembler source (Amx.h) of the zone's own scripts, empty: ones that do nothing
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
    std::string* source = nullptr; // inside a script or init-script block
    int number = 0;
    auto fail = [&](const std::string& what) { throw FormatError("sandbox description, line " + std::to_string(number) + ": " + what); };
    while (std::getline(in, line))
    {
        number++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (source)
        {
            // a script block's lines as they are ('#' is a hash in assembler source, ';' its comment), up to "end"
            std::istringstream first(line);
            std::string word;
            if (first >> word && word == "end") { source = nullptr; continue; }
            *source += line + "\n";
            continue;
        }
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
        if (what == "zone")
        {
            // "zone N", or "zone" alone: the next free number as the build stands (OrasWorkspace.h), given when it is built
            d.Zones.emplace_back();
            std::string n;
            if (words >> n) { try { d.Zones.back().Number = std::stoi(n); } catch (...) { fail("zone takes a number or nothing"); } }
            continue;
        }
        if (what == "pieces") { ok = (bool)(words >> d.Width >> d.Height); if (!ok) fail("pieces needs its numbers"); continue; }
        if (what == "blocks") { part = Blocks; continue; }
        if (d.Zones.empty()) fail(what + " before any zone");
        SandboxZone& z = d.Zones.back();
        if (what == "script" || what == "init-script")
        {
            std::string& block = what == "script" ? z.Script : z.InitScript;
            if (!block.empty()) fail(what + " given twice");
            block = "; " + what + " of zone " + std::to_string(z.Number) + "\n";
            source = &block;
            continue;
        }
        if (what == "template") fail("\"template\" is gone: zones are written from nothing (SINNOH_BUILD.md R2); give the area pack with \"pack P\"");
        if (what == "pack") ok = (bool)(words >> z.Pack);
        else if (what == "music") ok = (bool)(words >> z.Music);
        else if (what == "line") { std::string rest; std::getline(words, rest); z.Lines.push_back(rest.substr(rest.find_first_not_of(' ') == std::string::npos ? rest.size() : rest.find_first_not_of(' '))); }
        else if (what == "encounters") ok = (bool)(words >> z.Encounters);
        else if (what == "lighting") ok = (bool)(words >> z.Lighting);
        else if (what == "light" || what == "character-light")
        {
            // "light NAME R G B..." (OrasSandbox.h): a colour of the area pack's file 4, in each of its 12 entries. The file holds
            // its colours in planes of 12 floats (3 kinds x 4 times of day, FUN_0013D908 0x13D908 reads them, FUN_0012DDF4 blends
            // two entries and sets the field light's ambient and diffuse colours, FUN_00139124 and FUN_001391D4): a colour's red
            // at its group's offset, green 0x30 and blue 0x60 further; the direction the same way from 0x240, x y z, the way the light
            // goes, normalised by the game (FUN_0013D540; (0, -1, 0) in Littleroot, from above) (ORAS_ENGINE.md 2)
            static const std::map<std::string, uint32_t> colours = {{"ambient", 0x00}, {"diffuse", 0x90}, {"direction", 0x240}};
            std::string name; float rgb[3];
            ok = false;
            while (words >> name >> rgb[0] >> rgb[1] >> rgb[2])
            {
                const auto at = colours.find(name);
                if (at == colours.end()) fail("no light setting \"" + name + "\" (ambient, diffuse, direction)");
                // character-light: the second light set, the characters' (the same layout 0x2D0 further, read by FUN_0013DC4C)
                const uint32_t set = what == "light" ? 0 : 0x2D0;
                for (uint32_t c = 0; c < 3; c++)
                    for (uint32_t entry = 0; entry < 12; entry++) z.Light[set + at->second + c * 0x30 + entry * 4] = rgb[c];
                ok = true;
            }
        }
        else if (what == "camera")
        {
            // "camera NAME VALUE..." (OrasSandbox.h): each name a float of a camera preset, by its offset (Field_CameraApplyParams
            // 0x102CB460 and Field_CopyCameraParams 0x102CB128 read them, ORAS_ENGINE.md 2)
            static const std::map<std::string, uint32_t> settings = {{"height", 0x08}, {"pitch", 0x0C}, {"yaw", 0x10},
                {"near", 0x18}, {"far", 0x1C}, {"fov", 0x20}, {"distance", 0x24}};
            std::string name; float value;
            ok = false;
            while (words >> name >> value)
            {
                const auto at = settings.find(name);
                if (at == settings.end()) fail("no camera setting \"" + name + "\" (height, pitch, yaw, near, far, fov, distance)");
                z.Camera[at->second] = value; ok = true;
            }
        }
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
        const std::string which = "sandbox description, zone " + (z.Number >= 0 ? std::to_string(z.Number) : "#" + std::to_string(k + 1) + " (numbered when built)");
        if (z.SpawnX < 0) throw FormatError(which + ": its spawn is required");
        if ((z.Number < 0) != (d.Zones[0].Number < 0)) throw FormatError(which + ": every zone numbered, or none");
        if (&z == &d.Zones[0] && z.Pack < 0) throw FormatError(which + ": the first zone gives the area pack (pack P)");
        if (!used[k]) throw FormatError(which + ": no block of the blocks grid");
        if (k && z.Number >= 0 && z.Number != d.Zones[k - 1].Number + 1) throw FormatError(which + ": zones are numbered one after the other");
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
    if (source) fail("a script block without its \"end\"");
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

std::vector<std::string> BuildOrasSandbox(OrasWorkspace& ws, const std::string& description, const std::string& outDir,
                                          const std::function<std::string(const std::string&)>& checkNative)
{
    SandboxDescription d = ReadDescription(description);
    std::vector<std::string> log;
    char line[256];

    // the game's archives as the build has left them (OrasWorkspace.h): what earlier steps added is kept, numbers follow it
    const Garc &pieceArchive = ws.Original("a/0/3/9"), &matrixArchive = ws.Original("a/0/4/0"), &zoneArchive = ws.Original("a/0/1/3"), &areaArchive = ws.Original("a/0/1/4");
    Garc &newPieces = ws.Edited("a/0/3/9"), &newMatrices = ws.Edited("a/0/4/0"), &newZones = ws.Edited("a/0/1/3"), &newAreas = ws.Edited("a/0/1/4");
    if (d.Zones[0].Number < 0)
        for (size_t k = 0; k < d.Zones.size(); k++) d.Zones[k].Number = (int)(newZones.Count() + k);
    if ((size_t)d.Zones[0].Number != newZones.Count())
        throw FormatError("sandbox: zone " + std::to_string(d.Zones[0].Number) + " is not the next free member of a/0/1/3 (" + std::to_string(newZones.Count()) + " as the build stands)");
    for (const SandboxZone& z : d.Zones)
        if (z.Encounters >= 536 || z.Lighting >= 536) throw FormatError("sandbox: encounters and lighting are game zones, 0-535");

    // every zone draws its textures from the first zone's area pack (a/0/1/4 member: the game's assets), which the pieces are
    // built into
    const int pack = d.Zones[0].Pack;
    if (pack >= (int)areaArchive.Count()) throw FormatError("sandbox: no area pack " + std::to_string(pack));
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
    std::vector<int> nameLine(d.Zones.size(), -1);
    for (size_t k = 0; k < d.Zones.size(); k++)
        if (!d.Zones[k].Name.empty()) nameLine[k] = ws.AddPlaceName(d.Zones[k].Name);

    // the sandbox's own area packs: the game's 9 placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195: twelve small files each)
    // are filled, never a pack a game zone draws from, so no Hoenn place changes: the first with the shared pack the pieces were
    // built for (the first zone's pack and the textures they show), the next ones with that pack's copy under another light.
    // A pack appended past the game's 229 is refused: its zone sent the game into its fatal-error loop even with a/1/3/7 grown
    // to match (runs light1-light4; a/1/3/7 is one cause, read under the debugger in runs apk1-apk4, another is not found)
    auto takeFree = [&](const Bytes& data, const std::string& what) {
        const uint16_t slot = (uint16_t)ws.AddAreaPack(data, pack); // its characters' list: the shared pack's
        snprintf(line, sizeof line, "area pack %u (unused by the game): %s", slot, what.c_str());
        log.push_back(line);
        return slot;
    };
    const uint16_t ownPack = takeFree(packData, "the sandbox's pack, a copy of pack " + std::to_string(pack) + " with the pieces' textures");
    // a zone given `lighting` or `camera` gets a copy of that pack with them, one copy per different pair (free slots are few)
    // `lighting`: the area pack's file 4 of game zone `lighting`. File 4 is 2,944 bytes of floats (0.1, 0.4, 1.0...; 6 different
    // ones among the game's 229 packs), copied into a graphics object's buffer when the field is set up (Field_SceneSetup_AddModels
    // 0x102DDC84, Res_CopyToCachedBuffer 0x48E390; battles fill it from a/0/5/9). What reads it is not found, and an interior's
    // (zone 216's, run light5) changed nothing seen outdoors: it is not the scene's light (ORAS_ENGINE.md 2)
    // `camera`: settings of preset 0 of the pack's file 6, a table of 0x44-byte presets from offset 0 (Zone_ApplyZonePackData
    // 0x102E22D0 copies preset N, N the low byte of a u16 at +0xE of the area pack's loader, 0 when past the file's end; it was
    // 0 in Littleroot, run cam10, and the sandbox's zones use preset 0, runs cam0 and cam3; where N comes from is not found)
    std::map<std::tuple<int, std::map<uint32_t, float>, std::map<uint32_t, float>>, uint16_t> zonePacks;
    std::vector<uint16_t> packOf(d.Zones.size(), ownPack);
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        if (z.Lighting < 0 && z.Camera.empty() && z.Light.empty()) continue;
        const auto key = std::make_tuple(z.Lighting, z.Camera, z.Light);
        if (const auto found = zonePacks.find(key); found != zonePacks.end()) { packOf[k] = found->second; continue; }
        BinLinker copy = BinLinker::Read(packData, "AD");
        std::string what = "zone " + std::to_string(z.Number) + "'s pack, a copy of the sandbox's with";
        if (z.Lighting >= 0)
        {
            const int from = OrasZone::Read(Plain(zoneArchive.Sub((size_t)z.Lighting))).AreaPack();
            const Bytes light = BinLinker::Read(Plain(areaArchive.Sub((size_t)from)), "AD").Files.at(4);
            if (copy.Files.size() < 5 || light.size() != copy.Files[4].size()) throw FormatError("sandbox: an area pack's file 4 is not the size of the shared pack's");
            copy.Files[4] = light;
            what += " the file 4 of zone " + std::to_string(z.Lighting) + " (pack " + std::to_string(from) + ")";
        }
        if (!z.Camera.empty())
        {
            Bytes& presets = copy.Files.at(6);
            for (const auto& [at, value] : z.Camera)
            {
                if (at + 4 > presets.size()) throw FormatError("sandbox: the pack's camera presets (file 6) are too short");
                uint32_t bits; std::memcpy(&bits, &value, 4);
                for (int b = 0; b < 4; b++) presets[at + b] = (uint8_t)(bits >> (8 * b));
            }
            what += std::string(z.Lighting >= 0 ? " and" : "") + " its camera";
        }
        if (!z.Light.empty())
        {
            Bytes& light = copy.Files.at(4);
            for (const auto& [at, value] : z.Light)
            {
                if (at + 4 > light.size()) throw FormatError("sandbox: the pack's file 4 is too short for the light");
                uint32_t bits; std::memcpy(&bits, &value, 4);
                for (int b = 0; b < 4; b++) light[at + b] = (uint8_t)(bits >> (8 * b));
            }
            what += std::string(z.Lighting >= 0 || !z.Camera.empty() ? " and" : "") + " its light";
        }
        packOf[k] = zonePacks[key] = takeFree(copy.Write(), what);
    }

    // the zones, each written from nothing (OrasNewZone.h): the new matrix, its area pack, its own text file, its characters
    // and doors, its scripts, the spawn tile and the name
    // every 'D' of the map is one zone's door; each door's house inside is a new zone after the described ones on a game
    // interior's map (its matrix and area pack: the game's assets), its one warp leading back out to the door
    size_t doorsGiven = 0, doorsDrawn = 0;
    for (const SandboxZone& z : d.Zones) doorsGiven += z.Doors.size();
    for (const std::string& row : d.Map) doorsDrawn += (size_t)std::count(row.begin(), row.end(), 'D');
    if (doorsGiven != doorsDrawn) throw FormatError("sandbox: the map has " + std::to_string(doorsDrawn) + " doors ('D'), the zones give " + std::to_string(doorsGiven));
    struct Inside { int Zone, Map, Outside, Warp; size_t OutsideAt; };
    std::vector<Inside> insides;
    int nextZone = d.Zones.back().Number + 1;
    for (size_t k = 0; k < d.Zones.size(); k++)
        for (size_t w = 0; w < d.Zones[k].Doors.size(); w++) insides.push_back({nextZone++, d.Zones[k].Doors[w][2], d.Zones[k].Number, (int)w, k});

    // each new zone's text file (header word 3): a new member of the eight story text archives (a/0/7/9-a/0/8/6, one per
    // language; ORAS_ENGINE.md 6), the zone's lines in all eight, one empty line when it gives none
    const size_t zoneCount = d.Zones.size() + insides.size();
    std::vector<int> textOf(zoneCount, -1);
    for (size_t k = 0; k < zoneCount; k++)
        textOf[k] = ws.AddZoneText(k < d.Zones.size() && !d.Zones[k].Lines.empty() ? d.Zones[k].Lines : std::vector<std::string>{""});

    // a zone's scripts, assembled: its own (script / init-script), or ones that do nothing (OrasNewZone.h)
    const auto assemble = [&](const std::string& source, int number, const char* what) {
        if (!checkNative) throw FormatError("sandbox: zone " + std::to_string(number) + "'s " + what + ": no native tables to check it against");
        return AmxAssemble(source, checkNative);
    };
    const auto appendZone = [&](const OrasZone& zone, const Bytes& encounter, int number) {
        const int added = ws.AddZone(WriteNewZone(zone, encounter), encounter);
        if (added != number) throw FormatError("zone " + std::to_string(number) + " was appended as member " + std::to_string(added));
    };

    size_t insideAt = 0;
    for (size_t k = 0; k < d.Zones.size(); k++)
    {
        const SandboxZone& z = d.Zones[k];
        OrasZone zone;
        NewZoneFields f;
        f.Number = z.Number; f.AreaPack = packOf[k]; f.Matrix = (int)matrixIndex; f.Text = textOf[k];
        f.NameLine = nameLine[k]; f.Music = z.Music; f.SpawnX = z.SpawnX; f.SpawnZ = z.SpawnZ;
        zone.Header = NewZoneHeader(f);
        for (const auto& c : z.Characters)
            zone.Characters.push_back(NewCharacter((int)zone.Characters.size(), c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[6] == 1 ? c[7] : 0));
        for (const auto& door : z.Doors) zone.Doors.push_back(NewDoorWarp(insides.at(insideAt++).Zone, 0, door[0], door[1]));
        zone.Script = assemble(z.Script.empty() ? EmptyZoneScript : z.Script, z.Number, "script");
        zone.InitScript = assemble(z.InitScript.empty() ? EmptyInitScript : z.InitScript, z.Number, "init-script");
        const Bytes encounter = z.Encounters >= 0 ? BinLinker::Read(Plain(zoneArchive.Sub((size_t)z.Encounters)), "ZO").Files.at(3) : Bytes{};
        appendZone(zone, encounter, z.Number);
        snprintf(line, sizeof line, "zone %d: written from nothing (OrasNewZone.h), area pack %d, text member %d, %zu character(s), %s scripts, spawn (%.1f, %.1f), name %s, encounters %s",
                 z.Number, f.AreaPack, f.Text, zone.Characters.size(), z.Script.empty() && z.InitScript.empty() ? "empty" : "its own", z.SpawnX, z.SpawnZ,
                 nameLine[k] >= 0 ? ("\"" + z.Name + "\" (line " + std::to_string(nameLine[k]) + ")").c_str() : "none",
                 z.Encounters >= 0 ? ("of zone " + std::to_string(z.Encounters)).c_str() : "none");
        log.push_back(line);
    }
    for (size_t i = 0; i < insides.size(); i++)
    {
        // the inside: the game interior's map (its matrix and area pack) and the tile of its way out (its warp 0, a mat of kind 0:
        // where this map's exit is), read from the game zone on that map; nothing else of it
        const Inside& in = insides[i];
        const OrasZone map = OrasZone::Read(Plain(zoneArchive.Sub((size_t)in.Map)));
        if (map.Doors.empty() || map.Doors[0].Kind() != 0)
            throw FormatError("sandbox: zone " + std::to_string(in.Map) + " is not a house's inside (its warp 0 is not a way out, kind 0)");
        OrasZone zone;
        NewZoneFields f;
        f.Kind = NewZoneKind::Interior; f.Number = in.Zone; f.AreaPack = map.AreaPack(); f.Matrix = map.Matrix();
        f.Text = textOf[d.Zones.size() + i]; f.Overworld = in.Outside; f.NameLine = nameLine[in.OutsideAt];
        f.SpawnX = map.Doors[0].Raw[4] / 18.0f; f.SpawnZ = map.Doors[0].Raw[6] / 18.0f;
        zone.Header = NewZoneHeader(f);
        zone.Doors.push_back(NewExitWarp(in.Outside, in.Warp, map.Doors[0].Raw[4], map.Doors[0].Raw[6]));
        zone.Script = assemble(EmptyZoneScript, in.Zone, "script");
        zone.InitScript = assemble(EmptyInitScript, in.Zone, "init-script");
        appendZone(zone, Bytes{}, in.Zone);
        snprintf(line, sizeof line, "zone %d: the inside of zone %d's door %d, written from nothing on game zone %d's map (matrix %d, area pack %d), warp 0 back out",
                 in.Zone, in.Outside, in.Warp, in.Map, f.Matrix, f.AreaPack);
        log.push_back(line);
    }

    // the mod itself is written once by the build (OrasWorkspace::Write)
    const std::filesystem::path out(outDir);
    std::filesystem::create_directories(out);
    WriteFile((out / "sandbox_layout.txt").string(), Bytes(layouts.begin(), layouts.end()));
    return log;
}

}
