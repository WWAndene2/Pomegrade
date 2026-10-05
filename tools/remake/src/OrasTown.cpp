#include "OrasTown.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Bps.h"
#include "BchTextureFile.h"
#include "Garc.h"
#include "Gltf.h"
#include "NitroCompression.h"
#include "PicaTexture.h"
#include "TownBuilder.h"
#include "TownCheck.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <tuple>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }

void ReplaceMember(Garc& archive, const Garc& original, size_t index, const Bytes& plain, const std::string& tag)
{
    if (plain.size() < tag.size() || Text(plain, 0, tag.size()) != tag)
        throw FormatError("member " + std::to_string(index) + " is not a " + tag + " container (already compressed?)");
    archive.Set(index, IsLzCompressed(original.Sub(index)) ? Lz11Compress(plain) : plain);
    Bytes back = Garc(archive.Write()).Sub(index);
    if (IsLzCompressed(back)) back = LzDecompress(back);
    if (back != plain) throw FormatError("member " + std::to_string(index) + " does not read back identical");
}

// the zone's warps moved onto the new doors (its entries hold the zone matrix's pixel position of a tile:
// (tile + 0.5) * 18, as u16 at +0x0C and +0x10), and its area pack set to the donor's
static Bytes MoveZoneWarps(const Bytes& zoneData, const OrasTownOptions& o, const TownLayout& layout, std::vector<std::string>& log)
{
    const Bytes plain = IsLzCompressed(zoneData) ? LzDecompress(zoneData) : zoneData;
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
    return zone.Write(); // plain: ReplaceMember compresses it as the original was
}

// Textures of another area pack added to this one, under their own names (the pack's existing textures are untouched: other pieces of the
// area keep theirs). A wanted name the pack already holds with other content gets a suffix. The pack's main texture file (slot 11) is
// rewritten with BchWriteTextureFile; the result says each wanted name's final name, empty when the other pack has no such texture.
static Bytes ImportTextures(const Bytes& packData, const Bytes& fromData, size_t fromIndex, const std::vector<std::string>& wanted,
                            std::map<std::string, std::string>& finalName, std::vector<std::string>& log)
{
    BinLinker pack = BinLinker::Read(packData, "AD");
    if (pack.Write() != packData) throw FormatError("the area pack does not rewrite identical");
    const BinLinker from = BinLinker::Read(fromData, "AD");
    std::vector<BchTextureSource> all;
    std::set<std::string> taken;
    auto read = [&](const Bytes& file, bool own) {
        std::vector<BchTexture> list;
        if (file.empty() || !Bch::Is(file)) return list;
        for (BchTexture& t : Bch::Read(file).Textures) { if (own) taken.insert(t.Name); list.push_back(std::move(t)); }
        return list;
    };
    for (size_t slot : {(size_t)1, (size_t)11}) read(pack.Files.at(slot), true);
    for (BchTexture& t : read(pack.Files.at(11), false)) all.push_back({t.Name, t.Width, t.Height, t.Format, t.Data});
    size_t added = 0;
    for (const std::string& name : wanted)
    {
        const BchTexture* src = nullptr;
        std::vector<BchTexture> held[2] = {read(from.Files.at(1), false), read(from.Files.at(11), false)};
        for (auto& list : held) for (const BchTexture& t : list) if (t.Name == name && !t.Data.empty()) src = &t;
        if (!src) { finalName[name] = ""; log.push_back("texture " + name + " is not in area pack " + std::to_string(fromIndex)); continue; }
        std::string final = name;
        const auto same = std::find_if(all.begin(), all.end(), [&](const BchTextureSource& t) { return t.Name == name; });
        if (same != all.end() && same->Width == src->Width && same->Height == src->Height && same->Format == src->Format && same->Data == src->Data) { finalName[name] = name; continue; }
        if (taken.count(name)) final = name + "_" + std::to_string(fromIndex);
        taken.insert(final);
        all.push_back({final, src->Width, src->Height, src->Format, src->Data});
        finalName[name] = final;
        added++;
    }
    if (!added) return packData;
    pack.Files.at(11) = BchWriteTextureFile(all);
    log.push_back(std::to_string(added) + " texture(s) added to the area pack's main texture file (" + std::to_string(all.size()) + " in all)");
    return pack.Write();
}

// A texture made here, added to the pack's main texture file (slot 11) under its name, the pack's own textures untouched
static Bytes AddTexture(const Bytes& packData, const BchTextureSource& added, std::vector<std::string>& log)
{
    BinLinker pack = BinLinker::Read(packData, "AD");
    std::vector<BchTextureSource> all;
    for (const BchTexture& t : Bch::Read(pack.Files.at(11)).Textures)
    {
        if (t.Name == added.Name) throw FormatError("the area pack already holds a texture named " + added.Name);
        all.push_back({t.Name, t.Width, t.Height, t.Format, t.Data});
    }
    all.push_back(added);
    pack.Files.at(11) = BchWriteTextureFile(all);
    log.push_back("texture " + added.Name + " made and added to the area pack's main texture file (" + std::to_string(all.size()) + " in all)");
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

    // the grass: Littleroot's textures, added to the donor's area pack under their own names, and shown by the ground's materials
    const Bytes areas = oras.Read("a/0/1/4");
    const Garc areaArchive(areas);
    Bytes areaPack = Plain(areaArchive.Sub(o.AreaPack));
    if (o.GrassPack >= 0)
    {
        std::map<std::string, std::string> finalName;
        areaPack = ImportTextures(areaPack, Plain(areaArchive.Sub((size_t)o.GrassPack)), (size_t)o.GrassPack, {"chip_kusa_a", "chip_kusa_b", "chip_grass_edge"}, finalName, result.Log);
        sources.TargetGrass = !finalName["chip_kusa_a"].empty() && !finalName["chip_kusa_b"].empty() && !finalName["chip_grass_edge"].empty();
        sources.GroundTexture = finalName["chip_kusa_a"]; sources.LightTexture = finalName["chip_kusa_b"]; sources.EdgeTexture = finalName["chip_grass_edge"];
    }
    if (o.SnowPack >= 0)
    {
        std::map<std::string, std::string> finalName;
        areaPack = ImportTextures(areaPack, Plain(areaArchive.Sub((size_t)o.SnowPack)), (size_t)o.SnowPack, {"chip_icedoukutsu02"}, finalName, result.Log);
        sources.SnowTexture = finalName["chip_icedoukutsu02"];
    }
    if (o.SnowClumps && !sources.SnowTexture.empty())
    {
        // chip_alpha (Littleroot's area pack, 8): its shapes (alpha) kept, its colour the cave's snow (chip_icedoukutsu02, at
        // the same texel, the snow tiling): the stone cluster becomes a clump of snow
        const Bch littleroot = Bch::Read(BinLinker::Read(Plain(areaArchive.Sub(8)), "AD").Files.at(11));
        const Bch snowFile = Bch::Read(BinLinker::Read(Plain(areaArchive.Sub((size_t)o.SnowPack)), "AD").Files.at(11));
        const BchTexture *shapes = nullptr, *snow = nullptr;
        for (const BchTexture& t : littleroot.Textures) if (t.Name == "chip_alpha") shapes = &t;
        for (const BchTexture& t : snowFile.Textures) if (t.Name == "chip_icedoukutsu02") snow = &t;
        if (shapes && snow)
        {
            Bytes rgba = PicaTextureDecode(shapes->Data, shapes->Width, shapes->Height, shapes->Format);
            const Bytes white = PicaTextureDecode(snow->Data, snow->Width, snow->Height, snow->Format);
            for (uint32_t y = 0; y < shapes->Height; y++)
                for (uint32_t x = 0; x < shapes->Width; x++)
                {
                    uint8_t* p = &rgba[((size_t)y * shapes->Width + x) * 4];
                    const uint8_t* q = &white[((size_t)(y % snow->Height) * snow->Width + x % snow->Width) * 4];
                    p[0] = q[0]; p[1] = q[1]; p[2] = q[2];
                }
            areaPack = AddTexture(areaPack, {"snow_clump", shapes->Width, shapes->Height, 0, PicaTextureEncodeRgba8(rgba, shapes->Width, shapes->Height)}, result.Log);
            sources.SnowClumpTexture = "snow_clump";
        }
        else result.Log.push_back("snow clumps: chip_alpha or chip_icedoukutsu02 not found, none laid");
    }
    if (o.PondWall == 1)
    {
        // chip_gake_b (256 rows): its grass lip from row 140, stones below; the wall shows rows 140 to 204
        std::map<std::string, std::string> finalName;
        areaPack = ImportTextures(areaPack, Plain(areaArchive.Sub(8)), 8, {"chip_gake_b"}, finalName, result.Log);
        sources.BankTexture = finalName["chip_gake_b"];
        sources.BankV[0] = 140.0f / 256; sources.BankV[1] = 204.0f / 256;
    }
    if (o.PondWall == 2) { sources.BankTexture = "chip_soil_a"; sources.BankV[0] = 0; sources.BankV[1] = 0.25f; } // the paths': in the pack
    if (o.FencePack >= 0)
    {
        std::map<std::string, std::string> finalName;
        areaPack = ImportTextures(areaPack, Plain(areaArchive.Sub((size_t)o.FencePack)), (size_t)o.FencePack, {"c103_saku"}, finalName, result.Log);
        sources.FenceTexture = finalName["c103_saku"];
    }
    const Bytes town = BuildTown(result.Layout, sources, &result.Log);
    result.PieceBytes = town.size();

    // design rules (TownCheck.h): the textures the piece names must be in the area pack it will use; the size against the game's largest piece
    {
        std::set<std::string> available;
        for (const Bytes& f : BinLinker::Read(areaPack, "AD").Files)
            if (Bch::Is(f)) for (const BchTexture& t : Bch::Read(f).Textures) available.insert(t.Name);
        PieceBudget original;
        for (size_t i = 0; i < pieceArchive.Count(); i++)
        {
            if (!pieceArchive.Has(i)) continue;
            try
            {
                const Bytes raw = Plain(pieceArchive.Sub(i));
                if (raw.size() < 2 || raw[0] != 'G' || raw[1] != 'R') continue;
                const Bch bch = Bch::Read(BinLinker::Read(raw, "GR").Files.at(1)); // kept alive while its meshes are walked
                size_t vertices = 0;
                for (const BchMesh& m : bch.Models.at(0).Meshes) vertices += m.Vertices.size();
                original.MaxVertices = std::max(original.MaxVertices, vertices);
                original.MaxFileBytes = std::max(original.MaxFileBytes, raw.size());
            }
            catch (const FormatError&) {} // a piece that does not read does not set the bound
        }
        const BchModel model = Bch::Read(BinLinker::Read(town, "GR").Files.at(1)).Models.at(0);
        size_t vertices = 0;
        for (const BchMesh& m : model.Meshes) vertices += m.Vertices.size();
        result.Log.push_back("design rules: " + std::to_string(available.size()) + " textures in the area pack; piece " + std::to_string(town.size()) + " bytes, " + std::to_string(vertices) +
                             " vertices (the game's largest piece: " + std::to_string(original.MaxFileBytes) + " bytes, " + std::to_string(original.MaxVertices) + " vertices)");
        std::vector<TownIssue> issues = CheckMaterials(model, available);
        for (const TownIssue& i : CheckLayout(BinLinker::Read(town, "GR").Files.at(0), BinLinker::Read(town, "GR").Files.at(3))) issues.push_back(i);
        for (const TownIssue& i : CheckBudget(model, town.size(), original)) issues.push_back(i);
        std::string refusal;
        for (const TownIssue& i : issues)
        {
            result.Log.push_back(std::string(i.Error ? "ERROR: " : "warning: ") + i.Text);
            if (i.Error) refusal += (refusal.empty() ? "" : "; ") + i.Text;
        }
        if (!refusal.empty() && !o.AllowErrors) throw FormatError("the piece breaks a design rule, no mod written (--allow-errors to write it anyway): " + refusal);
    }

    Garc newPieces(pieces);
    ReplaceMember(newPieces, pieceArchive, o.TargetPiece, town, "GR");
    const Bytes zones = oras.Read("a/0/1/3");
    const Garc zoneArchive(zones);
    Garc newZones(zones);
    ReplaceMember(newZones, zoneArchive, o.Zone, MoveZoneWarps(zoneArchive.Sub(o.Zone), o, result.Layout, result.Log), "ZO");

    // the mod: BPS patches of the two archives, each checked by applying it back
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path out(o.OutDir);
    const std::filesystem::path root = out / "load" / "mods" / id / "romfs_ext";
    Garc newAreas(areas);
    std::vector<std::tuple<const char*, Bytes, Bytes>> changed = {{"a/0/3/9", newPieces.Write(), pieces}, {"a/0/1/3", newZones.Write(), zones}};
    if (o.GrassPack >= 0)
    {
        ReplaceMember(newAreas, areaArchive, o.AreaPack, areaPack, "AD");
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
    // the piece the mod writes, decompressed: `remake_tool topview town_piece.bin ...` and `mesh-json` read it as they read the game's
    WriteFile((out / "town_piece.bin").string(), town);
    return result;
}

}
