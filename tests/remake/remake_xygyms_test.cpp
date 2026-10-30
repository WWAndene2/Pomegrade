// X/Y's gym modules ported (OrasXyGymModules.h), on a module made here: its name, an import of each kind, a word, a sound
// id and its vtable given a slot, the relocations following it
#include "OrasXyGymModules.h"
#include <array>
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t Get(const Bytes& b, size_t at) { return (uint32_t)(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | (uint32_t)b[at + 3] << 24); }
static void Set(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k)); }
static void Field(Bytes& b, int index, uint32_t v) { Set(b, 0x80 + 4 * (size_t)index, v); }

// a CRO of the given segments (offset, size, type) at 0x100, with nothing else
static Bytes Module(const std::vector<std::array<uint32_t, 3>>& segments, size_t size)
{
    Bytes b(size, 0);
    Set(b, 0x80, 0x304F5243);
    Field(b, 18, 0x100);
    Field(b, 19, (uint32_t)segments.size());
    for (size_t i = 0; i < segments.size(); i++)
        for (int k = 0; k < 3; k++) Set(b, 0x100 + 12 * i + 4 * k, segments[i][k]);
    return b;
}

int main()
{
    // the static modules: code from 0x100000 (X) and 0x100000 (Omega Ruby), as static.crs gives absolute offsets
    XyGameModules x{Module({{0x100000, 0x1000, 0}}, 0x200), {}, 0x10200000}, oras{Module({{0x100000, 0x2000, 0}}, 0x200), {}, 0x10300000};
    x.Field = Module({{0x180, 0x80, 0}}, 0x300);
    oras.Field = Module({{0x180, 0x100, 0}}, 0x300);

    // the module: code 0x180-0x1A0, rodata 0x400-0x420 (the vtable's offset and type info at 0x400, slots 0x408-0x40C)
    Bytes m = Module({{0x180, 0x20, 0}, {0x400, 0x20, 1}}, 0x800);
    const std::string name = "DllUSGymTest";
    Field(m, 16, 0x600);
    Field(m, 17, (uint32_t)name.size() + 1);
    std::copy(name.begin(), name.end(), m.begin() + 0x600);
    // imports: |static| 0x100040 (tag: segment 0, offset 0x40) and DllField 0x10200190 (file 0x190: segment 0 + 0x10)
    const std::string st = "|static|", df = "DllField";
    std::copy(st.begin(), st.end(), m.begin() + 0x620);
    std::copy(df.begin(), df.end(), m.begin() + 0x630);
    Field(m, 28, 0x640);
    Field(m, 29, 2);
    Set(m, 0x640, 0x620); Set(m, 0x64C, 0x680); Set(m, 0x650, 1);
    Set(m, 0x654, 0x630); Set(m, 0x660, 0x688); Set(m, 0x664, 1);
    Set(m, 0x680, 0x40 << 4);
    Set(m, 0x688, 0x10 << 4);
    // a word, a sound id
    Set(m, 0x184, 0xE2800B03);
    Set(m, 0x188, 0xA0060);
    // internal relocations: the type info (0x404), the two slots (0x408, 0x40C), the constructor's pointer (code 0x18C ->
    // rodata + 8, the vtable)
    Field(m, 42, 0x700);
    Field(m, 43, 4);
    const auto reloc = [&](int i, uint32_t tag, uint8_t s, uint32_t addend) {
        Set(m, 0x700 + 12 * (size_t)i, tag); m[0x704 + 12 * (size_t)i] = 2; m[0x705 + 12 * (size_t)i] = s; Set(m, 0x708 + 12 * (size_t)i, addend);
    };
    reloc(0, 1 | 0x4 << 4, 1, 0x10);
    reloc(1, 1 | 0x8 << 4, 0, 0x4);
    reloc(2, 1 | 0xC << 4, 0, 0x8);
    reloc(3, 0 | 0xC << 4, 1, 0x8);

    XyModulePort p;
    p.XName = name;
    p.Name = "DllXyGymTest";
    p.Static = {{0x100040, 0x100840}};
    p.Field = {{0x10200190, 0x103001A0}};
    p.Words = {{0x184, 0xE2800B03, 0xE2800B01, "a word"}};
    p.Sounds = {{0x188, 0xA0060}};
    p.Vtables = {0x408};
    p.Slots = 2;
    p.InsertSlot = 1;
    p.InsertValue = 0x4D8734;
    const Bytes out = XyPortModule(m, x, oras, p, [](uint32_t id) { return id + 1; });

    check(std::string(out.begin() + 0x600, out.begin() + 0x60C) == "DllXyGymTest", "renamed");
    check(Get(out, 0x680) == (0x840u << 4), "the |static| import: Omega Ruby's tag (segment 0, + 0x840)");
    check(Get(out, 0x688) == (0x20u << 4), "the DllField import: Omega Ruby's tag (file 0x1A0: segment 0 + 0x20)");
    check(Get(out, 0x184) == 0xE2800B01 && Get(out, 0x188) == 0xA0061, "the word and the sound id changed");
    // the new vtable after the code (0x1A0): offset, type info, slots 0x1A8 (old 0), 0x1AC (the inserted), 0x1B0 (old 1)
    check(Get(out, 0x104) == 0x34, "the code segment grown to hold it (0x180 + 0x34)");
    check(Get(out, 0x1AC) == 0x4D8734, "the inserted slot holds its function");
    check(Get(out, 0x700) == (0x24u << 4) && Get(out, 0x70C) == (0x28u << 4) && Get(out, 0x718) == (0x30u << 4),
          "the type info and slot relocations write the new vtable (0x1A4, 0x1A8, 0x1B0)");
    check(out[0x729] == 0 && Get(out, 0x72C) == 0x28, "the constructor's pointer points at the new slot 0 (code + 0x28)");

    bool refused = false;
    try { XyModulePort q = p; q.Static.clear(); XyPortModule(m, x, oras, q, [](uint32_t id) { return id; }); }
    catch (const FormatError&) { refused = true; }
    check(refused, "an import not mapped is refused");
    // a thunk: the |static| import made a jump through a word after the code, its stub (code 0x190, the word at 0x194 the
    // import's batch writes) a branch to it
    {
        Bytes t = m;
        Set(t, 0x190, 0xE51FF004);
        Set(t, 0x684, 0x780);                       // the |static| import's batch
        Set(t, 0x780, 0 | 0x14 << 4); t[0x784] = 2; t[0x785] = 1; // its one relocation: segment 0 + 0x14 (0x194), the last
        XyModulePort q = p;
        q.Vtables.clear();
        q.Static.clear();
        q.Thunks = {{0x100040, 0x100900, {0xE3A02000}, "a third argument"}};
        const Bytes o = XyPortModule(t, x, oras, q, [](uint32_t id) { return id; });
        check(Get(o, 0x680) == (0x900u << 4), "a thunk's import resolves to Omega Ruby's function");
        check(Get(o, 0x1A0) == 0xE3A02000 && Get(o, 0x1A4) == 0xE51FF004 && Get(o, 0x1A8) == 0x100900,
              "the thunk after the code: its prefix, then a jump to the function");
        check(Get(o, 0x190) == (0xEA000000u | (((0x1A0u - 0x190u - 8) >> 2) & 0xFFFFFF)), "the import's stub branches to it");
    }

    // an archive request: X's mov r1, #0xae made Omega Ruby's number for it, 0x130, a/3/0/4
    {
        Bytes t = m;
        Set(t, 0x18C, 0xE3A010AE);
        XyModulePort q = p;
        q.Archive = 0xAE;
        q.ArchiveWords = {0x18C};
        const Bytes o = XyPortModule(t, x, oras, q, [](uint32_t id) { return id; }, XyGymArchiveNumber(0));
        check(XyGymArchiveNumber(0) == 0x130 && Get(o, 0x18C) == 0xE3A01E13, "the archive request: mov r1, #0x130 (0x13 rotated right by 28)");
        check(XyArchivePath(0x130) == "a/3/0/4" && XyArchivePath(0xAE) == "a/1/7/4", "an archive's path by its digits");
    }

    // Viola's request goes in r2 (Res_OpenArchive's own argument): mov r2, #0x6e made mov r2, #0x130, its register kept
    {
        Bytes t = m;
        Set(t, 0x18C, 0xE3A0206E);
        XyModulePort q = p;
        q.Archive = 0x6E;
        q.ArchiveWords = {0x18C};
        const Bytes o = XyPortModule(t, x, oras, q, [](uint32_t id) { return id; }, XyGymArchiveNumber(0));
        check(Get(o, 0x18C) == 0xE3A02E13, "the archive request in r2: mov r2, #0x130");
    }

    // DllField loading Grant's module: the default case's first word to the dispatcher, kind 1 to the loader, which jumps
    // to the module's handle + its constructor's offset
    const std::vector<CodeWord> lw = XyGymLoaderPatchAt(0x10385CAC);
    const auto word = [&](uint32_t address) -> const CodeWord* { for (const CodeWord& c : lw) if (c.Address == address) return &c; return nullptr; };
    const CodeWord* entry = word(0x10274210);
    check(entry && entry->Expected == 0xE1A01000 && entry->Value == (0xEA000000u | (((0x10385CACu - 0x10274210u - 8) >> 2) & 0xFFFFFF)),
          "the default case's first word (mov r1, r0) becomes a branch to the dispatcher");
    // the dispatcher: per gym a compare, r3 its record, a branch to the routine; then the default case's word and back
    const uint32_t n = (uint32_t)XyGyms().size(), backAt = 0x10385CAC + 12 * n, routine = backAt + 8, record = routine + 0x40;
    check(word(0x10385CAC) && word(0x10385CAC)->Value == 0xE3520001, "the dispatcher compares the kind with Grant's (1)");
    check(word(0x10385CB0) && word(0x10385CB0)->Value == (0x028F3F00u | (record - (0x10385CB0 + 8)) / 4), "r3 = Grant's record (addeq r3, pc, #)");
    check(word(0x10385CB4) && word(0x10385CB4)->Value == (0x0A000000u | (((routine - 0x10385CB4 - 8) >> 2) & 0xFFFFFF)), "beq the routine");
    const CodeWord* back = word(backAt + 4);
    check(word(backAt) && word(backAt)->Value == 0xE1A01000 && back && back->Value == (0xEA000000u | (((0x10274214u - (backAt + 4) - 8) >> 2) & 0xFFFFFF)),
          "any other kind: the default case's word, then back to it");
    check(word(routine + 0x0C) && word(routine + 0x0C)->Value == (0xEB000000u | (((0x10243C48u - (routine + 0x0C) - 8) >> 2) & 0xFFFFFF)),
          "the routine loads the module (the loader's stub)");
    check(word(record) && word(record)->Value == 0x544 && word(record + 4)->Value == 0x18, "Grant's record: constructor 0x544, object 0x18 bytes");
    const uint32_t path = record + word(record + 8)->Value;
    check(word(path) && word(path)->Value == 0x3A6D6F72 && word(path + 4)->Value == 0x6C6C442F, "its path: \"rom:/Dll...\"");
    return ok ? 0 : 1;
}
