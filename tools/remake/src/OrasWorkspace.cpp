#include "OrasWorkspace.h"
#include "Bps.h"
#include "NitroCompression.h"

#include <cstdio>
#include <filesystem>

namespace remake
{

OrasWorkspace::Archive& OrasWorkspace::Open(const std::string& path)
{
    auto found = Archives.find(path);
    if (found != Archives.end()) return found->second;
    Archive& a = Archives[path];
    a.Data = Oras.Read(path);
    a.Game = std::make_unique<Garc>(a.Data);
    a.Now = std::make_unique<Garc>(a.Data);
    return a;
}

const Garc& OrasWorkspace::Original(const std::string& path) { return *Open(path).Game; }
Garc& OrasWorkspace::Edited(const std::string& path) { return *Open(path).Now; }

std::vector<std::string> OrasWorkspace::Write(const std::string& outDir)
{
    std::vector<std::string> log;
    char id[17], line[256];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)Oras.ProgramId());
    const std::filesystem::path mod = std::filesystem::path(outDir) / "load" / "mods" / id;
    for (const auto& [path, a] : Archives)
    {
        const Bytes data = a.Now->Write();
        if (data == a.Data) continue;
        const Garc back(data);
        if (data.size() < a.Data.size())
        {
            const std::filesystem::path whole = mod / "romfs" / path;
            std::filesystem::create_directories(whole.parent_path());
            WriteFile(whole.string(), data);
            snprintf(line, sizeof line, "%s: %zu members, %zu bytes (shorter than the game's %zu): written whole under romfs/", path.c_str(), back.Count(), data.size(), a.Data.size());
        }
        else
        {
            const Bytes bps = BpsCreate(a.Data, data);
            if (BpsApply(a.Data, bps) != data) throw FormatError(path + ": the patch does not rebuild the file");
            const std::filesystem::path file = mod / "romfs_ext" / (path + ".bps");
            std::filesystem::create_directories(file.parent_path());
            WriteFile(file.string(), bps);
            snprintf(line, sizeof line, "%s: %zu members, patch %zu bytes, checked", path.c_str(), back.Count(), bps.size());
        }
        log.push_back(line);
        if (path == "a/0/1/3" && back.Count() > 536)
        {
            // member 536 is the zone header table, 56 bytes a zone number (ORAS_ENGINE.md 2)
            const Bytes& raw = back.Sub(536);
            const size_t rows = (IsLzCompressed(raw) ? LzDecompress(raw) : raw).size() / 56;
            snprintf(line, sizeof line, "zone tables: %zu header rows (oras-engine --zone-rows %zu)", rows, rows);
            log.push_back(line);
        }
    }
    return log;
}

}
