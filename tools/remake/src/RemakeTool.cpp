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
#include "Inventory.h"
#include "Narc.h"
#include "NdsRom.h"
#include "NitroCompression.h"
#include "FormatSniffer.h"
#include "Garc.h"
#include "Nsbmd.h"
#include "Png.h"
#include "TextureIndex.h"
#include "AreaData.h"
#include "Bch.h"
#include "BinLinker.h"
#include "N3dsRom.h"
#include "MapHeaders.h"
#include "N3dsWorld.h"
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
                    "  remake_tool oras-world <rom.nds> <matrix index> <out dir>\n"
                    "  remake_tool bch <file.bch|GR piece> <out.gltf> [textures...]\n  remake_tool oras-list <oras.3ds>\n  remake_tool oras-extract <oras.3ds> <path> <out>\n"
                    "  remake_tool oras-mod <oras.3ds> <out dir> <path>=<file>...\n");
    return 2;
}

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

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
        // decrypted 3DS game images
        if (cmd == "oras-list" || cmd == "oras-extract" || cmd == "oras-mod")
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
            if (cmd == "oras-mod" && argc >= 5)
            {
                const std::filesystem::path root = std::filesystem::path(argv[3]) / "load" / "mods" / id / "romfs";
                for (int i = 4; i < argc; i++)
                {
                    const std::string arg = argv[i];
                    const size_t eq = arg.find('=');
                    if (eq == std::string::npos) { fprintf(stderr, "expected <path>=<file>: %s\n", arg.c_str()); return 2; }
                    const std::string path = arg.substr(0, eq);
                    if (!game.Has(path)) { fprintf(stderr, "no %s in the game's RomFS: a mod only replaces files the game has\n", path.c_str()); return 1; }
                    const Bytes data = ReadFile(arg.substr(eq + 1));
                    // an archive the game reads as GARC must still be one
                    if (Garc::Is(game.Read(path))) Garc check(data);
                    std::filesystem::create_directories((root / path).parent_path());
                    WriteFile((root / path).string(), data);
                    printf("%s <- %s (%zu bytes)\n", path.c_str(), arg.substr(eq + 1).c_str(), data.size());
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
        if (cmd == "oras-world" && argc >= 5)
        {
            auto archive = [&](const char* path) {
                const NdsFile* f = rom.Find(path);
                if (!f) throw FormatError(std::string("no ") + path + " in the cartridge (not Platinum?)");
                return Narc(Plain(rom.Read(*f)));
            };
            const Narc matrices = archive("fielddata/mapmatrix/map_matrix.narc"), lands = archive("fielddata/land_data/land_data.narc");
            const Narc areas = archive("fielddata/areadata/area_data.narc"), events = archive("fielddata/eventdata/zone_event.narc");
            const Narc mapTex = archive("fielddata/areadata/area_map_tex/map_tex_set.narc");
            const Narc buildingTex = archive("fielddata/areadata/area_build_model/areabm_texset.narc");
            const Narc buildingModels = archive("fielddata/build_model/build_model.narc");
            size_t tableAt = 0;
            const std::vector<MapHeader> headers = FindMapHeaders(rom.Arm9(), areas.Count(), matrices.Count(), events.Count(), &tableAt);
            const size_t matrix = (size_t)atoi(argv[3]);
            const WorldMap world(MapMatrix::Read(Plain(matrices.Member(matrix))), lands);

            // each cell's textures: its zone's area (the cell's map header; matrices without headers: the
            // first zone using the matrix)
            int defaultZone = -1;
            for (size_t h = 0; h < headers.size() && defaultZone < 0; h++) if (headers[h].Matrix == matrix) defaultZone = (int)h;
            std::map<uint16_t, std::unique_ptr<Tex0>> texSets, buildingSets;
            std::map<uint16_t, Bytes> texFiles, buildingFiles;
            auto tex0 = [&](const Narc& narc, uint16_t id, std::map<uint16_t, Bytes>& files, std::map<uint16_t, std::unique_ptr<Tex0>>& sets) -> const Tex0* {
                if (!sets.count(id))
                {
                    sets[id] = nullptr;
                    if (id < narc.Count())
                    {
                        files[id] = Plain(narc.Member(id));
                        const long at = Tex0::Find(files[id]);
                        if (at >= 0) sets[id] = std::make_unique<Tex0>(files[id], (size_t)at);
                    }
                }
                return sets[id].get();
            };
            std::vector<CellTextures> cellTex(world.Cells.size());
            for (size_t c = 0; c < cellTex.size(); c++)
            {
                const int zone = world.Matrix.Headers[c] >= 0 ? world.Matrix.Headers[c] : defaultZone;
                if (zone < 0 || (size_t)zone >= headers.size() || headers[zone].Area >= areas.Count()) continue;
                const AreaData area = AreaData::Read(Plain(areas.Member(headers[zone].Area)));
                cellTex[c] = {tex0(mapTex, area.MapTextures, texFiles, texSets), tex0(buildingTex, area.BuildingSet, buildingFiles, buildingSets)};
            }

            // the warps of every zone on this matrix (their tiles are the matrix's)
            std::vector<NdsWarp> warps;
            for (size_t h = 0; h < headers.size(); h++)
            {
                if (headers[h].Matrix != matrix) continue;
                const ZoneEvents ev = ZoneEvents::Read(Plain(events.Member(headers[h].Events)));
                for (size_t i = 0; i < ev.Warps.size(); i++) warps.push_back({ev.Warps[i], (uint16_t)h, (uint16_t)i});
            }

            const std::filesystem::path out(argv[4]);
            std::filesystem::create_directories(out);
            // the DS tile, measured from the terrain models
            float cell = 0;
            world.Gltf(nullptr, nullptr, nullptr, &cell);
            const N3dsWorld oras = N3dsWorld::Translate(world, cell / LandTiles, warps);
            const std::string json = oras.Json();
            WriteFile((out / "world_oras.json").string(), Bytes(json.begin(), json.end()));
            const std::string gltf = world.Gltf(nullptr, &buildingModels, nullptr, nullptr, oras.Scale(), &cellTex);
            WriteFile((out / "world_oras.gltf").string(), Bytes(gltf.begin(), gltf.end()));
            size_t placed = 0, warped = 0;
            for (const N3dsPiece& p : oras.Pieces) { placed += p.Buildings.size(); warped += p.Warps.size(); }
            printf("%zu map headers at ARM9 +0x%zx; matrix %zu (%s): DS tile %g units, ORAS %ux%u pieces of %u tiles (%zu used), scale %g, "
                   "%zu buildings, %zu of %zu warps, %zu map texture sets\n",
                   headers.size(), tableAt, matrix, world.Matrix.Name.c_str(), oras.NdsTile, oras.Width, oras.Height, N3dsMapTiles,
                   oras.Pieces.size(), oras.Scale(), placed, warped, warps.size(), texSets.size());
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
