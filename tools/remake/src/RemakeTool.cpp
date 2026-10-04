// remake_tool: looks into DS cartridges for the Pokemon remake work.
//   remake_tool info <rom.nds>                        title, game code, file count
//   remake_tool inventory <rom.nds>                   every file and archive member, as JSON
//   remake_tool extract <rom.nds> <path> <out>        one file (LZ data decompressed)
//   remake_tool unpack <rom.nds> <path.narc> <dir>    every member of a NARC, decompressed
#include "Inventory.h"
#include "Narc.h"
#include "NdsRom.h"
#include "NitroCompression.h"
#include "FormatSniffer.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace remake;

static int Usage()
{
    fprintf(stderr, "usage:\n  remake_tool info <rom.nds>\n  remake_tool inventory <rom.nds>\n"
                    "  remake_tool extract <rom.nds> <path> <out>\n  remake_tool unpack <rom.nds> <path.narc> <dir>\n");
    return 2;
}

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

int main(int argc, char** argv)
{
    if (argc < 3) return Usage();
    const std::string cmd = argv[1];
    try
    {
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
