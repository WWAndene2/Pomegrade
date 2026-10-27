#include "OrasXyBackgrounds.h"
#include "BchUpgrade.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace remake
{

namespace
{

// where each game keeps the tables (OrasXyBackgrounds.h): the .code's (linked at 0x100000) and the modules', a module's
// address its file offset past its base (prototype/cro_link.py's placement); the counts as the tables' spans give them
struct BackgroundTables
{
    uint32_t Records, Values;                                    // .code
    uint32_t Base, Backdrops, LightA, LightB, Grounds, Sets, Sceneries; // DllBackGround
    size_t Entries, SetCount, SceneryCount;
    uint32_t BattleBase, Platforms;                              // DllBattle
    size_t PlatformCount;
};
constexpr BackgroundTables OrasTables = {0x595CD0, 93, 0x10000000, 0x10005000, 0x10005174, 0x100062E4, 0x10007454, 0x100075C8, 0x100075D4, 93, 1, 1, 0x10025000, 0x10106074, 92};
// X: the second light table is the first + 0xC30 (`add sl, fp, #0xC30` at 0x10003E04, one literal for both); 13 sets
// and 16 sceneries before the table's next data
constexpr BackgroundTables XTables = {0x551C04, 65, 0x10000000, 0x10005000, 0x10005104, 0x10005D34, 0x10006964, 0x10006A68, 0x10006B04, 65, 13, 16, 0x10022000, 0x100F8C3C, 65};
constexpr uint32_t CodeBase = 0x100000;

Bytes Span(const Bytes& data, uint32_t at, size_t size)
{
    if (at + size > data.size()) throw FormatError("background tables: a table past its file's end");
    return Bytes(data.begin() + at, data.begin() + at + size);
}

// the entry's index in `table` (`size` bytes each), appended when no entry is the same; never 0, which loads none
uint8_t EntryIndex(Bytes& table, size_t size, const Bytes& entry)
{
    for (size_t k = 1; k * size < table.size(); k++)
        if (std::equal(entry.begin(), entry.end(), table.begin() + k * size)) return (uint8_t)k;
    if (table.size() / size >= 256) throw FormatError("background tables: a table past 256 entries (a record byte)");
    table.insert(table.end(), entry.begin(), entry.end());
    return (uint8_t)(table.size() / size - 1);
}

void Put32(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k)); }

// two BCH files (any version) holding the same models and textures as the reader sees them: names, materials (textures,
// render flags, texture units, mappers), meshes (material, layer, every vertex and triangle) and textures (size, format,
// data), all equal
bool SameBch(const Bytes& a, const Bytes& b)
{
    const Bch x = Bch::Read(a), y = Bch::Read(b);
    if (x.Models.size() != y.Models.size() || x.Textures.size() != y.Textures.size()) return false;
    for (size_t m = 0; m < x.Models.size(); m++)
    {
        const BchModel &p = x.Models[m], &q = y.Models[m];
        if (p.Name != q.Name || p.Materials.size() != q.Materials.size() || p.Meshes.size() != q.Meshes.size()) return false;
        for (size_t k = 0; k < p.Materials.size(); k++)
        {
            const BchMaterial &u = p.Materials[k], &v = q.Materials[k];
            if (u.Name != v.Name || u.Flags != v.Flags || u.TextureUnits != v.TextureUnits) return false;
            for (int t = 0; t < 3; t++) if (u.Texture[t] != v.Texture[t] || u.Mappers[t] != v.Mappers[t]) return false;
        }
        for (size_t k = 0; k < p.Meshes.size(); k++)
        {
            const BchMesh &u = p.Meshes[k], &v = q.Meshes[k];
            if (u.Material != v.Material || u.Layer != v.Layer || u.Triangles != v.Triangles || u.Vertices.size() != v.Vertices.size()) return false;
            for (size_t i = 0; i < u.Vertices.size(); i++)
                if (std::memcmp(&u.Vertices[i], &v.Vertices[i], sizeof(BchVertex))) return false;
        }
    }
    for (const BchTexture& t : x.Textures)
    {
        const auto o = std::find_if(y.Textures.begin(), y.Textures.end(), [&](const BchTexture& u) { return u.Name == t.Name; });
        if (o == y.Textures.end() || o->Width != t.Width || o->Height != t.Height || o->Format != t.Format || o->Data != t.Data) return false;
    }
    return true;
}

}

Bytes XyBackgrounds::Tables() const
{
    Bytes out = {'X', 'Y', 'B', 'G'};
    for (const Bytes* t : {&Records, &Sets, &Sceneries, &Backdrops, &LightA, &LightB, &Grounds, &Platforms})
    {
        for (int k = 0; k < 4; k++) out.push_back((uint8_t)(t->size() >> (8 * k)));
        out.insert(out.end(), t->begin(), t->end());
    }
    return out;
}

int XyBackgroundValue(XyBackgroundNumbers& numbers, int x, bool narrow)
{
    if (x == 0) return 0;
    if (x < 0 || x >= (int)XTables.Values) throw FormatError("X/Y background value " + std::to_string(x) + " is not one of X's");
    auto known = numbers.Bank.find(x);
    if (known == numbers.Bank.end())
    {
        const int n = 1 + (int)numbers.Bank.size();
        if (n >= XyBackgroundBankScript) throw FormatError("background values: past the 127 of X/Y's bank");
        known = numbers.Bank.emplace(x, n).first;
    }
    return narrow ? known->second : XyBackgroundBankScript + known->second;
}

XyBackgrounds AddXyBackgrounds(OrasWorkspace& ws, N3dsRom& xRom, const XyBackgroundNumbers& numbers, std::vector<std::string>& log)
{
    const Bytes oCode = ws.Rom().Code(), oBg = ws.Rom().Read("DllBackGround.cro"), oBattle = ws.Rom().Read("DllBattle.cro");
    const Bytes xCode = xRom.Code(), xBg = xRom.Read("DllBackGround.cro"), xBattle = xRom.Read("DllBattle.cro");
    const BackgroundTables& o = OrasTables;
    const BackgroundTables& x = XTables;

    XyBackgrounds r;
    // Omega Ruby's tables as the game has them
    Bytes recordsA = Span(oCode, o.Records - CodeBase, 2 * o.Values), recordsB = Span(oCode, o.Records - CodeBase + 2 * o.Values, 5 * o.Values);
    recordsA.resize(2 * XyBackgroundRecords, 0);
    recordsB.resize(5 * XyBackgroundRecords, 0);
    r.Sets = Span(oBg, o.Sets - o.Base, 12 * o.SetCount);
    r.Sceneries = Span(oBg, o.Sceneries - o.Base, 0x18 * o.SceneryCount);
    r.Backdrops = Span(oBg, o.Backdrops - o.Base, 4 * o.Entries);
    r.LightA = Span(oBg, o.LightA - o.Base, 0x30 * o.Entries);
    r.LightB = Span(oBg, o.LightB - o.Base, 0x30 * o.Entries);
    r.Grounds = Span(oBg, o.Grounds - o.Base, 4 * o.Entries);
    r.Platforms = Span(oBattle, o.Platforms - o.BattleBase, 4 * o.PlatformCount);

    // X's members, converted and appended to a/1/3/6 (or Omega Ruby's same member), each once
    const Garc xFiles(xRom.Read("a/1/4/6"));
    Garc& files = ws.Edited("a/1/3/6");
    const Garc& gameFiles = ws.Original("a/1/3/6");
    std::vector<Bytes> gamePlain;
    for (size_t i = 0; i < gameFiles.Count(); i++) gamePlain.push_back(IsLzCompressed(gameFiles.Sub(i)) ? LzDecompress(gameFiles.Sub(i)) : gameFiles.Sub(i));
    std::map<uint32_t, uint32_t> memberOf;
    size_t shared = 0, added = 0;
    const auto member = [&](uint32_t xMember) -> uint32_t {
        const auto known = memberOf.find(xMember);
        if (known != memberOf.end()) return known->second;
        if (xMember >= xFiles.Count()) throw FormatError("X's a/1/4/6 has no member " + std::to_string(xMember));
        const Bytes& raw = xFiles.Sub(xMember);
        const bool compressed = IsLzCompressed(raw);
        Bytes plain = compressed ? LzDecompress(raw) : raw;
        const std::string where = "X's a/1/4/6 member " + std::to_string(xMember);
        if (Bch::Is(plain))
        {
            // a lone model (a ground) the same as one of Omega Ruby's, every part of it as the reader sees it, is Omega Ruby's
            // file (owner, 22 October: shared only when 100% the same). X's forest ground, the same as Omega Ruby's member 13,
            // converted drew a raised block over the backdrop (runs bgf1-bgf3, bgig: the difference not found in the file);
            // Omega Ruby's own draws right (run bgf_ix)
            for (size_t i = 0; i < gamePlain.size(); i++)
                if (Bch::Is(gamePlain[i]) && SameBch(plain, gamePlain[i])) { shared++; return memberOf[xMember] = (uint32_t)i; }
            plain = BchUpgradeTo21(plain);
        }
        else if (plain.size() >= 4 && plain[0] == 'B' && plain[1] == 'G')
        {
            BinLinker bg = BinLinker::Read(plain, "BG");
            for (Bytes& f : bg.Files)
                if (Bch::Is(f)) f = BchUpgradeTo21(f);
                else throw FormatError(where + ": a file of its BG container is no BCH");
            plain = bg.Write();
        }
        else throw FormatError(where + ": neither a BCH nor a BG container");
        uint32_t at = 0;
        const auto same = std::find(gamePlain.begin(), gamePlain.end(), plain);
        if (same != gamePlain.end()) { at = (uint32_t)(same - gamePlain.begin()); shared++; }
        else
        {
            at = (uint32_t)files.Count();
            files.Set(at, compressed ? Lz11Compress(plain) : plain);
            added++;
        }
        return memberOf[xMember] = at;
    };
    // an X entry whose first word is a member, with that member Omega Ruby's
    const auto withMember = [&](const Bytes& from, uint32_t at, size_t size) {
        Bytes e = Span(from, at, size);
        Put32(e, 0, member(U32(e, 0)));
        return e;
    };
    const auto check = [&](int value, const char* what, size_t index, size_t count) {
        if (index >= count) throw FormatError("X/Y background " + std::to_string(value) + ": its " + what + " " + std::to_string(index) + " is past X's table");
    };

    char line[200];
    for (const auto& [v, n] : numbers.Bank)
    {
        const int next = XyBackgroundBankRecord + n;
        if (v <= 0 || v >= (int)x.Values || n <= 0 || next >= XyBackgroundRecords) throw FormatError("X/Y background value " + std::to_string(v) + " or its number is out of range");
        const uint8_t* a = &xCode[x.Records - CodeBase + 2 * v];
        const uint8_t* b = &xCode[x.Records - CodeBase + 2 * x.Values + 5 * v];
        uint8_t ground = a[0], platform = b[0], backdrop = b[1], set = b[2];
        if (ground)
        {
            check(v, "ground", ground, x.Entries);
            ground = EntryIndex(r.Grounds, 4, withMember(xBg, x.Grounds - x.Base + 4 * ground, 4));
        }
        if (platform)
        {
            check(v, "platform", platform, x.PlatformCount);
            platform = EntryIndex(r.Platforms, 4, withMember(xBattle, x.Platforms - x.BattleBase + 4 * platform, 4));
        }
        if (backdrop)
        {
            // the backdrop's member and its two light tables are one entry, at one index in the three tables
            check(v, "backdrop", backdrop, x.Entries);
            const Bytes file = withMember(xBg, x.Backdrops - x.Base + 4 * backdrop, 4);
            const Bytes lightA = Span(xBg, x.LightA - x.Base + 0x30 * backdrop, 0x30), lightB = Span(xBg, x.LightB - x.Base + 0x30 * backdrop, 0x30);
            uint8_t at = 0;
            for (size_t k = 1; k * 4 < r.Backdrops.size() && !at; k++)
                if (std::equal(file.begin(), file.end(), r.Backdrops.begin() + 4 * k) && std::equal(lightA.begin(), lightA.end(), r.LightA.begin() + 0x30 * k) &&
                    std::equal(lightB.begin(), lightB.end(), r.LightB.begin() + 0x30 * k))
                    at = (uint8_t)k;
            if (!at)
            {
                if (r.Backdrops.size() / 4 >= 256) throw FormatError("background tables: past 256 backdrops");
                at = (uint8_t)(r.Backdrops.size() / 4);
                r.Backdrops.insert(r.Backdrops.end(), file.begin(), file.end());
                r.LightA.insert(r.LightA.end(), lightA.begin(), lightA.end());
                r.LightB.insert(r.LightB.end(), lightB.begin(), lightB.end());
            }
            backdrop = at;
        }
        if (set)
        {
            // its sceneries first (the pairs at +4: a scenery, its weight out of 100)
            check(v, "scenery set", set, x.SetCount);
            Bytes e = withMember(xBg, x.Sets - x.Base + 12 * set, 12);
            for (int p = 0; p < 3; p++)
                if (const uint8_t s = e[4 + 2 * p])
                {
                    check(v, "scenery", s, x.SceneryCount);
                    e[4 + 2 * p] = EntryIndex(r.Sceneries, 0x18, withMember(xBg, x.Sceneries - x.Base + 0x18 * s, 0x18));
                }
            set = EntryIndex(r.Sets, 12, e);
        }
        recordsA[2 * next] = ground;
        recordsA[2 * next + 1] = a[1];
        const uint8_t record[5] = {platform, backdrop, set, b[3], b[4]};
        std::copy(record, record + 5, recordsB.begin() + 5 * next);
        snprintf(line, sizeof line, "battle background: X/Y's %d as bank %d, record %d (ground %u, platform %u, backdrop %u, scenery set %u)", v, n, next, ground, platform, backdrop, set);
        log.push_back(line);
    }
    r.Records = recordsA;
    r.Records.insert(r.Records.end(), recordsB.begin(), recordsB.end());
    snprintf(line, sizeof line, "battle backgrounds: %zu X/Y values in their bank, a/1/3/6 +%zu members (%zu of X's the same as Omega Ruby's)", numbers.Bank.size(), added, shared);
    log.push_back(line);
    return r;
}

}
