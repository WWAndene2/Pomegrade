#include "OrasWorkspace.h"

#include "Bps.h"
#include "BinLinker.h"
#include "GameText.h"
#include "NitroCompression.h"
#include "OrasNewZone.h"
#include "OrasTown.h"
#include "OrasZone.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

void OrasWorkspace::ListCharacters(int pack, const std::vector<int>& models)
{
    // a game pack's own list, for a game zone given characters (edit Z): the missing models appended, its entries kept, so
    // its zones show what they showed
    Garc& perArea = Edited("a/1/3/7");
    const Bytes was = perArea.Sub((size_t)pack);
    const Bytes list = CharacterList(Plain(was), models);
    if (list.size() != Plain(was).size()) perArea.Set((size_t)pack, IsLzCompressed(was) ? Lz11Compress(list) : list);
}

bool OrasWorkspace::Swims(int model)
{
    const Garc& perArea = Original("a/1/3/7");
    for (size_t p = 0; p < perArea.Count(); p++)
    {
        const Bytes list = Plain(perArea.Sub(p));
        if (list.size() % 24) continue;
        for (size_t at = 0; at + 24 <= list.size(); at += 24)
            if ((list[at] | list[at + 1] << 8) == model) return list[at + 2] == 0x01 && list[at + 3] == 0x06 && list[at + 6] == 0;
    }
    return false;
}

Bytes OrasWorkspace::CharacterList(int like, const std::vector<int>& models)
{
    // a new pack's list holds only the models its zones show (pack `like`'s whole list when none is given), so the 31 a list
    // may hold go to them
    return CharacterList(models.empty() ? Plain(Original("a/1/3/7").Sub((size_t)like)) : Bytes{}, models);
}

Bytes OrasWorkspace::CharacterList(Bytes list, const std::vector<int>& models)
{
    // a/1/3/7 member <pack>: 24 bytes a character model the pack's zones may show (u16 model, ..., u16 a/0/2/1 member at byte
    // 20); a zone showing a model not in its pack's list hangs the game on entering it (runs h5, h64: models 174 and 64 on
    // pack 8's copy, 17 October). Each model missing is added with the entry a game pack lists it with (the 235 models listed
    // somewhere have one entry each, the same in every pack); a model no pack lists gets model 284's entry with its own
    // number as model and member (member = model holds for every listed model under 4096; the other bytes inferred, not read)
    const Garc& perArea = Original("a/1/3/7");
    if (list.size() % 24) throw FormatError("an a/1/3/7 member is not 24-byte entries");
    const auto listed = [](const Bytes& l, int model) {
        for (size_t at = 0; at + 24 <= l.size(); at += 24) if ((l[at] | l[at + 1] << 8) == model) return (long)at;
        return -1L;
    };
    for (int model : models)
    {
        if (model < 0 || model > 0xFFFF) throw FormatError("character model " + std::to_string(model) + " out of range");
        if (listed(list, model) >= 0) continue;
        Bytes entry;
        for (size_t p = 0; p < perArea.Count() && entry.empty(); p++)
        {
            const Bytes other = Plain(perArea.Sub(p));
            if (other.size() % 24) continue;
            if (const long at = listed(other, model); at >= 0) entry.assign(other.begin() + at, other.begin() + at + 24);
        }
        if (entry.empty())
        {
            for (size_t p = 0; p < perArea.Count() && entry.empty(); p++)
            {
                const Bytes other = Plain(perArea.Sub(p));
                if (other.size() % 24) continue;
                if (const long at = listed(other, 284); at >= 0) entry.assign(other.begin() + at, other.begin() + at + 24);
            }
            if (entry.empty()) throw FormatError("no a/1/3/7 entry for model 284 to copy");
            if (model >= 4096) throw FormatError("character model " + std::to_string(model) + " is listed in no area pack (its member is not its number)");
            entry[0] = entry[20] = (uint8_t)model;
            entry[1] = entry[21] = (uint8_t)(model >> 8);
        }
        list.insert(list.end(), entry.begin(), entry.end());
    }
    // the list's length: the game caches it after the player's own entry in 32 slots of 24 bytes (field character manager
    // +0x36CC) and stops in its fatal error when entries + 1 > 32 (cmp #0x21 at 0x3F7308, Event_LoadCharacterArchiveMember;
    // read 17 October), so 31. Checked: 31 load (run hfix), 33 hang entering the zone (run hC). oras-engine
    // --character-models N raises it to N (OrasEngine.cpp), CharacterModels then N
    if (list.size() / 24 > CharacterModels)
        throw FormatError("an area pack's character list would hold " + std::to_string(list.size() / 24) + " models, the game loads " +
                          std::to_string(CharacterModels) + (CharacterModels == 31 ? " (spread them over zones of other packs, or raise it with oras-engine --character-models)" : ""));
    return list;
}

int OrasWorkspace::AddAreaPack(const Bytes& pack, int like, const std::vector<int>& models)
{
    const Bytes characters = CharacterList(like, models);
    const Bytes charactersStored = IsLzCompressed(Original("a/1/3/7").Sub((size_t)like)) ? Lz11Compress(characters) : characters;
    // a pack the build has added already, byte for byte (the same game pack with the same textures added: every room of a kind)
    // shares its slot; a new one takes one of the game's 9 free slots, then goes past the game's packs
    for (const auto& [slot, data] : PacksAdded) if (data == pack && PackCharacters.at(slot) == characters) return slot;
    const Garc& areas = Original("a/0/1/4");
    const Garc& perAreaGame = Original("a/1/3/7");
    // the pack appended past the game's (ORAS_ENGINE.md 7, run apkA, 16 October: a zone on appended pack 230 loads): a/0/1/4's
    // last member is not a pack but a table of 44 bytes a pack (228 entries), which stays where it is, extended up to the new
    // pack with entries like `like`'s; a/1/3/7 (one member a pack) filled up to it likewise. The table moved last instead
    // (the new pack in its place) is fatal (run apk8), and so is an appended pack with no entry in it (run apk5)
    const auto append = [&]()
    {
        Garc& packs = Edited("a/0/1/4");
        Garc& perArea = Edited("a/1/3/7");
        const size_t tableAt = areas.Count() - 1, p = packs.Count(), entry = 44;
        const Bytes gameTable = areas.Sub(tableAt);
        Bytes table = Plain(packs.Sub(tableAt));
        if (table.size() < ((size_t)like + 1) * entry) throw FormatError("the area pack table has no entry for pack " + std::to_string(like));
        while (table.size() < (p + 1) * entry) table.insert(table.end(), table.begin() + like * entry, table.begin() + (like + 1) * entry);
        packs.Set(p, IsLzCompressed(areas.Sub(8)) ? Lz11Compress(pack) : pack);
        packs.Set(tableAt, IsLzCompressed(gameTable) ? Lz11Compress(table) : table);
        while (perArea.Count() < p) perArea.Set(perArea.Count(), perAreaGame.Sub((size_t)like));
        perArea.Set(p, charactersStored);
        PacksAdded[(int)p] = pack;
        PackCharacters[(int)p] = characters;
        return (int)p;
    };
    // POMEGRADE_APPEND_AREA_PACKS=1: appended even while a free slot is left, to test appending
    if (const char* force = std::getenv("POMEGRADE_APPEND_AREA_PACKS"); force && std::string(force) == "1") return append();
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
            Edited("a/1/3/7").Set(p, charactersStored);
            PacksAdded[(int)p] = pack;
            PackCharacters[(int)p] = characters;
            return (int)p;
        }
    return append();
}

void OrasWorkspace::RegisterHeaderZone(int header, int zone, bool interior, const std::vector<HeaderWarp>& warps)
{
    for (const HeaderZone& h : HeaderZones)
        if (h.Header == header) throw FormatError("map header " + std::to_string(header) + " has a zone already (" + std::to_string(h.Zone) + ")");
    HeaderZones.push_back({header, zone, interior, warps});
}

std::vector<std::string> OrasWorkspace::LinkWarps()
{
    std::vector<std::string> log;
    char line[256];
    std::map<int, const HeaderZone*> byHeader;
    for (const HeaderZone& h : HeaderZones) byHeader[h.Header] = &h;
    // the warps each zone keeps, in order: those whose header was built
    std::map<int, std::vector<const HeaderWarp*>> kept;
    for (const HeaderZone& h : HeaderZones)
        for (const HeaderWarp& w : h.Warps)
        {
            if (byHeader.count(w.DestHeader)) { kept[h.Zone].push_back(&w); continue; }
            snprintf(line, sizeof line, "zone %d (header %d): its warp at (%d, %d) to header %d leads nowhere yet (no step built that header): left out",
                     h.Zone, h.Header, w.TileX, w.TileZ, w.DestHeader);
            log.push_back(line);
        }
    // an interior belongs to the outdoor zone its warps lead out to, through other interiors if need be (a house's upstairs);
    // its floor is how many interiors lie between: stairs to a higher one go up
    std::map<int, int> overworld, floor;
    for (const HeaderZone& h : HeaderZones) if (!h.Interior) { overworld[h.Zone] = h.Zone; floor[h.Zone] = -1; }
    for (bool grew = true; grew;)
    {
        grew = false;
        for (const HeaderZone& h : HeaderZones)
            if (!overworld.count(h.Zone))
                for (const HeaderWarp* w : kept[h.Zone])
                {
                    const int to = byHeader.at(w->DestHeader)->Zone;
                    if (overworld.count(to)) { overworld[h.Zone] = overworld.at(to); floor[h.Zone] = floor.at(to) + 1; grew = true; break; }
                }
    }
    Garc& zones = Edited("a/0/1/3");
    Bytes table = Plain(zones.Sub(536));
    for (const HeaderZone& h : HeaderZones)
    {
        const Bytes data = Plain(zones.Sub((size_t)h.Zone));
        OrasZone zone = OrasZone::Read(data);
        zone.Doors.clear();
        for (const HeaderWarp* w : kept[h.Zone])
        {
            const HeaderZone& to = *byHeader.at(w->DestHeader);
            // the arrival: the destination's first warp leading back to this header
            int back = 0;
            const auto& theirs = kept[to.Zone];
            for (size_t k = 0; k < theirs.size(); k++) if (theirs[k]->DestHeader == h.Header) { back = (int)k; break; }
            if (!h.Interior) zone.Doors.push_back(NewDoorWarp(to.Zone, back, w->TileX, w->TileZ));
            else if (!to.Interior) zone.Doors.push_back(NewExitWarp(to.Zone, back, w->TileX * 18 + 9, w->TileZ * 18 + 9));
            else
            {
                if (!floor.count(h.Zone) || !floor.count(to.Zone)) throw FormatError("stairs between interiors that lead out to no outdoor zone");
                zone.Doors.push_back(NewStairsWarp(to.Zone, back, w->TileX, w->TileZ, w->Walk, floor.at(to.Zone) > floor.at(h.Zone)));
            }
        }
        if (h.Interior)
        {
            if (!overworld.count(h.Zone)) throw FormatError("interior zone " + std::to_string(h.Zone) + " (header " + std::to_string(h.Header) + ") leads out to no outdoor zone of the build");
            zone.Header[13] = (uint16_t)overworld.at(h.Zone);
        }
        const Bytes written = zone.Write(data);
        // a new zone has no member in the game's archive: compressed as the game's zones are (Littleroot's, member 6)
        zones.Set((size_t)h.Zone, IsLzCompressed(Original("a/0/1/3").Sub(6)) ? Lz11Compress(written) : written);
        for (size_t k = 0; k < 28; k++) { table[(size_t)h.Zone * RowBytes + 2 * k] = (uint8_t)zone.Header[k]; table[(size_t)h.Zone * RowBytes + 2 * k + 1] = (uint8_t)(zone.Header[k] >> 8); }
        std::string list;
        for (const ZoneDoor& d : zone.Doors) { snprintf(line, sizeof line, " %d.%d", d.DestZone(), d.DestWarp()); list += line; }
        snprintf(line, sizeof line, "zone %d (header %d%s): %zu warp(s) to zone.warp%s", h.Zone, h.Header, h.Interior ? ", interior" : "", zone.Doors.size(), list.c_str());
        log.push_back(line);
    }
    if (!HeaderZones.empty()) ReplaceMember(zones, Original("a/0/1/3"), 536, table, "");
    return log;
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
