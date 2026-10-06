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
//   remake_tool oras-region <platinum.nds> <oras.3ds> <out dir> --rect LEFT TOP WIDTH HEIGHT --zone HEADER:ZONE... [--auto-zones] [--others-out] [--plan] [--matrix-template M]
//                     [--model-matrix NN] [oras-town's kit options]
//                     a rectangle of Sinnoh's piece grid (as oras-world cuts it) rebuilt as a new ORAS map matrix: its pieces built
//                     as oras-town builds one, the zone grid from Platinum's map headers, each header on the ORAS zone given
//                     (-1: left out); --plan prints the rectangle's headers and builds nothing. Writes the mod, region_preview.gltf,
//                     region_plan.txt and region_piece_<x>_<y>.bin
//   remake_tool oras-save <main> [<out> <zone> <tile x> <tile z>]
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
#include "Bch.h"
#include "Bps.h"
#include "BinLinker.h"
#include "N3dsRom.h"
#include "MapHeaders.h"
#include "N3dsWorld.h"
#include "OrasInspect.h"
#include "OrasMeasure.h"
#include "TopView.h"
#include "OrasAppend.h"
#include "OrasRegion.h"
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
                    "  remake_tool oras-append-test <oras.3ds> <out dir> unused|piece|matrix|zone\n"
                    "  remake_tool oras-town <platinum.nds> <oras.3ds> <out dir> [--matrix N] [--left X --top Y] [--target P --donor P --trees P]\n"
                    "                    [--cell X Y] [--zone Z] [--area A] [--donor-pack P] [--grass P] [--snow P] [--fence P] [--snow-clumps 0|1] [--pond-wall 0-2] [--zone-pack 0|1] [--zone-warps 0|1] [--add-warps 0|1] [--piece 0|1] [--tree-reach N] [--door-type T] [--donor-as-is 0|1] [--pad-piece BYTES] [--piece-files MASK] [--allow-errors]\n"
                    "  remake_tool oras-code <oras.3ds> <out.bin>\n"
                    "  remake_tool oras-region <platinum.nds> <oras.3ds> <out dir> --rect LEFT TOP WIDTH HEIGHT --zone HEADER:ZONE... [--plan]\n"
                    "  remake_tool oras-sinnoh <platinum.nds> <oras.3ds> [strip width]\n"
                    "  remake_tool oras-save <main> [<out> <zone> <tile x> <tile z>]\n"
                    "                    [--matrix-template M] [--model-matrix NN] [oras-town's --matrix, --target, --donor, --trees, --donor-pack, --grass, ... --allow-errors]\n");
    return 2;
}

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

// the options oras-town and oras-region share: Platinum's matrix, the kit pieces and packs, the builder's switches. false: not one of them
static bool TownKitOption(const std::string& flag, int argc, char** argv, int& i, OrasTownOptions& options)
{
    auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return atoi(argv[at]); };
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
        if (cmd == "oras-append-test" && argc >= 5)
        {
            const std::string what = argv[4];
            const AppendTest test = what == "unused" ? AppendTest::Unused : what == "piece" ? AppendTest::Piece : what == "matrix" ? AppendTest::Matrix
                                  : what == "zone" ? AppendTest::Zone : throw FormatError("append test: unused, piece, matrix or zone");
            N3dsRom oras(argv[2]);
            for (const std::string& line : BuildAppendTest(oras, test, argv[3])) printf("%s\n", line.c_str());
            return 0;
        }
        if (cmd == "oras-town" && argc >= 5)
        {
            OrasTownOptions options;
            options.OutDir = argv[4];
            for (int i = 5; i < argc; i++)
            {
                const std::string flag = argv[i];
                auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return atoi(argv[at]); };
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
                auto number = [&](int at) { if (at >= argc) throw FormatError("missing a number after " + flag); return atoi(argv[at]); };
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
                else if (flag == "--auto-zones") options.AutoZones = true;
                else { fprintf(stderr, "unknown option %s\n", flag.c_str()); return 2; }
            }
            const NdsRom platinum(ReadFile(argv[2]));
            N3dsRom oras(argv[3]);
            for (const std::string& line : BuildOrasRegion(platinum, oras, options)) printf("%s%s", line.c_str(), !line.empty() && line.back() == '\n' ? "" : "\n");
            if (!options.PlanOnly) printf("mod written under %s: copy its load folder into the 3DS folder (Pomegrade/3DS)\n", options.OutDir.c_str());
            return 0;
        }
        // decrypted 3DS game images
        if (cmd == "oras-list" || cmd == "oras-extract" || cmd == "oras-mod" || cmd == "oras-patch")
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

        if (cmd == "oras-save" && argc >= 3)
        {
            // print where an ORAS save puts the player; with <out> <zone> <tile x> <tile z>, write a copy moved there
            OrasSave save = OrasSave::Read(ReadFile(argv[2]));
            printf("%zu blocks, checksums match; player in zone %d at tile (%.2f, %.2f)\n", save.Blocks.size(), save.Zone(), save.X() / 18, save.Z() / 18);
            if (argc >= 7)
            {
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
