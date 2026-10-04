// remake_tool: looks into DS cartridges for the Pokemon remake work.
//   remake_tool info <rom.nds>                        title, game code, file count
//   remake_tool inventory <rom.nds>                   every file and archive member, as JSON
//   remake_tool extract <rom.nds> <path> <out>        one file (LZ data decompressed)
//   remake_tool unpack <rom.nds> <path.narc> <dir>    every member of a NARC, decompressed
//   remake_tool garc <file.garc> <dir>                every sub-file of a 3DS GARC, decompressed
//   remake_tool textures <file.nsbtx|.nsbmd> <dir>    every texture as PNG
//   remake_tool model <file.nsbmd> <out.gltf> [tex.nsbtx] [model index]   a DS model as glTF
#include "Inventory.h"
#include "Narc.h"
#include "NdsRom.h"
#include "NitroCompression.h"
#include "FormatSniffer.h"
#include "Garc.h"
#include "Nsbmd.h"
#include "Png.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <string>

using namespace remake;

static int Usage()
{
    fprintf(stderr, "usage:\n  remake_tool info <rom.nds>\n  remake_tool inventory <rom.nds>\n"
                    "  remake_tool extract <rom.nds> <path> <out>\n  remake_tool unpack <rom.nds> <path.narc> <dir>\n"
                    "  remake_tool garc <file.garc> <dir>\n  remake_tool textures <file.nsbtx|.nsbmd> <dir>\n"
                    "  remake_tool model <file.nsbmd> <out.gltf> [tex.nsbtx] [model index]\n");
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

        const NdsRom rom(ReadFile(argv[2]));
        if (cmd == "info")
        {
            printf("title %s\ngame code %s\nfiles %zu\n", rom.Title().c_str(), rom.GameCode().c_str(), rom.Files().size());
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
