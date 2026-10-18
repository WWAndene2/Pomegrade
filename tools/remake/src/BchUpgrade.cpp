#include "BchUpgrade.h"

#include "Bch.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace remake
{

namespace
{

constexpr uint32_t MaterialOld = 0x58, MapperAt = 0x18, MappersBytes = 48, Removed = MappersBytes - 4;
constexpr uint32_t RawData = 4, RawTexture = 5;

void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b.at(at + i) = (uint8_t)(v >> (8 * i)); }
void Pad(Bytes& b, size_t align) { while (b.size() % align) b.push_back(0); }

struct Entry
{
    uint32_t Source = 0, Target = 0, At = 0; // At: byte offset in the source section
};

}

Bytes BchUpgradeTo21(const Bytes& bch)
{
    if (bch.size() < 0x38 || bch[4] != 0x07) throw FormatError("BCH upgrade: version 0x07 only");
    const BchSections s = BchSections::Read(bch);
    const uint32_t contentsLength = U32(bch, 0x1C), stringsLength = U32(bch, 0x20), commandsLength = U32(bch, 0x24);
    const uint32_t rawLength = U32(bch, 0x28);
    // 15 dictionaries of 12 bytes (SPICA's H3D: models, material parameters, shaders, textures, LUTs, lights, cameras,
    // fogs, then 6 kinds of animations, scenes): skeletal (8) and material (9) animations are converted (below), the other kinds
    // refused
    for (int k : {10, 11, 12, 13})
        if (U32(bch, s.Contents + 12 * k + 4)) throw FormatError("BCH upgrade: dictionary " + std::to_string(k) + " is not empty (not converted)");

    // the materials (their file offsets, from the reader's texture name words: names at +0x48 before 0x21)
    std::vector<uint32_t> materials;
    for (const BchModel& m : Bch::Read(bch).Models)
        for (const BchMaterial& mat : m.Materials) materials.push_back(mat.TextureNameWord[0] - 0x48 - s.Contents);
    std::sort(materials.begin(), materials.end());
    for (size_t i = 0; i < materials.size(); i++)
        if (materials[i] + MaterialOld > contentsLength || (i && materials[i] < materials[i - 1] + MaterialOld))
            throw FormatError("BCH upgrade: a material outside the contents or overlapping another");

    // a contents offset after the mappers leave their materials; a pointer into removed bytes is refused
    auto moved = [&](uint32_t old) {
        uint32_t shift = 0;
        for (uint32_t m : materials)
        {
            const uint32_t from = m + MapperAt + 4, to = m + MapperAt + MappersBytes;
            if (old >= to) shift += Removed;
            else if (old >= from) throw FormatError("BCH upgrade: a pointer into a material's texture mappers");
        }
        return old - shift;
    };

    // the relocation table, read as entries (SPICA's numbering is the same for 0x07 and 0x21)
    std::vector<Entry> entries;
    for (uint32_t off = 0; off + 4 <= s.RelocationLength; off += 4)
    {
        const uint32_t v = U32(bch, s.Relocation + off);
        Entry e{v >> 29, (v >> 25) & 0xF, v & 0x1FFFFFF};
        if (e.Target != 1) e.At <<= 2;
        if (e.Source > 3) throw FormatError("BCH upgrade: a pointer in section " + std::to_string(e.Source) + " (not converted)");
        entries.push_back(e);
    }

    // contents: the materials' mappers out, a pointer in their place; the mappers after the contents' end
    Bytes contents(bch.begin() + s.Contents, bch.begin() + s.Contents + contentsLength);
    Bytes newContents;
    std::vector<uint32_t> mapperWords; // new contents offsets of the pointers to the moved mappers
    Bytes mappers;
    {
        uint32_t at = 0;
        for (uint32_t m : materials)
        {
            newContents.insert(newContents.end(), contents.begin() + at, contents.begin() + m + MapperAt + 4);
            mapperWords.push_back((uint32_t)newContents.size() - 4);
            mappers.insert(mappers.end(), contents.begin() + m + MapperAt, contents.begin() + m + MapperAt + MappersBytes);
            at = m + MapperAt + MappersBytes;
        }
        newContents.insert(newContents.end(), contents.begin() + at, contents.end());
    }
    Pad(newContents, 4);
    const uint32_t mappersAt = (uint32_t)newContents.size();
    newContents.insert(newContents.end(), mappers.begin(), mappers.end());
    for (size_t i = 0; i < mapperWords.size(); i++) Put32(newContents, mapperWords[i], mappersAt + (uint32_t)(i * 16 * 3));

    // material animations (dictionary 9): their key frame groups rewritten. Before 0x20 a group is its curve (start and
    // end frames, repeats, curve index: 12 bytes), a pointer to a block (X's files: 8 bytes on, a word 1 in between; read
    // from the pointer), the block (start, end, u8 interpolation, u8 quantisation, u16 key count, 1 / duration, value
    // scale, value offset, frame scale) and after it the keys' pointer; from 0x20 the curve, the interpolation,
    // quantisation and count, value scale, offset, frame scale, 1 / duration, the keys' pointer (36 bytes: SPICA, and
    // seen in ORAS's character 28's eye animation). Each new group goes after the contents' end, its element's slot
    // repointed to it; the keys stay where they are. An element's content: flags, then a slot a component (float and
    // texture 1, vector 2 or 3, RGBA 4), a group's pointer when it has a relocation, else a constant
    std::set<uint32_t> toContents;
    for (const Entry& e : entries) if (e.Source == 0 && e.Target == 0) toContents.insert(e.At);
    auto pointer = [&](uint32_t at) {
        if (!toContents.count(at)) throw FormatError("BCH upgrade: no pointer where a material animation has one");
        return U32(contents, at);
    };
    std::vector<std::pair<uint32_t, uint32_t>> groupSlots; // an old slot's offset, its group's new offset
    std::vector<uint32_t> keyWords;                         // new offsets of the new groups' key pointers
    // skeletal animations (dictionary 8) the same way: a Transform element (primitive 4) holds 9 slots, scale, rotation and
    // translation x, y, z, each a constant or a key frame group as a material animation's (SPICA H3DAnimTransform); a
    // QuatTransform (7: X's doors' opening, t01_door) or MtxTransform (9) element holds its own lists, the same in both
    // versions (SPICA: no field depends on it), kept
    for (const uint32_t dictionary : {8u, 9u})
    for (uint32_t i = 0, list = U32(contents, 12 * dictionary + 4) ? pointer(12 * dictionary) : 0; i < U32(contents, 12 * dictionary + 4); i++)
    {
        const uint32_t anim = pointer(list + 4 * i), elements = U32(contents, anim + 16);
        // the animation's type and flags bytes (after its name) are in the other order before 0x20: X's 52 material
        // animations all read 1, 0 and ORAS's 87 all 0, 1 (flags 0, type 1 = material, SPICA); left as X has them, ORAS
        // reads a looping skeletal animation and never plays the blink (runs bl_x2 against bl_o, 17 October)
        std::swap(newContents.at(moved(anim) + 4), newContents.at(moved(anim) + 5));
        for (uint32_t j = 0; j < elements; j++)
        {
            const uint32_t el = pointer(pointer(anim + 12) + 4 * j);
            const uint16_t prim = (uint16_t)(contents.at(el + 6) | contents.at(el + 7) << 8);
            int slots = prim == 0 || prim == 6 ? 1 : prim == 2 ? 2 : prim == 3 ? 3 : prim == 5 ? 4 : 0;
            if (dictionary == 8) slots = prim == 4 ? 9 : prim == 7 || prim == 9 ? -1 : 0;
            if (slots < 0) continue;
            if (!slots) throw FormatError("BCH upgrade: an animation (dictionary " + std::to_string(dictionary) + ") of primitive type " + std::to_string(prim) + " (not converted)");
            for (int k = 0; k < slots; k++)
            {
                const uint32_t slot = el + 12 + 4 * (uint32_t)k;
                if (!toContents.count(slot)) continue;
                const uint32_t group = pointer(slot), block = pointer(group + 12);
                Pad(newContents, 4);
                const uint32_t at = (uint32_t)newContents.size();
                auto copy = [&](uint32_t from, uint32_t n) { newContents.insert(newContents.end(), contents.begin() + from, contents.begin() + from + n); };
                copy(group, 12);                                   // the curve
                copy(block + 8, 4);                                // interpolation, quantisation, count
                copy(block + 16, 12);                              // value scale, value offset, frame scale
                copy(block + 12, 4);                               // 1 / duration
                newContents.resize(newContents.size() + 4, 0);
                Put32(newContents, at + 32, moved(pointer(block + 28)));
                keyWords.push_back(at + 32);
                groupSlots.push_back({slot, at});
            }
        }
    }

    // the pointers follow their data: a pointer word in the contents moves; a value into the contents is mapped
    Bytes strings(bch.begin() + s.Strings, bch.begin() + s.Strings + stringsLength);
    Bytes commands(bch.begin() + s.Commands, bch.begin() + s.Commands + commandsLength);
    for (Entry& e : entries)
    {
        Bytes& source = e.Source == 0 ? newContents : e.Source == 1 ? strings : commands;
        const uint32_t oldAt = e.At;
        if (e.Source == 0) e.At = moved(e.At);
        if (e.Target == 0) Put32(source, e.At, moved(U32(e.Source == 0 ? contents : source, oldAt)));
        // a texture address in a command list (the word before a texture unit's address register: 0x85, 0x95, 0x9D)
        if (e.Source >= 2 && e.Target == RawData && e.At + 8 <= source.size())
        {
            const uint32_t reg = U32(source, e.At + 4) & 0xFFFF;
            if (reg == 0x85 || reg == 0x95 || reg == 0x9D) e.Target = RawTexture;
        }
    }
    for (uint32_t w : mapperWords) entries.push_back({0, 0, w});
    for (uint32_t w : keyWords) entries.push_back({0, 0, w});
    for (const auto& [slot, group] : groupSlots) Put32(newContents, moved(slot), group);
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.Source != b.Source ? a.Source < b.Source : a.At < b.At; });

    // the file: header (0x44), contents, strings, commands (each 4-aligned as in the games' files), raw data 0x80-aligned
    // at the same offset modulo 0x80 as the old one is 0 (X's is), RawExt empty, the relocation table
    if (s.Raw % 0x80) throw FormatError("BCH upgrade: the raw data is not 0x80-aligned");
    Bytes out(0x44, 0);
    const uint32_t contentsAt = (uint32_t)out.size();
    out.insert(out.end(), newContents.begin(), newContents.end());
    const uint32_t stringsAt = (uint32_t)out.size();
    out.insert(out.end(), strings.begin(), strings.end());
    Pad(out, 0x20);
    const uint32_t commandsAt = (uint32_t)out.size();
    out.insert(out.end(), commands.begin(), commands.end());
    Pad(out, 0x80);
    const uint32_t rawAt = (uint32_t)out.size();
    out.insert(out.end(), bch.begin() + s.Raw, bch.begin() + s.Raw + rawLength);
    Pad(out, 4);
    const uint32_t relocAt = (uint32_t)out.size();
    uint16_t rawPointers = 0;
    for (const Entry& e : entries)
    {
        if (e.Target != 1 && e.At % 4) throw FormatError("BCH upgrade: an unaligned pointer");
        const uint32_t field = e.Target != 1 ? e.At >> 2 : e.At;
        if (field > 0x1FFFFFF) throw FormatError("BCH upgrade: a pointer beyond 25 bits");
        const uint32_t v = field | e.Target << 25 | e.Source << 29;
        for (int k = 0; k < 4; k++) out.push_back((uint8_t)(v >> (8 * k)));
        if (e.Target >= RawData && e.Target <= 13) rawPointers++;
    }
    const uint32_t relocLength = (uint32_t)out.size() - relocAt;

    out[0] = 'B'; out[1] = 'C'; out[2] = 'H'; out[3] = 0;
    out[4] = 0x21; out[5] = 0x21; out[6] = (uint8_t)(42607 & 0xFF); out[7] = (uint8_t)(42607 >> 8);
    const uint32_t addresses[6] = {contentsAt, stringsAt, commandsAt, rawAt, relocAt, relocAt};
    const uint32_t lengths[6] = {(uint32_t)newContents.size(), stringsLength, commandsLength, rawLength, 0, relocLength};
    for (int k = 0; k < 6; k++) { Put32(out, 8 + 4 * k, addresses[k]); Put32(out, 0x20 + 4 * k, lengths[k]); }
    Put32(out, 0x38, 4u * rawPointers);                  // uninitialised data: a word a raw-data pointer (SPICA, ORAS's files)
    Put32(out, 0x3C, U32(bch, 0x34));                    // uninitialised commands, as X's
    out[0x40] = 1; out[0x41] = 0;                        // flags: from the new converter
    out[0x42] = (uint8_t)rawPointers; out[0x43] = (uint8_t)(rawPointers >> 8);
    return out;
}

}
