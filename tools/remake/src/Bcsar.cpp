#include "Bcsar.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace remake
{

namespace
{
void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }
void Put32(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b.at(at + k) = (uint8_t)(v >> (8 * k)); }

struct Layout
{
    const Bytes& B;
    size_t Info = 0, Body = 0, File = 0, InfoSize = 0, StringTable = 0;
    explicit Layout(const Bytes& b) : B(b)
    {
        if (b.size() < 0x40 || std::memcmp(&b[0], "CSAR", 4) != 0) throw FormatError("not a sound archive (no CSAR)");
        for (uint16_t k = 0; k < U16(b, 0x10); k++)
        {
            const uint16_t type = U16(b, 0x14 + 12 * k);
            const size_t at = U32(b, 0x18 + 12 * k);
            if (type == 0x2001) { Info = at; InfoSize = U32(b, 0x1C + 12 * k); }
            if (type == 0x2002) File = at;
            if (type == 0x2000) StringTable = at + 8 + U32(b, at + 12);
        }
        if (!Info || !File || std::memcmp(&b[Info], "INFO", 4) != 0) throw FormatError("a sound archive without INFO or FILE");
        Body = Info + 8;
    }
    size_t Table(int k) const { return Body + U32(B, Body + 8 * (size_t)k + 4); } // 0 sounds, 6 files
    size_t Count(int k) const { return U32(B, Table(k)); }
    // the 3DS adds an offset in 32 bits: the grown tables, past their entries, reach them with offsets that wrap
    size_t Entry(int k, size_t i) const { return (uint32_t)(Table(k) + U32(B, Table(k) + 8 + 8 * i)); }
    std::string Name(uint32_t string) const
    {
        if (!StringTable || string >= U32(B, StringTable)) return "";
        const size_t at = StringTable + U32(B, StringTable + 8 + 12 * (size_t)string), size = U32(B, StringTable + 12 + 12 * (size_t)string);
        return std::string(B.begin() + (long)at, B.begin() + (long)(at + size - 1));
    }
};

// tables of INFO's body (Layout::Table): 0 sounds, 2 banks, 3 wave archives, 6 files
constexpr int Sounds = 0, Banks = 2, Warcs = 3, Files = 6;

// an internal file's place in the archive and its size (entry 0x220C: a reference to its location, a FILE-body offset and
// a size); {0, 0} for an external or missing one
std::pair<size_t, size_t> FileAt(const Layout& l, uint32_t file)
{
    const Bytes& b = l.B;
    if (file >= l.Count(Files)) return {0, 0};
    const size_t e = l.Entry(Files, file);
    if (U16(b, e) != 0x220C) return {0, 0};
    const size_t loc = e + U32(b, e + 4), at = U32(b, loc + 4), size = U32(b, loc + 8);
    if (at == 0xFFFFFFFF || l.File + 8 + at + size > b.size()) return {0, 0};
    return {l.File + 8 + at, size};
}

// a file's bytes with its version word (+8) cleared: 0x0100 in X/Y's, 0x0101 in Omega Ruby's for the same contents
Bytes Unversioned(const Layout& l, uint32_t file)
{
    const auto [at, size] = FileAt(l, file);
    Bytes out(l.B.begin() + (long)at, l.B.begin() + (long)(at + size));
    if (out.size() >= 12) Put32(out, 8, 0);
    return out;
}

uint32_t ItemFile(const Layout& l, int table, uint32_t id) { return (id & 0xFFFFFF) < l.Count(table) ? U32(l.B, l.Entry(table, id & 0xFFFFFF)) : 0xFFFFFFFF; }

// a bank file's wave id table (its INFO body's first reference): its place and count
std::pair<size_t, size_t> WaveTable(const Bytes& bank)
{
    if (bank.size() < 0x20 || std::memcmp(&bank[0], "CBNK", 4) != 0) throw FormatError("sound archive: a bank that is no CBNK");
    for (uint16_t k = 0; k < U16(bank, 0x10); k++)
        if (U16(bank, 0x14 + 12 * k) == 0x5800)
        {
            const size_t body = U32(bank, 0x18 + 12 * k) + 8;
            const size_t table = body + U32(bank, body + 4);
            return {table + 4, U32(bank, table)};
        }
    throw FormatError("sound archive: a bank without INFO");
}

// a sequence sound's bank table (its details' reference +4): its place and count
std::pair<size_t, size_t> BankTable(const Bytes& b, size_t entry)
{
    const size_t details = entry + U32(b, entry + 16), table = details + U32(b, details + 4);
    return {table + 4, U32(b, table)};
}

// what a sound plays, game-independent (sound_compare.py's key): its sequence file, then each bank with its wave archive ids
// as their archives' contents
Bytes SoundKey(const Layout& l, size_t sound, std::map<uint32_t, Bytes>& warcCache)
{
    const Bytes& b = l.B;
    const size_t e = l.Entry(Sounds, sound);
    Bytes key = Unversioned(l, U32(b, e));
    const auto [table, count] = BankTable(b, e);
    for (size_t k = 0; k < count; k++)
    {
        Bytes bank = Unversioned(l, ItemFile(l, Banks, U32(b, table + 4 * k)));
        if (bank.empty()) continue;
        const auto [waves, n] = WaveTable(bank);
        Bytes withArchives;
        size_t from = 0;
        for (size_t w = 0; w < n; w++)
        {
            const uint32_t warc = U32(bank, waves + 8 * w);
            if (!warcCache.count(warc)) warcCache[warc] = Unversioned(l, ItemFile(l, Warcs, warc));
            withArchives.insert(withArchives.end(), bank.begin() + (long)from, bank.begin() + (long)(waves + 8 * w));
            const Bytes& contents = warcCache[warc];
            withArchives.insert(withArchives.end(), contents.begin(), contents.end());
            from = waves + 8 * w + 4;
        }
        withArchives.insert(withArchives.end(), bank.begin() + (long)from, bank.end());
        key.insert(key.end(), withArchives.begin(), withArchives.end());
    }
    return key;
}

// The archive without the given byte ranges of INFO (the tables a growth replaced and left unused, each a table's count and
// references): every reference that crosses one is moved, the sections after INFO follow (INFO shrinks by a multiple of
// 0x40, padded at its end, as FILE starts on 0x40 in the game's archives). The references moved: INFO's body to its
// tables, each table's (0-6, the eighth the project's information) to its entries; an entry's own references stay within
// it (read 29 October on both games' archives: sounds, banks, wave archives, players, groups, files)
Bytes WithoutInfoRanges(const Bytes& b, std::vector<std::pair<size_t, size_t>> ranges)
{
    const Layout l(b);
    std::sort(ranges.begin(), ranges.end());
    size_t removed = 0;
    for (const auto& [at, size] : ranges)
    {
        if (at < l.Body || at + size > l.Info + l.InfoSize) throw FormatError("sound archive: a range to remove outside INFO");
        removed += size;
    }
    const size_t kept = removed & ~(size_t)0x3F, pad = removed - kept;
    const auto moved = [&](size_t at) { size_t before = 0; for (const auto& [r, size] : ranges) if (r + size <= at) before += size; return at - before; };
    Bytes info(b.begin() + (long)l.Info, b.begin() + (long)(l.Info + l.InfoSize));
    const auto put = [&](size_t at, uint32_t v) { Put32(info, at - l.Info, v); };
    for (int k = 0; k < 8; k++)
    {
        const size_t table = l.Table(k);
        put(l.Body + 8 * (size_t)k + 4, (uint32_t)(moved(table) - l.Body));
        if (k == 7) continue;
        for (size_t i = 0; i < l.Count(k); i++) put(table + 8 + 8 * i, (uint32_t)(moved(l.Entry(k, i)) - moved(table)));
    }
    Bytes out(b.begin(), b.begin() + (long)l.Info);
    size_t from = l.Info;
    for (const auto& [at, size] : ranges)
    {
        out.insert(out.end(), info.begin() + (long)(from - l.Info), info.begin() + (long)(at - l.Info));
        from = at + size;
    }
    out.insert(out.end(), info.begin() + (long)(from - l.Info), info.end());
    out.insert(out.end(), pad, 0);
    out.insert(out.end(), b.begin() + (long)(l.Info + l.InfoSize), b.end());
    Put32(out, l.Info + 4, (uint32_t)(l.InfoSize - kept));
    for (uint16_t k = 0; k < U16(b, 0x10); k++)
    {
        const size_t at = U32(b, 0x18 + 12 * k);
        if (at == l.Info) Put32(out, 0x1C + 12 * k, (uint32_t)(l.InfoSize - kept));
        else if (at > l.Info) Put32(out, 0x18 + 12 * k, (uint32_t)(at - kept));
    }
    Put32(out, 0x0C, (uint32_t)out.size());
    return out;
}

uint32_t Options(const Bytes& b, size_t entry, int bit) // the option value of a set flag bit, or ~0
{
    const uint32_t flags = U32(b, entry + 20);
    if (!(flags & (1u << bit))) return 0xFFFFFFFF;
    size_t k = 0;
    for (int f = 0; f < bit; f++) if (flags & (1u << f)) k++;
    return U32(b, entry + 24 + 4 * k);
}
}

size_t BcsarSoundCount(const Bytes& bcsar) { return Layout(bcsar).Count(0); }

uint8_t BcsarSoundVolume(const Bytes& bcsar, size_t sound)
{
    const Layout l(bcsar);
    if (sound >= l.Count(Sounds)) throw FormatError("sound archive: no sound " + std::to_string(sound));
    return bcsar[l.Entry(Sounds, sound) + 8];
}

void BcsarSetSoundVolume(Bytes& bcsar, size_t sound, uint8_t volume)
{
    const size_t at = [&] {
        const Layout l(bcsar);
        if (sound >= l.Count(Sounds)) throw FormatError("sound archive: no sound " + std::to_string(sound));
        return l.Entry(Sounds, sound) + 8;
    }();
    bcsar[at] = volume;
}

std::vector<std::string> BcsarSoundNames(const Bytes& b)
{
    const Layout l(b);
    std::vector<std::string> names;
    for (size_t i = 0; i < l.Count(0); i++)
    {
        const uint32_t name = Options(b, l.Entry(0, i), 0);
        names.push_back(name == 0xFFFFFFFF ? "" : l.Name(name));
    }
    return names;
}

std::vector<BcsarStream> BcsarStreams(const Bytes& b)
{
    const Layout l(b);
    std::vector<BcsarStream> out;
    for (size_t i = 0; i < l.Count(0); i++)
    {
        const size_t e = l.Entry(0, i);
        if (U16(b, e + 12) != 0x2201) continue;
        BcsarStream s;
        s.Index = i;
        s.Volume = b.at(e + 8);
        const uint32_t name = Options(b, e, 0);
        if (name != 0xFFFFFFFF) s.Name = l.Name(name);
        const uint32_t file = U32(b, e) & 0xFFFFFF;
        if (file >= l.Count(6)) continue;
        const size_t fe = l.Entry(6, file);
        if (U16(b, fe) != 0x220D) continue;
        const size_t path = fe + U32(b, fe + 4);
        s.File.assign((const char*)&b[path]);
        // X/Y's loop: the details' flags at +0xC, their values after
        const size_t d = e + U32(b, e + 16);
        // (Omega Ruby's details have their pitch there, the float 1.0, and their tracks at +0x24: no flags)
        const size_t tracks = U32(b, d + 8);
        const uint32_t flags = U32(b, d + 0xC);
        if (tracks >= 0x14 && tracks < 0x24 && flags < 0x100)
        {
            size_t at = d + 0x10;
            if (flags & 1) { s.HasLoop = (b.at(at + 1) & 1) != 0; at += 4; }
            if (flags & 2) { s.LoopStart = U32(b, at); at += 4; }
            if (flags & 4) { s.LoopEnd = U32(b, at); at += 4; }
            if (!(flags & 6)) s.HasLoop = false;
        }
        out.push_back(s);
    }
    return out;
}

Bytes BcsarAppendStreams(const Bytes& b, size_t like, const std::vector<NewStreamSound>& sounds, size_t& first)
{
    const Layout l(b);
    const size_t sounds0 = l.Count(0), files0 = l.Count(6);
    if (like + 1 >= sounds0 || U16(b, l.Entry(0, like) + 12) != 0x2201) throw FormatError("sound archive: no stream sound " + std::to_string(like) + " to copy");
    // the template: its entry with its details (up to the next sound's entry), and its file entry's header
    const size_t te = l.Entry(0, like), templateSize = l.Entry(0, like + 1) - te;
    const Bytes entry(b.begin() + (long)te, b.begin() + (long)(te + templateSize));
    const size_t tf = l.Entry(6, U32(b, te) & 0xFFFFFF);
    if (U16(b, tf) != 0x220D) throw FormatError("sound archive: the template's file is not external");
    const size_t pathAt = U32(b, tf + 4);

    // appended inside INFO: the two tables, grown, then the entries
    Bytes added;
    const size_t base = l.Info + l.InfoSize; // where `added` starts in the file
    const auto align = [&](size_t a) { while (added.size() % a) added.push_back(0); };
    const size_t soundTable = added.size(), soundCount = sounds0 + sounds.size();
    added.resize(soundTable + 4 + 8 * soundCount);
    align(4);
    const size_t fileTable = added.size(), fileCount = files0 + sounds.size();
    added.resize(fileTable + 4 + 8 * fileCount);
    align(4);
    Put32(added, soundTable, (uint32_t)soundCount);
    Put32(added, fileTable, (uint32_t)fileCount);
    for (size_t i = 0; i < sounds0; i++)
    {
        Put16(added, soundTable + 4 + 8 * i, U16(b, l.Table(0) + 4 + 8 * i));
        Put32(added, soundTable + 8 + 8 * i, (uint32_t)(l.Entry(0, i) - (base + soundTable)));
    }
    for (size_t i = 0; i < files0; i++)
    {
        Put16(added, fileTable + 4 + 8 * i, U16(b, l.Table(6) + 4 + 8 * i));
        Put32(added, fileTable + 8 + 8 * i, (uint32_t)(l.Entry(6, i) - (base + fileTable)));
    }
    for (size_t k = 0; k < sounds.size(); k++)
    {
        const size_t at = added.size();
        added.insert(added.end(), entry.begin(), entry.end());
        Put32(added, at, (uint32_t)(files0 + k));
        added[at + 8] = sounds[k].Volume;
        Put16(added, soundTable + 4 + 8 * (sounds0 + k), 0x2200);
        Put32(added, soundTable + 8 + 8 * (sounds0 + k), (uint32_t)(at - soundTable));
        align(4);
    }
    for (size_t k = 0; k < sounds.size(); k++)
    {
        const size_t at = added.size();
        added.insert(added.end(), b.begin() + (long)tf, b.begin() + (long)(tf + pathAt));
        added.insert(added.end(), sounds[k].File.begin(), sounds[k].File.end());
        added.push_back(0);
        align(4);
        Put16(added, fileTable + 4 + 8 * (files0 + k), 0x220A);
        Put32(added, fileTable + 8 + 8 * (files0 + k), (uint32_t)(at - fileTable));
    }
    align(0x40); // FILE moves by a multiple of 0x40, as it starts in the game's archives

    Bytes out(b.begin(), b.begin() + (long)base);
    out.insert(out.end(), added.begin(), added.end());
    out.insert(out.end(), b.begin() + (long)base, b.end());
    const size_t grown = added.size();
    // INFO's references to the two tables (from its body), its size; the header's section sizes and offsets, the file size
    Put32(out, l.Body + 4, (uint32_t)(base + soundTable - l.Body));
    Put32(out, l.Body + 8 * 6 + 4, (uint32_t)(base + fileTable - l.Body));
    Put32(out, l.Info + 4, (uint32_t)(l.InfoSize + grown));
    for (uint16_t k = 0; k < U16(b, 0x10); k++)
    {
        const size_t at = U32(b, 0x18 + 12 * k);
        if (at == l.Info) Put32(out, 0x1C + 12 * k, (uint32_t)(l.InfoSize + grown));
        else if (at > l.Info) Put32(out, 0x18 + 12 * k, (uint32_t)(at + grown));
    }
    Put32(out, 0x0C, (uint32_t)out.size());
    first = sounds0;
    // the two tables replaced, unused: removed (the game holds INFO in memory)
    return WithoutInfoRanges(out, {{l.Table(0), 4 + 8 * sounds0}, {l.Table(6), 4 + 8 * files0}});
}

std::vector<std::pair<size_t, size_t>> BcsarDifferentSequences(const Bytes& oras, const Bytes& xy)
{
    const Layout lo(oras), lx(xy);
    const std::vector<std::string> orasNames = BcsarSoundNames(oras), xNames = BcsarSoundNames(xy);
    std::map<std::string, size_t> orasOfName;
    for (size_t i = 0; i < orasNames.size(); i++) if (!orasNames[i].empty()) orasOfName.emplace(orasNames[i], i);
    std::map<uint32_t, Bytes> orasWarcs, xWarcs;
    std::vector<std::pair<size_t, size_t>> out;
    for (size_t i = 0; i < xNames.size(); i++)
    {
        const auto o = orasOfName.find(xNames[i]);
        if (o == orasOfName.end() || U16(xy, lx.Entry(Sounds, i) + 12) != 0x2203 || U16(oras, lo.Entry(Sounds, o->second) + 12) != 0x2203) continue;
        if (SoundKey(lx, i, xWarcs) != SoundKey(lo, o->second, orasWarcs)) out.push_back({i, o->second});
    }
    return out;
}

Bytes BcsarAppendSequences(const Bytes& b, const Bytes& xy, const std::vector<std::pair<size_t, size_t>>& sounds, size_t& first, std::string& addedLog)
{
    const Layout l(b), lx(xy);
    const size_t sounds0 = l.Count(Sounds), banks0 = l.Count(Banks), warcs0 = l.Count(Warcs), files0 = l.Count(Files);
    // the archive's own banks and wave archives by their contents, to share them
    std::map<Bytes, uint32_t> orasWarcOf, orasBankOf;
    for (uint32_t w = 0; w < warcs0; w++)
    {
        Bytes c = Unversioned(l, ItemFile(l, Warcs, w));
        if (!c.empty()) orasWarcOf.emplace(std::move(c), w);
    }
    for (uint32_t k = 0; k < banks0; k++)
    {
        Bytes c = Unversioned(l, ItemFile(l, Banks, k));
        if (!c.empty()) orasBankOf.emplace(std::move(c), k);
    }

    std::vector<Bytes> newFiles;                       // appended to FILE: file files0 + k
    std::vector<uint32_t> newWarcFiles, newBankFiles;  // the added wave archives' and banks' files
    std::map<uint32_t, uint32_t> warcOf, bankOf;       // X/Y's id (low 24 bits) -> the archive's
    const auto addFile = [&](Bytes data) { newFiles.push_back(std::move(data)); return (uint32_t)(files0 + newFiles.size() - 1); };
    const auto fileBytes = [&](const Layout& from, uint32_t file)
    {
        const auto [at, size] = FileAt(from, file);
        if (!size) throw FormatError("sound archive: X/Y's file " + std::to_string(file) + " is not internal");
        return Bytes(from.B.begin() + (long)at, from.B.begin() + (long)(at + size));
    };
    const auto warc = [&](uint32_t xId)
    {
        const uint32_t x = xId & 0xFFFFFF;
        if (warcOf.count(x)) return warcOf[x];
        const uint32_t file = ItemFile(lx, Warcs, x);
        const auto same = orasWarcOf.find(Unversioned(lx, file));
        if (same != orasWarcOf.end()) return warcOf[x] = same->second;
        newWarcFiles.push_back(addFile(fileBytes(lx, file)));
        return warcOf[x] = (uint32_t)(warcs0 + newWarcFiles.size() - 1);
    };
    const auto bank = [&](uint32_t xId)
    {
        const uint32_t x = xId & 0xFFFFFF;
        if (bankOf.count(x)) return bankOf[x];
        Bytes data = fileBytes(lx, ItemFile(lx, Banks, x));
        const auto [waves, n] = WaveTable(data);
        for (size_t w = 0; w < n; w++) Put32(data, waves + 8 * w, 0x05000000 | warc(U32(data, waves + 8 * w)));
        Bytes plain = data;
        Put32(plain, 8, 0);
        const auto same = orasBankOf.find(plain);
        if (same != orasBankOf.end()) return bankOf[x] = same->second;
        newBankFiles.push_back(addFile(std::move(data)));
        return bankOf[x] = (uint32_t)(banks0 + newBankFiles.size() - 1);
    };
    // the sounds' entries: X/Y's (with its details, 100 bytes in both games), its file, banks, player and name renumbered
    std::vector<Bytes> entries;
    for (const auto& [x, oras] : sounds)
    {
        if (x + 1 >= lx.Count(Sounds)) throw FormatError("sound archive: X/Y's sound " + std::to_string(x) + " is its last");
        const size_t xe = lx.Entry(Sounds, x), oe = l.Entry(Sounds, oras);
        Bytes entry(xy.begin() + (long)xe, xy.begin() + (long)lx.Entry(Sounds, x + 1));
        if (U16(entry, 12) != 0x2203 || U32(entry, 20) != U32(b, oe + 20)) throw FormatError("sound archive: X/Y's sound " + std::to_string(x) + " is not a sequence laid out as Omega Ruby's");
        Put32(entry, 0, addFile(fileBytes(lx, U32(entry, 0))));
        Put32(entry, 4, U32(b, oe + 4));                       // the player
        if (U32(entry, 20) & 1) Put32(entry, 24, U32(b, oe + 24)); // the name (option bit 0's value, the first)
        const auto [table, count] = BankTable(entry, 0);
        for (size_t k = 0; k < count; k++) Put32(entry, table + 4 * k, 0x03000000 | bank(U32(entry, table + 4 * k)));
        entries.push_back(std::move(entry));
    }

    // appended inside INFO: the four tables, grown, then the new entries
    Bytes added;
    const size_t base = l.Info + l.InfoSize;
    const auto align = [&](size_t a) { while (added.size() % a) added.push_back(0); };
    const int tables[4] = {Sounds, Banks, Warcs, Files};
    const size_t newCount[4] = {entries.size(), newBankFiles.size(), newWarcFiles.size(), newFiles.size()};
    size_t tableAt[4];
    for (int t = 0; t < 4; t++)
    {
        const int k = tables[t];
        tableAt[t] = added.size();
        const size_t count = l.Count(k) + newCount[t];
        added.resize(tableAt[t] + 4 + 8 * count);
        Put32(added, tableAt[t], (uint32_t)count);
        for (size_t i = 0; i < l.Count(k); i++)
        {
            Put16(added, tableAt[t] + 4 + 8 * i, U16(b, l.Table(k) + 4 + 8 * i));
            Put32(added, tableAt[t] + 8 + 8 * i, (uint32_t)(l.Entry(k, i) - (base + tableAt[t])));
        }
        align(4);
    }
    const auto entry = [&](int t, size_t i, const Bytes& data)
    {
        const size_t at = added.size();
        added.insert(added.end(), data.begin(), data.end());
        align(4);
        const size_t slot = tableAt[t] + 4 + 8 * (l.Count(tables[t]) + i);
        Put16(added, slot, U16(b, l.Table(tables[t]) + 4)); // the table's entry type (0x2200 a sound, ...)
        Put32(added, slot + 4, (uint32_t)(at - tableAt[t]));
    };
    for (size_t i = 0; i < entries.size(); i++) entry(0, i, entries[i]);
    for (size_t i = 0; i < newBankFiles.size(); i++)
    {
        // file, a reference (0x0100) to its wave archive item table (relative to the entry, +0x14: count 0), no options
        Bytes e(24, 0);
        Put32(e, 0, newBankFiles[i]);
        Put16(e, 4, 0x0100);
        Put32(e, 8, 0x14);
        entry(1, i, e);
    }
    for (size_t i = 0; i < newWarcFiles.size(); i++)
    {
        Bytes e(12, 0); // file, 0, no options (as Omega Ruby's unnamed ones)
        Put32(e, 0, newWarcFiles[i]);
        entry(2, i, e);
    }
    // the files: an internal file's entry (a reference to its location, 0, the location 0x1F00: offset in FILE's body, size)
    const size_t fileTemplate = l.Entry(Files, U32(b, l.Entry(Sounds, sounds.empty() ? 0 : sounds[0].second)));
    if (U16(b, fileTemplate) != 0x220C) throw FormatError("sound archive: the template sound's file is not internal");
    const size_t fileEnd = b.size();                       // FILE is the archive's last section: the files go at its end
    std::vector<size_t> fileAt;
    size_t end = fileEnd;
    for (const Bytes& f : newFiles)
    {
        end = (end + 0x1F) & ~(size_t)0x1F;
        fileAt.push_back(end);
        end += f.size();
    }
    for (size_t i = 0; i < newFiles.size(); i++)
    {
        Bytes e(b.begin() + (long)fileTemplate, b.begin() + (long)fileTemplate + 0x18);
        Put32(e, 0x10, 0); // set once INFO's growth is known
        Put32(e, 0x14, (uint32_t)newFiles[i].size());
        entry(3, i, e);
    }
    align(0x40);
    const size_t grown = added.size();
    // the new files' offsets from FILE's body, which moves by `grown`
    for (size_t i = 0; i < newFiles.size(); i++)
    {
        const size_t slot = tableAt[3] + 4 + 8 * (files0 + i);
        const size_t at = tableAt[3] + U32(added, slot + 4);
        Put32(added, at + 0x10, (uint32_t)(fileAt[i] - (l.File + 8)));
    }
    if (l.File < l.Info || U32(b, l.File + 4) + l.File != fileEnd) throw FormatError("sound archive: FILE is not its last section");

    Bytes out(b.begin(), b.begin() + (long)base);
    out.insert(out.end(), added.begin(), added.end());
    out.insert(out.end(), b.begin() + (long)base, b.end());
    for (size_t i = 0; i < newFiles.size(); i++)
    {
        out.resize(fileAt[i] + grown, 0);
        out.insert(out.end(), newFiles[i].begin(), newFiles[i].end());
    }
    while (out.size() % 0x20) out.push_back(0);
    for (int t = 0; t < 4; t++) Put32(out, l.Body + 8 * (size_t)tables[t] + 4, (uint32_t)(base + tableAt[t] - l.Body));
    Put32(out, l.Info + 4, (uint32_t)(l.InfoSize + grown));
    const size_t fileSize = out.size() - (l.File + grown);
    for (uint16_t k = 0; k < U16(b, 0x10); k++)
    {
        const size_t at = U32(b, 0x18 + 12 * k);
        if (at == l.Info) Put32(out, 0x1C + 12 * k, (uint32_t)(l.InfoSize + grown));
        else if (at == l.File) { Put32(out, 0x18 + 12 * k, (uint32_t)(at + grown)); Put32(out, 0x1C + 12 * k, (uint32_t)fileSize); }
        else if (at > l.Info) Put32(out, 0x18 + 12 * k, (uint32_t)(at + grown));
    }
    Put32(out, l.File + grown + 4, (uint32_t)fileSize);
    Put32(out, 0x0C, (uint32_t)out.size());
    first = sounds0;
    // the four tables replaced, unused: removed (the game holds INFO in memory: with X/Y's streams too, INFO grown by
    // 0x3A200 bytes stopped the game at boot, an allocation failing in its sound setup, 29 October)
    out = WithoutInfoRanges(out, {{l.Table(Sounds), 4 + 8 * sounds0}, {l.Table(Banks), 4 + 8 * banks0}, {l.Table(Warcs), 4 + 8 * warcs0},
                                  {l.Table(Files), 4 + 8 * files0}});
    addedLog = std::to_string(newBankFiles.size()) + " bank(s) and " + std::to_string(newWarcFiles.size()) + " wave archive(s) added, " +
               std::to_string(bankOf.size() - newBankFiles.size()) + " and " + std::to_string(warcOf.size() - newWarcFiles.size()) + " shared";
    return out;
}


std::vector<uint32_t> BcsarSoundFiles(const Bytes& b, size_t sound)
{
    const Layout l(b);
    if (sound >= l.Count(Sounds)) throw FormatError("sound archive: no sound " + std::to_string(sound));
    const size_t e = l.Entry(Sounds, sound);
    std::vector<uint32_t> files{U32(b, e)};
    if (U16(b, e + 12) != 0x2203) return files; // not a sequence: its file alone
    const auto [table, count] = BankTable(b, e);
    for (size_t k = 0; k < count; k++)
    {
        const uint32_t bankFile = ItemFile(l, Banks, U32(b, table + 4 * k));
        if (bankFile == 0xFFFFFFFF) continue;
        files.push_back(bankFile);
        const auto [at, size] = FileAt(l, bankFile);
        if (!size) continue;
        const Bytes bank(b.begin() + (long)at, b.begin() + (long)(at + size));
        const auto [waves, n] = WaveTable(bank);
        for (size_t w = 0; w < n; w++)
        {
            const uint32_t warcFile = ItemFile(l, Warcs, U32(bank, waves + 8 * w));
            if (warcFile != 0xFFFFFFFF) files.push_back(warcFile);
        }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

Bytes BcsarWithGroupSounds(const Bytes& b, const std::string& groupName, const std::vector<size_t>& sounds, size_t& added)
{
    const Layout l(b);
    constexpr int Groups = 4;
    size_t groupEntry = 0;
    bool found = false;
    for (size_t g = 0; g < l.Count(Groups) && !found; g++)
    {
        // a group's entry: its file, then option flags (bit 0: the name's string index, the value after them)
        const size_t e = l.Entry(Groups, g);
        if ((U32(b, e + 4) & 1) && l.Name(U32(b, e + 8)) == groupName) { groupEntry = e; found = true; }
    }
    if (!found) throw FormatError("sound archive: no group " + groupName);
    const uint32_t groupFile = U32(b, groupEntry);
    const auto [at, size] = FileAt(l, groupFile);
    if (!size) throw FormatError("sound archive: group " + groupName + "'s file is not internal");
    const Bytes old(b.begin() + (long)at, b.begin() + (long)(at + size));
    // CGRP: a 0x40 header (section references at 0x14, 12 bytes each: type, 0, offset, size), then INFO (0x7800: u32
    // count, references (0x7900, offset from the body) to 16-byte entries: file, then a link's 0 and ~0 ~0: the file is
    // loaded from the archive), FILE (0x7801: embedded files, none in a linked group) and INFX (0x7802: its items)
    if (std::memcmp(&old[0], "CGRP", 4) != 0 || U16(old, 0x10) != 3 || U16(old, 0x14) != 0x7800) throw FormatError("sound archive: group " + groupName + " is not a CGRP laid out as expected");
    const size_t info = U32(old, 0x18) + 8, count = U32(old, info);
    std::vector<uint32_t> list;
    for (size_t i = 0; i < count; i++)
    {
        const size_t entry = info + U32(old, info + 4 + 8 * i + 4);
        if (U32(old, entry + 8) != 0xFFFFFFFF) throw FormatError("sound archive: group " + groupName + " embeds its files");
        list.push_back(U32(old, entry));
    }
    const Bytes entryTemplate(old.begin() + (long)(info + U32(old, info + 8)), old.begin() + (long)(info + U32(old, info + 8) + 16));
    // INFX: u32 count, references (0x7901) to 8-byte entries: an item (0x01000000 | a sound) and its load flags (~0: all
    // of its data, as SEQ_SE_FT_04's)
    const size_t infx = U32(old, 0x18 + 24) + 8, itemCount = U32(old, infx);
    std::vector<std::pair<uint32_t, uint32_t>> items;
    for (size_t i = 0; i < itemCount; i++)
    {
        const size_t entry = infx + U32(old, infx + 4 + 8 * i + 4);
        items.push_back({U32(old, entry), U32(old, entry + 4)});
    }
    added = 0;
    for (const size_t sound : sounds)
    {
        const uint32_t item = 0x01000000u | (uint32_t)sound;
        if (std::none_of(items.begin(), items.end(), [&](const auto& i) { return i.first == item; })) { items.push_back({item, 0xFFFFFFFF}); added++; }
        for (const uint32_t f : BcsarSoundFiles(b, sound))
            if (std::find(list.begin(), list.end(), f) == list.end()) list.push_back(f);
    }
    if (!added) return b;

    Bytes body(4 + 8 * list.size() + 16 * list.size(), 0);
    Put32(body, 0, (uint32_t)list.size());
    for (size_t i = 0; i < list.size(); i++)
    {
        Put16(body, 4 + 8 * i, 0x7900);
        Put32(body, 8 + 8 * i, (uint32_t)(4 + 8 * list.size() + 16 * i));
        std::copy(entryTemplate.begin(), entryTemplate.end(), body.begin() + (long)(4 + 8 * list.size() + 16 * i));
        Put32(body, 4 + 8 * list.size() + 16 * i, list[i]);
    }
    Bytes infoSection = {'I', 'N', 'F', 'O', 0, 0, 0, 0};
    infoSection.insert(infoSection.end(), body.begin(), body.end());
    while (infoSection.size() % 0x20) infoSection.push_back(0);
    Put32(infoSection, 4, (uint32_t)infoSection.size());
    Bytes cgrp(old.begin(), old.begin() + 0x40);
    Put32(cgrp, 0x18, 0x40);
    Put32(cgrp, 0x1C, (uint32_t)infoSection.size());
    cgrp.insert(cgrp.end(), infoSection.begin(), infoSection.end());
    { // FILE as it was
        const size_t from = U32(old, 0x18 + 12), length = U32(old, 0x1C + 12);
        Put32(cgrp, 0x18 + 12, (uint32_t)cgrp.size());
        cgrp.insert(cgrp.end(), old.begin() + (long)from, old.begin() + (long)(from + length));
    }
    { // INFX with the items
        Bytes x = {'I', 'N', 'F', 'X', 0, 0, 0, 0};
        Bytes xb(4 + 16 * items.size(), 0);
        Put32(xb, 0, (uint32_t)items.size());
        for (size_t i = 0; i < items.size(); i++)
        {
            Put16(xb, 4 + 8 * i, 0x7901);
            Put32(xb, 8 + 8 * i, (uint32_t)(4 + 8 * items.size() + 8 * i));
            Put32(xb, 4 + 8 * items.size() + 8 * i, items[i].first);
            Put32(xb, 8 + 8 * items.size() + 8 * i, items[i].second);
        }
        x.insert(x.end(), xb.begin(), xb.end());
        while (x.size() % 0x20) x.push_back(0);
        Put32(x, 4, (uint32_t)x.size());
        Put32(cgrp, 0x18 + 24, (uint32_t)cgrp.size());
        Put32(cgrp, 0x1C + 24, (uint32_t)x.size());
        cgrp.insert(cgrp.end(), x.begin(), x.end());
    }
    Put32(cgrp, 0x0C, (uint32_t)cgrp.size());

    // the file replaced in place, the files after it moved by the growth (files sit on 0x20 boundaries)
    const size_t oldSpan = (size + 0x1F) & ~(size_t)0x1F, newSpan = (cgrp.size() + 0x1F) & ~(size_t)0x1F, delta = newSpan - oldSpan;
    if (l.File < l.Info || U32(b, l.File + 4) + l.File != b.size()) throw FormatError("sound archive: FILE is not its last section");
    Bytes out(b.begin(), b.begin() + (long)at);
    out.insert(out.end(), cgrp.begin(), cgrp.end());
    out.resize(at + newSpan, 0);
    out.insert(out.end(), b.begin() + (long)std::min(b.size(), at + oldSpan), b.end());
    const uint32_t oldOffset = (uint32_t)(at - (l.File + 8));
    for (size_t f = 0; f < l.Count(Files); f++)
    {
        const size_t e = l.Entry(Files, f);
        if (U16(b, e) != 0x220C) continue;
        const size_t loc = e + U32(b, e + 4);
        const uint32_t offset = U32(b, loc + 4);
        if (offset == 0xFFFFFFFF) continue;
        if (f == groupFile) Put32(out, loc + 8, (uint32_t)cgrp.size());
        else if (offset > oldOffset) Put32(out, loc + 4, offset + (uint32_t)delta);
    }
    const size_t fileSize = out.size() - l.File;
    for (uint16_t k = 0; k < U16(b, 0x10); k++)
        if (U32(b, 0x18 + 12 * k) == l.File) Put32(out, 0x1C + 12 * k, (uint32_t)fileSize);
    Put32(out, l.File + 4, (uint32_t)fileSize);
    Put32(out, 0x0C, (uint32_t)out.size());
    return out;
}

}
