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
// removed (they would stand at their Hoenn tile somewhere in the new matrix), doors beyond the warps get none (a zone's warp count
// is its own: each door needs its own interior, ORAS_LITTLEROOT.md 0, next step 3)
static Bytes MoveZone(const Bytes& plain, size_t zoneIndex, size_t matrix, const std::vector<std::pair<int, int>>& doors, std::vector<std::string>& log)
{
    BinLinker zone = BinLinker::Read(plain, "ZO");
    if (zone.Write() != plain) throw FormatError("zone " + std::to_string(zoneIndex) + " does not rewrite identical");
    const OrasZone before = OrasZone::Read(plain);
    Put16(zone.Files.at(0), 2 * 2, (uint16_t)matrix);
    Bytes& entries = zone.Files.at(1);
    const int files = entries.at(4), npcs = entries.at(5), warps = entries.at(6);
    const size_t first = 12 + files * 0x14 + npcs * 0x30; // the warps follow the size, four counts, the fifth count, furniture and characters
    const int placed = std::min<int>(warps, (int)doors.size()), removed = warps - placed;
    for (int k = 0; k < placed; k++)
    {
        Put16(entries, first + k * 0x18 + 8, (uint16_t)(doors[k].first * 18 + 9));
        Put16(entries, first + k * 0x18 + 12, (uint16_t)(doors[k].second * 18 + 9));
    }
    if (removed)
    {
        entries.erase(entries.begin() + first + placed * 0x18, entries.begin() + first + warps * 0x18);
        entries.at(6) = (uint8_t)placed;
        const uint32_t size = U32(entries, 0) - removed * 0x18;
        for (int k = 0; k < 4; k++) entries.at(k) = (uint8_t)(size >> (8 * k));
    }
    const Bytes out = zone.Write();
    const OrasZone check = OrasZone::Read(out); // the events file's size rule and counts must still hold
    if ((int)check.Doors.size() != placed || check.Matrix() != (int)matrix) throw FormatError("zone " + std::to_string(zoneIndex) + " does not read back as written");
    log.push_back(F("zone %zu: matrix %d -> %zu, area pack %d kept; %d of its %d warps on %zu doors, %d removed%s; kept from Hoenn: %zu characters, %zu furniture, "
                    "%zu triggers, %zu other entries, its scripts; spawn tile (%.1f, %.1f) kept",
                    zoneIndex, before.Matrix(), matrix, before.AreaPack(), placed, warps, doors.size(), removed,
                    (int)doors.size() > warps ? " (doors without a warp lead nowhere)" : "", before.Characters.size(), before.Furniture.size(),
                    before.Triggers.size(), before.Others.size(), before.SpawnTileX(), before.SpawnTileZ()));
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

std::vector<std::string> BuildOrasRegion(const NdsRom& platinum, N3dsRom& oras, const OrasRegionOptions& o)
{
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
        plan += F("header %d: %d blocks -> %s\n", h, n, z == o.Zones.end() ? "no ORAS zone given" : z->second < 0 ? "left out" : F("ORAS zone %d", z->second).c_str());
        if (z == o.Zones.end()) missing += F(" --zone %d:<ORAS zone or -1>", h);
    }
    log.push_back(plan);
    if (o.PlanOnly) return log;
    if (!missing.empty()) throw FormatError("every map header in the region needs an ORAS zone or -1:" + missing);
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
        matrix.File1 = model.File1;
        matrix.Lead[0] = model.Lead[0]; matrix.Lead[1] = model.Lead[1]; // file 0's first two words, meaning unknown
    }
    for (size_t k = 0; k < headers.size(); k++)
        if (headers[k] >= 0 && o.Zones.at(headers[k]) >= 0) matrix.Zones[k] = (uint16_t)o.Zones.at(headers[k]);

    const PieceBudget budget = GamePieceBudget(pieceArchive);
    std::map<int, std::vector<std::pair<int, int>>> doorsOf; // ORAS zone -> its doors, in matrix tiles
    std::vector<GltfPart> parts;
    std::vector<GltfMaterial> materials;
    const std::filesystem::path out(o.OutDir);
    std::filesystem::create_directories(out);
    std::string layouts;
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
            to.AreaPack = (size_t)pack;
            const TownLayout layout = TownLayout::Read(world, (o.Left + x) * TownTiles, (o.Top + y) * TownTiles);
            char name[32];
            snprintf(name, sizeof name, "world%02d_%02d_%02d", o.ModelMatrix, x, y);
            if (strlen(name) != 13) throw FormatError("a piece's model name must be 13 characters as the game's (world<NN>_<x>_<y>)");
            log.push_back(F("piece (%d, %d), Sinnoh (%d, %d): zone %d, area pack %d, %zu doors", x, y, o.Left + x, o.Top + y, owner, pack, layout.Doors.size()));
            Bytes piece;
            try { piece = BuildTownPiece(layout, to, pieceArchive, areaArchive, budget, x, y, name, packs[pack], log); }
            catch (const FormatError& e) { throw FormatError(F("piece (%d, %d), Sinnoh (%d, %d): ", x, y, o.Left + x, o.Top + y) + e.what()); }
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
                if (z == o.Zones.end() || z->second < 0) { log.push_back(F("door at (%d, %d) of header %u: no ORAS zone, no warp", x * TownTiles + d.Column, y * TownTiles + d.Row, d.Zone)); continue; }
                doorsOf[z->second].push_back({x * TownTiles + d.Column, y * TownTiles + d.Row});
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
    log.push_back(F("matrix: a/0/4/0 member %zu, %d x %d pieces, file 0's first words and file 1 copied from matrix %zu", matrixIndex, o.Width, o.Height, o.MatrixTemplate));

    for (int z : used)
    {
        // the doors north to south, west to east, as TownLayout orders them within a piece
        auto& doors = doorsOf[z];
        std::sort(doors.begin(), doors.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second < b.second : a.first < b.first; });
        ReplaceMember(newZones, zoneArchive, (size_t)z, MoveZone(Plain(zoneArchive.Sub((size_t)z)), (size_t)z, matrixIndex, doors, log), "ZO");
        // where the zone's spawn tile (Littleroot's: where a new game and a save's warp land, inferred) falls on the new matrix
        const OrasZone& zone = zoneOf.at(z);
        const int bx = (int)(zone.SpawnTileX() / BlockTiles), bz = (int)(zone.SpawnTileZ() / BlockTiles);
        const bool inside = bx >= 0 && bz >= 0 && bx < bw && bz < bh;
        log.push_back(F("zone %d: its spawn tile (%.1f, %.1f) lies %s", z, zone.SpawnTileX(), zone.SpawnTileZ(),
                        !inside ? "outside the new matrix" : matrix.Zone(bx, bz) == z ? "on its own block"
                        : matrix.Zone(bx, bz) == OrasMatrix::None ? "on a block of no zone" : F("on a block of zone %d", matrix.Zone(bx, bz)).c_str()));
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
        if (data.size() < original->size()) throw FormatError(std::string(path) + ": shorter than the game's file; Azahar would leave its old tail");
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
