// remake_tool: looks into DS cartridges for the Pokemon remake work.
//   remake_tool info <rom.nds>                        title, game code, file count
//   remake_tool inventory <rom.nds>                   every file and archive member, as JSON
//   remake_tool extract <rom.nds> <path> <out>        one file (LZ data decompressed)
//   remake_tool unpack <rom.nds> <path.narc> <dir>    every member of a NARC, decompressed
//   remake_tool garc <file.garc> <dir>                every sub-file of a 3DS GARC, decompressed
//   remake_tool textures <file.nsbtx|.nsbmd> <dir>    every texture as PNG
//   remake_tool model <file.nsbmd> <out.gltf> [tex.nsbtx] [model index]   a DS model as glTF
//   remake_tool texindex <rom.nds>                    every texture under its emulator dump name (TSV)
//   remake_tool identify <rom.nds> <dump dir>         which ROM file each dumped texture comes from
//   remake_tool world <map_matrix.narc> <matrix index> <land_data.narc> <out dir>
//                     [map textures.nsbtx|-] [buildings.narc|-] [building textures.nsbtx|-]
//                     the world: world.json, collision.png, world.gltf
//   remake_tool oras-world <rom.nds> <matrix index> <out dir>
//                     a Platinum map matrix translated to Omega Ruby / Alpha Sapphire's scale and
//                     40-tile map pieces, with its warps and each cell's own textures:
//                     world_oras.json (the editor's file), world_oras.gltf
//   remake_tool bch <file.bch|GR piece> <out.gltf> [textures...]
//                     a 3DS model (ORAS map pieces: their terrain) as glTF; textures: BCH files or
//                     ORAS area packs (a/0/1/4 entries: "AD") whose textures the model uses
//   remake_tool oras-list <oras.3ds>                  a decrypted 3DS game's RomFS files, with sizes
//   remake_tool oras-extract <oras.3ds> <path> <out>  one RomFS file
//   remake_tool oras-find <oras.3ds> <text>...
//   remake_tool oras-members <oras.3ds> <archive> <first> <last>
//   remake_tool oras-hex <oras.3ds> <archive> <member> <file|-1> <offset> <length>
//   remake_tool oras-member <oras.3ds> <archive> <member> <file|-1> <out>
//   remake_tool oras-asset <oras.3ds> <archive> <member> <file|-1> [list] | export <dir> | edit <out dir> recolour:<texture>:<hue>:<sat>:<bright>[:<around>:<range>]...
//   remake_tool oras-layout <oras.3ds> <archive> <member> [file]  DARC/BCLYT/BCLAN/BCLIM described
//   remake_tool oras-text <oras.3ds> <archive> <member>   a game text file's lines
//   remake_tool oras-script <oras.3ds> <zone>|all [init]  a zone's script, unpacked and disassembled (all: every script, checked)
//   remake_tool oras-copy <oras.3ds> <out dir> <archive> <dst>[-<dst last>]=<src>[-<src last>] | <dst>=<archive>:<member>:<file> | <dst>=motion:<archive>:<member>:<file>:<slot>[:<frames>] ...
//   remake_tool oras-mod <oras.3ds> <out dir> <path>=<file>...
//                     an Azahar mod: each file replaces that RomFS path, laid out as Azahar loads
//                     mods (<out dir>/load/mods/<program id>/romfs/<path>); copy <out dir>/load
//                     into the 3DS folder (Pomegrade/3DS)
//   remake_tool oras-town <platinum.nds> <oras.3ds> <out dir> [--matrix N] [--left X --top Y] [--target P --donor P --trees P]
//                     [--cell X Y] [--zone Z] [--area A] [--donor-pack P] [--grass P] [--snow P] [--fence P] [--snow-clumps 0|1] [--pond-wall 0-2] [--zone-pack 0|1] [--zone-warps 0|1] [--add-warps 0|1] [--piece 0|1] [--tree-reach N] [--door-type T] [--donor-as-is 0|1] [--pad-piece BYTES] [--piece-files MASK] [--allow-errors]
//                     a Platinum window of 40x40 tiles (default: Twinleaf Town) rebuilt as an ORAS map piece with
//                     ORAS's own assets, as an Azahar mod (BPS patches), with town_preview.gltf, town_layout.txt and
//                     town_piece.bin (the piece the mod writes, decompressed)
//   remake_tool oras-code <oras.3ds> <out.bin>   the game's ExeFS .code, decompressed (ARM, loaded at 0x100000)
//   remake_tool oras-region <platinum.nds> <oras.3ds> <out dir> --rect LEFT TOP WIDTH HEIGHT --zone HEADER:ZONE... [--auto-zones] [--others-out] [--new-zones] [--no-triggers] [--no-characters] [--plan] [--matrix-template M]
//                     [--model-matrix NN] [oras-town's kit options]
//                     a rectangle of Sinnoh's piece grid (as oras-world cuts it) rebuilt as a new ORAS map matrix: its pieces built
//                     as oras-town builds one, the zone grid from Platinum's map headers, each header on the ORAS zone given
//                     (-1: left out); --plan prints the rectangle's headers and builds nothing. Writes the mod, region_preview.gltf,
//                     region_plan.txt and region_piece_<x>_<y>.bin
//   remake_tool oras-sandbox <oras.3ds> <out dir> <description.txt>
//   remake_tool oras-save <main> [<out> <zone> <tile x> <tile z> [--template <save> [--blocks A,B,...]]
//                     where an ORAS save puts the player (OrasSave.h); with the rest, a copy with the player moved there
//   remake_tool oras-sinnoh <platinum.nds> <oras.3ds> [strip width, default 5]
//                     the plan for all of Sinnoh, nothing written: strips of whole piece columns (one matrix each), their
//                     oras-region rectangles and map headers, the edge warps between strips, the ORAS zones needed and available
//   remake_tool oras-texture <oras.3ds> <area pack> <name> <out.png>   one texture of an area pack (a/0/1/4 member), as a PNG
//   remake_tool mesh-json <GR piece file|file.bch> <out.json>   a terrain model's meshes as JSON, to measure them: for each
//                     mesh its index, layer, material, textures, vertices [x, z, y, u, v, r, g, b, a] and triangles
//   remake_tool oras-inspect <oras.3ds> zone|piece|area|matrix|piece-names|zones|matrices|archives|piece-budget|coll-sizes|zone-reach <index>
//   remake_tool oras-topview <oras.3ds> <piece> <out.png> [--grid] [--tiles] [--doors] [--points] [--px N]   (topview <GR file> <out.png> for a mod's piece)
//                     what an ORAS zone (a/0/1/3), map piece (a/0/3/9) or area pack (a/0/1/4) is made of, as text
//   remake_tool oras-verify <oras.3ds>                the tooling's readers and writers checked against the real game (exit 1 on a failure)
//   remake_tool oras-catalog <oras.3ds> <out dir>     packs.tsv and pieces.tsv: every area pack's textures, every map piece's meshes and materials
//   remake_tool oras-patch <oras.3ds> <out dir> <path>=<file>...
//                     the same as BPS patches against the game's own files (<out dir>/load/mods/
//                     <program id>/romfs_ext/<path>.bps): a few changed pieces of a large archive
//                     make a small mod; each patch is applied back and checked before it is written
#include "Inventory.h"
#include "Narc.h"
#include "NdsRom.h"
#include "NitroCompression.h"
#include "FormatSniffer.h"
#include "Garc.h"
#include "Nsbmd.h"
#include "Png.h"
#include "PicaTexture.h"
#include "TextureIndex.h"
#include "AreaData.h"
#include "AssetEdit.h"
#include "Bch.h"
#include "TownCheck.h"
#include "Bps.h"
#include "BinLinker.h"
#include "GfMotion.h"
#include "Layout.h"
#include "Darc.h"
#include "Bclim.h"
#include "OrasZone.h"
#include "GameText.h"
#include "Amx.h"
#include "N3dsRom.h"
#include "MapHeaders.h"
#include "N3dsWorld.h"
#include "OrasInspect.h"
#include "OrasMeasure.h"
#include "TopView.h"
#include "OrasAppend.h"
#include "OrasEngine.h"
#include "OrasRegion.h"
#include "OrasSandbox.h"
#include "OrasSave.h"
#include "OrasTown.h"
#include "PlatinumWorld.h"
#include "ZoneEvents.h"
#include "WorldMap.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <map>
#include <functional>
#include <set>
#include <string>

using namespace remake;

static int Usage()
{
    fprintf(stderr, "usage:\n  remake_tool info <rom.nds>\n  remake_tool inventory <rom.nds>\n"
                    "  remake_tool extract <rom.nds> <path> <out>\n  remake_tool unpack <rom.nds> <path.narc> <dir>\n"
                    "  remake_tool garc <file.garc> <dir>\n  remake_tool textures <file.nsbtx|.nsbmd> <dir>\n"
                    "  remake_tool model <file.nsbmd> <out.gltf> [tex.nsbtx] [model index]\n"
                    "  remake_tool texindex <rom.nds>\n  remake_tool identify <rom.nds> <dump dir>\n"
                    "  remake_tool world <map_matrix.narc> <matrix index> <land_data.narc> <out dir>\n"
                    "                    [map textures.nsbtx|-] [buildings.narc|-] [building textures.nsbtx|-]\n"
                    "  remake_tool oras-world <rom.nds> <matrix index> <out dir>\n  remake_tool platinum-zones <rom.nds>\n"
                    "  remake_tool bch <file.bch|GR piece> <out.gltf> [textures...]\n  remake_tool oras-list <oras.3ds>\n  remake_tool oras-extract <oras.3ds> <path> <out>\n"
                    "  remake_tool oras-mod <oras.3ds> <out dir> <path>=<file>...\n"
                    "  remake_tool oras-inspect <oras.3ds> zone|piece|area|matrix|piece-names|zones|matrices|archives|piece-budget|coll-sizes|zone-reach <index>\n"
                    "  remake_tool oras-measure <oras.3ds> <out dir>\n"
                    "  remake_tool oras-topview <oras.3ds> <piece> <out.png> [--grid] [--tiles] [--doors] [--points] [--px N]\n"
                    "  remake_tool topview <GR piece file> <out.png> [--grid] [--tiles] [--doors] [--points] [--px N]\n"
                    "  remake_tool oras-verify <oras.3ds>\n  remake_tool oras-catalog <oras.3ds> <out dir>\n"
                    "  remake_tool oras-texture <oras.3ds> <area pack> <name> <out.png>\n  remake_tool mesh-json <GR piece file|file.bch> <out.json>\n"
                    "  remake_tool oras-patch <oras.3ds> <out dir> <path>=<file>...\n"
                    "  remake_tool oras-append-test <oras.3ds> <out dir> unused|piece|matrix|zone|zone-raised|crowd [N]\n"
                    "  remake_tool oras-zone-check <oras.3ds>\n"
                    "  remake_tool oras-engine <oras.3ds> <out dir> [--memory 64|72|80|96|124] [--linear-heap BYTES] [--normal-heap BYTES] [--zone-rows N [--script-mask ZONE:MASK]...] [--characters N] [--heap ID:BYTES]...\n"
                    "  remake_tool oras-town <platinum.nds> <oras.3ds> <out dir> [--matrix N] [--left X --top Y] [--target P --donor P --trees P]\n"
                    "                    [--cell X Y] [--zone Z] [--area A] [--donor-pack P] [--grass P] [--snow P] [--fence P] [--snow-clumps 0|1] [--pond-wall 0-2] [--zone-pack 0|1] [--zone-warps 0|1] [--add-warps 0|1] [--piece 0|1] [--tree-reach N] [--door-type T] [--donor-as-is 0|1] [--pad-piece BYTES] [--piece-files MASK] [--allow-errors]\n"
                    "  remake_tool oras-code <oras.3ds> <out.bin>\n"
                    "  remake_tool oras-region <platinum.nds> <oras.3ds> <out dir> --rect LEFT TOP WIDTH HEIGHT --zone HEADER:ZONE... [--plan]\n"
                    "  remake_tool oras-sinnoh <platinum.nds> <oras.3ds> [strip width]\n"
                    "  remake_tool oras-sandbox <oras.3ds> <out dir> <description.txt>\n"
                    "  remake_tool oras-save <main> [<out> <zone> <tile x> <tile z> [--template <save> [--blocks A,B,...]]\n"
                    "                    [--matrix-template M] [--model-matrix NN] [oras-town's --matrix, --target, --donor, --trees, --donor-pack, --grass, ... --allow-errors]\n");
    return 2;
}

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

// one file of a 2-letter container (a member LZ-decompressed), checked to be one
static Bytes ContainerFile(const Bytes& plain, size_t file)
{
    if (plain.size() < 2) throw FormatError("a member of " + std::to_string(plain.size()) + " bytes is no container");
    return BinLinker::Read(plain, std::string(plain.begin(), plain.begin() + 2)).Files.at(file);
}

// a member, LZ-decompressed, or one file of its container (file -1: the member itself)
static Bytes MemberOrFile(const Garc& archive, size_t member, int file)
{
    const Bytes plain = Plain(archive.Sub(member));
    return file >= 0 ? ContainerFile(plain, (size_t)file) : plain;
}

// an edited archive as the mod Azahar loads: a BPS patch of the game's file, checked by applying it back, or, when the
// archive came out shorter, the archive whole under romfs/; an LZ member last written gets zero padding first so the archive
// keeps its size (an LZ stream ends at its declared size: the zeros are never read; whole, a/0/0/8 made a 930 MB mod, t2)
static int WriteArchiveMod(Garc& g, const Bytes& original, size_t last, const std::string& path, const std::filesystem::path& base)
{
    Bytes data = g.Write();
    if (data.size() < original.size() && last != SIZE_MAX && IsLzCompressed(g.Sub(last)))
    {
        Bytes padded = g.Sub(last);
        const size_t pad = (original.size() - data.size() + 3) / 4 * 4;
        padded.resize(padded.size() + pad, 0);
        g.Set(last, padded);
        data = g.Write();
        printf("%s member %zu: %zu bytes of padding after its LZ data, the archive keeps its size\n", path.c_str(), last, pad);
    }
    Garc check(data);
    if (data.size() < original.size())
    {
        std::filesystem::create_directories((base / "romfs" / path).parent_path());
        WriteFile((base / "romfs" / path).string(), data);
        printf("%s: %zu bytes, shorter than the game's %zu: written whole under romfs/\n", path.c_str(), data.size(), original.size());
    }
    else
    {
        const Bytes bps = BpsCreate(original, data);
        if (BpsApply(original, bps) != data) { fprintf(stderr, "%s: the patch does not rebuild the file\n", path.c_str()); return 1; }
        std::filesystem::create_directories((base / "romfs_ext" / path).parent_path());
        WriteFile((base / "romfs_ext" / (path + ".bps")).string(), bps);
        printf("%s: patch %zu bytes, checked\n", path.c_str(), bps.size());
    }
    return 0;
}

// an option's number: decimal, or hexadecimal after 0x (masks: --piece-files 0x7E); anything else is refused rather than read
// as 0, as atoi did (four headless variants of --piece-files 0x7E.. each built Littleroot's whole piece, run 37546725658)
static int OptionNumber(const char* text, const std::string& flag)
{
    const std::string t = text;
    const bool hex = t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X');
    size_t used = 0;
    long v = 0;
    try { v = std::stol(hex ? t.substr(2) : t, &used, hex ? 16 : 10); } catch (const std::exception&) { used = 0; }
    if (used == 0 || used != (hex ? t.size() - 2 : t.size())) throw FormatError("not a number after " + flag + ": " + t);
    return (int)v;
}

// the options oras-town and oras-region share: Platinum's matrix, the kit pieces and packs, the builder's switches. false: not one of them
static bool TownKitOption(const std::string& flag, int argc, char** argv, int& i, OrasTownOptions& options)
{
    auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return OptionNumber(argv[at], flag); };
    if (flag == "--matrix") options.Matrix = (size_t)number(++i);
    else if (flag == "--target") options.TargetPiece = (size_t)number(++i);
    else if (flag == "--donor") options.DonorPiece = (size_t)number(++i);
    else if (flag == "--trees") options.TreePiece = (size_t)number(++i);
    else if (flag == "--donor-pack") options.DonorPack = (size_t)number(++i);
    else if (flag == "--grass") options.GrassPack = number(++i);
    else if (flag == "--snow") options.SnowPack = number(++i);
    else if (flag == "--fence") options.FencePack = number(++i);
    else if (flag == "--snow-clumps") options.SnowClumps = number(++i) != 0;
    else if (flag == "--pond-wall") options.PondWall = number(++i);
    else if (flag == "--door-type") options.DoorType = (uint32_t)number(++i);
    else if (flag == "--tree-reach") options.TreeReach = number(++i);
    else if (flag == "--donor-as-is") options.DonorAsIs = number(++i) != 0;
    else if (flag == "--pad-piece") options.PadPiece = (size_t)number(++i);
    else if (flag == "--piece-files") options.PieceFiles = (unsigned)number(++i);
    else if (flag == "--allow-errors") options.AllowErrors = true;
    else return false;
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 3) return Usage();
    const std::string cmd = argv[1];
    try
    {
        // loose files
        if (cmd == "garc" && argc >= 4)
        {
            const Garc garc(Plain(ReadFile(argv[2])));
            std::filesystem::create_directories(argv[3]);
            size_t written = 0;
            for (size_t i = 0; i < garc.Count(); i++)
                for (size_t s = 0; s < garc.SubCount(i); s++)
                {
                    if (!garc.Has(i, s)) continue;
                    const Bytes m = Plain(garc.Sub(i, s));
                    WriteFile((std::filesystem::path(argv[3]) / (std::to_string(i) + (s ? "_" + std::to_string(s) : "") + "." + Sniff(m).Id)).string(), m);
                    written++;
                }
            printf("%zu entries, %zu files\n", garc.Count(), written);
            return 0;
        }
        if (cmd == "bch" && argc >= 4)
        {
            Bytes file = Plain(ReadFile(argv[2]));
            if (Text(file, 0, 2) == "GR") file = BinLinker::Read(file, "GR").Files.at(1); // a map piece: its terrain
            Bch bch = Bch::Read(file);
            // textures from other files: BCH files, or containers holding them (an ORAS area: "AD")
            for (int i = 4; i < argc; i++)
            {
                const Bytes t = Plain(ReadFile(argv[i]));
                std::vector<Bytes> files;
                if (Bch::Is(t)) files.push_back(t);
                else if (t.size() > 4 && std::isalpha(t[0]) && std::isalpha(t[1]))
                    files = BinLinker::Read(t, Text(t, 0, 2)).Files;
                for (const Bytes& f : files)
                {
                    if (!Bch::Is(f)) continue;
                    Bch more = Bch::Read(f);
                    for (BchTexture& x : more.Textures) bch.Textures.push_back(std::move(x));
                }
            }
            std::vector<GltfPart> parts;
            std::vector<GltfMaterial> materials;
            const float origin[3] = {0, 0, 0};
            size_t meshes = 0, triangles = 0;
            for (const BchModel& m : bch.Models)
            {
                AppendBchModel(m, bch.Textures, origin, parts, materials);
                meshes += m.Meshes.size();
                for (const BchMesh& mesh : m.Meshes) triangles += mesh.Triangles.size() / 3;
            }
            const std::string gltf = WriteGltf(parts, materials);
            WriteFile(argv[3], Bytes(gltf.begin(), gltf.end()));
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
            for (const GltfPart& p : parts)
                for (const GxVertex& v : p.Mesh.Vertices)
                    for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], v.Position[k]); hi[k] = std::max(hi[k], v.Position[k]); }
            size_t textured = 0;
            for (const GltfMaterial& m : materials) textured += !m.Png.empty();
            printf("BCH version 0x%x: %zu models, %zu meshes, %zu triangles, %zu textures, %zu of %zu materials textured; bounds x %g..%g y %g..%g z %g..%g\n", bch.Version,
                   bch.Models.size(), meshes, triangles, bch.Textures.size(), textured, materials.size(), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]);
            return 0;
        }
        if (cmd == "textures" && argc >= 4)
        {
            const Bytes file = Plain(ReadFile(argv[2]));
            const long at = Tex0::Find(file);
            if (at < 0) { fprintf(stderr, "no TEX0 block\n"); return 1; }
            const Tex0 tex(file, (size_t)at);
            std::filesystem::create_directories(argv[3]);
            for (size_t t = 0; t < tex.Textures().size(); t++)
            {
                const Tex0Texture& x = tex.Textures()[t];
                try { WriteFile((std::filesystem::path(argv[3]) / (x.Name + ".png")).string(), EncodePng(x.Format.Width, x.Format.Height, tex.Decode(t))); }
                catch (const FormatError& e) { fprintf(stderr, "%s: %s\n", x.Name.c_str(), e.what()); }
            }
            printf("%zu textures, %zu palettes\n", tex.Textures().size(), tex.Palettes().size());
            return 0;
        }
        if (cmd == "model" && argc >= 4)
        {
            const Nsbmd model(Plain(ReadFile(argv[2])));
            std::unique_ptr<Tex0> tex;
            Bytes texFile;
            if (argc >= 5 && std::string(argv[4]) != "-")
            {
                texFile = Plain(ReadFile(argv[4]));
                const long at = Tex0::Find(texFile);
                if (at >= 0) tex = std::make_unique<Tex0>(texFile, (size_t)at);
            }
            const size_t index = argc >= 6 ? (size_t)atoi(argv[5]) : 0;
            const std::string gltf = ModelToGltf(model, index, tex.get());
            WriteFile(argv[3], Bytes(gltf.begin(), gltf.end()));
            printf("%zu models; model %zu: %zu shapes, %zu materials\n", model.Models().size(), index,
                   model.Models().at(index).Shapes.size(), model.Models().at(index).Materials.size());
            return 0;
        }
        if ((cmd == "oras-verify" && argc >= 3) || (cmd == "oras-catalog" && argc >= 4))
        {
            N3dsRom game(argv[2]);
            if (cmd == "oras-catalog") { fputs(CatalogGame(game, argv[3]).c_str(), stdout); return 0; }
            bool passed = false;
            fputs(VerifyGame(game, passed).c_str(), stdout);
            return passed ? 0 : 1;
        }
        if (cmd == "oras-inspect" && argc >= 5)
        {
            N3dsRom game(argv[2]);
            const std::string what = argv[3];
            const size_t index = (size_t)atoi(argv[4]);
            std::string text;
            if (what == "zone") text = InspectZone(game, index);
            else if (what == "piece") text = InspectPiece(game, index);
            else if (what == "area") text = InspectArea(game, index);
            else if (what == "matrix") text = InspectMatrix(game, index);
            else if (what == "piece-names") text = InspectPieceNames(game);
            else if (what == "zones") text = InspectZones(game);
            else if (what == "matrices") text = InspectMatrices(game);
            else if (what == "archives") text = InspectArchives(game);
            else if (what == "piece-budget") text = InspectPieceBudget(game);
            else if (what == "coll-sizes") text = InspectCollSizes(game);
            else if (what == "zone-reach") text = InspectZoneReach(game);
            else return Usage();
            fputs(text.c_str(), stdout);
            return 0;
        }
        if (cmd == "oras-measure" && argc >= 4)
        {
            N3dsRom game(argv[2]);
            fputs(MeasureGame(game, argv[3]).c_str(), stdout);
            return 0;
        }
        if (cmd == "oras-texture" && argc >= 6)
        {
            // an area pack's texture (any of its BCH files), decoded, rows top-down as PNG shows them
            N3dsRom game(argv[2]);
            const BinLinker pack = BinLinker::Read(Plain(Garc(game.Read("a/0/1/4")).Sub((size_t)atoi(argv[3]))), "AD");
            for (const Bytes& f : pack.Files)
            {
                if (f.empty() || !Bch::Is(f)) continue;
                for (const BchTexture& t : Bch::Read(f).Textures)
                    if (t.Name == argv[4] && !t.Data.empty())
                    {
                        WriteFile(argv[5], EncodePng(t.Width, t.Height, PicaTextureDecode(t.Data, t.Width, t.Height, t.Format)));
                        printf("%s: %ux%u, format %u\n", t.Name.c_str(), t.Width, t.Height, (unsigned)t.Format);
                        return 0;
                    }
            }
            fprintf(stderr, "no texture %s in area pack %s\n", argv[4], argv[3]);
            return 1;
        }
        if (cmd == "mesh-json" && argc >= 4)
        {
            Bytes file = Plain(ReadFile(argv[2]));
            if (Text(file, 0, 2) == "GR") file = BinLinker::Read(file, "GR").Files.at(1); // a map piece: its terrain
            const Bch bch = Bch::Read(file);
            if (bch.Models.empty()) { fprintf(stderr, "no model\n"); return 1; }
            const BchModel& m = bch.Models[0];
            auto quoted = [](const std::string& text) { // a JSON string: quotes and backslashes escaped
                std::string q = "\"";
                for (char c : text) { if (c == '"' || c == '\\') q += '\\'; q += c; }
                return q + "\"";
            };
            std::string json = "[";
            char num[160];
            for (size_t i = 0; i < m.Meshes.size(); i++)
            {
                const BchMesh& me = m.Meshes[i];
                const BchMaterial& mat = m.Materials.at(me.Material);
                json += std::string(i ? "," : "") + "{\"mesh\":" + std::to_string(i) + ",\"layer\":" + std::to_string(me.Layer) + ",\"material\":" + quoted(mat.Name) +
                        ",\"tex\":" + quoted(mat.Texture[0]) + ",\"tex1\":" + quoted(mat.Texture[1]) + ",\"v\":[";
                for (size_t k = 0; k < me.Vertices.size(); k++)
                {
                    const BchVertex& v = me.Vertices[k];
                    snprintf(num, sizeof num, "%s[%.3f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f]", k ? "," : "", v.Position[0], v.Position[2], v.Position[1],
                             v.TexCoord[0], v.TexCoord[1], v.Colour[0], v.Colour[1], v.Colour[2], v.Colour[3]);
                    json += num;
                }
                json += "],\"t\":[";
                for (size_t k = 0; k < me.Triangles.size(); k++) json += (k ? "," : "") + std::to_string(me.Triangles[k]);
                json += "]}";
            }
            json += "]\n";
            WriteFile(argv[3], Bytes(json.begin(), json.end()));
            printf("%zu meshes written to %s\n", m.Meshes.size(), argv[3]);
            return 0;
        }
        if (cmd == "piece-tiles" && argc >= 3)
        {
            // the tile values of map pieces (GR files, as oras-region writes them: region_piece_<x>_<y>.bin), how many tiles
            // hold each, and whether the game itself uses the value (TownCheck's established set): to compare a piece that
            // freezes the game with one that does not (ORAS_LITTLEROOT.md 0, Route 201)
            for (int a = 2; a < argc; a++)
            {
                const Bytes tiles = BinLinker::Read(Plain(ReadFile(argv[a])), "GR").Files.at(0);
                std::map<uint32_t, int> count;
                for (int t = 0; t < 1600 && 4 + (size_t)t * 4 + 4 <= tiles.size(); t++) count[U32(tiles, 4 + (size_t)t * 4)]++;
                printf("%s:", argv[a]);
                for (const auto& [value, n] : count) printf(" %08X x%d%s", value, n, TileValueEstablished(value) ? "" : " (not the game's)");
                printf("\n");
            }
            return 0;
        }
        if ((cmd == "oras-topview" && argc >= 5) || (cmd == "topview" && argc >= 4))
        {
            // oras-topview <oras.3ds> <piece> <out.png> | topview <GR piece file> <out.png>, then [--grid] [--tiles] [--doors] [--points] [--px N]
            const bool fromGame = cmd == "oras-topview";
            const int firstFlag = fromGame ? 5 : 4;
            TopViewOptions view;
            for (int i = firstFlag; i < argc; i++)
            {
                const std::string flag = argv[i];
                if (flag == "--grid") view.Grid = true;
                else if (flag == "--tiles") view.Tiles = true;
                else if (flag == "--doors") view.Doors = true;
                else if (flag == "--points") view.Points = true;
                else if (flag == "--px" && i + 1 < argc) view.PixelsPerTile = atoi(argv[++i]);
                else return Usage();
            }
            Bytes piece;
            if (fromGame)
            {
                N3dsRom game(argv[2]);
                piece = Garc(game.Read("a/0/3/9")).Sub((size_t)atoi(argv[3]));
            }
            else piece = ReadFile(argv[2]);
            if (IsLzCompressed(piece)) piece = LzDecompress(piece);
            WriteFile(argv[fromGame ? 4 : 3], RenderTopViewOfPiece(piece, view));
            printf("legend:");
            for (int k = 0; k <= (int)TopViewKind::Other; k++)
            {
                uint8_t rgb[3];
                TopViewColour((TopViewKind)k, rgb);
                printf(" %s=#%02X%02X%02X", TopViewName((TopViewKind)k), rgb[0], rgb[1], rgb[2]);
            }
            printf("\n");
            if (view.Points) printf("points: lighter-grass border dark green, path border dark brown, blade strip orange, its tips red, its roots blue\n");
            return 0;
        }
        if (cmd == "oras-zone-check" && argc >= 3)
        {
            // every zone of a/0/1/3 read and written back (OrasZone::Write): the writer must give the game's own bytes
            N3dsRom oras(argv[2]);
            const Garc zones(oras.Read("a/0/1/3"));
            int same = 0, differ = 0;
            for (size_t i = 0; i < zones.Count(); i++)
            {
                const Bytes plain = Plain(zones.Sub(i));
                if (plain.size() < 4 || plain[0] != 'Z' || plain[1] != 'O') continue; // members 536, 537: the tables
                if (OrasZone::Read(plain).Write(plain) == plain) same++;
                else { differ++; printf("zone %zu: written back differently\n", i); }
            }
            printf("%d zones written back identical, %d differ\n", same, differ);
            return differ ? 1 : 0;
        }
        if (cmd == "oras-engine" && argc >= 4)
        {
            // the engine patches (OrasEngine.h, ORAS_ENGINE.md 4): --memory 64|72|80|96, --linear-heap BYTES, --zone-rows N,
            // --script-mask ZONE:MASK (repeatable, with --zone-rows), --heap ID:BYTES (repeatable)
            OrasEngineOptions o;
            for (int i = 4; i < argc; i++)
            {
                const std::string flag = argv[i];
                if (i + 1 >= argc) throw FormatError(flag + ": a value is missing");
                const std::string value = argv[++i];
                if (flag == "--memory")
                {
                    const int mb = OptionNumber(value.c_str(), flag);
                    o.Memory = mb == 64 ? OrasMemoryMode::Prod64 : mb == 72 ? OrasMemoryMode::Dev3_72 : mb == 80 ? OrasMemoryMode::Dev2_80
                             : mb == 96 ? OrasMemoryMode::Dev1_96 : mb == 124 ? OrasMemoryMode::New124
                             : throw FormatError("--memory: 64, 72, 80, 96 or 124 (MB; 124 needs the emulator's New 3DS setting; 178 stops the game at boot)");
                }
                else if (flag == "--linear-heap") o.LinearHeap = (uint32_t)OptionNumber(value.c_str(), flag);
                else if (flag == "--normal-heap") o.NormalHeap = (uint32_t)OptionNumber(value.c_str(), flag);
                else if (flag == "--zone-rows") o.ZoneRows = (uint32_t)OptionNumber(value.c_str(), flag);
                else if (flag == "--characters") o.Characters = (uint32_t)OptionNumber(value.c_str(), flag);
                else if (flag == "--script-mask")
                {
                    const size_t colon = value.find(':');
                    if (colon == std::string::npos) throw FormatError("--script-mask ZONE:MASK");
                    o.ScriptMasks[(uint32_t)OptionNumber(value.substr(0, colon).c_str(), flag)] = (uint32_t)OptionNumber(value.substr(colon + 1).c_str(), flag);
                }
                else if (flag == "--heap")
                {
                    const size_t colon = value.find(':');
                    if (colon == std::string::npos) throw FormatError("--heap ID:BYTES");
                    o.HeapSizes[(uint32_t)OptionNumber(value.substr(0, colon).c_str(), flag)] = (uint32_t)OptionNumber(value.substr(colon + 1).c_str(), flag);
                }
                else throw FormatError("oras-engine: unknown option " + flag);
            }
            N3dsRom oras(argv[2]);
            for (const std::string& line : BuildEngineMod(oras, o, argv[3])) printf("%s\n", line.c_str());
            return 0;
        }
        if (cmd == "oras-append-test" && argc >= 5)
        {
            const std::string what = argv[4];
            const AppendTest test = what == "unused" ? AppendTest::Unused : what == "piece" ? AppendTest::Piece : what == "matrix" ? AppendTest::Matrix
                                  : what == "zone" ? AppendTest::Zone : what == "zone-raised" ? AppendTest::ZoneRaised
                                  : what == "crowd" ? AppendTest::Crowd : throw FormatError("append test: unused, piece, matrix, zone, zone-raised or crowd");
            N3dsRom oras(argv[2]);
            const size_t crowd = argc >= 6 ? (size_t)OptionNumber(argv[5], "crowd size") : 30;
            for (const std::string& line : BuildAppendTest(oras, test, argv[3], crowd)) printf("%s\n", line.c_str());
            return 0;
        }
        if (cmd == "oras-town" && argc >= 5)
        {
            OrasTownOptions options;
            options.OutDir = argv[4];
            for (int i = 5; i < argc; i++)
            {
                const std::string flag = argv[i];
                auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return OptionNumber(argv[at], flag); };
                if (TownKitOption(flag, argc, argv, i, options)) continue;
                if (flag == "--left") options.Left = number(++i);
                else if (flag == "--top") options.Top = number(++i);
                else if (flag == "--cell") { options.CellX = number(++i); options.CellY = number(++i); }
                else if (flag == "--zone") options.Zone = (size_t)number(++i);
                else if (flag == "--area") options.AreaPack = (size_t)number(++i);
                else if (flag == "--zone-pack") options.ZonePack = number(++i) != 0;
                else if (flag == "--piece") options.WritePiece = number(++i) != 0;
                else if (flag == "--add-warps") options.AddWarps = number(++i) != 0;
                else if (flag == "--zone-warps") options.ZoneWarps = number(++i) != 0;
                else { fprintf(stderr, "unknown option %s\n", flag.c_str()); return 2; }
            }
            const NdsRom platinum(ReadFile(argv[2]));
            N3dsRom oras(argv[3]);
            const OrasTownResult result = BuildOrasTown(platinum, oras, options);
            for (const std::string& line : result.Log) printf("%s%s", line.c_str(), !line.empty() && line.back() == '\n' ? "" : "\n");
            printf("mod written under %s: copy its load folder into the 3DS folder (Pomegrade/3DS)\n", options.OutDir.c_str());
            return 0;
        }
        if (cmd == "oras-code" && argc >= 4)
        {
            // the game's code, decompressed, for a disassembler (prototype/code_find.py); where "KAGE" lies, against section 7
            N3dsRom oras(argv[2]);
            const Bytes code = oras.Code();
            WriteFile(argv[3], code);
            printf(".code: %zu bytes (0x%zX), loaded at 0x100000\n", code.size(), code.size());
            for (size_t k = 0; k + 4 <= code.size(); k++)
                if (code[k] == 'K' && code[k + 1] == 'A' && code[k + 2] == 'G' && code[k + 3] == 'E') printf("\"KAGE\" at 0x%zX\n", 0x100000 + k);
            return 0;
        }
        if (cmd == "oras-region" && argc >= 5)
        {
            OrasRegionOptions options;
            options.OutDir = argv[4];
            for (int i = 5; i < argc; i++)
            {
                const std::string flag = argv[i];
                auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return OptionNumber(argv[at], flag); };
                if (TownKitOption(flag, argc, argv, i, options.Town)) continue;
                if (flag == "--rect") { options.Left = number(++i); options.Top = number(++i); options.Width = number(++i); options.Height = number(++i); }
                else if (flag == "--zone")
                {
                    if (++i >= argc) throw FormatError("missing <map header>:<ORAS zone> after --zone");
                    const std::string pair = argv[i];
                    const size_t colon = pair.find(':');
                    if (colon == std::string::npos) throw FormatError("--zone takes <map header>:<ORAS zone or -1>, not " + pair);
                    options.Zones[atoi(pair.substr(0, colon).c_str())] = atoi(pair.substr(colon + 1).c_str());
                }
                else if (flag == "--matrix-template") options.MatrixTemplate = (size_t)number(++i);
                else if (flag == "--model-matrix") options.ModelMatrix = number(++i);
                else if (flag == "--plan") options.PlanOnly = true;
                else if (flag == "--others-out") options.OthersOut = true;
                else if (flag == "--new-zones") options.NewZones = true;
                else if (flag == "--solid-piece-tiles") options.SolidPieceTiles = (uint32_t)number(++i);
                else if (flag == "--skip-solid-pieces") options.SkipSolidPieces = true;
                else if (flag == "--tile-replace" && i + 1 < argc)
                {
                    const std::string pair = argv[++i];
                    const size_t colon = pair.find(':');
                    if (colon == std::string::npos) throw FormatError("--tile-replace FROM:TO, got " + pair);
                    options.TileReplace.push_back({(uint32_t)OptionNumber(pair.substr(0, colon).c_str(), flag), (uint32_t)OptionNumber(pair.substr(colon + 1).c_str(), flag)});
                }
                else if (flag == "--auto-zones") options.AutoZones = true;
                else if (flag == "--no-triggers") options.NoTriggers = true;
                else if (flag == "--no-characters") options.NoCharacters = true;
                else { fprintf(stderr, "unknown option %s\n", flag.c_str()); return 2; }
            }
            const NdsRom platinum(ReadFile(argv[2]));
            N3dsRom oras(argv[3]);
            for (const std::string& line : BuildOrasRegion(platinum, oras, options)) printf("%s%s", line.c_str(), !line.empty() && line.back() == '\n' ? "" : "\n");
            if (!options.PlanOnly) printf("mod written under %s: copy its load folder into the 3DS folder (Pomegrade/3DS)\n", options.OutDir.c_str());
            return 0;
        }
        // decrypted 3DS game images
        if (cmd == "oras-list" || cmd == "oras-find" || cmd == "oras-members" || cmd == "oras-copy" || cmd == "oras-hex" || cmd == "oras-member" || cmd == "oras-layout" || cmd == "oras-text" || cmd == "oras-script" || cmd == "oras-extract" || cmd == "oras-mod" || cmd == "oras-patch" || cmd == "oras-asset")
        {
            N3dsRom game(argv[2]);
            char id[17];
            snprintf(id, sizeof id, "%016llX", (unsigned long long)game.ProgramId());
            if (cmd == "oras-list")
            {
                printf("program %s, product %s, %zu files\n", id, game.ProductCode().c_str(), game.Files().size());
                for (const auto& [path, at] : game.Files()) printf("%12llu %s\n", (unsigned long long)at.second, path.c_str());
                return 0;
            }
            if (cmd == "oras-extract" && argc >= 5) { WriteFile(argv[4], game.Read(argv[3])); return 0; }
            if (cmd == "oras-copy" && argc >= 6)
            {
                // members of an archive copied over others of the same archive, as they are (every sub-file):
                // <dst first>-<dst last>=<src first>-<src last> or <dst>=<src>; the mod is a BPS patch, or the archive whole
                // under romfs/ when it comes out shorter (Azahar would keep a shorter patched file's old tail)
                const std::string path = argv[4];
                const Bytes original = game.Read(path);
                const Garc source(original);
                Garc g(original);
                size_t last = SIZE_MAX; // the last member written
                for (int i = 5; i < argc; i++)
                {
                    const std::string arg = argv[i];
                    const size_t eq = arg.find('=');
                    if (eq == std::string::npos) { fprintf(stderr, "expected <dst>=<src>: %s\n", arg.c_str()); return 2; }
                    const std::string src = arg.substr(eq + 1);
                    if (src.rfind("motion:", 0) == 0)
                    {
                        // <dst>=motion:<archive>:<member>:<file>:<slot>: a title-style motion pack built from a Pokemon's
                        // (ORAS_TITLE.md 1b): u32 count 2, the offsets of slot 0 (the skeleton) and slot 1, the end, then the
                        // skeleton and the one motion taken from the Pokemon pack's slot <slot> (1: ba10_waitA01);
                        // ...:<slot>:<frames>: the motion looped up to <frames> (a multiple of its own), LoopGfMotion
                        std::vector<std::string> f;
                        for (size_t at = 7, next; at <= src.size(); at = next + 1)
                        {
                            next = src.find(':', at);
                            if (next == std::string::npos) next = src.size();
                            f.push_back(src.substr(at, next - at));
                        }
                        if (f.size() != 4 && f.size() != 5) { fprintf(stderr, "expected motion:<archive>:<member>:<file>:<slot>[:<frames>]: %s\n", src.c_str()); return 2; }
                        const Garc other(game.Read(f[0]));
                        const Bytes& m = other.Sub((size_t)std::stoul(f[1]));
                        const Bytes pack = ContainerFile(Plain(m), (size_t)std::stoul(f[2]));
                        const uint32_t count = U32(pack, 0);
                        const size_t slot = (size_t)std::stoul(f[3]);
                        if (count < 2 || slot == 0 || slot >= count || pack.size() < 4 + 4 * (count + 1)) { fprintf(stderr, "%s: not a motion pack with slot %zu\n", src.c_str(), slot); return 1; }
                        // a slot ends where the next non-empty one (or the pack) does
                        auto slotBytes = [&](size_t k) {
                            const uint32_t from = U32(pack, 4 + 4 * k);
                            uint32_t to = U32(pack, 4 + 4 * count);
                            for (size_t j = k + 1; j < count; j++) if (U32(pack, 4 + 4 * j)) { to = U32(pack, 4 + 4 * j); break; }
                            if (!from || to < from || to > pack.size()) throw FormatError("motion pack slot " + std::to_string(k) + " is empty or out of the pack");
                            // GfMotion counts a motion's alignments from its start, right only for a 4-aligned slot
                            if (from % 4) throw FormatError("motion pack slot " + std::to_string(k) + " is not 4-aligned in its pack");
                            return Bytes(pack.begin() + from, pack.begin() + to);
                        };
                        const Bytes skeleton = slotBytes(0);
                        Bytes motion = slotBytes(slot);
                        if (f.size() == 5)
                        {
                            const int period = GfMotionFrames(motion), frames = std::stoi(f[4]);
                            if (period < 1) { fprintf(stderr, "%s: the motion has no frames to loop\n", src.c_str()); return 1; }
                            if (frames < period || frames % period) { fprintf(stderr, "%s: %d frames is not a multiple of the motion's %d\n", src.c_str(), frames, period); return 2; }
                            if (skeleton.size() % 4) throw FormatError("the skeleton would leave the motion unaligned");
                            motion = LoopGfMotion(motion, frames / period);
                            printf("motion of %d frames looped %d times: %d frames\n", period, frames / period, frames);
                        }
                        Bytes out(16, 0);
                        auto put32 = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) out[at + k] = (uint8_t)(v >> (8 * k)); };
                        put32(0, 2); put32(4, 16); put32(8, (uint32_t)(16 + skeleton.size())); put32(12, (uint32_t)(16 + skeleton.size() + motion.size()));
                        out.insert(out.end(), skeleton.begin(), skeleton.end());
                        out.insert(out.end(), motion.begin(), motion.end());
                        const size_t dst = (size_t)std::stoul(arg.substr(0, eq));
                        const Bytes& old = source.Sub(dst);
                        g.Set(dst, IsLzCompressed(old) ? Lz11Compress(out) : out);
                        printf("%s member %zu <- %s: skeleton %zu bytes, motion %zu bytes\n", path.c_str(), dst, src.c_str(), skeleton.size(), motion.size());
                        last = dst;
                        continue;
                    }
                    if (src.find(':') != std::string::npos)
                    {
                        // <dst>=<archive>:<member>:<file>: one file of a 2-letter container member of another archive (an
                        // animation of a Pokemon's PB pack), written as the member, LZ-compressed as the member it replaces
                        const size_t c1 = src.find(':'), c2 = src.find(':', c1 + 1);
                        if (c2 == std::string::npos) { fprintf(stderr, "expected <archive>:<member>:<file>: %s\n", src.c_str()); return 2; }
                        const Garc other(game.Read(src.substr(0, c1)));
                        const Bytes& m = other.Sub((size_t)std::stoul(src.substr(c1 + 1, c2 - c1 - 1)));
                        const Bytes plain = IsLzCompressed(m) ? LzDecompress(m) : m;
                        const BinLinker pack = BinLinker::Read(plain, std::string(plain.begin(), plain.begin() + 2));
                        const Bytes file = pack.Files.at((size_t)std::stoul(src.substr(c2 + 1)));
                        const size_t dst = (size_t)std::stoul(arg.substr(0, eq));
                        if (file.size() < 4 || std::string(file.begin(), file.begin() + 3) != "BCH") { fprintf(stderr, "%s is not a BCH file\n", src.c_str()); return 1; }
                        ReplaceMember(g, source, dst, file, "BCH");
                        printf("%s member %zu <- %s (%zu bytes)\n", path.c_str(), dst, src.c_str(), file.size());
                        last = dst;
                        continue;
                    }
                    auto range = [](const std::string& r, size_t& lo, size_t& hi) {
                        const size_t dash = r.find('-');
                        lo = (size_t)std::stoul(r.substr(0, dash));
                        hi = dash == std::string::npos ? lo : (size_t)std::stoul(r.substr(dash + 1));
                    };
                    size_t d0, d1, s0, s1;
                    range(arg.substr(0, eq), d0, d1);
                    range(src, s0, s1);
                    if (d1 - d0 != s1 - s0 || d1 >= g.Count() || s1 >= g.Count()) { fprintf(stderr, "ranges of different lengths or past the archive: %s\n", arg.c_str()); return 2; }
                    for (size_t k = 0; k <= d1 - d0; k++)
                    {
                        for (size_t sub = 0; sub < std::max(source.SubCount(s0 + k), g.SubCount(d0 + k)); sub++)
                            g.Set(d0 + k, source.Has(s0 + k, sub) ? source.Sub(s0 + k, sub) : Bytes{}, sub);
                        printf("%s member %zu <- member %zu\n", path.c_str(), d0 + k, s0 + k);
                        last = d0 + k;
                    }
                }
                return WriteArchiveMod(g, original, last, path, std::filesystem::path(argv[3]) / "load" / "mods" / id);
            }
            if (cmd == "oras-asset" && argc >= 6)
            {
                // asset customization (AssetEdit.h): a member's models and textures listed, its textures exported as PNG,
                // or edited and written as a mod. <file>: one file of the member's 2-letter container, -1 the member
                //   oras-asset <oras.3ds> <archive> <member> <file> [list]
                //   oras-asset <oras.3ds> <archive> <member> <file> export <dir>
                //   oras-asset <oras.3ds> <archive> <member> <file> edit <out dir> recolour:<texture>:<hue>:<saturation>:<brightness>[:<around>:<range>]...
                const std::string path = argv[3];
                const Bytes original = game.Read(path);
                const Garc source(original);
                const size_t member = (size_t)std::stoul(argv[4]);
                const int file = atoi(argv[5]);
                const Bytes plain = Plain(source.Sub(member));
                const std::string tag = plain.size() >= 2 ? std::string(plain.begin(), plain.begin() + 2) : "";
                const Bytes target = file >= 0 ? ContainerFile(plain, (size_t)file) : plain;
                const std::string where = path + " member " + std::to_string(member) + (file >= 0 ? " file " + std::to_string(file) : "");
                if (!Bch::Is(target)) { fprintf(stderr, "%s is not a BCH model file\n", where.c_str()); return 1; }
                const Bch bch = Bch::Read(target);
                const std::string what = argc >= 7 ? argv[6] : "list";
                if (what == "list")
                {
                    printf("%s: BCH version 0x%02X, %zu model(s), %zu texture(s)\n", where.c_str(), (unsigned)bch.Version, bch.Models.size(), bch.Textures.size());
                    for (const BchModel& m : bch.Models) printf("  model %s: %zu meshes, %zu materials\n", m.Name.c_str(), m.Meshes.size(), m.Materials.size());
                    for (const BchTexture& t : bch.Textures) printf("  texture %s: %ux%u, format %u\n", t.Name.c_str(), t.Width, t.Height, (unsigned)t.Format);
                    return 0;
                }
                if (what == "export" && argc >= 8)
                {
                    std::filesystem::create_directories(argv[7]);
                    for (const BchTexture& t : bch.Textures)
                    {
                        if (t.Data.empty()) continue;
                        const std::string out = (std::filesystem::path(argv[7]) / (t.Name + ".png")).string();
                        WriteFile(out, EncodePng(t.Width, t.Height, PicaTextureDecode(t.Data, t.Width, t.Height, t.Format)));
                        printf("%s: %ux%u, format %u\n", out.c_str(), t.Width, t.Height, (unsigned)t.Format);
                    }
                    return 0;
                }
                if (what == "edit" && argc >= 9)
                {
                    Bytes edited = target;
                    for (int i = 8; i < argc; i++)
                    {
                        const std::string op = argv[i];
                        std::vector<std::string> f;
                        for (size_t at = 0, next; at <= op.size(); at = next + 1)
                        {
                            next = op.find(':', at);
                            if (next == std::string::npos) next = op.size();
                            f.push_back(op.substr(at, next - at));
                        }
                        if (f[0] == "recolour" && (f.size() == 5 || f.size() == 7))
                        {
                            const double hue = std::stod(f[2]), sat = std::stod(f[3]), bright = std::stod(f[4]);
                            const double around = f.size() == 7 ? std::stod(f[5]) : 0, range = f.size() == 7 ? std::stod(f[6]) : 180;
                            edited = BchEditTexture(edited, f[1], [&](Bytes& rgba, uint32_t, uint32_t) { Recolour(rgba, hue, sat, bright, around, range); });
                            printf("texture %s: hue %+g degrees, saturation x%g, brightness x%g%s\n", f[1].c_str(), hue, sat, bright,
                                   range < 180 ? (" (colours within " + f[6] + " degrees of hue " + f[5] + ")").c_str() : "");
                        }
                        else { fprintf(stderr, "unknown edit %s (recolour:<texture>:<hue>:<saturation>:<brightness>[:<around>:<range>])\n", op.c_str()); return 2; }
                    }
                    Bytes newPlain = edited;
                    std::string newTag = "BCH";
                    if (file >= 0)
                    {
                        BinLinker pack = BinLinker::Read(plain, tag);
                        pack.Files.at((size_t)file) = edited;
                        newPlain = pack.Write();
                        newTag = tag;
                    }
                    Garc g(original);
                    ReplaceMember(g, source, member, newPlain, newTag);
                    // the model file before and after, beside the mod: the previews shown before it is built in (render_model.js)
                    std::filesystem::create_directories(argv[7]);
                    WriteFile((std::filesystem::path(argv[7]) / "asset_before.bch").string(), target);
                    WriteFile((std::filesystem::path(argv[7]) / "asset_after.bch").string(), edited);
                    return WriteArchiveMod(g, original, member, path, std::filesystem::path(argv[7]) / "load" / "mods" / id);
                }
                fprintf(stderr, "oras-asset: list, export <dir> or edit <out dir> <edit>...\n");
                return 2;
            }
            if (cmd == "oras-layout" && argc >= 5)
            {
                // a member holding a layout's files, described: a DARC archive's files one by one, a BCLYT/BCLAN
                // (Layout.h), a BCLIM image (Bclim.h); <file>: one file of its 2-letter container
                const Garc g(game.Read(argv[3]));
                const Bytes d = MemberOrFile(g, (size_t)atoi(argv[4]), argc >= 6 ? atoi(argv[5]) : -1);
                auto describe = [](const std::string& name, const Bytes& b) {
                    const std::string m = b.size() >= 4 ? std::string(b.begin(), b.begin() + 4) : "";
                    if (m == "CLYT" || m == "CLAN") return name + ": " + DescribeLayout(b);
                    try
                    {
                        const ClimImage c = ClimImage::Read(b);
                        char line[160];
                        snprintf(line, sizeof line, "%s: BCLIM %u x %u %s (stored %u x %u)\n", name.c_str(), c.Width, c.Height, ClimFormatName(c.Format), c.StoredWidth, c.StoredHeight);
                        return std::string(line);
                    }
                    catch (const FormatError&) {}
                    char line[160];
                    snprintf(line, sizeof line, "%s: %zu bytes, starts %s\n", name.c_str(), b.size(), m.c_str());
                    return std::string(line);
                };
                if (d.size() >= 4 && std::string(d.begin(), d.begin() + 4) == "darc")
                    for (const DarcFile& file : ReadDarc(d)) printf("%s", describe(file.Path, file.Data).c_str());
                else printf("%s", describe(std::string("member ") + argv[4], d).c_str());
                return 0;
            }
            if (cmd == "oras-text" && argc >= 5)
            {
                // the lines of a game text file (a GARC member, GameText.h), one a line, numbered
                const Garc g(game.Read(argv[3]));
                const std::vector<std::string> lines = ReadGameText(Plain(g.Sub((size_t)atoi(argv[4]))));
                for (size_t i = 0; i < lines.size(); i++) printf("%zu\t%s\n", i, lines[i].c_str());
                return 0;
            }
            if (cmd == "oras-script" && argc >= 4)
            {
                // a zone's script (a/0/1/3 member, OrasZone), or with "init" its init script, disassembled (Amx.h)
                const Garc g(game.Read("a/0/1/3"));
                if (std::string(argv[3]) == "all")
                {
                    // every zone's two scripts disassembled, the listings' own checks summed: the opcode table on the whole game
                    size_t scripts = 0, failed = 0, cells = 0, unknown = 0, callsOk = 0, calls = 0, jumpsOk = 0, jumps = 0;
                    for (size_t i = 0; i < g.Count(); i++)
                    {
                        OrasZone z;
                        try { z = OrasZone::Read(Plain(g.Sub(i))); } catch (const FormatError&) { continue; }
                        for (const Bytes* script : {&z.InitScript, &z.Script})
                        {
                            if (script->empty()) continue;
                            scripts++;
                            try
                            {
                                const std::string d = AmxDisassemble(*script);
                                const std::string last = d.substr(d.rfind('\n', d.size() - 2) + 1);
                                size_t c, u, n, dc, co, ca, jo, ja;
                                if (sscanf(last.c_str(), "%zu code cells, %zu not an opcode, %zu natives, %zu data cells; %zu of %zu calls land on a proc, %zu of %zu jumps",
                                           &c, &u, &n, &dc, &co, &ca, &jo, &ja) == 8)
                                { cells += c; unknown += u; callsOk += co; calls += ca; jumpsOk += jo; jumps += ja; }
                                if (u || co != ca || jo != ja) printf("zone %zu %s: %s", i, script == &z.Script ? "script" : "init", last.c_str());
                            }
                            catch (const FormatError& e) { failed++; printf("zone %zu: %s\n", i, e.what()); }
                        }
                    }
                    printf("%zu scripts (%zu not read), %zu code cells, %zu not an opcode; %zu of %zu calls land on a proc, %zu of %zu jumps on an instruction\n",
                           scripts, failed, cells, unknown, callsOk, calls, jumpsOk, jumps);
                    return 0;
                }
                const OrasZone z = OrasZone::Read(Plain(g.Sub((size_t)atoi(argv[3]))));
                const Bytes& script = argc >= 5 && std::string(argv[4]) == "init" ? z.InitScript : z.Script;
                const std::vector<std::string> natives = AmxNatives(script);
                for (size_t i = 0; i < natives.size(); i++) printf("native %zu %s\n", i, natives[i].c_str());
                printf("%s", AmxDisassemble(script).c_str());
                return 0;
            }
            if (cmd == "oras-member" && argc >= 7)
            {
                // a member (LZ-decompressed), or one file of its 2-letter container (<file> -1: the member itself), written
                // to <out>: for tools/remake/spica, which reads the 3DS formats SPICA knows
                const Garc g(game.Read(argv[3]));
                const Bytes d = MemberOrFile(g, (size_t)atoi(argv[4]), atoi(argv[5]));
                WriteFile(argv[6], d);
                printf("%s member %s%s: %zu bytes\n", argv[3], argv[4], atoi(argv[5]) >= 0 ? (std::string(" file ") + argv[5]).c_str() : "", d.size());
                return 0;
            }
            if (cmd == "oras-hex" && argc >= 8)
            {
                // bytes of a member (LZ-decompressed), or of one file of its 2-letter container (<file> -1: the member itself),
                // from <offset>, <length> of them, 16 a line: to read formats the tool does not know yet (ORAS_TITLE.md)
                const Garc g(game.Read(argv[3]));
                const Bytes d = MemberOrFile(g, (size_t)atoi(argv[4]), atoi(argv[5]));
                const size_t from = (size_t)std::stoul(argv[6], nullptr, 0), length = (size_t)std::stoul(argv[7], nullptr, 0);
                printf("%zu bytes in all\n", d.size());
                for (size_t at = from; at < std::min(d.size(), from + length); at += 16)
                {
                    printf("%06zX:", at);
                    for (size_t k = at; k < at + 16 && k < d.size(); k++) printf(" %02X", d[k]);
                    printf("\n");
                }
                return 0;
            }
            if (cmd == "oras-members" && argc >= 6)
            {
                // what members of an archive hold, nested: size, LZ, 2-letter containers and their files, BCH models, textures
                // and the names inside (the title screen's members against a Pokemon's, ORAS_TITLE.md)
                const Garc g(game.Read(argv[3]));
                std::function<void(const Bytes&, const std::string&)> describe = [&](const Bytes& d, const std::string& pad) {
                    if (d.size() >= 4 && d[0] == 'B' && d[1] == 'C' && d[2] == 'H' && d[3] == 0)
                    {
                        printf("%sBCH %zu bytes", pad.c_str(), d.size());
                        try
                        {
                            const Bch b = Bch::Read(d);
                            printf(": %zu models, %zu textures\n", b.Models.size(), b.Textures.size());
                            for (const BchModel& m : b.Models) printf("%s  model %s: %zu meshes, %zu materials\n", pad.c_str(), m.Name.c_str(), m.Meshes.size(), m.Materials.size());
                            for (const BchTexture& t : b.Textures) printf("%s  texture %s %ux%u format %d\n", pad.c_str(), t.Name.c_str(), t.Width, t.Height, t.Format);
                        }
                        catch (const std::exception& e) { printf(" (not read: %s)\n", e.what()); }
                        // every name in it (animations, bones, materials)
                        std::set<std::string> names;
                        for (size_t i = 0; i < d.size();)
                        {
                            size_t e = i;
                            while (e < d.size() && d[e] >= 0x20 && d[e] < 0x7F) e++;
                            if (e - i >= 4 && e < d.size() && d[e] == 0) names.insert(std::string(d.begin() + i, d.begin() + e));
                            i = e + 1;
                        }
                        std::string all;
                        for (const std::string& n : names) all += " " + n;
                        printf("%s  names (%zu):%s\n", pad.c_str(), names.size(), all.substr(0, 3000).c_str());
                        return;
                    }
                    if (d.size() >= 12 && isupper(d[0]) && isupper(d[1]))
                    {
                        try
                        {
                            const BinLinker c = BinLinker::Read(d, std::string(d.begin(), d.begin() + 2));
                            printf("%scontainer %s, %zu files, %zu bytes\n", pad.c_str(), c.Tag.c_str(), c.Files.size(), d.size());
                            for (size_t i = 0; i < c.Files.size(); i++)
                            {
                                printf("%s  file %zu:\n", pad.c_str(), i);
                                describe(c.Files[i], pad + "    ");
                            }
                            return;
                        }
                        catch (const std::exception&) {}
                    }
                    printf("%s%zu bytes, starts", pad.c_str(), d.size());
                    for (size_t i = 0; i < std::min<size_t>(16, d.size()); i++) printf(" %02X", d[i]);
                    printf("\n");
                };
                for (size_t i = (size_t)atoi(argv[4]); i <= (size_t)atoi(argv[5]) && i < g.Count(); i++)
                    for (size_t sub = 0; sub < g.SubCount(i); sub++)
                    {
                        if (!g.Has(i, sub)) continue;
                        const Bytes& m = g.Sub(i, sub);
                        const bool lz = IsLzCompressed(m);
                        Bytes plain = m;
                        if (lz) try { plain = LzDecompress(m); } catch (const std::exception&) {}
                        printf("member %zu%s: %zu bytes%s\n", i, sub ? ("." + std::to_string(sub)).c_str() : "", m.size(), lz ? (", LZ to " + std::to_string(plain.size())).c_str() : "");
                        describe(plain, "  ");
                    }
                return 0;
            }
            if (cmd == "oras-find" && argc >= 4)
            {
                // where texts lie (model and texture names: pm0383 for Groudon's models): every RomFS file, and every member of
                // every GARC, LZ-decompressed when compressed; prints file, member, sub-file and offset of each hit
                // a text "hex:5210" is that byte string (a u16 4178), found at any alignment
                std::vector<std::string> texts;
                for (int i = 3; i < argc; i++)
                {
                    std::string t = argv[i];
                    if (t.rfind("hex:", 0) == 0)
                    {
                        std::string bytes;
                        for (size_t k = 4; k + 1 < t.size(); k += 2) bytes.push_back((char)std::stoi(t.substr(k, 2), nullptr, 16));
                        t = bytes;
                    }
                    texts.push_back(t);
                }
                auto scan = [&](const Bytes& d, const std::string& where) {
                    for (const std::string& t : texts)
                        for (auto it = std::search(d.begin(), d.end(), t.begin(), t.end()); it != d.end();
                             it = std::search(it + 1, d.end(), t.begin(), t.end()))
                        {
                            const size_t at = (size_t)(it - d.begin());
                            size_t e = at;
                            while (e < d.size() && e - at < 40 && d[e] >= 0x20 && d[e] < 0x7F) e++;
                            std::string around;
                            for (size_t k = at >= 8 ? at - 8 : 0; k < std::min(d.size(), at + t.size() + 8); k++) { char h[4]; snprintf(h, sizeof h, "%02X", d[k]); around += h; around += k + 1 == at ? "|" : " "; }
                            printf("%s +0x%zX: %s  [%s]\n", where.c_str(), at, std::string(d.begin() + at, d.begin() + e).c_str(), around.c_str());
                        }
                };
                for (const auto& [path, at] : game.Files())
                {
                    const Bytes data = game.Read(path);
                    if (!Garc::Is(data)) { scan(data, path); continue; }
                    const Garc g(data);
                    for (size_t i = 0; i < g.Count(); i++)
                        for (size_t sub = 0; sub < g.SubCount(i); sub++)
                        {
                            if (!g.Has(i, sub)) continue;
                            const Bytes& m = g.Sub(i, sub);
                            Bytes plain;
                            try { plain = IsLzCompressed(m) ? LzDecompress(m) : m; } catch (const std::exception&) { plain = m; }
                            scan(plain, path + " member " + std::to_string(i) + (sub ? "." + std::to_string(sub) : ""));
                        }
                }
                return 0;
            }
            if ((cmd == "oras-mod" || cmd == "oras-patch") && argc >= 5)
            {
                const bool patch = cmd == "oras-patch";
                const std::filesystem::path root = std::filesystem::path(argv[3]) / "load" / "mods" / id / (patch ? "romfs_ext" : "romfs");
                for (int i = 4; i < argc; i++)
                {
                    const std::string arg = argv[i];
                    const size_t eq = arg.find('=');
                    if (eq == std::string::npos) { fprintf(stderr, "expected <path>=<file>: %s\n", arg.c_str()); return 2; }
                    const std::string path = arg.substr(0, eq);
                    if (!game.Has(path)) { fprintf(stderr, "no %s in the game's RomFS: a mod only replaces files the game has\n", path.c_str()); return 1; }
                    const Bytes data = ReadFile(arg.substr(eq + 1)), original = game.Read(path);
                    // an archive the game reads as GARC must still be one
                    if (Garc::Is(original)) Garc check(data);
                    // Azahar keeps a patched file at least its original size (see Bps.h)
                    if (patch && data.size() < original.size()) { fprintf(stderr, "%s: shorter than the game's file; Azahar would leave its old tail (use oras-mod)\n", path.c_str()); return 1; }
                    std::filesystem::create_directories((root / path).parent_path());
                    if (!patch)
                    {
                        WriteFile((root / path).string(), data);
                        printf("%s <- %s (%zu bytes)\n", path.c_str(), arg.substr(eq + 1).c_str(), data.size());
                        continue;
                    }
                    const Bytes bps = BpsCreate(original, data);
                    if (BpsApply(original, bps) != data) { fprintf(stderr, "%s: the patch does not rebuild the file\n", path.c_str()); return 1; }
                    WriteFile((root / (path + ".bps")).string(), bps);
                    printf("%s <- %s (%zu bytes, patch %zu bytes, checked)\n", path.c_str(), arg.substr(eq + 1).c_str(), data.size(), bps.size());
                }
                printf("mod written under %s: copy %s into the 3DS folder (Pomegrade/3DS)\n", root.string().c_str(),
                       (std::filesystem::path(argv[3]) / "load").string().c_str());
                return 0;
            }
            return Usage();
        }
        if (cmd == "world" && argc >= 6)
        {
            const Narc matrices(Plain(ReadFile(argv[2])));
            const WorldMap world(MapMatrix::Read(Plain(matrices.Member((size_t)atoi(argv[3])))), Narc(Plain(ReadFile(argv[4]))));
            auto given = [&](int i) { return argc > i && std::string(argv[i]) != "-"; };
            Bytes texFile, buildingTexFile;
            std::unique_ptr<Tex0> tex, buildingTex;
            auto loadTex = [&](int i, Bytes& file, std::unique_ptr<Tex0>& t) {
                if (!given(i)) return;
                file = Plain(ReadFile(argv[i]));
                const long at = Tex0::Find(file);
                if (at >= 0) t = std::make_unique<Tex0>(file, (size_t)at);
            };
            loadTex(6, texFile, tex);
            loadTex(8, buildingTexFile, buildingTex);
            std::unique_ptr<Narc> buildings;
            if (given(7)) buildings = std::make_unique<Narc>(Plain(ReadFile(argv[7])));
            const std::filesystem::path out(argv[5]);
            std::filesystem::create_directories(out);
            const std::string json = world.Json();
            WriteFile((out / "world.json").string(), Bytes(json.begin(), json.end()));
            WriteFile((out / "collision.png").string(), world.CollisionPng());
            float cell = 0;
            const std::string gltf = world.Gltf(tex.get(), buildings.get(), buildingTex.get(), &cell);
            WriteFile((out / "world.gltf").string(), Bytes(gltf.begin(), gltf.end()));
            size_t maps = 0;
            for (const auto& c : world.Cells) maps += c.has_value();
            printf("matrix %s: %ux%u, %zu maps read, %zu errors, cell size %g\n", world.Matrix.Name.c_str(), world.Matrix.Width,
                   world.Matrix.Height, maps, world.Errors.size(), cell);
            for (const std::string& e : world.Errors) fprintf(stderr, "%s\n", e.c_str());
            return 0;
        }

        if (cmd == "oras-sandbox" && argc == 5)
        {
            // a new zone from a text description (OrasSandbox.h); the game reads it with oras-engine --zone-rows
            N3dsRom oras(argv[2]);
            const Bytes text = ReadFile(argv[4]);
            for (const std::string& line : BuildOrasSandbox(oras, std::string(text.begin(), text.end()), argv[3])) printf("%s\n", line.c_str());
            return 0;
        }
        if (cmd == "oras-save" && argc >= 3)
        {
            // print where an ORAS save puts the player; with <out> <zone> <tile x> <tile z>, write a copy moved there
            OrasSave save = OrasSave::Read(ReadFile(argv[2]));
            printf("%zu blocks, checksums match; player in zone %d at tile (%.2f, %.2f)\n", save.Blocks.size(), save.Zone(), save.X() / 18, save.Z() / 18);
            if (argc >= 7)
            {
                // --template FILE: the player block taken from that save first (a save the game made in the target matrix)
                // --blocks A,B,...: which blocks to take by id; by default block 10, the one a move into another matrix needs (runs
                // matrix3-8, 8 October: the player block 4 and blocks 3-6 are not enough, block 10 alone is; its content: not read)
                std::string blocks = "10";
                for (int i = 7; i + 1 < argc; i++)
                    if (std::string(argv[i]) == "--blocks") blocks = argv[i + 1];
                for (int i = 7; i + 1 < argc; i++)
                    if (std::string(argv[i]) == "--template")
                    {
                        const OrasSave other = OrasSave::Read(ReadFile(argv[i + 1]));
                        std::stringstream list(blocks);
                        for (std::string id; std::getline(list, id, ',');) save.TakeBlockFrom(other, (uint16_t)atoi(id.c_str()));
                        save.WriteChecksums();
                        printf("blocks %s taken from %s\n", blocks.c_str(), argv[i + 1]);
                    }
                save.MoveTo(atoi(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]));
                const OrasSave check = OrasSave::Read(save.Data);
                WriteFile(argv[3], save.Data);
                printf("written %s: zone %d at tile (%.2f, %.2f)\n", argv[3], check.Zone(), check.X() / 18, check.Z() / 18);
            }
            return 0;
        }
        const NdsRom rom(ReadFile(argv[2]));
        if (cmd == "info")
        {
            printf("title %s\ngame code %s\nfiles %zu\n", rom.Title().c_str(), rom.GameCode().c_str(), rom.Files().size());
            return 0;
        }
        if (cmd == "texindex")
        {
            printf("name\tpath\ttexture\tpalette\tformat\n");
            for (const TextureSource& t : IndexTextures(rom))
                printf("%s\t%s\t%s\t%s\t%d\n", t.Name.c_str(), t.Path.c_str(), t.Texture.c_str(), t.Palette.c_str(), t.Format);
            return 0;
        }
        if (cmd == "identify" && argc >= 4)
        {
            // the dump folder: <emulator textures dir>/<game code>/dump/tex_*.png
            std::multimap<std::string, TextureSource> byName;
            for (TextureSource& t : IndexTextures(rom)) byName.emplace(t.Name, std::move(t));
            size_t found = 0, total = 0;
            std::set<std::string> files;
            for (const auto& e : std::filesystem::directory_iterator(argv[3]))
            {
                const std::string stem = e.path().stem().string();
                if (e.path().extension() != ".png" || stem.rfind("tex_", 0) != 0) continue;
                total++;
                const auto [first, last] = byName.equal_range(stem);
                if (first == last) { printf("%s\t(not in a TEX0 file)\n", stem.c_str()); continue; }
                found++;
                for (auto it = first; it != last; ++it)
                {
                    const TextureSource& t = it->second;
                    printf("%s\t%s\t%s\t%s\n", stem.c_str(), t.Path.c_str(), t.Texture.c_str(), t.Palette.c_str());
                    files.insert(t.Path);
                }
            }
            fprintf(stderr, "%zu of %zu dumped textures found, from %zu files\n", found, total, files.size());
            return 0;
        }
        if (cmd == "platinum-zones")
        {
            // the zones Sinnoh needs: the overworld matrix's zones, then every zone a warp leads to from those, transitively
            // (houses, floors, caves); a map header nothing reaches is not counted
            const PlatinumWorld plat(rom, 0);
            std::set<int> overworld, reached;
            for (int h : plat.World.Matrix.Headers) if (h >= 0) overworld.insert(h);
            std::vector<int> todo(overworld.begin(), overworld.end());
            reached = overworld;
            size_t badEvents = 0;
            while (!todo.empty())
            {
                const int h = todo.back(); todo.pop_back();
                if (h < 0 || (size_t)h >= plat.Headers.size()) continue;
                try
                {
                    const ZoneEvents ev = ZoneEvents::Read(Plain(plat.Events.Member(plat.Headers[h].Events)));
                    for (const ZoneWarp& w : ev.Warps)
                        if (w.DestHeader < plat.Headers.size() && reached.insert(w.DestHeader).second) todo.push_back(w.DestHeader);
                }
                catch (const FormatError&) { badEvents++; }
            }
            std::map<int, int> byMatrix;
            for (int h : reached) byMatrix[plat.Headers[h].Matrix]++;
            printf("map headers %zu; overworld (matrix 0) zones %zu; reached through warps %zu (events not read: %zu); unreached %zu\n",
                   plat.Headers.size(), overworld.size(), reached.size(), badEvents, plat.Headers.size() - reached.size());
            printf("reached zones by matrix:");
            for (const auto& [m, n] : byMatrix) printf(" %d:%d", m, n);
            printf("\n");
            // the most characters (overworld objects) a reached header lists, against ORAS's per-zone bound (26, raised to
            // at most 32 by oras-engine --characters: ORAS_ENGINE.md 4.5)
            size_t most = 0, over26 = 0, over32 = 0;
            int mostHeader = -1;
            for (int h : reached)
                try
                {
                    const size_t n = ZoneEvents::Read(Plain(plat.Events.Member(plat.Headers[h].Events))).Objects.size();
                    if (n > most) { most = n; mostHeader = h; }
                    over26 += n > 26;
                    over32 += n > 32;
                }
                catch (const FormatError&) {}
            printf("characters: at most %zu (header %d); %zu headers list more than 26, %zu more than 32\n", most, mostHeader, over26, over32);
            // the headers no warp reaches, one line each: their matrix and event file, what the events hold, the reached headers
            // sharing their matrix or events, and how many times the scripts hold the bytes BE 00 <header> (a guess at a warp
            // command, the script format not being decoded: a hint, not a proof)
            std::map<uint16_t, std::vector<int>> eventsOf, matrixOf;
            for (int h : reached) { eventsOf[plat.Headers[h].Events].push_back(h); matrixOf[plat.Headers[h].Matrix].push_back(h); }
            const NdsFile* scriptFile = rom.Find("fielddata/script/scr_seq.narc");
            if (!scriptFile) throw FormatError("no fielddata/script/scr_seq.narc in the cartridge");
            const Narc scripts(Plain(rom.Read(*scriptFile)));
            size_t live = 0;
            for (size_t h = 0; h < plat.Headers.size(); h++)
            {
                if (reached.count((int)h)) continue;
                const MapHeader& m = plat.Headers[h];
                size_t objects = 0, warps = 0, bg = 0, coord = 0;
                try { const ZoneEvents ev = ZoneEvents::Read(Plain(plat.Events.Member(m.Events))); objects = ev.Objects.size(); warps = ev.Warps.size(); bg = ev.BgEvents; coord = ev.CoordEvents; }
                catch (const std::exception&) {}
                size_t hits = 0;
                for (size_t f = 0; f < scripts.Count(); f++)
                {
                    const Bytes& b = scripts.Member(f);
                    for (size_t k = 0; k + 3 < b.size(); k++) if (b[k] == 0xBE && b[k + 1] == 0 && b[k + 2] == (h & 0xFF) && b[k + 3] == (h >> 8)) hits++;
                }
                const bool empty = objects + warps + bg + coord == 0;
                if (!empty || hits) live++;
                printf("unreached %zu: matrix %u events %u (objects %zu warps %zu bg %zu coord %zu)%s%s, script hits %zu\n", h, m.Matrix, m.Events, objects, warps, bg, coord,
                       eventsOf.count(m.Events) ? " events shared with a reached header" : "", matrixOf.count(m.Matrix) ? " matrix shared with a reached header" : "", hits);
            }
            printf("unreached with events or script hits: %zu of %zu\n", live, plat.Headers.size() - reached.size());
            return 0;
        }
        if (cmd == "oras-sinnoh" && argc >= 4)
        {
            const NdsRom platinum(ReadFile(argv[2]));
            N3dsRom oras(argv[3]);
            for (const std::string& line : PlanSinnoh(platinum, oras, argc >= 5 ? atoi(argv[4]) : 5)) printf("%s\n", line.c_str());
            return 0;
        }
        if (cmd == "oras-world" && argc >= 5)
        {
            const size_t matrix = (size_t)atoi(argv[3]);
            const PlatinumWorld plat(rom, matrix);
            const WorldMap& world = plat.World;

            const std::filesystem::path out(argv[4]);
            std::filesystem::create_directories(out);
            // the DS tile, measured from the terrain models
            float cell = 0;
            world.Gltf(nullptr, nullptr, nullptr, &cell);
            const N3dsWorld oras = N3dsWorld::Translate(world, cell / LandTiles, plat.Warps);
            const std::string json = oras.Json();
            WriteFile((out / "world_oras.json").string(), Bytes(json.begin(), json.end()));
            const std::string gltf = world.Gltf(nullptr, &plat.BuildingModels, nullptr, nullptr, oras.Scale(), &plat.CellTex);
            WriteFile((out / "world_oras.gltf").string(), Bytes(gltf.begin(), gltf.end()));
            size_t placed = 0, warped = 0;
            for (const N3dsPiece& p : oras.Pieces) { placed += p.Buildings.size(); warped += p.Warps.size(); }
            printf("%zu map headers at ARM9 +0x%zx; matrix %zu (%s): DS tile %g units, ORAS %ux%u pieces of %u tiles (%zu used), scale %g, "
                   "%zu buildings, %zu of %zu warps, %zu map texture sets\n",
                   plat.Headers.size(), plat.HeaderTableAt, matrix, world.Matrix.Name.c_str(), oras.NdsTile, oras.Width, oras.Height, N3dsMapTiles,
                   oras.Pieces.size(), oras.Scale(), placed, warped, plat.Warps.size(), plat.MapTextureSets());
            if (oras.NdsTile != NdsTileUnits)
                fprintf(stderr, "warning: DS tile measured %g units, not %g: check the terrain models\n", oras.NdsTile, NdsTileUnits);
            for (const std::string& e : world.Errors) fprintf(stderr, "%s\n", e.c_str());
            return 0;
        }
        if (cmd == "inventory") { fputs(InventoryJson(rom).c_str(), stdout); return 0; }
        if ((cmd == "extract" || cmd == "unpack") && argc >= 5)
        {
            const NdsFile* f = rom.Find(argv[3]);
            if (!f) { fprintf(stderr, "no file %s\n", argv[3]); return 1; }
            const Bytes data = Plain(rom.Read(*f));
            if (cmd == "extract") { WriteFile(argv[4], data); return 0; }
            const Narc narc(data);
            std::filesystem::create_directories(argv[4]);
            for (size_t i = 0; i < narc.Count(); i++)
            {
                const Bytes m = Plain(narc.Member(i));
                const std::string name = narc.Name(i).empty() ? std::to_string(i) + "." + Sniff(m).Id : narc.Name(i);
                WriteFile((std::filesystem::path(argv[4]) / name).string(), m);
            }
            printf("%zu members\n", narc.Count());
            return 0;
        }
        return Usage();
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
