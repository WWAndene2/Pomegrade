#include "OrasXyPlaces.h"

namespace remake
{

namespace
{
constexpr uint32_t CodeBase = 0x100000;
constexpr uint32_t XyEntryBytes = 20;

uint16_t Half(const Bytes& b, size_t at) { return (uint16_t)(b.at(at) | b.at(at + 1) << 8); }
void PutHalf(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }

size_t XyEntry(const Bytes& xCode, uint32_t k)
{
    const size_t at = XyPlacesTable - CodeBase + XyEntryBytes * k;
    if (at + XyEntryBytes > xCode.size()) throw FormatError("X's places table past its .code");
    // checked against the owner's X (28 October): its first place's flag 0xAA0, a fly point
    if (Half(xCode, XyPlacesTable - CodeBase) != 0xAA0) throw FormatError("X's places table not where it was read (another version of X?)");
    return at;
}
}

std::set<int> XyPlaceFlags(const Bytes& xCode)
{
    std::set<int> flags;
    for (uint32_t k = 0; k < XyPlaceCount; k++) flags.insert(Half(xCode, XyEntry(xCode, k)));
    return flags;
}

Bytes XyPlacesToOras(const Bytes& xCode, const XyNumbers& numbers, std::vector<std::string>& log)
{
    Bytes out;
    size_t left = 0;
    for (uint32_t k = 0; k < XyPlaceCount; k++)
    {
        const size_t at = XyEntry(xCode, k);
        Bytes e(xCode.begin() + (long)at, xCode.begin() + (long)at + PlaceBytes);
        const auto center = numbers.Zones.find(Half(e, 4)), town = numbers.Zones.find(Half(e, 8));
        const auto flag = numbers.Flags.find(Half(e, 0));
        if (center == numbers.Zones.end() || town == numbers.Zones.end() || flag == numbers.Flags.end()) { left++; continue; }
        PutHalf(e, 0, (uint16_t)flag->second);
        PutHalf(e, 4, (uint16_t)center->second.Zone);
        PutHalf(e, 8, (uint16_t)town->second.Zone);
        out.insert(out.end(), e.begin(), e.end());
    }
    log.push_back("places: " + std::to_string(out.size() / PlaceBytes) + " of X/Y's " + std::to_string(XyPlaceCount) + " given (" +
                  std::to_string(left) + " with a zone not in this build)");
    return out;
}

std::vector<CodeWord> XyPlacesPatch(uint32_t table, uint32_t count)
{
    if (count % 2 || count > 0xFF || count < OrasPlaceCount) throw FormatError("places: an even count of 78-254");
    std::vector<CodeWord> w;
    for (const uint32_t literal : {0x3D4130u, 0x3D417Cu, 0x3D424Cu, 0x3D4338u, 0x3D43E4u, 0x3E806Cu})
        w.push_back({literal, OrasPlacesTable, table, "a places table reader's literal"});
    // mov r2, #0x4E (the loop's bound in a register), cmp rN, #0x4E (the others)
    for (const auto& [at, game] : std::initializer_list<std::pair<uint32_t, uint32_t>>{
             {0x3D4034, 0xE3A0204E}, {0x3D4148, 0xE350004E}, {0x3D41BC, 0xE354004E}, {0x3D4234, 0xE354004E},
             {0x3D4274, 0xE350004E}, {0x3D4374, 0xE350004E}, {0x3D43B4, 0xE350004E}, {0x3E7F60, 0xE3A0204E}})
        w.push_back({at, game, (game & ~0xFFu) | count, "a places table bound, 78 -> " + std::to_string(count)});
    return w;
}

}
