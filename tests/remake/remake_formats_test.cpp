// The remake tooling's readers on data built here to each format's layout: a
// small DS cartridge (header, NitroFS directories and names, allocation
// table) holding a NARC whose members are an LZ10-compressed NSBMD and an
// NSBTX; LZ10 and LZ11 streams written by hand; and damaged data, which
// must be refused with an error, never read past its end.
#include "FormatSniffer.h"
#include "Inventory.h"
#include "Narc.h"
#include "NdsRom.h"
#include "NitroCompression.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void Put16(Bytes& b, size_t at, uint16_t v) { b[at] = v & 0xFF; b[at + 1] = v >> 8; }
static void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b[at + i] = (v >> (8 * i)) & 0xFF; }
static void Append(Bytes& b, const std::string& s) { b.insert(b.end(), s.begin(), s.end()); }
static void Align4(Bytes& b) { while (b.size() % 4) b.push_back(0); }

// a NARC of the given members, no names
static Bytes MakeNarc(const std::vector<Bytes>& members)
{
    Bytes btaf(12 + members.size() * 8, 0), gmif;
    Append(gmif, "GMIF"); gmif.resize(8);
    uint32_t off = 0;
    Bytes data;
    for (size_t i = 0; i < members.size(); i++)
    {
        Put32(btaf, 12 + i * 8, off);
        data.insert(data.end(), members[i].begin(), members[i].end());
        off += (uint32_t)members[i].size();
        Put32(btaf, 16 + i * 8, off);
        while (data.size() % 4) { data.push_back(0xFF); off++; }
    }
    memcpy(btaf.data(), "BTAF", 4); Put32(btaf, 4, (uint32_t)btaf.size()); Put16(btaf, 8, (uint16_t)members.size());
    Bytes btnf(16, 0); memcpy(btnf.data(), "BTNF", 4); Put32(btnf, 4, 16); Put32(btnf, 8, 4); Put16(btnf, 14, 1);
    Put32(gmif, 4, (uint32_t)(8 + data.size()));
    gmif.insert(gmif.end(), data.begin(), data.end());
    Bytes n(16, 0); memcpy(n.data(), "NARC", 4); Put16(n, 4, 0xFFFE); Put16(n, 6, 0x0100); Put16(n, 12, 16); Put16(n, 14, 3);
    n.insert(n.end(), btaf.begin(), btaf.end()); n.insert(n.end(), btnf.begin(), btnf.end()); n.insert(n.end(), gmif.begin(), gmif.end());
    Put32(n, 8, (uint32_t)n.size());
    return n;
}

// "ABCABCABCABC": three literals, then a reference 3 back, 9 long
static Bytes Lz10() { return {0x10, 12, 0, 0, 0x10, 'A', 'B', 'C', 0x60, 0x02}; }
static Bytes Lz11Short() { return {0x11, 12, 0, 0, 0x10, 'A', 'B', 'C', 0x80, 0x02}; }
// "A" then 20 more: a reference 1 back, 20 long, in LZ11's 3-byte form
static Bytes Lz11Long() { return {0x11, 21, 0, 0, 0x40, 'A', 0x00, 0x30, 0x00}; }

// a cartridge: root/fielddata/land_data.narc (file 0), root/readme.txt (file 1),
// and file 2 unnamed (an overlay)
static Bytes MakeRom(const Bytes& narc)
{
    Bytes rom(0x200, 0);
    memcpy(rom.data(), "POKEMON PL", 10); memcpy(rom.data() + 0x0C, "CPUE", 4);
    // name table: root (dir 0xF000) and fielddata (0xF001)
    Bytes fnt(16, 0);
    // root's names: "readme.txt" (file 1), dir "fielddata" -> 0xF001
    Bytes root; root.push_back(10); Append(root, "readme.txt"); root.push_back(0x80 | 9); Append(root, "fielddata"); root.push_back(0x01); root.push_back(0xF0); root.push_back(0);
    Bytes sub; sub.push_back(14); Append(sub, "land_data.narc"); sub.push_back(0);
    Put32(fnt, 0, 16); Put16(fnt, 4, 1); Put16(fnt, 6, 2);                         // root: names from file 1, 2 directories
    Put32(fnt, 8, (uint32_t)(16 + root.size())); Put16(fnt, 12, 0); Put16(fnt, 14, 0xF000); // fielddata: names from file 0
    fnt.insert(fnt.end(), root.begin(), root.end()); fnt.insert(fnt.end(), sub.begin(), sub.end());
    const uint32_t fntAt = (uint32_t)rom.size();
    rom.insert(rom.end(), fnt.begin(), fnt.end()); Align4(rom);
    const uint32_t fatAt = (uint32_t)rom.size();
    rom.resize(rom.size() + 3 * 8, 0);
    auto place = [&](int id, const Bytes& data) {
        Align4(rom);
        Put32(rom, fatAt + id * 8, (uint32_t)rom.size());
        rom.insert(rom.end(), data.begin(), data.end());
        Put32(rom, fatAt + id * 8 + 4, (uint32_t)rom.size());
    };
    place(0, narc);
    Bytes text; Append(text, "hello");
    place(1, text);
    place(2, Bytes(8, 0x77));
    Put32(rom, 0x40, fntAt); Put32(rom, 0x44, (uint32_t)fnt.size()); Put32(rom, 0x48, fatAt); Put32(rom, 0x4C, 3 * 8);
    return rom;
}

int main()
{
    // compression
    check(LzDecompress(Lz10()) == Bytes({'A','B','C','A','B','C','A','B','C','A','B','C'}), "LZ10: literals and a back-reference");
    check(LzDecompress(Lz11Short()) == LzDecompress(Lz10()), "LZ11: short reference");
    check(LzDecompress(Lz11Long()) == Bytes(21, 'A'), "LZ11: 3-byte reference (length 17 and up)");
    bool refused = false;
    try { Bytes bad = Lz10(); bad.resize(7); LzDecompress(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "LZ: truncated data refused");
    refused = false;
    try { LzDecompress({0x10, 4, 0, 0, 0x80, 0x00, 0x05}); } catch (const FormatError&) { refused = true; }
    check(refused, "LZ: reference before the start refused");

    // archive
    Bytes model; Append(model, "BMD0"); model.resize(12, 0); // 12 bytes, as Lz10 decompresses
    Bytes modelLz = {0x10, 12, 0, 0, 0x00, 'B', 'M', 'D', '0', 0, 0, 0, 0, 0x00, 0, 0, 0, 0};
    Bytes tex; Append(tex, "BTX0"); tex.resize(20, 0);
    const Bytes narcBytes = MakeNarc({modelLz, tex});
    const Narc narc(narcBytes);
    check(narc.Count() == 2 && narc.Member(1) == tex, "NARC: members read");
    check(DeepKind(narc.Member(0)) == "lz10:nsbmd" && DeepKind(narc.Member(1)) == "nsbtx", "kinds: LZ10-compressed model, texture");
    check(LzDecompress(narc.Member(0)) == model, "NARC member decompressed");
    refused = false;
    try { Bytes bad = narcBytes; bad.resize(40); Narc n(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "NARC: truncated archive refused");

    // cartridge
    WriteFile("synthetic.nds", MakeRom(narcBytes)); // for trying remake_tool on it
    const NdsRom rom(MakeRom(narcBytes));
    check(rom.Title() == "POKEMON PL" && rom.GameCode() == "CPUE", "cartridge: title and game code");
    check(rom.Files().size() == 3, "cartridge: 3 files");
    const NdsFile* f = rom.Find("fielddata/land_data.narc");
    check(f && f->Id == 0 && rom.Read(*f) == narcBytes, "NitroFS: file in a subdirectory found by path, read whole");
    check(rom.Find("readme.txt") && rom.Find("readme.txt")->Id == 1, "NitroFS: file in the root");
    check(rom.Files()[2].Path == "overlay/2", "NitroFS: an unnamed file listed as an overlay");
    const std::string json = InventoryJson(rom);
    check(json.find("\"path\": \"fielddata/land_data.narc\"") != std::string::npos && json.find("\"members\": 2") != std::string::npos &&
          json.find("\"lz10:nsbmd\": 1") != std::string::npos && json.find("\"nsbtx\": 1") != std::string::npos,
          "inventory: the archive and its members' kinds");
    refused = false;
    try { Bytes bad = MakeRom(narcBytes); Put32(bad, 0x48, 0xFFFFFF0); NdsRom r(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "cartridge: allocation table outside the image refused");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    if (!ok) puts(json.c_str());
    return ok ? 0 : 1;
}
