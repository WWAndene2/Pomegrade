#include "OrasXyGymModules.h"

#include <cstdio>

namespace remake
{

namespace
{
// a CRO's header fields as Azahar's loader reads them (cro_helper.h; prototype/cro_dis.py's HEADER_FIELDS): u32 each
// after the 0x80-byte hash
enum Field { ModuleNameOffset = 16, ModuleNameSize = 17, SegmentTableOffset = 18, SegmentNum = 19,
             ImportModuleTableOffset = 28, ImportModuleNum = 29, InternalRelocationTableOffset = 42, InternalRelocationNum = 43 };

struct Segment { uint32_t Offset, Size, Type; };

void Put(Bytes& b, size_t at, uint32_t v)
{
    if (at + 4 > b.size()) throw FormatError("a module is cut short");
    for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k));
}

uint32_t Header(const Bytes& b, int field) { return U32(b, 0x80 + 4 * (size_t)field); }

std::vector<Segment> Segments(const Bytes& b)
{
    if (U32(b, 0x80) != 0x304F5243) throw FormatError("not a CRO module (no CRO0)");
    std::vector<Segment> s;
    for (uint32_t i = 0; i < Header(b, SegmentNum); i++)
    {
        const size_t at = Header(b, SegmentTableOffset) + 12 * (size_t)i;
        s.push_back({U32(b, at), U32(b, at + 4), U32(b, at + 8)});
    }
    return s;
}

// a segment tag (index in the low 4 bits, offset in the rest) as an address: the segment's offset (absolute in static.crs,
// a file offset in a module) plus base
uint32_t TagAddress(const std::vector<Segment>& s, uint32_t tag, uint32_t base)
{
    const uint32_t i = tag & 0xF, off = tag >> 4;
    if (i >= s.size() || off >= s[i].Size) throw FormatError("a segment tag outside its segment");
    return base + s[i].Offset + off;
}

uint32_t AddressTag(const std::vector<Segment>& s, uint32_t address, uint32_t base)
{
    for (uint32_t i = 0; i < s.size(); i++)
        if (s[i].Type != 3 && s[i].Size && address >= base + s[i].Offset && address < base + s[i].Offset + s[i].Size)
            return i | (address - base - s[i].Offset) << 4;
    char text[96];
    snprintf(text, sizeof text, "0x%08X is in no segment of the target module", address);
    throw FormatError(text);
}

std::string Hex(uint32_t v)
{
    char text[16];
    snprintf(text, sizeof text, "0x%08X", v);
    return text;
}
}

Bytes XyPortModule(const Bytes& xModule, const XyGameModules& x, const XyGameModules& oras, const XyModulePort& port,
                   const std::function<uint32_t(uint32_t)>& sound)
{
    Bytes out = xModule;
    std::vector<Segment> seg = Segments(out);
    const std::vector<Segment> xStatic = Segments(x.Static), oStatic = Segments(oras.Static);
    const std::vector<Segment> xField = Segments(x.Field), oField = Segments(oras.Field);

    // the name
    const uint32_t nameAt = Header(out, ModuleNameOffset);
    const std::string xName(out.begin() + nameAt, out.begin() + nameAt + port.XName.size());
    if (xName != port.XName || out[nameAt + port.XName.size()] != 0) throw FormatError("the module is not " + port.XName);
    if (port.Name.size() != port.XName.size()) throw FormatError(port.Name + ": not the length of " + port.XName);
    std::copy(port.Name.begin(), port.Name.end(), out.begin() + nameAt);

    // the imports: each anonymous import's tag, of |static| or DllField, made Omega Ruby's
    for (uint32_t m = 0; m < Header(out, ImportModuleNum); m++)
    {
        const size_t entry = Header(out, ImportModuleTableOffset) + 20 * (size_t)m;
        std::string module;
        for (size_t k = U32(out, entry); out.at(k); k++) module += (char)out[k];
        if (U32(out, entry + 8) != 0) throw FormatError(port.XName + ": indexed imports from " + module + " (not handled)");
        const bool isStatic = module == "|static|";
        if (!isStatic && module != "DllField") throw FormatError(port.XName + ": imports from " + module + " (not handled)");
        const auto& map = isStatic ? port.Static : port.Field;
        for (uint32_t i = 0; i < U32(out, entry + 16); i++)
        {
            const size_t at = U32(out, entry + 12) + 8 * (size_t)i;
            const uint32_t from = isStatic ? TagAddress(xStatic, U32(out, at), 0) : TagAddress(xField, U32(out, at), x.FieldBase);
            const auto to = map.find(from);
            if (to == map.end()) throw FormatError(port.XName + ": its import of " + module + " " + Hex(from) + " is not mapped");
            Put(out, at, isStatic ? AddressTag(oStatic, to->second, 0) : AddressTag(oField, to->second, oras.FieldBase));
        }
    }

    // its own words
    for (const XyModulePort::Word& w : port.Words)
    {
        if (U32(out, w.Offset) != w.Expected) throw FormatError(port.XName + " " + Hex(w.Offset) + ": not X's word (" + w.Why + ")");
        Put(out, w.Offset, w.Value);
    }
    for (const auto& [offset, id] : port.Sounds)
    {
        if (U32(out, offset) != id) throw FormatError(port.XName + " " + Hex(offset) + ": not X's sound id " + Hex(id));
        Put(out, offset, sound(id));
    }

    // the vtable, with the slot Omega Ruby added, after the code segment: the offset to the top and the type info before
    // its slots, as X's
    if (port.Slots)
    {
        const uint32_t start = (seg[0].Offset + seg[0].Size + 3) & ~3u, slots = start + 8, end = slots + 4 * (port.Slots + 1);
        for (size_t i = 1; i < seg.size(); i++)
            if (seg[i].Type != 3 && seg[i].Size && seg[i].Offset < end && seg[i].Offset + seg[i].Size > start)
                throw FormatError(port.XName + ": no room for the new vtable after the code");
        for (uint32_t k = start; k < end; k++)
            if (out.at(k) != 0) throw FormatError(port.XName + ": the bytes after the code are not free");
        // the new place of an old vtable word (-2, -1: the offset to the top, the type info; then the slots)
        const auto newPlace = [&](uint32_t old) -> uint32_t {
            const int index = ((int)old - (int)port.Vtable) / 4;
            if (index < 0) return slots + 4 * (uint32_t)index;
            return slots + 4 * (uint32_t)(index < (int)port.InsertSlot ? index : index + 1);
        };
        for (uint32_t old = port.Vtable - 8; old < port.Vtable + 4 * port.Slots; old += 4) Put(out, newPlace(old), U32(out, old));
        Put(out, slots + 4 * port.InsertSlot, port.InsertValue);
        Put(out, (size_t)Header(out, SegmentTableOffset) + 4, end - seg[0].Offset);
        seg[0].Size = end - seg[0].Offset;
        // the relocations writing the old vtable's words now write the new one's; those pointing at the old vtable point
        // at the new one
        int written = 0, pointing = 0;
        for (uint32_t i = 0; i < Header(out, InternalRelocationNum); i++)
        {
            const size_t at = Header(out, InternalRelocationTableOffset) + 12 * (size_t)i;
            const uint32_t target = TagAddress(seg, U32(out, at), 0);
            if (target >= port.Vtable - 8 && target < port.Vtable + 4 * port.Slots)
            {
                Put(out, at, AddressTag(seg, newPlace(target), 0));
                written++;
            }
            const uint32_t s = out.at(at + 5), addend = U32(out, at + 8);
            if (s < seg.size() && seg[s].Offset + addend == port.Vtable)
            {
                out[at + 5] = 0;
                Put(out, at + 8, slots - seg[0].Offset);
                pointing++;
            }
        }
        if (written != (int)port.Slots + 1 || pointing == 0)
            throw FormatError(port.XName + ": the vtable's relocations are not as read (" + std::to_string(written) + " written, " +
                              std::to_string(pointing) + " pointing)");
    }
    return out;
}

const XyModulePort& XyGymRock()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymRock";
        p.Name = "DllXyGymRock";
        // X's .code -> Omega Ruby's (xy_import_match.py; "near" ones read)
        p.Static = {
            {0x4B94D8, 0x4F40B0},
            {0x2FF520, 0x2FF8C0}, // the heap's delete (free from the block's header at -0x20); Omega Ruby's also called by OrasXyNickname
            {0x14F220, 0x1F9544}, // an empty function (an assert, empty in this build) -> Util_Nop_001F9544
            {0x42494C, 0x44E720}, // a sound's volume by id: the same loop over 14 handles for X's 10
            {0x425E54, 0x44FDC8}, // a sound stopped by id (the same guard and loop, 14 handles)
            {0x426304, 0x450274},
            {0x42E728, 0x45BB00},
            {0x42E7A0, 0x45BB78},
            {0x58E8AC, 0x5D4458}, // the vtable of __cxxabiv1::__class_type_info (type info, by its name)
            {0x58E8BC, 0x5D4468}, // the vtable of __cxxabiv1::__si_class_type_info
        };
        p.Field = {
            {0x1027AB84, 0x102CC730}, // a position -> its grid cell
            {0x102DDD60, 0x10327D20}, // the field's player ([+0x158] in X, [+0x164] in Omega Ruby)
        };
        // the player's position: + 0xC64 in X, + 0x6F8 in Omega Ruby (add r0, r0, #0x400; add r0, r0, #0x2F8)
        p.Words = {{0x21C, 0xE2800B03, 0xE2800B01, "the player's position: add #0xC00 -> #0x400"},
                   {0x228, 0xE2800064, 0xE2800FBE, "the player's position: add #0x64 -> #0x2F8"}};
        p.Sounds = {{0x280, 0xA0060}, {0x290, 0xA0060}}; // the ambient sound set to a volume, then stopped
        p.Vtable = 0x1568;
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Constructor = 0x544;
        p.ObjectSize = 0x18;
        return p;
    }();
    return port;
}

}
