#include "OrasXyGymModules.h"
#include "OrasXyDustCloud.h"

#include "Bch.h"
#include "BchUpgrade.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <set>
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

uint32_t ArmBranch(uint32_t from, uint32_t to, bool link, uint32_t cond = 0xE)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

constexpr uint32_t NoModule = 0x10274210;     // Field_LoadAreaSpecificModule's default case (the kinds that load nothing)
constexpr uint32_t Exit = 0x10274230;         // `pop {r4, r5, r6, pc}`: the cases' way out when the allocation fails
constexpr uint32_t LoadModule = 0x10243C48;   // DllField's import stub of the module loader (0x36F284)
constexpr uint32_t New = 0x102421F8;          // DllField's import stub of the allocation (size, heap)

std::string Hex(uint32_t v)
{
    char text[16];
    snprintf(text, sizeof text, "0x%08X", v);
    return text;
}
}

uint32_t XyGymArchiveNumber(size_t k) { return 0x130 + 0x10 * (uint32_t)k; }

std::string XyArchivePath(uint32_t number)
{
    if (number > 999) throw FormatError("an archive number past 999");
    char text[16];
    snprintf(text, sizeof text, "a/%u/%u/%u", number / 100, number / 10 % 10, number % 10);
    return text;
}

Bytes XyGymArchiveToOras(const Bytes& garc, std::vector<std::string>& report)
{
    Garc g(garc);
    const auto upgrade = [&](Bytes& f, const std::string& where) {
        if (!Bch::Is(f) || f.size() <= 4 || f[4] != 0x07) return false;
        try { f = BchUpgradeTo21(f); return true; }
        catch (const FormatError& e) { report.push_back(where + " kept as it is: " + e.what()); return false; }
    };
    for (size_t i = 0; i < g.Count(); i++)
    {
        // a member LZ11-compressed (X's Ice one is) is converted inside and compressed again: the loader decompresses by
        // the header's type (Res_Decompress 0x36AF08)
        const bool compressed = !g.Sub(i).empty() && g.Sub(i)[0] == 0x11;
        Bytes member = compressed ? LzDecompress(g.Sub(i)) : g.Sub(i);
        int converted = 0;
        if (upgrade(member, "member " + std::to_string(i))) converted++;
        else if (member.size() >= 4 && std::isalpha(member[0]) && std::isalpha(member[1]))
        {
            BinLinker pack = BinLinker::Read(member, std::string(member.begin(), member.begin() + 2));
            for (size_t k = 0; k < pack.Files.size(); k++)
                if (upgrade(pack.Files[k], "member " + std::to_string(i) + " file " + std::to_string(k))) converted++;
            if (converted) member = pack.Write();
        }
        if (converted)
        {
            g.Set(i, compressed ? Lz11Compress(member) : member);
            report.push_back("member " + std::to_string(i) + ": " + std::to_string(converted) + " BCH 0x07 -> 0x21");
        }
    }
    return g.Write();
}

Bytes XyPortModule(const Bytes& xModule, const XyGameModules& x, const XyGameModules& oras, const XyModulePort& port,
                   const std::function<uint32_t(uint32_t)>& sound, uint32_t archive)
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

    // the imports: each anonymous import's tag, of |static| or DllField, made Omega Ruby's (a thunk's import: its batch of
    // relocations kept, to point its stubs at the thunk below)
    std::vector<std::pair<const XyModulePort::Thunk*, uint32_t>> thunked; // a thunk, its import's batch (file offset)
    std::deque<XyModulePort::Thunk> moved;                                // the thunks made for DllField imports
    std::vector<std::pair<uint32_t, uint32_t>> copied;                     // a copied function's X address, its import's batch
    int64_t anchorWord = -1;                                               // the word the loader writes for FieldAnchor
    std::vector<std::pair<const XyModulePort::FieldVtable*, uint32_t>> vtableBatches; // a FieldVtable, its import's batch
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
            // a thunk: given, or a DllField import whose Omega Ruby function is in the .code (Omega Ruby moved the area
            // modules' API there: X's 0x1027E618 is 0x452A00...), which a DllField import cannot name; the import itself
            // then resolves to DllField's first word, unused (its stubs branch to the thunk)
            bool isCopy = false;
            for (const XyModulePort::Copy& c : port.Copies) isCopy |= (from & ~1u) == c.XAddress;
            if (isCopy)
            {
                // its stubs branch to the copy (below); the import itself resolves to the .code's (or DllField's) first
                // word, unused
                Put(out, at, isStatic ? AddressTag(oStatic, 0x100000, 0) : AddressTag(oField, oras.FieldBase + oField[0].Offset, oras.FieldBase));
                copied.push_back({from & ~1u, U32(out, at + 4)});
                continue;
            }
            const XyModulePort::FieldVtable* fieldVtable = nullptr;
            for (const XyModulePort::FieldVtable& v : port.FieldVtables) if (!isStatic && v.XImport == from) fieldVtable = &v;
            if (fieldVtable)
            {
                // its words are given the module's own vtable below; the import itself resolves to DllField's first word,
                // and its relocations write a word of their own
                Put(out, at, AddressTag(oField, oras.FieldBase + oField[0].Offset, oras.FieldBase));
                vtableBatches.push_back({fieldVtable, U32(out, at + 4)});
                continue;
            }
            if (!isStatic && port.FieldAnchor && from == port.FieldAnchor) anchorWord = TagAddress(seg, U32(out, U32(out, at + 4)), 0);
            const XyModulePort::Thunk* thunk = nullptr;
            for (const XyModulePort::Thunk& t : port.Thunks) if (t.XTarget == from) thunk = &t;
            const auto to = map.find(from);
            if (!thunk && !isStatic && to != map.end() && to->second < 0x10000000)
                thunk = &moved.emplace_back(XyModulePort::Thunk{from, to->second, {}, "moved to the .code"});
            if (thunk)
            {
                Put(out, at, isStatic ? AddressTag(oStatic, thunk->Target, 0) : AddressTag(oField, oras.FieldBase + oField[0].Offset, oras.FieldBase));
                thunked.push_back({thunk, U32(out, at + 4)});
                continue;
            }
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
    for (const uint32_t offset : port.ArchiveWords)
    {
        // `mov rd, #n`: X's archive, then Omega Ruby's number for it (an immediate rotated by an even amount: 0x130 is
        // 0x13 rotated right by 28); rd kept (Res_OpenArchive takes it in r2, Viola's module calls it directly)
        const uint32_t rd = U32(out, offset) & 0xF000;
        if ((U32(out, offset) & ~0xF000u) != (0xE3A00000 | port.Archive)) throw FormatError(port.XName + " " + Hex(offset) + ": not X's request of archive " + Hex(port.Archive));
        if (archive)
        {
            uint32_t rot = 0, imm = archive;
            while (imm > 0xFF && rot < 15) { rot++; imm = archive << (2 * rot) | archive >> (32 - 2 * rot); }
            if (imm > 0xFF) throw FormatError(port.XName + ": archive " + Hex(archive) + " is no ARM immediate");
            Put(out, offset, 0xE3A00000 | rd | rot << 8 | imm);
        }
    }
    for (const auto& [offset, id] : port.Sounds)
    {
        if (U32(out, offset) != id) throw FormatError(port.XName + " " + Hex(offset) + ": not X's sound id " + Hex(id));
        Put(out, offset, sound(id));
    }

    // the bytes after the code segment, free up to the next segment: the code segment grows over what is written there
    const auto reserve = [&](uint32_t bytes) -> uint32_t {
        const uint32_t start = (seg[0].Offset + seg[0].Size + 3) & ~3u, end = start + bytes;
        for (size_t i = 1; i < seg.size(); i++)
            if (seg[i].Type != 3 && seg[i].Size && seg[i].Offset < end && seg[i].Offset + seg[i].Size > start)
                throw FormatError(port.XName + ": no room after the code");
        for (uint32_t k = start; k < end; k++)
            if (out.at(k) != 0) throw FormatError(port.XName + ": the bytes after the code are not free");
        seg[0].Size = end - seg[0].Offset;
        Put(out, (size_t)Header(out, SegmentTableOffset) + 4, seg[0].Size);
        return start;
    };

    // the thunks: each written after the code, and each stub of its import (the word its batch's relocations write, after
    // `ldr pc, [pc, #-4]`) made a branch to it
    for (const auto& [thunk, batch] : thunked)
    {
        const uint32_t code = reserve(4 * ((uint32_t)thunk->Prefix.size() + 2));
        for (size_t k = 0; k < thunk->Prefix.size(); k++) Put(out, code + 4 * k, thunk->Prefix[k]);
        Put(out, code + 4 * (uint32_t)thunk->Prefix.size(), 0xE51FF004);             // ldr pc, [pc, #-4]
        Put(out, code + 4 * (uint32_t)thunk->Prefix.size() + 4, thunk->Target);       // .code: no relocation
        for (size_t at = batch;; at += 12)
        {
            const uint32_t stub = TagAddress(seg, U32(out, at), 0) - 4;
            if (U32(out, stub) != 0xE51FF004) throw FormatError(port.XName + ": an import of " + Hex(thunk->XTarget) + " not through a stub");
            Put(out, stub, ArmBranch(stub, code, false));
            if (out.at(at + 5)) break; // the batch's last
        }
    }

    // internal relocations added (a word's file offset, the code segment's offset it holds), written at the end
    std::vector<std::pair<uint32_t, uint32_t>> added;
    if (!port.FieldVtables.empty() && port.Copies.empty()) throw FormatError(port.XName + ": a DllField vtable with nothing copied");

    // the copies of X's .code functions, then a veneer per X function they call
    if (!port.Copies.empty())
    {
        uint32_t total = 0;
        for (const XyModulePort::Copy& c : port.Copies) total += (c.Size + 3) & ~3u;
        const uint32_t base = reserve(total);
        std::map<uint32_t, uint32_t> placed; // X address -> its copy's file offset
        uint32_t next = base;
        for (const XyModulePort::Copy& c : port.Copies)
        {
            // X's .code, or X's DllField (the file at its linked base)
            const bool fromField = c.XAddress >= x.FieldBase && x.FieldBase;
            const Bytes& source = fromField ? x.Field : x.Code;
            const uint32_t base = fromField ? x.FieldBase : 0x100000;
            if (c.XAddress < base || c.XAddress - base + c.Size > source.size()) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + " outside X's .code and DllField");
            std::copy(source.begin() + (c.XAddress - base), source.begin() + (c.XAddress - base + c.Size), out.begin() + next);
            placed[c.XAddress] = next;
            next += (c.Size + 3) & ~3u;
        }
        const auto copyOf = [&](uint32_t xAddress) -> int64_t {
            for (const XyModulePort::Copy& c : port.Copies)
                if (xAddress >= c.XAddress && xAddress < c.XAddress + c.Size) return (int64_t)placed[c.XAddress] + (xAddress - c.XAddress);
            return -1;
        };
        std::map<uint32_t, uint32_t> veneers; // Omega Ruby's address -> its veneer
        const auto veneer = [&](uint32_t target) {
            auto it = veneers.find(target);
            if (it != veneers.end()) return it->second;
            const uint32_t v = reserve(8);
            Put(out, v, 0xE51FF004); // ldr pc, [pc, #-4]
            Put(out, v + 4, target); // .code (Thumb when odd): no relocation
            return veneers[target] = v;
        };
        // Omega Ruby's DllField function at `target` (its linked address): the anchor's word read, the distance added
        std::map<uint32_t, uint32_t> fieldVeneers;
        const auto fieldVeneer = [&](uint32_t target) {
            auto it = fieldVeneers.find(target);
            if (it != fieldVeneers.end()) return it->second;
            const auto anchor = port.Field.find(port.FieldAnchor);
            if (anchorWord < 0 || anchor == port.Field.end() || anchor->second < 0x10000000) throw FormatError(port.XName + ": a DllField call with no DllField anchor imported");
            const uint32_t v = reserve(40);
            const uint32_t words[] = {0xE52D0004,  // str r0, [sp, #-4]!
                                      0xE59F0014,  // ldr r0, [pc, #20]: the anchor word's distance
                                      0xE08F0000,  // add r0, pc, r0
                                      0xE5900000,  // ldr r0, [r0]: where the loader put the anchor
                                      0xE59FC00C,  // ldr ip, [pc, #12]: the distance between the functions
                                      0xE080C00C,  // add ip, r0, ip
                                      0xE49D0004,  // ldr r0, [sp], #4
                                      0xE12FFF1C,  // bx ip
                                      (uint32_t)anchorWord - (v + 16), target - anchor->second};
            for (size_t k = 0; k < 10; k++) Put(out, v + 4 * (uint32_t)k, words[k]);
            return fieldVeneers[target] = v;
        };
        for (const XyModulePort::Copy& c : port.Copies)
        {
            const uint32_t at = placed[c.XAddress];
            // the words pc-relative loads read: data, not instructions
            std::set<uint32_t> literals;
            for (uint32_t k = 0; k < c.Size; k += 4)
            {
                const uint32_t w = U32(out, at + k);
                if ((w & 0x0F7F0000) == 0x051F0000) // ldr rX, [pc, #+-imm12]
                {
                    const int64_t to = (int64_t)k + 8 + ((w >> 23) & 1 ? (int64_t)(w & 0xFFF) : -(int64_t)(w & 0xFFF));
                    if (to < 0 || to + 4 > c.Size) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + " reads a literal outside it at +" + Hex((uint32_t)k));
                    literals.insert((uint32_t)to);
                }
                else if ((w & 0x0F3F0F00) == 0x0D1F0A00) // vldr sN, [pc, #+-imm8*4]
                {
                    const int64_t to = (int64_t)k + 8 + ((w >> 23) & 1 ? 4 * (int64_t)(w & 0xFF) : -4 * (int64_t)(w & 0xFF));
                    if (to < 0 || to + 4 > c.Size) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + " reads a literal outside it at +" + Hex((uint32_t)k));
                    literals.insert((uint32_t)to);
                }
            }
            for (uint32_t k = 0; k < c.Size; k += 4)
            {
                const uint32_t w = U32(out, at + k), xAt = c.XAddress + k;
                if (literals.count(k))
                {
                    if (std::find(port.CopySounds.begin(), port.CopySounds.end(), w) != port.CopySounds.end())
                        Put(out, at + k, sound(w));
                    else if (w >= 0x100000 && w < 0x100000 + 0x800000) // an X .code or data address
                    {
                        const auto m = port.Literals.find(w);
                        if (m == port.Literals.end()) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + "'s literal " + Hex(w) + " is not mapped");
                        Put(out, at + k, m->second);
                    }
                    continue;
                }
                const bool branch = (w & 0x0E000000) == 0x0A000000 && w >> 28 != 0xF, blx = (w & 0xFE000000) == 0xFA000000;
                if (!branch && !blx) continue;
                const int32_t offset = ((int32_t)(w << 8) >> 6) + (blx ? (int32_t)((w >> 23) & 2) : 0);
                const uint32_t target = xAt + 8 + (uint32_t)offset;
                const int64_t local = copyOf(target);
                if (local >= 0 && !blx) { Put(out, at + k, (w & 0xFF000000) | ((((uint32_t)local - (at + k) - 8) >> 2) & 0xFFFFFF)); continue; }
                if (local >= 0 || (target >= c.XAddress && target < c.XAddress + c.Size)) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + ": a blx into a copy at +" + Hex(k));
                // through X DllField's import stub: what it reaches (this module's own function: a branch inside it)
                uint32_t reached = target;
                const auto stub = port.FieldStubs.find(target);
                if (stub != port.FieldStubs.end())
                {
                    reached = stub->second;
                    if (port.XLinked && reached >= port.XLinked && reached - port.XLinked < seg[0].Offset + seg[0].Size && !blx)
                    {
                        Put(out, at + k, (w & 0xFF000000) | (((reached - port.XLinked - (at + k) - 8) >> 2) & 0xFFFFFF));
                        continue;
                    }
                }
                const auto m = port.Static.find(blx ? reached | 1 : reached);
                const auto f = port.Field.find(reached);
                if (m == port.Static.end() && f == port.Field.end()) throw FormatError(port.XName + ": copy " + Hex(c.XAddress) + " calls X's " + Hex(reached) + ", not mapped");
                const uint32_t v = m != port.Static.end() ? veneer(m->second) : f->second >= 0x10000000 ? fieldVeneer(f->second) : veneer(f->second);
                // a bl / b keeps its condition and link bit; a blx becomes bl to the veneer (which enters Thumb by the odd address)
                const uint32_t kept = blx ? 0xEB000000 : (w & 0xFF000000);
                Put(out, at + k, kept | (((v - (at + k) - 8) >> 2) & 0xFFFFFF));
            }
        }
        // each DllField class's vtable in the module: its slots a copy or a veneer to Omega Ruby's method; the words its
        // import wrote (X's vtable + 8) now hold it (an internal relocation each), the import's own relocations a word of
        // their own
        for (const auto& [fv, batch] : vtableBatches)
        {
            const uint32_t start = reserve(8 + 4 * (uint32_t)fv->XSlots.size()), slots = start + 8;
            Put(out, start, 0);
            Put(out, start + 4, 0); // no type info (X's is DllField's own; nothing reached through it is ported)
            for (size_t i = 0; i < fv->XSlots.size(); i++)
            {
                const uint32_t xs = fv->XSlots[i];
                const int64_t local = copyOf(xs);
                uint32_t to;
                if (local >= 0) to = (uint32_t)local;
                else
                {
                    const auto f = port.Field.find(xs);
                    if (f == port.Field.end() || f->second < 0x10000000) throw FormatError(port.XName + ": the DllField vtable's slot " + Hex(xs) + " is not mapped");
                    to = fieldVeneer(f->second);
                }
                added.push_back({slots + 4 * (uint32_t)i, to});
            }
            for (size_t b = batch;; b += 12)
            {
                const uint32_t word = TagAddress(seg, U32(out, b), 0);
                if (U32(out, b + 8) != 8) throw FormatError(port.XName + ": the DllField vtable's import is not X's vtable + 8 at " + Hex(word));
                const uint32_t spare = reserve(4);
                Put(out, b, AddressTag(seg, spare, 0));
                added.push_back({word, slots});
                if (out.at(b + 5)) break;
            }
        }

        // the stubs of the imports of copied functions: branches to the copy
        for (const auto& [xAddress, batch] : copied)
            for (size_t b = batch;; b += 12)
            {
                const uint32_t stub = TagAddress(seg, U32(out, b), 0) - 4;
                if (U32(out, stub) != 0xE51FF004) throw FormatError(port.XName + ": an import of " + Hex(xAddress) + " not through a stub");
                Put(out, stub, ArmBranch(stub, placed.at(xAddress), false));
                if (out.at(b + 5)) break;
            }
    }

    // each vtable, with the slot Omega Ruby added, after the code segment: the offset to the top and the type info before
    // its slots, as X's
    for (const uint32_t vtable : port.Vtables)
    {
        const uint32_t start = reserve(8 + 4 * (port.Slots + 1)), slots = start + 8;
        // the new place of an old vtable word (-2, -1: the offset to the top, the type info; then the slots)
        const auto newPlace = [&](uint32_t old) -> uint32_t {
            const int index = ((int)old - (int)vtable) / 4;
            if (index < 0) return slots + 4 * (uint32_t)index;
            return slots + 4 * (uint32_t)(index < (int)port.InsertSlot ? index : index + 1);
        };
        for (uint32_t old = vtable - 8; old < vtable + 4 * port.Slots; old += 4) Put(out, newPlace(old), U32(out, old));
        Put(out, slots + 4 * port.InsertSlot, port.InsertValue);
        // the relocations writing the old vtable's words now write the new one's; those pointing at the old vtable point
        // at the new one
        int written = 0, pointing = 0;
        for (uint32_t i = 0; i < Header(out, InternalRelocationNum); i++)
        {
            const size_t at = Header(out, InternalRelocationTableOffset) + 12 * (size_t)i;
            const uint32_t target = TagAddress(seg, U32(out, at), 0);
            if (target >= vtable - 8 && target < vtable + 4 * port.Slots)
            {
                Put(out, at, AddressTag(seg, newPlace(target), 0));
                written++;
            }
            const uint32_t s = out.at(at + 5), addend = U32(out, at + 8);
            if (s < seg.size() && seg[s].Offset + addend == vtable)
            {
                out[at + 5] = 0;
                Put(out, at + 8, slots - seg[0].Offset);
                pointing++;
            }
        }
        if (written != (int)port.Slots + 1 || pointing == 0)
            throw FormatError(port.XName + ": the vtable at " + Hex(vtable) + "'s relocations are not as read (" + std::to_string(written) +
                              " written, " + std::to_string(pointing) + " pointing)");
    }
    // the added internal relocations at the table's end (type 2, an absolute address: the code segment's base plus the
    // offset): what follows the table (the data segment) moves down over the padding at the file's end (0xCC in X's gym
    // modules)
    if (!added.empty())
    {
        const uint32_t table = Header(out, InternalRelocationTableOffset), count = Header(out, InternalRelocationNum);
        const uint32_t insert = table + 12 * count, grow = 12 * (uint32_t)added.size();
        for (size_t k = out.size() - grow; k < out.size(); k++)
            if (out[k] != 0 && out[k] != 0xCC) throw FormatError(port.XName + ": no free bytes at the end for the added relocations");
        std::copy_backward(out.begin() + insert, out.end() - grow, out.end());
        for (size_t i = 0; i < added.size(); i++)
        {
            const uint32_t at = insert + 12 * (uint32_t)i;
            Put(out, at, AddressTag(seg, added[i].first, 0));
            out[at + 4] = 2; // R_ARM_ABS32
            out[at + 5] = 0; // the code segment
            out[at + 6] = out[at + 7] = 0;
            Put(out, at + 8, added[i].second - seg[0].Offset);
        }
        Put(out, 0x80 + 4 * InternalRelocationNum, count + (uint32_t)added.size());
        // every table or segment after the insertion point moves down
        for (const int field : {1, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 44})
            if (Header(out, field) >= insert && Header(out, field) != 0) Put(out, 0x80 + 4 * (size_t)field, Header(out, field) + grow);
        for (uint32_t i = 0; i < Header(out, SegmentNum); i++)
        {
            const size_t at = Header(out, SegmentTableOffset) + 12 * (size_t)i;
            if (U32(out, at + 4) && U32(out, at) >= insert) Put(out, at, U32(out, at) + grow);
        }
    }
    return out;
}

const std::vector<XyGym>& XyGyms()
{
    // X's kind -> a kind Omega Ruby's Field_LoadAreaSpecificModule has no module for: 1, 2, 9, 12, 17-20 (its cases going
    // to the default one), or 26 and up (past its switch's bound, `cmp r2, #0x1A`, read 31 October: kind 22 is not free,
    // its case 0x10274180 loads a module)
    static const std::vector<XyGym> gyms = {
        {3, 1, XyGymRock()},
        {0xE, 2, XyGymIce()},
        {0x19, 9, XyGymElec(), 0}, {0x1A, 12, XyGymElec(), 1}, {0x1B, 17, XyGymElec(), 2},
        {0x18, 18, XyRoute13()}, // not a gym: Route 13's dust clouds (OrasXyDustCloud.h)
        {2, 19, XyGymInsect()},
        {0, 20, XyGymFighting()}, // X's kind 0: zone 175 only (the others 0xFFFF)
        {4, 26, XyGymGrass()},
    };
    return gyms;
}

uint32_t XyGymArchiveNumberOf(const XyModulePort& port)
{
    // in the order oras-engine ports them: each module once, its archive the next number
    std::set<std::string> ported;
    size_t k = 0;
    for (const XyGym& g : XyGyms())
    {
        if (!ported.insert(g.Port.Name).second || !g.Port.Archive) continue;
        if (&g.Port == &port) return XyGymArchiveNumber(k);
        k++;
    }
    throw FormatError(port.XName + ": no archive among the ported modules");
}

uint16_t XyGymAreaKind(uint16_t xKind)
{
    for (const XyGym& g : XyGyms())
        if (g.XKind == xKind) return g.OrasKind;
    return 0xFFFF;
}

std::vector<CodeWord> XyGymLoaderPatchAt(uint32_t cave)
{
    std::vector<CodeWord> w;
    const std::vector<XyGym>& gyms = XyGyms();
    // the dispatcher: the default case's first word (the kind still in r2) branches to it; each gym's kind sets r3 to its
    // record (the constructor's offset, the object's size, its path from the record) and goes to the routine; any other
    // kind goes back to the default case after its first word. The cave is small (DllField's last page, shared): one
    // routine, a record per gym, a path per module
    const uint32_t dispatch = (cave + 3) & ~3u, back = dispatch + 12 * (uint32_t)gyms.size(), routine = back + 8;
    const uint32_t records = routine + 0x40;
    uint32_t paths = records + 12 * (uint32_t)gyms.size();
    std::map<std::string, uint32_t> pathAt;
    w.push_back({NoModule, 0xE1A01000, ArmBranch(NoModule, dispatch, false), "Field_LoadAreaSpecificModule's default case: to the X/Y gyms' dispatcher"});
    for (size_t i = 0; i < gyms.size(); i++)
    {
        const XyGym& g = gyms[i];
        const XyModulePort& p = g.Port;
        const uint32_t d = dispatch + 12 * (uint32_t)i, record = records + 12 * (uint32_t)i, distance = record - (d + 4 + 8);
        if (distance > 0xFF * 4 || distance % 4) throw FormatError("the X/Y gyms' records are too far from their dispatcher");
        w.push_back({d, 0, 0xE3520000 | g.OrasKind, "X/Y gyms' dispatcher (cave): cmp r2, #" + std::to_string(g.OrasKind)});
        w.push_back({d + 4, 0, 0x028F3F00 | distance / 4, "X/Y gyms' dispatcher (cave): addeq r3, pc, #: " + p.Name + "'s record"});
        w.push_back({d + 8, 0, ArmBranch(d + 8, routine, false, 0x0), "X/Y gyms' dispatcher (cave): beq the loader"});
        const std::string path = "rom:/" + p.Name + ".cro";
        if (!pathAt.count(path))
        {
            pathAt[path] = paths;
            Bytes text(path.begin(), path.end());
            text.resize((text.size() + 4) & ~(size_t)3, 0);
            for (size_t k = 0; k < text.size(); k += 4)
                w.push_back({paths + (uint32_t)k, 0, (uint32_t)(text[k] | text[k + 1] << 8 | text[k + 2] << 16 | (uint32_t)text[k + 3] << 24), p.Name + "'s path (cave)"});
            paths += (uint32_t)text.size();
        }
        const XyModulePort::Class& c = p.Classes.at(g.Class);
        w.push_back({record, 0, c.Constructor, p.Name + "'s record (cave): its constructor's offset"});
        w.push_back({record + 4, 0, c.ObjectSize, p.Name + "'s record (cave): its object's size"});
        w.push_back({record + 8, 0, pathAt[path] - record, p.Name + "'s record (cave): its path, from the record"});
    }
    w.push_back({back, 0, 0xE1A01000, "X/Y gyms' dispatcher (cave): mov r1, r0 (the default case's first word)"});
    w.push_back({back + 4, 0, ArmBranch(back + 4, NoModule + 4, false), "X/Y gyms' dispatcher (cave): back to the default case"});
    // entered as the game's cases (r4 the field object, r5 the heap, {r4, r5, r6, lr} pushed), r3 the record
    const uint32_t code[] = {
        0xE1A06003,                                   // 00 mov r6, r3              the record
        0xE5960008,                                   // 04 ldr r0, [r6, #8]
        0xE0860000,                                   // 08 add r0, r6, r0          the module's path
        ArmBranch(routine + 0x0C, LoadModule, true),  // 0C bl the loader
        0xE58401A4,                                   // 10 str r0, [r4, #0x1a4]    the handle, as the game's cases
        0xE1A01005,                                   // 14 mov r1, r5
        0xE5960004,                                   // 18 ldr r0, [r6, #4]        the object's size
        ArmBranch(routine + 0x1C, New, true),         // 1C bl the allocation
        0xE3500000,                                   // 20 cmp r0, #0
        ArmBranch(routine + 0x24, Exit, false, 0x0),  // 24 beq the cases' exit
        0xE596C000,                                   // 28 ldr ip, [r6]            the constructor's offset
        0xE59421A4,                                   // 2C ldr r2, [r4, #0x1a4]
        0xE082C00C,                                   // 30 add ip, r2, ip
        0xE1A01004,                                   // 34 mov r1, r4
        0xE8BD4070,                                   // 38 pop {r4, r5, r6, lr}
        0xE12FFF1C,                                   // 3C bx ip                   the constructor (object, field)
    };
    for (uint32_t k = 0; k < 16; k++) w.push_back({routine + 4 * k, 0, code[k], "the X/Y gyms' loader (cave)"});
    return w;
}

// what X's gym modules import alike, mapped (read 20 October)
std::map<uint32_t, uint32_t> SharedStatic()
{
    return {
        {0x1001D9, 0x10018D}, {0x100205, 0x100295}, // Thumb helpers (the same bytes)
        {0x2FF520, 0x2FF8C0},                       // the heap's delete (Omega Ruby's also called by OrasXyNickname)
        {0x14F220, 0x1F9544},                       // an empty function (an assert, empty in this build) -> Util_Nop_001F9544
        {0x122920, 0x123480},                       // an allocation from a heap
        {0x369914, 0x381B00}, {0x3699A8, 0x381B94}, {0x369AD0, 0x381CBC}, {0x369B58, 0x381D44}, // gfl::grp::g3d, X's +0x181EC
        {0x36A758, 0x382944}, {0x36A7E4, 0x3829D0}, // H3dResAnim's (two identical copies in Omega Ruby: this one at +0x181EC)
        {0x3739F0, 0x38B374}, {0x373D18, 0x38B69C}, // X's +0x17984
        {0x37EC00, 0x3964E8}, // a vector set at +0x138 (Omega Ruby's +0x130): the functions around both line up
        {0x49B680, 0x4CCDDC}, // its getter (+0x130 in Omega Ruby)
        {0x42E728, 0x45BB00}, {0x42E7A0, 0x45BB78},
        {0x58E8AC, 0x5D4458}, {0x58E8BC, 0x5D4468}, // the C++ type infos' vtables
    };
}

// the area modules' API, which Omega Ruby moved from DllField to its .code (its own gym modules import these five:
// 0x452A00, 0x452E7C, 0x452EE8, 0x452330, 0x452168), its loader 0x4525BC, and the field's getters
std::map<uint32_t, uint32_t> SharedField()
{
    return {
        {0x1027E618, 0x452A00}, {0x1027EDAC, 0x452E7C}, {0x1027EF88, 0x452EE8}, {0x1027F2F8, 0x452330}, {0x1027F44C, 0x452168},
        {0x1027E964, 0x4525BC}, // the loader (an archive read and its members unpacked): its fields from +0x2C 4 bytes on
        {0x102CD7DC, 0x45248C},
        {0x102DDD30, 0x10327CF0}, // the field's getter of +0xF0 (Omega Ruby's +0x100: the getters line up)
        {0x102DDD60, 0x10327D20}, // the field's player ([+0x158] in X, [+0x164] in Omega Ruby)
    };
}

// the loader's request (object, archive): Omega Ruby's takes a third word, how many members, 0 all
const XyModulePort::Thunk Request{0x1027ED9C, 0x452E5C, {0xE3A02000}, "the loader's request: mov r2, #0 (all members)"};

// the loader object's allocation (mov r0, #0x3C): 0x40 bytes in Omega Ruby (its count at +0x30 for X's +0x2C; Omega
// Ruby's modules allocate 0x40)
XyModulePort::Word LoaderSize(uint32_t offset) { return {offset, 0xE3A0003C, 0xE3A00040, "the loader object's size: 0x3C -> 0x40"}; }

template <typename Map> Map Merged(Map a, const Map& b) { a.insert(b.begin(), b.end()); return a; }

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
        // and the field's members Omega Ruby keeps elsewhere (prototype/xy_module_layout.py --words, 30 October): + 0xFC is
        // its + 0x10C (X's + 0xFC + 0xC4, the ambience, written on the way back from a won battle, was a null write)
        p.Words = {{0x21C, 0xE2800B03, 0xE2800B01, "the player's position: add #0xC00 -> #0x400"},
                   {0x228, 0xE2800064, 0xE2800FBE, "the player's position: add #0x64 -> #0x2F8"},
                   {0x410, 0xE59220FC, 0xE592210C, "the field's + 0xFC -> + 0x10C"},
                   {0x468, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   {0x4EC, 0xE59110FC, 0xE591110C, "the field's + 0xFC -> + 0x10C"},
                   {0x500, 0xE59220FC, 0xE592210C, "the field's + 0xFC -> + 0x10C"}};
        p.Sounds = {{0x280, 0xA0060}, {0x290, 0xA0060}}; // the ambient sound set to a volume, then stopped
        p.Vtables = {0x1568};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0x544, 0x18}};
        return p;
    }();
    return port;
}

const XyModulePort& XyGymIce()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymIce";
        p.Name = "DllXyGymIce";
        // X's .code -> Omega Ruby's (xy_import_match.py; pairs of identical copies taken in their order, read 20 October)
        p.Static = Merged(SharedStatic(), {
            {0x359E3C, 0x372748},
            {0x3D1A90, 0x3F2AA4}, {0x3D1AAC, 0x3F2AC0},
            {0x4A3FD8, 0x4DBDF4},
        });
        p.Field = Merged(SharedField(), {
            {0x1027F1B0, 0x102CC758}, // two identical copies in Omega Ruby: the one whose callee is X's (0x1027670C, 0x102C8574)
            {0x102832E4, 0x102D1644}, // (Omega Ruby's gym modules import it)
            {0x10282940, 0x102D0D2C},
        });
        p.Thunks = {Request};
        p.Words = {LoaderSize(0x180C),
                   // the field's members Omega Ruby keeps elsewhere (prototype/xy_module_layout.py --words, 30 October)
                   {0x1580, 0xE59020FC, 0xE590210C, "the field's + 0xFC -> + 0x10C"},
                   {0x1594, 0xE59020FC, 0xE590210C, "the field's + 0xFC -> + 0x10C"},
                   {0x15A8, 0xE59020FC, 0xE590210C, "the field's + 0xFC -> + 0x10C"},
                   {0x16D8, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"}};
        p.Archive = 0xAE; // a/1/7/4
        p.ArchiveWords = {0x1698};
        p.Vtables = {0x2620};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0x176C, 0x36C}};
        return p;
    }();
    return port;
}

const XyModulePort& XyGymElec()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymElec";
        p.Name = "DllXyGymElec";
        p.Static = Merged(SharedStatic(), {
            {0x3D09A0, 0x3F18A4}, {0x425B1C, 0x44FA8C},
        });
        p.Field = SharedField();
        p.Thunks = {Request};
        p.Words = {LoaderSize(0xEA8), LoaderSize(0x1364), LoaderSize(0x1704),
                   // the field's member Omega Ruby keeps elsewhere (prototype/xy_module_layout.py --words, 30 October)
                   {0xD74, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"}};
        p.Archive = 0xEB; // a/2/3/5
        p.ArchiveWords = {0xD34, 0x1300, 0x1684};
        p.Vtables = {0x2108, 0x2144, 0x2180}; // its three classes'
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        // X's kinds 0x19, 0x1A, 0x1B (X/Y zones 79-82, 77, 78), each its class (X's DllField cases, read 20 October)
        p.Classes = {{0xDF4, 0x1DC}, {0x16CC, 0x38}, {0x1348, 0x18}};
        return p;
    }();
    return port;
}

const XyModulePort& XyGymInsect()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymInsect";
        p.Name = "DllXyGymInsect";
        // X's .code -> Omega Ruby's: the same functions (xy_import_match.py), then those found through their callers' copies
        // (xy_callee_map.py, votes) or by their own code (30 October; DEVELOPMENT_NOTE.md 3e, Viola's gym)
        p.Static = Merged(SharedStatic(), {
            {0x300EF8, 0x300B28}, {0x3032E3, 0x302F53}, {0x1048C9, 0x1048D5}, {0x352488, 0x36AF08}, {0x132D1C, 0x13222C},
            {0x11C3CC, 0x11C930}, {0x355F6C, 0x36EA38}, {0x355FB8, 0x36EA84}, {0x3561A4, 0x36EC70}, {0x356200, 0x36ECCC},
            {0x11C3E8, 0x11C94C}, {0x371888, 0x3891DC}, {0x379DB8, 0x39181C}, {0x37E388, 0x395C70}, {0x37E398, 0x395C80},
            {0x37E444, 0x395D2C}, {0x385F68, 0x39D6E8}, {0x3864EC, 0x39DBEC}, {0x38673C, 0x39DF2C}, {0x13A50C, 0x1397B8},
            {0x12B094, 0x12B96C}, {0x12B274, 0x12BB4C}, {0x12B284, 0x12BB5C}, {0x3D037C, 0x3F1324}, {0x425B1C, 0x44FA8C},
            {0x4566EC, 0x48A4AC}, {0x4567D8, 0x486BC8}, {0x45690C, 0x486CFC}, {0x456984, 0x48A7A4}, {0x1228EC, 0x12344C},
            {0x1173DC, 0x12FB40},
            {0x300DF9, 0x3009F5}, // Thumb (push, bl, blx, pop the same): 14 callers' copies agree
            {0x36FD78, 0x3878F8}, // 6 callers agree
            {0x49CAF4, 0x4CE2DC}, // 11 callers agree
            {0x4AA98C, 0x4E3594}, // 18 callers agree (xy_import_match.py: near, 0.90)
            {0x45672C, 0x486B1C}, // 0.98 alike, the same writes (the object's +0x14 among them)
            {0x456970, 0x48A790}, // identical (clears +0, +4, +8)
            {0x4B3D9C, 0x4C4D0C}, // identical, `ldrsb r0, [r0, #0x14]` (the object 0x45672C fills)
            {0x390550, 0x3A7D54}, {0x11E46C, 0x11EB78}, // identical setters
            {0x390560, 0x1F9544}, // a bare return -> Util_Nop_001F9544
            // called by the copies below
            {0x499368, 0x4CAA58}, // 0.99 (xy_callee_map.py: X's 0x37AADC = Omega Ruby's 0x392540 calls it there)
            {0x376FC4, 0x38EAC0}, // 1.00
            {0x49A03C, 0x4CB7A4}, {0x2F9B90, 0x312498}, {0x2FEAA4, 0x2FF034}, {0x481FA8, 0x4B2A08},
        });
        p.Field = Merged(SharedField(), {
            {0x1027AB84, 0x102CC730}, // a position -> its grid cell
            {0x1027F5CC, 0x102CDFD0}, // the zone's character list (80 callers agree; DllUSR13's too)
        });
        // X's own .code (xy_only_code.py: no copy in Omega Ruby): a helper over a model's 0x34-byte entries, its pool and data
        p.Copies = {
            {0x455C30, 276, "its entries built (a model's list at +0x164)"},
            {0x455F28, 544, "one entry set"},
            {0x455D44, 484, "the entries updated"},
            {0x456148, 656, "an entry's material"},
            {0x4563D8, 788, "an entry's draw"},
            {0x2F66E4, 188, "a material's value (X's library, no copy in Omega Ruby)"},
            {0x488AB8, 108, "(0.74 to Omega Ruby's 0x4AC9DC: X's kept)"},
            {0x367AD4, 24, "the name \"IDEdgeEnable\": add r0, pc, #0; bx lr, then the string"},
            // a model's node drawn with the camera: X's 0x378548 and 0x3785E4 are twins but for the model's getters
            // (0x499368 and slot 0x70 here, 0x49B440 and slot 0x3C there); Omega Ruby has only the second (0x390048,
            // 1.00 to 0x3785E4), mapped here before: it read the model's + 0x28 (flags, 1) as a pointer, an unmapped
            // read at 0x95 every frame (run vA2, 31 October)
            {0x378548, 156, "a model's node drawn with the camera (with its literal 0.0)"},
        };
        p.Literals = {
            {0x5D0F10, 0x6172DC}, // a class's identifier, compared with a virtual getter's answer: 39 copies agree
            {0x59C238, 0x5E1F3C}, // 16 constant bytes (1, 2, 1, 0), the same in Omega Ruby
            {0x59C2D4, 0x587170}, // 16 constant bytes (1, 1, 1, 0), the same in Omega Ruby
        };
        // the player's position: + 0xC64 in X, + 0x6F8 in Omega Ruby (as Grant's); the field's + 0xF0 object's + 0xAB8 is
        // its + 0xAA4 (X's DllField 0x1029F19C aligned with Omega Ruby's 0x102E06BC, 0.98: + 0xAB4 / + 0xAB8 / + 0xAC0 each
        // - 0x14; both read the member's + 0x94 the same way; its + 0x98, read here, inferred the same, not seen in Omega
        // Ruby); run vA1 (31 October) read the member null at + 0x4A4 once the dew drops loaded
        p.Words = {{0x390, 0xE2800B03, 0xE2800B01, "the player's position: add #0xC00 -> #0x400"},
                   {0x394, 0xE2800064, 0xE2800FBE, "the player's position: add #0x64 -> #0x2F8"},
                   {0x4A4, 0xE5900AB8, 0xE5900AA4, "the field's + 0xF0 object's + 0xAB8 -> + 0xAA4"},
                   {0xE90, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"}};
        // its dew drops' models and materials (dew01-dew15, leafdew1-4): X's a/1/1/0, opened by Res_OpenArchive (0xCB0,
        // `mov r2, #0x6E`); Omega Ruby's a/1/1/0 is another file, so its 17 resources (ids 0x35-0x45) stayed unresolved
        // and the dew drops' build read null (runs vI1, vD1, 30 October)
        p.Archive = 0x6E; // a/1/1/0
        p.ArchiveWords = {0xCB0};
        p.Sounds = {{0x4D8, 0xA004E}};
        p.Vtables = {0x2204};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0xF4C, 0x46C}}; // X's kind 2 case: the object 0x46C bytes
        return p;
    }();
    return port;
}

const XyModulePort& XyGymFighting()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymFighting";
        p.Name = "DllXyGymFighting";
        // X's .code -> Omega Ruby's: the same functions (xy_import_match.py), then by their callers' copies (xy_callee_map.py)
        p.Static = Merged(SharedStatic(), {
            {0x12B27C, 0x12BB54}, {0x108CF0, 0x108CB0}, {0x109498, 0x109434}, {0x140170, 0x12FD48},
            {0x4AAA70, 0x4E3674}, // near, 0.93 (the dust clouds' port maps it the same)
            {0x3D0A20, 0x3F1924}, // 10 callers agree
            // called by the copies below
            {0x425B1C, 0x44FA8C}, {0x425BD4, 0x44FB44}, {0x4AB150, 0x4E3FDC}, {0x3D1DE0, 0x3F2E04},
            {0x3D149C, 0x3F2354}, // 1.00, the next 0.62
        });
        p.Field = Merged(SharedField(), {
            {0x1027DBB8, 0x102C13C8}, // 3 callers agree
            {0x102832E4, 0x102D1644},
            {0x1027F5CC, 0x102CDFD0}, // the zone's character list (Viola's port maps it the same)
            {0x1027F4C4, 0x102C8EF8}, // the same function (called by a copy)
            // the DllField class's methods Omega Ruby has the same (its vtable 0x1034946C: slots 0-3 and 5)
            {0x1022B3FC, 0x10268A20}, {0x1022B3EC, 0x10268A10},
            {0x1022B0FC, 0x102688BC}, {0x1022B108, 0x102688C8}, {0x1022B104, 0x102688C4},
        });
        p.Thunks = {Request};
        p.Words = {LoaderSize(0xFD4),
                   // the field's members Omega Ruby keeps elsewhere (prototype/xy_module_layout.py --words)
                   {0x2B4, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   {0x338, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   {0x8C0, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   {0xEE4, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"}};
        p.Archive = 0x5C; // a/0/9/2
        p.ArchiveWords = {0xEA4};
        // the DllField class's update, X's own, and the X-own DllField functions it calls (their callers all X's own)
        p.Copies = {
            {0x1022B10C, 736, "the class's update (slot 4) and its literals, up to slot 1 (0x1022B3EC): Omega Ruby's 0x102688CC is another (0.32)"},
            {0x1027B22C, 156, "called by it"},
            {0x1027B1A8, 116, "called by it, with a sound id"},
            {0x1027B138, 112, "called by it, with a sound id"},
            {0x1027A0DC, 268, "called by it, and its literals"},
        };
        p.CopySounds = {0xA000C, 0xA000D, 0xA000E, 0xA000F};
        // X DllField's import stubs the copies call, with what each reaches (X's linked program)
        p.FieldStubs = {
            {0x101F91F8, 0x425B1C}, {0x101F9200, 0x14F220}, {0x101F9A80, 0x425BD4}, {0x101FA230, 0x4AB150},
            {0x101FA510, 0x4AAA70}, {0x101FA530, 0x3D149C}, {0x101FA538, 0x3D1DE0},
            {0x101FADF8, 0x1078A4C8}, {0x101FAE00, 0x1078ADB4}, {0x101FAE08, 0x1078A410}, {0x101FAE10, 0x1078AD64},
            {0x101FAE18, 0x1078ADDC}, // her module's own functions
        };
        p.XLinked = 0x1078A000;
        p.FieldAnchor = 0x1027F5CC;
        p.FieldVtables = {{0x102FE8A4, {0x1022B3FC, 0x1022B3EC, 0x1022B0FC, 0x1022B108, 0x1022B10C, 0x1022B104}}};
        p.Vtables = {0x2114};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0xF60, 0x180}}; // X's kind 0 case: the object 0x180 bytes
        return p;
    }();
    return port;
}

const XyModulePort& XyGymGrass()
{
    static const XyModulePort port = [] {
        XyModulePort p;
        p.XName = "DllUSGymGrass";
        p.Name = "DllXyGymGrass";
        // X's .code -> Omega Ruby's: the same functions (xy_import_match.py), then read
        p.Static = Merged(SharedStatic(), {
            {0x36FD78, 0x3878F8}, // as Viola's (6 callers agree)
            {0x379DB8, 0x39181C}, {0x37E388, 0x395C70}, {0x37E398, 0x395C80}, {0x37E444, 0x395D2C},
            {0x4AF324, 0x4E9E1C}, // BadgeGetFlag's: the same bytes (oras-engine --badges 16 lets it take 8-15)
            // called by the copies below
            {0x144EB4, 0x144CA8}, {0x371888, 0x3891DC},
        });
        p.Field = Merged(SharedField(), {
            {0x10277E64, 0x4CE280}, // 4 callers agree (moved to the .code, as the dust clouds' port maps it)
        });
        p.Thunks = {Request};
        p.Words = {LoaderSize(0x8DC),
                   {0x4E8, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   {0x818, 0xE5900050, 0xE5900060, "the field's + 0x50 -> + 0x60"},
                   // Ramos's badge read by the module: X's 3, Kalos's 11 (XyMoveBadges); the player's status at the game
                   // data's + 0x3418 in Omega Ruby (+ 0x3410 in X: 31 and 33 places read it so; + 0x418 is no ARM
                   // immediate, so the pair is Omega Ruby's own, + 0x3400 then + 0x18)
                   {0x4E4, 0xE3A01003, 0xE3A0100B, "Ramos's badge: mov r1, #3 -> #11"},
                   {0x4F4, 0xE2800A03, 0xE2800B0D, "the status: add #0x3000 -> #0x3400"},
                   {0x4F8, 0xE2800E41, 0xE2800018, "the status: add #0x410 -> #0x18"},
                   // the game data's + 0x12E4 + 4 read when it starts (compared with 4): + 0x12EC in Omega Ruby (`add #0x1000;
                   // add #0x2E4` in 47 places of X's code, `add #0x1000; add #0x2EC` in 23 of Omega Ruby's and no + 0x12E4)
                   {0x828, 0xE2800FB9, 0xE2800FBB, "the game data's + 0x12E4 -> + 0x12EC"},
                   {0x860, 0xE2800FB9, 0xE2800FBB, "the game data's + 0x12E4 -> + 0x12EC"}};
        p.Archive = 0x93; // a/1/4/7
        p.ArchiveWords = {0x7D8};
        // X's own: no copy in Omega Ruby (searched: no function calls slot 0x28 then sets bit 0x800; the two DllField
        // ones are called by X-own code only, xy_callee_map.py)
        p.Copies = {
            {0x37022C, 52, "a model's sub-object (its slot 0x28) given a value at + 0x4C, then + 0x88's bit 0x800 set"},
            {0x102CD5D0, 12, "a DllField getter: [[r0 + 0x1C] + 4 r1]"},
            {0x102CD908, 344, "a DllField function building a model (its literal pool included)"},
        };
        p.FieldStubs = {{0x101F9200, 0x14F220}, {0x101F9218, 0x122920}, {0x101FCBF0, 0x371888}};
        p.Vtables = {0x1074};
        p.Slots = 13;
        p.InsertSlot = 12;
        p.InsertValue = 0x4D8734; // Util_ReturnZero_004D8734
        p.Classes = {{0x894, 0x44C}}; // X's kind 4 case: the object 0x44C bytes
        return p;
    }();
    return port;
}

}
