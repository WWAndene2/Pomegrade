#include "OrasRegion.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Bps.h"
#include "Garc.h"
#include "Gltf.h"
#include "NitroCompression.h"
#include "OrasMatrix.h"
#include "OrasZone.h"
#include "PlatinumWorld.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }
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

constexpr int BlockTiles = TownTiles / OrasMatrix::BlocksPerPiece; // a zone block: 10 x 10 tiles

// the Platinum map header at a tile of the matrix's tile grid, -1 outside the matrix or where no map is
static int HeaderAt(const WorldMap& world, int gx, int gy)
{
    if (gx < 0 || gy < 0) return -1;
    const uint32_t cx = (uint32_t)gx / LandTiles, cy = (uint32_t)gy / LandTiles;
    if (cx >= world.Matrix.Width || cy >= world.Matrix.Height) return -1;
    const size_t cell = world.Matrix.Cell(cx, cy);
    return world.Cells[cell] ? world.Matrix.Headers[cell] : -1;
}

// The zone moved onto the new matrix (header word 2) and its warps onto `doors` (matrix tiles, in order); warps beyond the doors are
// removed (they would stand at their Hoenn tile somewhere in the new matrix). A door whose Platinum destination has an ORAS zone of
// its own (Interior) gets a warp into it, arriving at the interior's warp 0 (ORAS_LITTLEROOT.md 0, next step 3: each door its own
// interior); when the zone has fewer warps than that, warps are added, copies of its first door warp (kind 1) with the door's
// position and destination. A door without an interior keeps the Hoenn destination of the warp it is given, or gets none when
// the zone has no warp left for it; adding warps up to a door with an interior needs every door before it to have one too, so
// that no copy leads into a Hoenn house by accident (the owner refused that: OrasTown.h, AddWarps)
// a door's own interior: the ORAS zone its Platinum destination header is given, when that zone is not on the matrix; -1
// otherwise. Decides both which doors are kept and where each kept door's warp leads
static int InteriorOf(const OrasRegionOptions& o, const std::set<int>& used, unsigned destHeader)
{
    const auto in = o.Zones.find((int)destHeader);
    return in != o.Zones.end() && in->second >= 0 && !used.count(in->second) ? in->second : -1;
}

// entries parked rather than removed, so the zone keeps its size and layout: their tile x, z words (wordX, wordX + 1)
// set far outside any matrix
static void Park(Bytes& entries, size_t first, int count, size_t stride, size_t wordX)
{
    for (int k = 0; k < count; k++)
    {
        Put16(entries, first + k * stride + 2 * wordX, 0xFFFF);
        Put16(entries, first + k * stride + 2 * wordX + 2, 0xFFFF);
    }
}

static Bytes MoveZone(const Bytes& plain, size_t zoneIndex, size_t matrix, const std::vector<RegionDoor>& doors, bool noTriggers, bool noCharacters, std::vector<std::string>& log)
{
    BinLinker zone = BinLinker::Read(plain, "ZO");
    if (zone.Write() != plain) throw FormatError("zone " + std::to_string(zoneIndex) + " does not rewrite identical");
    const OrasZone before = OrasZone::Read(plain);
    Put16(zone.Files.at(0), 2 * 2, (uint16_t)matrix);
    Bytes& entries = zone.Files.at(1);
    const int files = entries.at(4), npcs = entries.at(5), warps = entries.at(6);
    const size_t first = 12 + files * 0x14 + npcs * 0x30; // the warps follow the size, four counts, the fifth count, furniture and characters
    int wanted = std::min<int>(warps, (int)doors.size());
    for (int k = 0; k < (int)doors.size(); k++)
        if (doors[k].Interior >= 0) wanted = std::max(wanted, k + 1);
    int added = 0;
    if (wanted > warps)
    {
        std::string bare;
        for (int k = warps; k < wanted; k++)
            if (doors[k].Interior < 0) bare += F(" (%d, %d) to header %d", doors[k].X, doors[k].Y, doors[k].DestHeader);
        if (!bare.empty())
            throw FormatError(F("zone %zu has %d warps; a warp added for a door with an interior needs the doors before it to have one too: give "
                                "an interior (--zone <header>:<ORAS zone>) to the doors at", zoneIndex, warps) + bare);
        int model = -1;
        for (int k = 0; k < warps && model < 0; k++) if (before.Doors[k].Kind() == 1) model = k;
        if (model < 0) throw FormatError(F("zone %zu has no door warp (kind 1) to copy for its added doors", zoneIndex));
        const Bytes copy(entries.begin() + first + model * 0x18, entries.begin() + first + (model + 1) * 0x18);
        added = wanted - warps;
        for (int k = 0; k < added; k++) entries.insert(entries.begin() + first + (warps + k) * 0x18, copy.begin(), copy.end());
        entries.at(6) = (uint8_t)wanted;
        const uint32_t size = U32(entries, 0) + added * 0x18;
        for (int k = 0; k < 4; k++) entries.at(k) = (uint8_t)(size >> (8 * k));
    }
    const int total = warps + added, placed = std::min<int>(total, (int)doors.size()), removed = total - placed;
    int linked = 0;
    for (int k = 0; k < placed; k++)
    {
        const size_t at = first + k * 0x18;
        Put16(entries, at + 8, (uint16_t)(doors[k].X * 18 + 9));
        Put16(entries, at + 12, (uint16_t)(doors[k].Y * 18 + 9));
        if (doors[k].Interior < 0) continue;
        if (U16(entries, at + 4) % 256 != 1) throw FormatError(F("zone %zu: warp %d is not a door warp (kind %d), it cannot lead into an interior", zoneIndex, k, U16(entries, at + 4) % 256));
        Put16(entries, at, (uint16_t)doors[k].Interior);
        Put16(entries, at + 2, 0);
        linked++;
    }
    if (removed)
    {
        entries.erase(entries.begin() + first + placed * 0x18, entries.begin() + first + total * 0x18);
        entries.at(6) = (uint8_t)placed;
        const uint32_t size = U32(entries, 0) - removed * 0x18;
        for (int k = 0; k < 4; k++) entries.at(k) = (uint8_t)(size >> (8 * k));
    }
    const int triggers = noTriggers ? entries.at(7) : 0, characters = noCharacters ? npcs : 0;
    Park(entries, first + placed * 0x18, triggers, 0x18, 6);   // the triggers follow the warps; tile x, z at words 6, 7
    Park(entries, 12 + files * 0x14, characters, 0x30, 20);   // the characters follow the furniture; words 20, 21
    const Bytes out = zone.Write();
    const OrasZone check = OrasZone::Read(out); // the events file's size rule and counts must still hold
    if ((int)check.Doors.size() != placed || (triggers && check.Triggers.at(0).TileX() != 0xFFFF) || (characters && check.Characters.at(0).TileX() != 0xFFFF) || check.Matrix() != (int)matrix) throw FormatError("zone " + std::to_string(zoneIndex) + " does not read back as written");
    for (int k = 0; k < placed; k++)
        if (doors[k].Interior >= 0 && (check.Doors[k].DestZone() != doors[k].Interior || check.Doors[k].DestWarp() != 0))
            throw FormatError(F("zone %zu: warp %d does not read back leading into zone %d", zoneIndex, k, doors[k].Interior));
    log.push_back(F("zone %zu: matrix %d -> %zu, area pack %d kept; %d of its %d warps (%d added) on %zu doors, %d into their own interior, %d removed%s; "
                    "kept from Hoenn: %zu characters%s, %zu furniture, %zu triggers%s, %zu other entries, its scripts; spawn tile (%.1f, %.1f) kept",
                    zoneIndex, before.Matrix(), matrix, before.AreaPack(), placed, total, added, doors.size(), linked, removed,
                    (int)doors.size() > total ? " (doors without a warp lead nowhere)" : "", before.Characters.size(), characters ? " (moved off the map)" : "", before.Furniture.size(),
                    before.Triggers.size(), triggers ? " (moved off the map)" : "", before.Others.size(), before.SpawnTileX(), before.SpawnTileZ()));
    return out;
}

// An interior zone's warp 0 (its way out, ORAS_LITTLEROOT.md 0, next step 3) pointed back at the door that leads in: zone `town`,
// arriving at its warp `warp`. The interior keeps its own map, characters and scripts (Hoenn's house, until Platinum's is rebuilt)
static Bytes LinkInterior(const Bytes& plain, size_t zoneIndex, int town, int warp, std::vector<std::string>& log)
{
    BinLinker zone = BinLinker::Read(plain, "ZO");
    if (zone.Write() != plain) throw FormatError("zone " + std::to_string(zoneIndex) + " does not rewrite identical");
    const OrasZone before = OrasZone::Read(plain);
    if (before.Doors.empty()) throw FormatError(F("interior zone %zu has no warp to lead back out", zoneIndex));
    // an interior's exit is kind 0 (zone 223, Littleroot's first house: its warp 0 to zone 6), where a door into it is kind 1; edges (2, 3)
    // join overworld sections and are no way out of a house
    const int kind = before.Doors[0].Kind();
    if (kind == 2 || kind == 3) throw FormatError(F("interior zone %zu: its warp 0 is a section edge (kind %d), not a way out of a house", zoneIndex, kind));
    Bytes& entries = zone.Files.at(1);
    const size_t first = 12 + entries.at(4) * 0x14 + entries.at(5) * 0x30;
    Put16(entries, first, (uint16_t)town);
    Put16(entries, first + 2, (uint16_t)warp);
    const Bytes out = zone.Write();
    const OrasZone check = OrasZone::Read(out);
    if (check.Doors[0].DestZone() != town || check.Doors[0].DestWarp() != warp) throw FormatError(F("interior zone %zu does not read back as written", zoneIndex));
    log.push_back(F("zone %zu (interior, matrix %d): warp 0 leads back to zone %d warp %d (was zone %d warp %d); %zu other warps kept", zoneIndex,
                    before.Matrix(), town, warp, before.Doors[0].DestZone(), before.Doors[0].DestWarp(), before.Doors.size() - 1));
    return out;
}

// the texture names the piece's drawn meshes show
static std::set<std::string> ShownTextures(const Bytes& piece)
{
    std::set<std::string> names;
    const Bch bch = Bch::Read(BinLinker::Read(piece, "GR").Files.at(1));
    const BchModel& model = bch.Models.at(0);
    for (const BchMesh& mesh : model.Meshes)
    {
        if (mesh.Triangles.empty() || mesh.Material >= model.Materials.size()) continue;
        for (const std::string& name : model.Materials[mesh.Material].Texture)
            if (!name.empty() && name != "projection_dummy") names.insert(name);
    }
    return names;
}

std::vector<std::string> BuildOrasRegion(const NdsRom& platinum, N3dsRom& oras, const OrasRegionOptions& given)
{
    OrasRegionOptions o = given; // AutoZones adds to Zones
    std::vector<std::string> log;
    if (o.Width <= 0 || o.Height <= 0) throw FormatError("the region needs a width and a height in pieces");
    const PlatinumWorld world(platinum, o.Town.Matrix);
    const int bw = o.Width * OrasMatrix::BlocksPerPiece, bh = o.Height * OrasMatrix::BlocksPerPiece;

    // each zone block's Platinum header, read at the block's centre tile
    std::vector<int> headers((size_t)bw * bh);
    std::map<int, int> blocksOf;
    for (int bz = 0; bz < bh; bz++)
        for (int bx = 0; bx < bw; bx++)
        {
            const int h = HeaderAt(world.World, o.Left * TownTiles + bx * BlockTiles + BlockTiles / 2, o.Top * TownTiles + bz * BlockTiles + BlockTiles / 2);
            headers[(size_t)bz * bw + bx] = h;
            if (h >= 0) blocksOf[h]++;
        }
    std::string plan = F("region: pieces %d..%d x %d..%d of Sinnoh's grid (Platinum matrix %zu tiles %d..%d x %d..%d); map header of each 10-tile block, "
                         "a piece every 4 columns and rows ('.': no map)\n", o.Left, o.Left + o.Width - 1, o.Top, o.Top + o.Height - 1, o.Town.Matrix,
                         o.Left * TownTiles, (o.Left + o.Width) * TownTiles - 1, o.Top * TownTiles, (o.Top + o.Height) * TownTiles - 1);
    for (int bz = 0; bz < bh; bz++)
    {
        if (bz && bz % OrasMatrix::BlocksPerPiece == 0) plan += "\n";
        for (int bx = 0; bx < bw; bx++)
        {
            const int h = headers[(size_t)bz * bw + bx];
            plan += (bx && bx % OrasMatrix::BlocksPerPiece == 0 ? "  " : " ") + (h < 0 ? std::string("   .") : F("%4d", h));
        }
        plan += "\n";
    }
    std::string missing;
    for (const auto& [h, n] : blocksOf)
    {
        const auto z = o.Zones.find(h);
        plan += F("header %d: %d blocks -> %s\n", h, n, z == o.Zones.end() ? (o.OthersOut ? "left out (--others-out)" : "no ORAS zone given") : z->second < 0 ? "left out" : F("ORAS zone %d", z->second).c_str());
        if (z == o.Zones.end()) missing += F(" --zone %d:<ORAS zone or -1>", h);
    }
    log.push_back(plan);
    if (o.PlanOnly) return log;
    if (o.AutoZones)
    {
        // every header of the region not given a zone gets one: first the zones of Hoenn's overworld grids (outdoor area packs), then the
        // empty zones (oras-sinnoh counts 107 of them against Sinnoh's 66 overworld headers); header 0, Platinum's scenery with no
        // events (forest, sea), is left out as before. Zones already given (a save's zone, interiors) are not reused
        const Garc zo(oras.Read("a/0/1/3")), mm(oras.Read("a/0/4/0"));
        std::set<int> taken;
        for (const auto& [h, z] : o.Zones) if (z >= 0) taken.insert(z);
        std::set<int> overworld;
        for (size_t m = 0; m < mm.Count(); m++)
            try { for (uint16_t z : OrasMatrix::Read(Plain(mm.Sub(m))).Zones) if (z != OrasMatrix::None) overworld.insert(z); } catch (const FormatError&) {}
        std::vector<int> pool(overworld.begin(), overworld.end());
        for (size_t i = 0; i < zo.Count(); i++)
            try
            {
                const OrasZone z = OrasZone::Read(Plain(zo.Sub(i)));
                if (z.Characters.empty() && z.Doors.empty() && z.Triggers.empty() && z.Furniture.empty() && !overworld.count((int)i)) pool.push_back((int)i);
            }
            catch (const FormatError&) {}
        size_t next = 0;
        std::string assigned;
        for (const auto& [h, n] : blocksOf)
        {
            if (o.Zones.count(h)) continue;
            if (h == 0) { o.Zones[h] = -1; continue; }
            while (next < pool.size() && taken.count(pool[next])) next++;
            if (next >= pool.size()) throw FormatError(F("--auto-zones: no reusable ORAS zone left for header %d", h));
            o.Zones[h] = pool[next];
            taken.insert(pool[next++]);
            assigned += F(" %d:%d", h, o.Zones[h]);
        }
        log.push_back("auto zones (header:zone):" + assigned);
        missing.clear();
    }
    if (!missing.empty() && !o.OthersOut) throw FormatError("every map header in the region needs an ORAS zone or -1 (or --others-out):" + missing);
    std::set<int> used;
    for (const auto& [h, z] : o.Zones) if (z >= 0 && blocksOf.count(h)) { if (!used.insert(z).second) throw FormatError(F("ORAS zone %d is given to two map headers", z)); }

    const Bytes pieces = oras.Read("a/0/3/9"), matrices = oras.Read("a/0/4/0"), zones = oras.Read("a/0/1/3"), areas = oras.Read("a/0/1/4");
    const Garc pieceArchive(pieces), matrixArchive(matrices), zoneArchive(zones), areaArchive(areas);
    Garc newPieces(pieces), newMatrices(matrices), newZones(zones), newAreas(areas);
    std::map<int, OrasZone> zoneOf;
    for (int z : used) zoneOf.emplace(z, OrasZone::Read(Plain(zoneArchive.Sub((size_t)z))));
    std::map<int, Bytes> packs; // area pack -> its plain bytes, as the pieces add to it
    for (const auto& [z, zone] : zoneOf) if (!packs.count(zone.AreaPack())) packs[zone.AreaPack()] = Plain(areaArchive.Sub((size_t)zone.AreaPack()));
    const std::map<int, Bytes> originalPacks = packs;

    OrasMatrix matrix;
    matrix.Width = (uint16_t)o.Width; matrix.Height = (uint16_t)o.Height;
    matrix.Pieces.assign((size_t)o.Width * o.Height, OrasMatrix::None);
    matrix.Zones.assign((size_t)bw * bh, OrasMatrix::None);
    matrix.Third.assign((size_t)o.Width * o.Height, OrasMatrix::None); // data in 7 of the game's matrices, meaning unknown (next step 5)
    {
        const OrasMatrix model = OrasMatrix::Read(Plain(matrixArchive.Sub(o.MatrixTemplate)));
        // file 1: rectangles in world units (u32 count, then per rectangle f32 x min, x max, z min, z max and a u32, 324 bytes in
        // all 431 matrices; matrix 1 holds Littleroot's three, the largest x 711-3555, z 1269-3805). The player must stand in one:
        // with Littleroot's copied, s1's Twinleaf (z about 16,000) lay outside them all and the field never started (all3), while
        // r5 and m1 (z about 3,070, inside) loaded. One rectangle covers the whole matrix (camera areas, inferred)
        matrix.File1 = model.File1;
        std::fill(matrix.File1.begin(), matrix.File1.end(), 0);
        const float whole[4] = {0.0f, o.Width * 720.0f, 0.0f, o.Height * 720.0f};
        matrix.File1[0] = 1;
        std::memcpy(&matrix.File1[4], whole, sizeof whole);
        matrix.Lead[0] = model.Lead[0]; matrix.Lead[1] = model.Lead[1]; // file 0's first two words, meaning unknown
    }
    for (size_t k = 0; k < headers.size(); k++)
        if (headers[k] >= 0 && o.Zones.count(headers[k]) && o.Zones.at(headers[k]) >= 0) matrix.Zones[k] = (uint16_t)o.Zones.at(headers[k]);

    const PieceBudget budget = GamePieceBudget(pieceArchive);
    std::map<int, std::vector<RegionDoor>> doorsOf; // ORAS zone -> its doors, in matrix tiles
    std::vector<GltfPart> parts;
    std::vector<GltfMaterial> materials;
    const std::filesystem::path out(o.OutDir);
    std::filesystem::create_directories(out);
    std::string layouts;
    int skipped = 0;
    for (int y = 0; y < o.Height; y++)
        for (int x = 0; x < o.Width; x++)
        {
            // the zones of the piece's 16 blocks; the piece belongs to the one with most (its area pack builds it)
            std::map<int, int> count;
            for (int k = 0; k < 16; k++)
            {
                const uint16_t z = matrix.Zone(x * 4 + k % 4, y * 4 + k / 4);
                if (z != OrasMatrix::None) count[z]++;
            }
            if (count.empty()) continue; // no map, or only left-out headers: no piece
            const int owner = std::max_element(count.begin(), count.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
            const int pack = zoneOf.at(owner).AreaPack();
            OrasTownOptions to = o.Town;
            to.CloseEdges = false; // pieces meet their neighbours
            to.AreaPack = (size_t)pack;
            TownLayout layout = TownLayout::Read(world, (o.Left + x) * TownTiles, (o.Top + y) * TownTiles);
            // a door of a header left out of the region (-1 or not given) gets no house, no door model and no warp: r2 built two
            // houses 39 tiles wide at Route 201's exit warps (header 334, each read twice), and they took the door-model slots,
            // so the last real door (the top-right house) got no door model on the phone
            for (auto d = layout.Doors.begin(); d != layout.Doors.end();)
            {
                const auto z = o.Zones.find(d->Zone);
                // a door is kept only when it leads into an interior of its own (its destination header given an ORAS zone
                // that is not on the matrix): with --auto-zones every header has a zone, and s3's Twinleaf piece got 6 doors
                // for Littleroot's 4 door-model entries, so the real doors lost their models on the phone (all4)
                if (z != o.Zones.end() && z->second >= 0 && InteriorOf(o, used, d->DestZone) >= 0) { ++d; continue; }
                log.push_back(F("door at (%d, %d) of header %u to header %u: %s, no house, door model or warp", x * TownTiles + d->Column,
                                y * TownTiles + d->Row, d->Zone, d->DestZone, z == o.Zones.end() || z->second < 0 ? "header left out" : "no interior given"));
                d = layout.Doors.erase(d);
            }
            char name[32];
            snprintf(name, sizeof name, "world%02d_%02d_%02d", o.ModelMatrix, x, y);
            if (strlen(name) != 13) throw FormatError("a piece's model name must be 13 characters as the game's (world<NN>_<x>_<y>)");
            log.push_back(F("piece (%d, %d), Sinnoh (%d, %d): zone %d, area pack %d, %zu doors", x, y, o.Left + x, o.Top + y, owner, pack, layout.Doors.size()));
            Bytes piece;
            try { piece = BuildTownPiece(layout, to, pieceArchive, areaArchive, budget, x, y, name, packs[pack], log); }
            catch (const FormatError& e)
            {
                const std::string what = F("piece (%d, %d), Sinnoh (%d, %d): ", x, y, o.Left + x, o.Top + y) + e.what();
                if (!o.Town.AllowErrors) throw FormatError(what);
                // a whole region is built even where a piece cannot be yet: the cell is left without a piece, its blocks keep their zone
                log.push_back("error, piece left out (--allow-errors): " + what);
                skipped++;
                continue;
            }
            if (o.SolidPieceTiles)
            {
                // a piece of wall only (its blocks hold no map: forest, the region's edge): its tile grid filled with another
                // value, to test whether such a piece is what freezes the game (Route 201, ORAS_LITTLEROOT.md 0)
                BinLinker gr = BinLinker::Read(piece, "GR");
                Bytes& tiles = gr.Files.at(0);
                bool wall = tiles.size() >= 4 + 1600 * 4;
                for (size_t t = 0; wall && t < 1600; t++) wall = U32(tiles, 4 + t * 4) == 0x01000021;
                if (wall)
                {
                    for (size_t t = 0; t < 1600; t++)
                        for (int k = 0; k < 4; k++) tiles[4 + t * 4 + k] = (uint8_t)(o.SolidPieceTiles >> (8 * k));
                    piece = gr.Write();
                    log.push_back(F("piece (%d, %d): all wall, its 1600 tiles set to 0x%08X (--solid-piece-tiles)", x, y, o.SolidPieceTiles));
                }
            }
            if (!o.TileReplace.empty())
            {
                // one tile value turned into another in every built piece: which value freezes the game (Route 201)
                BinLinker gr = BinLinker::Read(piece, "GR");
                Bytes& tiles = gr.Files.at(0);
                int changed = 0;
                for (size_t t = 0; t < 1600 && 4 + t * 4 + 4 <= tiles.size(); t++)
                    for (const auto& [from, to] : o.TileReplace)
                        if (U32(tiles, 4 + t * 4) == from)
                        {
                            for (int k = 0; k < 4; k++) tiles[4 + t * 4 + k] = (uint8_t)(to >> (8 * k));
                            changed++;
                            break;
                        }
                if (changed) { piece = gr.Write(); log.push_back(F("piece (%d, %d): %d tiles changed (--tile-replace)", x, y, changed)); }
            }
            const size_t index = AppendMember(newPieces, pieceArchive, o.Town.TargetPiece, piece, "GR");
            matrix.Piece(x, y) = (uint16_t)index;
            log.push_back(F("piece (%d, %d): a/0/3/9 member %zu, %zu bytes", x, y, index, piece.size()));

            // the other zones on the piece: their packs get the textures it shows, from the pack it was built with
            const std::set<std::string> shown = ShownTextures(piece);
            for (const auto& [z, n] : count)
            {
                const int other = zoneOf.at(z).AreaPack();
                if (other == pack) continue;
                std::map<std::string, std::string> finalName;
                packs[other] = ImportTextures(packs[other], packs[pack], (size_t)pack, {shown.begin(), shown.end()}, finalName, log);
                for (const auto& [want, got] : finalName)
                    if (got != want) log.push_back(F("warning: area pack %d holds its own %s: zone %d shows it on piece (%d, %d)", other, want.c_str(), z, x, y));
            }

            // its doors, by the ORAS zone of their Platinum header
            for (const TownDoor& d : layout.Doors)
            {
                const auto z = o.Zones.find(d.Zone);
                if (z == o.Zones.end() || z->second < 0) continue; // left out above
                doorsOf[z->second].push_back({x * TownTiles + d.Column, y * TownTiles + d.Row, d.DestZone, InteriorOf(o, used, d.DestZone)});
            }

            // the preview: the piece placed at its cell (a piece spans -360..360 around its centre)
            BchModel model = Bch::Read(BinLinker::Read(piece, "GR").Files.at(1)).Models.at(0);
            std::vector<BchMesh> kept; // shadow decals left out: a preview cannot blend them as the console does
            for (const BchMesh& m : model.Meshes)
            {
                const BchMaterial& mat = model.Materials[m.Material];
                if (mat.Texture[0].find("shadow") == std::string::npos && mat.Texture[1].find("shadow") == std::string::npos) kept.push_back(m);
            }
            model.Meshes = kept;
            std::vector<BchTexture> textures;
            for (const Bytes& f : BinLinker::Read(packs[pack], "AD").Files)
                if (Bch::Is(f)) for (BchTexture& t : Bch::Read(f).Textures) textures.push_back(std::move(t));
            const float origin[3] = {(x + 0.5f) * TownTiles * 18, 0, (y + 0.5f) * TownTiles * 18};
            AppendBchModel(model, textures, origin, parts, materials);
            layouts += F("piece (%d, %d)\n", x, y) + layout.Text();
            WriteFile((out / F("region_piece_%d_%d.bin", x, y)).string(), piece);
        }

    const Bytes matrixData = matrix.Write();
    if (OrasMatrix::Read(matrixData).Write() != matrixData) throw FormatError("the new matrix does not read back identical");
    const size_t matrixIndex = AppendMember(newMatrices, matrixArchive, o.MatrixTemplate, matrixData, "MM");
    if (skipped) log.push_back(F("%d piece(s) left out on errors (listed above)", skipped));
    log.push_back(F("matrix: a/0/4/0 member %zu, %d x %d pieces, file 0's first words and file 1 copied from matrix %zu", matrixIndex, o.Width, o.Height, o.MatrixTemplate));

    std::map<int, std::pair<int, int>> interiors; // interior zone -> the zone and warp of the door leading in
    for (int z : used)
    {
        // the doors north to south, west to east, as TownLayout orders them within a piece
        auto& doors = doorsOf[z];
        std::sort(doors.begin(), doors.end(), [](const RegionDoor& a, const RegionDoor& b) { return a.Y != b.Y ? a.Y < b.Y : a.X < b.X; });
        ReplaceMember(newZones, zoneArchive, (size_t)z, MoveZone(Plain(zoneArchive.Sub((size_t)z)), (size_t)z, matrixIndex, doors, o.NoTriggers, o.NoCharacters, log), "ZO");
        for (size_t k = 0; k < doors.size(); k++)
        {
            if (doors[k].Interior < 0) continue;
            if (!interiors.emplace(doors[k].Interior, std::make_pair(z, (int)k)).second)
                throw FormatError(F("ORAS zone %d is the interior of two doors: its warp 0 leads back to one only", doors[k].Interior));
            ReplaceMember(newZones, zoneArchive, (size_t)doors[k].Interior,
                          LinkInterior(Plain(zoneArchive.Sub((size_t)doors[k].Interior)), (size_t)doors[k].Interior, z, (int)k, log), "ZO");
        }
        // where the zone's spawn tile (Littleroot's: where a new game and a save's warp land, inferred) falls on the new matrix
        const OrasZone& zone = zoneOf.at(z);
        const int bx = (int)(zone.SpawnTileX() / BlockTiles), bz = (int)(zone.SpawnTileZ() / BlockTiles);
        const bool inside = bx >= 0 && bz >= 0 && bx < bw && bz < bh;
        log.push_back(F("zone %d: its spawn tile (%.1f, %.1f) lies %s", z, zone.SpawnTileX(), zone.SpawnTileZ(),
                        !inside ? "outside the new matrix" : matrix.Zone(bx, bz) == z ? "on its own block"
                        : matrix.Zone(bx, bz) == OrasMatrix::None ? "on a block of no zone" : F("on a block of zone %d", matrix.Zone(bx, bz)).c_str()));
        if (inside)
        {
            // what the save's player stands on there (a save made at the spawn keeps that tile, inferred): Platinum's collision around it,
            // '#' solid, '~' water, 'g' tall grass, '.' free, '@' the tile itself (still '#' or '~' under it if solid)
            const int tx = (int)zone.SpawnTileX(), tz = (int)zone.SpawnTileZ();
            const TownLayout around = TownLayout::Read(world, (o.Left + tx / TownTiles) * TownTiles, (o.Top + tz / TownTiles) * TownTiles);
            const int cx = tx % TownTiles, cz = tz % TownTiles;
            std::string grid;
            for (int r = std::max(0, cz - 3); r <= std::min(TownTiles - 1, cz + 3); r++)
            {
                grid += "\n  ";
                for (int c = std::max(0, cx - 6); c <= std::min(TownTiles - 1, cx + 6); c++)
                    grid += r == cz && c == cx && around.Collision[r][c] == '.' ? '@' : around.Collision[r][c];
            }
            log.push_back(F("zone %d: the spawn tile is %s (role '%c'); collision around it:", z,
                            around.Collision[cz][cx] == '.' || around.Collision[cz][cx] == 'g' ? "walkable" : "SOLID", around.Vis[cz][cx]) + grid);
        }
    }

    // where one can walk, by Platinum's collision, inside blocks that have a zone (a block of no zone is outside the map): each
    // walkable area that holds tiles of more than one zone, or of a zone with doors, with the zones it joins. A path the owner
    // could not take (r4: Twinleaf to Route 201) shows here as two areas instead of one
    {
        const int tw = bw * BlockTiles, th = bh * BlockTiles;
        auto free = [&](int mx, int mz) {
            if (matrix.Zone(mx / BlockTiles, mz / BlockTiles) == OrasMatrix::None) return false;
            const int gx = o.Left * TownTiles + mx, gy = o.Top * TownTiles + mz;
            if (gx < 0 || gy < 0 || gx / (int)LandTiles >= (int)world.World.Matrix.Width || gy / (int)LandTiles >= (int)world.World.Matrix.Height) return false;
            const auto& cell = world.World.Cells[world.World.Matrix.Cell(gx / LandTiles, gy / LandTiles)];
            return cell && !LandData::Solid(cell->Permissions[(gy % LandTiles) * LandTiles + gx % LandTiles]);
        };
        std::vector<int> area((size_t)tw * th, -1);
        int areas = 0;
        for (int start = 0; start < tw * th; start++)
        {
            if (area[start] >= 0 || !free(start % tw, start / tw)) continue;
            std::vector<int> stack{start};
            area[start] = areas;
            std::map<int, int> tilesOf;
            while (!stack.empty())
            {
                const int at = stack.back();
                stack.pop_back();
                const int mx = at % tw, mz = at / tw;
                tilesOf[matrix.Zone(mx / BlockTiles, mz / BlockTiles)]++;
                const int next[4][2] = {{mx + 1, mz}, {mx - 1, mz}, {mx, mz + 1}, {mx, mz - 1}};
                for (const auto& n : next)
                    if (n[0] >= 0 && n[1] >= 0 && n[0] < tw && n[1] < th && area[(size_t)n[1] * tw + n[0]] < 0 && free(n[0], n[1]))
                    {
                        area[(size_t)n[1] * tw + n[0]] = areas;
                        stack.push_back(n[1] * tw + n[0]);
                    }
            }
            size_t total = 0;
            for (const auto& [zz, n] : tilesOf) total += n;
            if (total >= 20)
            {
                std::string list;
                for (const auto& [zz, n] : tilesOf) list += F(" zone %d (%d tiles)", zz, n);
                log.push_back(F("walkable area %d, %zu tiles, from tile (%d, %d):", areas, total, start % tw, start / tw) + list);
            }
            areas++;
        }
    }
    for (const auto& [pack, data] : packs)
        if (data != originalPacks.at(pack)) ReplaceMember(newAreas, areaArchive, (size_t)pack, data, "AD");

    // the mod: BPS patches of the four archives, each checked by applying it back
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = out / "load" / "mods" / id / "romfs_ext";
    for (const auto& [path, archive, original] : {std::tuple<const char*, Garc*, const Bytes*>{"a/0/3/9", &newPieces, &pieces}, {"a/0/4/0", &newMatrices, &matrices},
                                                  {"a/0/1/3", &newZones, &zones}, {"a/0/1/4", &newAreas, &areas}})
    {
        const Bytes data = archive->Write();
        if (data == *original) continue;
        if (data.size() < original->size())
        {
            // a recompressed member can come out shorter (s1's zones): a patched file shorter than the game's would keep the old
            // file's tail (Bps.h), so the archive is shipped whole in romfs/, which Azahar serves as it is (oras-mod's way)
            const std::filesystem::path whole = out / "load" / "mods" / id / "romfs" / path;
            std::filesystem::create_directories(whole.parent_path());
            Garc check(data);
            WriteFile(whole.string(), data);
            log.push_back(F("%s: %zu members, %zu bytes (shorter than the game's %zu): written whole under romfs/", path, check.Count(), data.size(), original->size()));
            continue;
        }
        const Bytes bps = BpsCreate(*original, data);
        if (BpsApply(*original, bps) != data) throw FormatError(std::string(path) + ": the patch does not rebuild the file");
        std::filesystem::create_directories((root / path).parent_path());
        WriteFile((root / (std::string(path) + ".bps")).string(), bps);
        log.push_back(F("%s: %zu members, patch %zu bytes, checked", path, Garc(data).Count(), bps.size()));
    }
    const std::string gltf = WriteGltf(parts, materials);
    WriteFile((out / "region_preview.gltf").string(), Bytes(gltf.begin(), gltf.end()));
    layouts = plan + layouts;
    WriteFile((out / "region_plan.txt").string(), Bytes(layouts.begin(), layouts.end()));
    return log;
}

}

namespace remake
{

std::vector<std::string> PlanSinnoh(const NdsRom& platinum, N3dsRom& oras, int stripWidth)
{
    if (stripWidth <= 0) throw FormatError("the strip width must be at least one piece");
    std::vector<std::string> log;
    const PlatinumWorld world(platinum, 0);
    const WorldMap& w = world.World;
    const int tilesX = (int)(w.Matrix.Width * LandTiles), tilesY = (int)(w.Matrix.Height * LandTiles);
    const int gw = (tilesX + TownTiles - 1) / TownTiles, gh = (tilesY + TownTiles - 1) / TownTiles;
    auto passable = [&](int gx, int gy) {
        if (gx < 0 || gy < 0 || gx >= tilesX || gy >= tilesY) return false;
        const auto& cell = w.Cells[w.Matrix.Cell((uint32_t)gx / LandTiles, (uint32_t)gy / LandTiles)];
        return cell && !LandData::Solid(cell->Permissions[(gy % LandTiles) * LandTiles + gx % LandTiles]);
    };
    // a piece is used when a map lies under any of its tiles; its headers are read block by block, as oras-region reads them
    std::vector<char> used((size_t)gw * gh, 0);
    std::vector<std::map<int, int>> headersOf((size_t)gw * gh);
    for (int py = 0; py < gh; py++)
        for (int px = 0; px < gw; px++)
        {
            for (int by = 0; by < OrasMatrix::BlocksPerPiece; by++)
                for (int bx = 0; bx < OrasMatrix::BlocksPerPiece; bx++)
                {
                    const int h = HeaderAt(w, px * TownTiles + bx * BlockTiles + BlockTiles / 2, py * TownTiles + by * BlockTiles + BlockTiles / 2);
                    if (h >= 0) headersOf[(size_t)py * gw + px][h]++;
                }
            for (int t = 0; t < TownTiles * TownTiles && !used[(size_t)py * gw + px]; t++)
                if (HeaderAt(w, px * TownTiles + t % TownTiles, py * TownTiles + t / TownTiles) >= 0) used[(size_t)py * gw + px] = 1;
        }
    size_t usedCount = 0;
    for (char u : used) usedCount += u;
    log.push_back(F("Sinnoh (Platinum matrix 0, %d x %d tiles) as ORAS pieces of %d tiles: %d x %d grid, %zu pieces used", tilesX, tilesY, TownTiles, gw, gh, usedCount));

    // strips of whole columns, as tall as their used pieces: neighbouring strips meet on east and west edges only, the only edge
    // kinds seen in ORAS (2 and 3, ORAS_LITTLEROOT.md 2b); north and south edges are never needed
    std::set<int> allHeaders;
    for (int left = 0; left < gw; left += stripWidth)
    {
        const int right = std::min(gw, left + stripWidth);
        int top = gh, bottom = -1;
        std::map<int, int> headers;
        size_t pieces = 0;
        for (int py = 0; py < gh; py++)
            for (int px = left; px < right; px++)
                if (used[(size_t)py * gw + px])
                {
                    top = std::min(top, py); bottom = std::max(bottom, py); pieces++;
                    for (const auto& [h, n] : headersOf[(size_t)py * gw + px]) headers[h] += n;
                }
        if (bottom < 0) { log.push_back(F("strip x %d..%d: no map", left, right - 1)); continue; }
        const int width = right - left, height = bottom - top + 1;
        std::string list;
        for (const auto& [h, n] : headers) { if (h) allHeaders.insert(h); list += F(" %d", h); }
        log.push_back(F("strip x %d..%d: oras-region --rect %d %d %d %d, %d cells (%zu with a piece)%s, %zu map headers:%s", left, right - 1, left, top, width, height,
                        width * height, pieces, width * height > 140 ? " - over the game's largest matrix (140 cells), untried" : height > 10 ? " - taller than any matrix of the game (10), untried" : "",
                        headers.size(), list.c_str()));
        // where the strip meets the next one: runs of tiles passable on both sides of the edge, each one warp per side (spans of up
        // to 15 tiles seen: an edge 19 tiles long is two warps of 15 and 4)
        if (right >= gw) continue;
        const int ex = right * TownTiles;
        int warps = 0;
        std::string runs;
        for (int gy = 0; gy < gh * TownTiles;)
        {
            if (!(passable(ex - 1, gy) && passable(ex, gy))) { gy++; continue; }
            int end = gy;
            while (end < gh * TownTiles && passable(ex - 1, end) && passable(ex, end)) end++;
            runs += F(" z %d..%d", gy, end - 1);
            warps += (end - gy + 14) / 15;
            gy = end;
        }
        log.push_back(F("  edge with the next strip at tile x %d: %s -> %d warps on each side", ex, runs.empty() ? " no crossing" : runs.c_str(), warps));
    }

    // the ORAS zones to give the headers: those on Hoenn's overworld grids (outdoor area packs), the empty ones, the rest
    const Garc zo(oras.Read("a/0/1/3")), mm(oras.Read("a/0/4/0"));
    std::set<int> overworld;
    for (size_t m = 0; m < mm.Count(); m++)
        try { for (uint16_t z : OrasMatrix::Read(Plain(mm.Sub(m))).Zones) if (z != OrasMatrix::None) overworld.insert(z); } catch (const FormatError&) {}
    size_t empty = 0;
    for (size_t i = 0; i < zo.Count(); i++)
        try
        {
            const OrasZone z = OrasZone::Read(Plain(zo.Sub(i)));
            if (z.Characters.empty() && z.Doors.empty() && z.Triggers.empty() && z.Furniture.empty() && !overworld.count((int)i)) empty++;
        }
        catch (const FormatError&) {}
    const ptrdiff_t spare = (ptrdiff_t)(overworld.size() + empty) - (ptrdiff_t)allHeaders.size();
    log.push_back(F("map headers on Sinnoh's overworld (header 0, the scenery with no events, left out): %zu; ORAS zones on Hoenn's overworld grids: %zu, "
                    "empty zones: %zu: ", allHeaders.size(), overworld.size(), empty) +
                  (spare >= 0 ? F("%zd to spare, no interior or code-reached zone needed", spare)
                              : F("%zd more must come from Hoenn's interiors or the zones its code reaches", -spare)));
    return log;
}

}
