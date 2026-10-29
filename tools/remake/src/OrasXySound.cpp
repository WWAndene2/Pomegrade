#include "OrasXySound.h"

#include "Bps.h"

#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstring>

namespace remake
{

namespace
{
constexpr uint32_t CodeBase = 0x100000, Categories = 0x17;

uint32_t At(const Bytes& code, uint32_t address)
{
    if (address < CodeBase || address - CodeBase + 4 > code.size()) throw FormatError("sound ids: an address outside the code");
    return U32(code, address - CodeBase);
}

// every id of a code's categories and the archive item it resolves to
std::map<uint32_t, uint32_t> AllIds(const Bytes& code, uint32_t table)
{
    std::map<uint32_t, uint32_t> ids;
    for (uint32_t c = 0; c < Categories; c++)
    {
        const uint32_t count = At(code, table + 8 * c), list = At(code, table + 8 * c + 4);
        if (!list) continue;
        for (uint32_t n = 0; n < count; n++) ids[c << 16 | n] = At(code, list + 4 * n);
    }
    return ids;
}

}

uint32_t SoundCategoryTable(const Bytes& code, const std::string& archivePath)
{
    const auto it = std::search(code.begin(), code.end(), archivePath.begin(), archivePath.end());
    if (it == code.end()) throw FormatError("sound ids: no " + archivePath + " in the code");
    const size_t path = (size_t)(it - code.begin());
    // the table: category 0 (0, 0), then category 1's count and its list, just past the path
    for (size_t at = path & ~(size_t)3; at >= 8 && path - at < 0x400; at -= 4)
        if (U32(code, at) == 0 && U32(code, at + 4) == 0 && U32(code, at + 8) > 0 && U32(code, at + 8) < 0x400 &&
            U32(code, at + 12) > CodeBase + path && U32(code, at + 12) < CodeBase + path + 0x80)
            return (uint32_t)(CodeBase + at);
    throw FormatError("sound ids: no category table before " + archivePath);
}

XySounds XySounds::Read(const Bytes& orasCode, const Bytes& orasArchive, const Bytes& xCode, const Bytes& xArchive)
{
    XySounds s;
    s.Streams = BcsarStreams(xArchive);
    s.First = BcsarSoundCount(orasArchive);
    std::map<size_t, size_t> rank; // X/Y's stream sound -> its place among the appended
    for (size_t k = 0; k < s.Streams.size(); k++) rank[s.Streams[k].Index] = k;
    s.Sequences = BcsarDifferentSequences(orasArchive, xArchive);
    s.SequenceId = (uint32_t)s.Streams.size() + 1;
    std::map<size_t, size_t> sequence; // X/Y's sound effect ported -> its place among them
    for (size_t j = 0; j < s.Sequences.size(); j++) sequence[s.Sequences[j].first] = j;

    const std::map<uint32_t, uint32_t> orasIds = AllIds(orasCode, SoundCategoryTable(orasCode, "rom:/sound/sango_sound.bcsar"));
    const std::map<uint32_t, uint32_t> xIds = AllIds(xCode, SoundCategoryTable(xCode, "rom:/sound/xy_sound.bcsar"));
    std::map<uint32_t, uint32_t> orasIdOfItem;
    for (const auto& [id, item] : orasIds) orasIdOfItem.emplace(item, id); // the first id of an item
    const std::vector<std::string> orasNames = BcsarSoundNames(orasArchive), xNames = BcsarSoundNames(xArchive);
    std::map<std::string, size_t> orasSoundOfName;
    for (size_t index = 0; index < orasNames.size(); index++) if (!orasNames[index].empty()) orasSoundOfName.emplace(orasNames[index], index);

    for (const auto& [id, item] : xIds)
    {
        // sounds only (item type 1): the other categories list banks (3), players (4), wave archives (5) and groups (6),
        // which no zone or script plays
        if ((item >> 24) != 1) continue;
        const size_t sound = item & 0xFFFFFF;
        const auto stream = rank.find(sound);
        if (stream != rank.end()) { s.Ids[id] = (uint32_t)(stream->second + 1); continue; } // category 0
        const auto ported = sequence.find(sound);
        if (ported != sequence.end()) { s.Ids[id] = s.SequenceId + (uint32_t)ported->second; continue; } // category 0
        // not a stream of X/Y's: Omega Ruby's sound of its name, by the first id Omega Ruby's categories give it
        const std::string name = sound < xNames.size() ? xNames[sound] : "";
        const auto oras = orasSoundOfName.find(name);
        const auto same = oras == orasSoundOfName.end() ? orasIdOfItem.end() : orasIdOfItem.find(0x01000000u | (uint32_t)oras->second);
        if (same != orasIdOfItem.end()) { s.Ids[id] = same->second; continue; }
        char line[96];
        snprintf(line, sizeof line, "0x%X (%s)", id, name.empty() ? "no name" : name.c_str());
        s.Unmatched.push_back(line);
    }
    return s;
}

std::vector<std::string> WriteXySoundArchive(N3dsRom& oras, const Bytes& original, Bytes archive, const Bytes& xyArchive, const XySounds& sounds,
                                             bool streams, const std::string& outDir)
{
    std::vector<std::string> log;
    size_t first = 0;
    std::string shared;
    archive = BcsarAppendSequences(archive, xyArchive, sounds.Sequences, first, shared);
    if (first != sounds.First + (streams ? sounds.Streams.size() : 0)) throw FormatError("sound effects: the archive's first new sound is not the one the ids were given");
    const Bytes bps = BpsCreate(original, archive);
    if (BpsApply(original, bps) != archive) throw FormatError("sango_sound.bcsar: the patch does not rebuild the archive");
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path dir = std::filesystem::path(outDir) / "load" / "mods" / id / "romfs_ext" / "sound";
    std::filesystem::create_directories(dir);
    WriteFile((dir / "sango_sound.bcsar.bps").string(), bps);
    char line[200];
    snprintf(line, sizeof line, "sound effects: %zu of X/Y's ported as sounds %zu-%zu, ids %u-%u of category 0; %s (archive patch %zu bytes)",
             sounds.Sequences.size(), first, first + sounds.Sequences.size() - 1, sounds.SequenceId, sounds.SequenceId + (uint32_t)sounds.Sequences.size() - 1,
             shared.c_str(), bps.size());
    log.push_back(line);
    return log;
}

uint32_t XySounds::Translate(uint32_t xId) const
{
    const auto it = Ids.find(xId);
    return it == Ids.end() ? xId : it->second;
}

}
