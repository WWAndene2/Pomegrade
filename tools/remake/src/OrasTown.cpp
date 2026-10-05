#include "OrasTown.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Bps.h"
#include "Garc.h"
#include "Gltf.h"
#include "NitroCompression.h"
#include "TownBuilder.h"

#include <filesystem>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }

// a member put back as the game keeps it: compressed when it was
static void SetMember(Garc& garc, const Garc& original, size_t index, const Bytes& data)
{
    garc.Set(index, IsLzCompressed(original.Sub(index)) ? Lz11Compress(data) : data);
    Bytes back = Garc(garc.Write()).Sub(index);
    if (IsLzCompressed(back)) back = LzDecompress(back);
    if (back != data) throw FormatError("member " + std::to_string(index) + " does not read back identical");
}

// the zone's warps moved onto the new doors (its entries hold the zone matrix's pixel position of a tile:
// (tile + 0.5) * 18, as u16 at +0x0C and +0x10), and its area pack set to the donor's
static Bytes MoveZoneWarps(const Bytes& zoneData, const OrasTownOptions& o, const TownLayout& layout, std::vector<std::string>& log)
{
    const bool lz = IsLzCompressed(zoneData);
    const Bytes plain = lz ? LzDecompress(zoneData) : zoneData;
    BinLinker zone = BinLinker::Read(plain, "ZO");
    if (zone.Write() != plain) throw FormatError("the zone does not rewrite identical");
    Bytes& entries = zone.Files.at(1);
    const int files = entries.at(4), npcs = entries.at(5), warps = entries.at(6);
    const size_t first = 8 + files * 0x14 + npcs * 0x30;
    const int placed = std::min<int>(warps, (int)layout.Doors.size());
    for (int k = 0; k < placed; k++)
    {
        const size_t at = first + k * 0x18;
        const int x = (o.CellX * TownTiles + layout.Doors[k].Column) * 18 + 9, y = (o.CellY * TownTiles + layout.Doors[k].Row) * 18 + 9;
        Put16(entries, at + 0xC, (uint16_t)x);
        Put16(entries, at + 0x10, (uint16_t)y);
    }
    log.push_back("zone " + std::to_string(o.Zone) + ": " + std::to_string(placed) + " of its " + std::to_string(warps) + " warps moved onto " +
                  std::to_string(layout.Doors.size()) + " doors" + (layout.Doors.size() > (size_t)warps ? " (the zone has no warp for the others)" : ""));
    Put16(zone.Files.at(0), 2, (uint16_t)o.AreaPack);
    const Bytes out = zone.Write();
    return lz ? Lz11Compress(out) : out;
}

// Pixels of one area pack's textures put under another pack's texture names (same size and format: a texture's data
// is not relocated, so it is overwritten in place). Pack contents that have no such texture are skipped, with a note.
// pairs: {name in the pack to change, name in the pack to copy from}
static Bytes CopyTexturePixels(const Bytes& packData, const Bytes& fromData, const std::vector<std::pair<std::string, std::string>>& pairs,
                               std::vector<std::string>& log, size_t& copied)
{
    BinLinker pack = BinLinker::Read(packData, "AD");
    if (pack.Write() != packData) throw FormatError("the area pack does not rewrite identical");
    const BinLinker from = BinLinker::Read(fromData, "AD");
    copied = 0;
    for (const auto& [to, source] : pairs)
    {
        const BchTexture* src = nullptr;
        std::vector<BchTexture> fromTextures;
        for (const Bytes& f : from.Files) if (Bch::Is(f)) for (BchTexture& t : Bch::Read(f).Textures) if (t.Name == source) { fromTextures.push_back(t); src = &fromTextures.back(); }
        bool done = false;
        for (Bytes& f : pack.Files)
        {
            if (!Bch::Is(f) || done) continue;
            for (const BchTexture& t : Bch::Read(f).Textures)
            {
                if (t.Name != to) continue;
                if (!src || src->Width != t.Width || src->Height != t.Height || src->Format != t.Format || src->Data.size() != t.Data.size())
                {
                    log.push_back("texture " + to + ": no " + source + " of the same size and format to copy, left as it is");
                    done = true;
                    break;
                }
                std::copy(src->Data.begin(), src->Data.end(), f.begin() + t.DataOffset);
                copied++;
                done = true;
                break;
            }
        }
        if (!done) log.push_back("texture " + to + " is not in the area pack, nothing to replace");
    }
    return pack.Write();
}

OrasTownResult BuildOrasTown(const NdsRom& platinum, N3dsRom& oras, const OrasTownOptions& o)
{
    OrasTownResult result;
    const PlatinumWorld world(platinum, o.Matrix);
    result.Layout = TownLayout::Read(world, o.Left, o.Top);
    for (const auto& [name, count] : result.Layout.UnknownTextures)
        result.Log.push_back("texture without a role: " + name + " (" + std::to_string(count) + " tiles), treated as grass: add it to TownLayout.cpp");

    // the three ORAS pieces
    const Bytes pieces = oras.Read("a/0/3/9");
    const Garc pieceArchive(pieces);
    auto piece = [&](size_t i) { return Plain(pieceArchive.Sub(i)); };
    TownSources sources;
    sources.Target = piece(o.TargetPiece); sources.Donor = piece(o.DonorPiece); sources.Trees = piece(o.TreePiece);
    sources.CellX = o.CellX; sources.CellY = o.CellY;

    // the grass: Littleroot's pixels under the donor's grass texture names, in the donor's area pack
    const Bytes areas = oras.Read("a/0/1/4");
    const Garc areaArchive(areas);
    Bytes areaPack = Plain(areaArchive.Sub(o.AreaPack));
    if (o.GrassPack >= 0)
    {
        size_t copied = 0;
        const std::vector<std::pair<std::string, std::string>> grass = {{"chip_kusa", "chip_kusa_a"}, {"chip_kusa_b", "chip_kusa_b"}, {"chip_kusa_edge", "chip_grass_edge"}};
        areaPack = CopyTexturePixels(areaPack, Plain(areaArchive.Sub((size_t)o.GrassPack)), grass, result.Log, copied);
        sources.TargetGrass = copied == grass.size();
        result.Log.push_back("area pack " + std::to_string(o.AreaPack) + ": " + std::to_string(copied) + " of " + std::to_string(grass.size()) + " grass textures replaced by area pack " + std::to_string(o.GrassPack) + "'s");
    }
    const Bytes town = BuildTown(result.Layout, sources, &result.Log);
    result.PieceBytes = town.size();

    Garc newPieces(pieces);
    SetMember(newPieces, pieceArchive, o.TargetPiece, town);
    const Bytes zones = oras.Read("a/0/1/3");
    const Garc zoneArchive(zones);
    Garc newZones(zones);
    SetMember(newZones, zoneArchive, o.Zone, MoveZoneWarps(zoneArchive.Sub(o.Zone), o, result.Layout, result.Log));

    // the mod: BPS patches of the two archives, each checked by applying it back
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path out(o.OutDir);
    const std::filesystem::path root = out / "load" / "mods" / id / "romfs_ext";
    Garc newAreas(areas);
    std::vector<std::tuple<const char*, Bytes, Bytes>> changed = {{"a/0/3/9", newPieces.Write(), pieces}, {"a/0/1/3", newZones.Write(), zones}};
    if (o.GrassPack >= 0)
    {
        SetMember(newAreas, areaArchive, o.AreaPack, areaPack);
        changed.emplace_back("a/0/1/4", newAreas.Write(), areas);
    }
    for (const auto& [path, data, original] : changed)
    {
        if (data.size() < original.size()) throw FormatError(std::string(path) + ": shorter than the game's file; Azahar would leave its old tail");
        const Bytes bps = BpsCreate(original, data);
        if (BpsApply(original, bps) != data) throw FormatError(std::string(path) + ": the patch does not rebuild the file");
        std::filesystem::create_directories((root / path).parent_path());
        WriteFile((root / (std::string(path) + ".bps")).string(), bps);
        result.Log.push_back(std::string(path) + ": patch " + std::to_string(bps.size()) + " bytes, checked");
    }

    // the preview: the new piece's terrain with the donor's area pack textures
    std::vector<BchTexture> textures;
    for (const Bytes& f : BinLinker::Read(areaPack, "AD").Files)
        if (Bch::Is(f)) for (BchTexture& t : Bch::Read(f).Textures) textures.push_back(std::move(t));
    std::vector<GltfPart> parts;
    std::vector<GltfMaterial> materials;
    const float origin[3] = {0, 0, 0};
    BchModel model = Bch::Read(BinLinker::Read(town, "GR").Files.at(1)).Models.at(0);
    // shadow decals left out: a preview cannot blend them as the console does
    std::vector<BchMesh> kept;
    for (const BchMesh& m : model.Meshes)
    {
        const BchMaterial& mat = model.Materials[m.Material];
        if (mat.Texture[0].find("shadow") == std::string::npos && mat.Texture[1].find("shadow") == std::string::npos) kept.push_back(m);
    }
    model.Meshes = kept;
    AppendBchModel(model, textures, origin, parts, materials);
    const std::string gltf = WriteGltf(parts, materials);
    WriteFile((out / "town_preview.gltf").string(), Bytes(gltf.begin(), gltf.end()));
    const std::string text = result.Layout.Text();
    WriteFile((out / "town_layout.txt").string(), Bytes(text.begin(), text.end()));
    return result;
}

}
