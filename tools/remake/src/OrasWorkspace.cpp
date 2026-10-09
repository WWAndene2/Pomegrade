#include "OrasWorkspace.h"
#include "Bps.h"
#include "BinLinker.h"
#include "GameText.h"
#include "NitroCompression.h"
#include "OrasTown.h"

#include <algorithm>
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

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
static constexpr size_t RowBytes = 56; // a zone's row of the header table

int OrasWorkspace::AddZone(const Bytes& zone, const Bytes& encounters)
{
    Garc& zones = Edited("a/0/1/3");
    const Garc& game = Original("a/0/1/3");
    // compressed as the game's zones are (Littleroot's, member 6)
    const size_t number = AppendMember(zones, game, 6, zone, "ZO");
    Bytes table = Plain(zones.Sub(536));
    if (table.size() < 536 * RowBytes || table.size() % RowBytes) throw FormatError("member 536 is not the zone header table (56-byte rows, 536 or more)");
    BinLinker en = BinLinker::Read(Plain(zones.Sub(537)), "EN");
    if (en.Files.size() < 536) throw FormatError("member 537 is not the encounter container (536 files or more)");
    const Bytes header = BinLinker::Read(zone, "ZO").Files.at(0);
    if (header.size() != RowBytes) throw FormatError("a zone's header is not 56 bytes");
    while (table.size() < number * RowBytes) table.insert(table.end(), header.begin(), header.end());
    if (table.size() != number * RowBytes) throw FormatError("the header table holds a row for zone " + std::to_string(number) + " already");
    table.insert(table.end(), header.begin(), header.end());
    while (en.Files.size() < number) en.Files.push_back(Bytes{});
    en.Files.push_back(encounters);
    ReplaceMember(zones, game, 536, table, "");
    ReplaceMember(zones, game, 537, en.Write(), "EN");
    return (int)number;
}

int OrasWorkspace::AddZoneText(const std::vector<std::string>& lines)
{
    const Bytes file = WriteGameText(lines);
    if (ReadGameText(file) != lines) throw FormatError("a zone's text does not read back");
    int member = -1;
    for (int a = 9; a <= 16; a++)
    {
        const std::string path = "a/0/" + std::to_string(7 + a / 10) + "/" + std::to_string(a % 10);
        Garc& texts = Edited(path);
        const int index = (int)texts.Count();
        texts.Set((size_t)index, IsLzCompressed(Original(path).Sub(0)) ? Lz11Compress(file) : file);
        if (member < 0) member = index;
        else if (index != member) throw FormatError(path + ": not as many members as a/0/7/9");
    }
    return member;
}

int OrasWorkspace::AddPlaceName(const std::string& name)
{
    int line = -1;
    for (int a = 1; a <= 8; a++)
    {
        const std::string path = "a/0/7/" + std::to_string(a);
        Garc& names = Edited(path);
        const Bytes file = Plain(names.Sub(90));
        const std::vector<std::string> lines = ReadGameText(file);
        const Bytes added = AppendGameTextLine(file, name);
        const std::vector<std::string> back = ReadGameText(added);
        if (back.size() != lines.size() + 1 || !std::equal(lines.begin(), lines.end(), back.begin()) || back.back() != name)
            throw FormatError(path + " member 90: the added place name does not read back");
        if (line < 0) line = (int)lines.size();
        else if ((int)lines.size() != line) throw FormatError(path + " member 90: not as many place names as a/0/7/1's");
        ReplaceMember(names, Original(path), 90, added, "");
    }
    if (line > 0x3FF) throw FormatError("no place name number left (10 bits)");
    return line;
}

int OrasWorkspace::AddAreaPack(const Bytes& pack, int like)
{
    const Garc& areas = Original("a/0/1/4");
    const Garc& perAreaGame = Original("a/1/3/7");
    const size_t slots = std::min(areas.Count(), perAreaGame.Count());
    if (PacksTaken.empty()) PacksTaken.assign(slots, false);
    // the packs every zone of the build draws from (the header table's word 1, game zones and new ones alike)
    std::vector<bool> used(slots, false);
    const Bytes table = Plain(Edited("a/0/1/3").Sub(536));
    for (size_t z = 0; (z + 1) * RowBytes <= table.size(); z++)
    {
        const size_t p = (size_t)(table[z * RowBytes + 2] | table[z * RowBytes + 3] << 8);
        if (p < slots) used[p] = true;
    }
    for (size_t p = 0; p < slots; p++)
        if (!used[p] && !PacksTaken[p])
        {
            PacksTaken[p] = true;
            ReplaceMember(Edited("a/0/1/4"), areas, p, pack, "AD");
            Edited("a/1/3/7").Set(p, perAreaGame.Sub((size_t)like));
            return (int)p;
        }
    throw FormatError("no free area pack left (the game has " + std::to_string(std::count(used.begin(), used.end(), false)) + " unused ones)");
}

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
