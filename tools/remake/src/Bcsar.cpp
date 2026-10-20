#include "Bcsar.h"

#include <cstring>

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
    return out;
}

}
