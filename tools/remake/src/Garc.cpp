#include "Garc.h"

namespace remake
{

void Garc::Set(size_t i, Bytes data, size_t sub)
{
    if (sub >= 32) throw FormatError("GARC: sub-file bit " + std::to_string(sub) + " past 31");
    if (i > Entries.size()) throw FormatError("GARC: entry " + std::to_string(i) + " past the end");
    if (i == Entries.size()) { Entries.emplace_back(); Present.push_back(0); }
    if (Entries[i].size() <= sub) Entries[i].resize(sub + 1);
    Entries[i][sub] = std::move(data);
    Present[i] |= 1u << sub;
}

static void Put16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void Put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }

Bytes Garc::Write() const
{
    if (VersionNumber != 0x0400) throw FormatError("GARC: only version 4 (X/Y, ORAS) is written");
    const size_t count = Entries.size();
    if (count > 0xFFFF) throw FormatError("GARC: more than 65535 entries");
    Bytes fato, fatb, data;
    uint32_t largest = 0;
    for (size_t i = 0; i < count; i++)
    {
        Put32(fato, (uint32_t)fatb.size());
        Put32(fatb, Present[i]);
        for (int bit = 0; bit < 32; bit++)
        {
            if (!(Present[i] >> bit & 1)) continue;
            const Bytes& f = Entries[i][bit];
            const uint32_t start = (uint32_t)data.size();
            data.insert(data.end(), f.begin(), f.end());
            while (data.size() % 4) data.push_back(0xFF);
            Put32(fatb, start); Put32(fatb, (uint32_t)data.size()); Put32(fatb, (uint32_t)f.size());
            if (f.size() > largest) largest = (uint32_t)f.size();
        }
    }
    const uint32_t headerSize = 0x1C, fatoSize = 12 + (uint32_t)fato.size(), fatbSize = 12 + (uint32_t)fatb.size();
    const uint32_t dataStart = headerSize + fatoSize + fatbSize + 12;
    Bytes out;
    out.reserve(dataStart + data.size());
    out.insert(out.end(), {'C', 'R', 'A', 'G'});
    Put32(out, headerSize); Put16(out, 0xFEFF); Put16(out, VersionNumber); Put32(out, 4);
    Put32(out, dataStart); Put32(out, dataStart + (uint32_t)data.size()); Put32(out, largest);
    out.insert(out.end(), {'O', 'T', 'A', 'F'}); Put32(out, fatoSize); Put16(out, (uint16_t)count); Put16(out, 0xFFFF);
    out.insert(out.end(), fato.begin(), fato.end());
    out.insert(out.end(), {'B', 'T', 'A', 'F'}); Put32(out, fatbSize); Put32(out, (uint32_t)count);
    out.insert(out.end(), fatb.begin(), fatb.end());
    out.insert(out.end(), {'B', 'M', 'I', 'F'}); Put32(out, 12); Put32(out, (uint32_t)data.size());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

bool Garc::Is(const Bytes& data)
{
    return data.size() >= 0x1C && Text(data, 0, 4) == "CRAG";
}

Garc::Garc(const Bytes& data)
{
    if (!Is(data)) throw FormatError("not a GARC");
    const uint32_t headerSize = U32(data, 4);
    VersionNumber = U16(data, 0x0A);
    if (VersionNumber != 0x0400 && VersionNumber != 0x0600)
        throw FormatError("GARC: unknown version " + std::to_string(VersionNumber));
    const uint32_t dataStart = U32(data, 0x10);

    size_t at = headerSize;
    if (Text(data, at, 4) != "OTAF") throw FormatError("GARC: no FATO section");
    const uint32_t fatoSize = U32(data, at + 4);
    const uint16_t count = U16(data, at + 8);
    const size_t offsets = at + 12;
    at += fatoSize;
    if (Text(data, at, 4) != "BTAF") throw FormatError("GARC: no FATB section");
    const size_t fatb = at + 12; // after magic, size, entry count

    for (uint16_t i = 0; i < count; i++)
    {
        size_t e = fatb + U32(data, offsets + i * 4);
        const uint32_t mask = U32(data, e);
        e += 4;
        std::vector<Bytes> subs;
        for (int bit = 0; bit < 32; bit++)
        {
            if (!(mask >> bit & 1)) continue;
            subs.resize(bit + 1);
            const uint32_t start = U32(data, e), end = U32(data, e + 4), length = U32(data, e + 8);
            e += 12; // start, end (padded to 4 bytes), length (the sub-file's own)
            if (end < start || length > end - start) throw FormatError("GARC: entry " + std::to_string(i) + " doesn't fit its range");
            subs[bit] = Slice(data, dataStart + start, length);
        }
        Entries.push_back(std::move(subs));
        Present.push_back(mask);
    }
}

}
