#include "OrasXyEffectPack.h"

#include "Bch.h"
#include "BchUpgrade.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"

#include <iterator>

namespace remake
{

namespace
{
uint32_t Branch(uint32_t cond, uint32_t from, uint32_t to, bool link)
{
    return cond << 28 | (link ? 0x0B000000u : 0x0A000000u) | (((to - from - 8) >> 2) & 0xFFFFFF);
}

// X's grass01-03: {model, animation} in X's a/0/6/6, for the grass kinds 1-3 (X's tables, DllField + 0xF3678 / + 0xF368C)
constexpr uint32_t XyFlowerGrass[3][2] = {{0x32, 0x31}, {0x40, 0x3F}, {0x4E, 0x4D}};
// Omega Ruby's grass tables (DllField + 0xF5C64 models, + 0xF5C7C animations, 6 kinds; its read-only segment), and what
// they name for kinds 1-3
constexpr uint32_t GrassModels = 0x10337C64, GrassAnimations = 0x10337C7C;
constexpr uint32_t OrasGrassModel = 0x2A, OrasGrassAnimation = 0x29;

uint32_t LastId(bool flowerGrass) { return flowerGrass ? XyFlowerGrassFirstId + 2 * 3 - 1 : XySwirlFirstId + 2; }
}

Bytes XyEffectsArchive(const Bytes& orasGarc, const Bytes& xGarc, bool flowerGrass)
{
    // both games' archives hold one TM of the field's effect files (X's a/0/6/6: 204, Omega Ruby's a/0/6/4: 189), the
    // effect pack's id the entry's number (the swirl's three BCH found in X's by their bytes as X loaded them, run xd2)
    Garc o(orasGarc);
    const Garc x(xGarc);
    const auto plain = [](const Bytes& m) { return !m.empty() && m[0] == 0x11 ? LzDecompress(m) : m; };
    const bool compressed = !o.Sub(0).empty() && o.Sub(0)[0] == 0x11;
    BinLinker ot = BinLinker::Read(plain(o.Sub(0)), "TM");
    const BinLinker xt = BinLinker::Read(plain(x.Sub(0)), "TM");
    if (o.Count() != 1 || ot.Files.size() != XySwirlFirstId || xt.Files.size() != 204)
        throw FormatError("the effects' archives: not Omega Ruby's 189 files and X's 204");
    std::vector<uint32_t> ids = {XySwirlFirstId, XySwirlFirstId + 1, XySwirlFirstId + 2};
    if (flowerGrass)
        for (const auto& [model, animation] : XyFlowerGrass)
        {
            ids.push_back(model);
            ids.push_back(animation);
        }
    for (const uint32_t id : ids)
    {
        const Bytes& f = xt.Files.at(id);
        if (!Bch::Is(f) || f[4] != 0x07) throw FormatError("the effects' archives: X's file " + std::to_string(id) + " not a BCH 0x07");
        ot.Files.push_back(BchUpgradeTo21(f));
    }
    if (ot.Files.size() != LastId(flowerGrass) + 1) throw FormatError("the effects' archives: the new files are not at their ids");
    const Bytes tm = ot.Write();
    o.Set(0, compressed ? Lz11Compress(tm) : tm);
    return o.Write();
}

std::vector<CodeWord> XyEffectPackPatchAt(uint32_t cave, bool flowerGrass)
{
    cave = (cave + 3) & ~3u;
    std::vector<CodeWord> w;
    // the field's effect pack (its builder 0x1026D628, its lookup 0x102AA3A8): ids 8-0xAC (0xA5 entries of 0x4C bytes, the
    // TM a/0/6/4's entries at their own numbers) -> 8 to the last new id
    const uint32_t entries = LastId(flowerGrass) - 7;
    w.push_back({0x1026D680, 0xE3A040A5, 0xE3A04000 | entries, "the effect pack: its entries, 0xA5 -> X/Y's"});
    w.push_back({0x1026D7D4, 0x3104, 8 + 0x4C * entries, "the effect pack: its entries' allocation"});
    w.push_back({0x102AA3AC, 0xE35200A4, 0xE3520000 | (entries - 1), "the effect pack's lookup: its last id"});
    // its builder skips Omega Ruby's own entries past the game's (0xAD-0xBC: 7 models, 4 visibility, 2 skeletal and 2
    // material animations the game never loads as effects): loaded, they left two of the scene's animations without a
    // resource, read near address 0 every frame on X/Y's zones (PCs 0x38A28C, 0x4CCDF4: about 5,000 reads a run; bisected
    // 26 October, runs sc1x-sc3x: none without the pack grown). Their entries stay empty, as the lookup found them before
    // (+ 0x3C 0: none). The loop's `add r0, r4, #8` (the id) calls the routine, which goes on to the next entry for these
    constexpr uint32_t BuilderId = 0x1026D6E8, BuilderNext = 0x1026D79C;
    constexpr uint32_t SkipFirst = 8 + 0xA5, SkipLast = XySwirlFirstId - 1;
    static_assert(SkipLast - SkipFirst < 0x100, "the routine compares with an 8-bit immediate");
    w.push_back({BuilderId, 0xE2840008, Branch(0xE, BuilderId, cave, true), "the effect pack's builder: Omega Ruby's ids 0xAD-0xBC skipped"});
    const uint32_t skipCode[] = {
        0xE2840008,                                     // 00 add r0, r4, #8        the id (the game's word)
        0xE240C000 | SkipFirst,                         // 04 sub ip, r0, #0xAD
        0xE35C0000 | (SkipLast - SkipFirst),            // 08 cmp ip, #0xF
        0x812FFF1E,                                     // 0C bxhi lr               another id: loaded
        Branch(0xE, cave + 0x10, BuilderNext, false),   // 10 b the next entry
    };
    for (uint32_t k = 0; k < std::size(skipCode); k++) w.push_back({cave + 4 * k, 0, skipCode[k], "the effect pack's skipped ids (cave)"});
    return w;
}

std::vector<CodeWord> XyFlowerGrassTableWords(uint32_t room)
{
    std::vector<CodeWord> w;
    for (uint32_t k = 0; k < 3; k++)
    {
        w.push_back({GrassModels + room + 4 * (k + 1), OrasGrassModel, XyFlowerGrassFirstId + 2 * k, "the grass kinds' models: X/Y's flower grass"});
        w.push_back({GrassAnimations + room + 4 * (k + 1), OrasGrassAnimation, XyFlowerGrassFirstId + 2 * k + 1, "the grass kinds' animations: X/Y's flower grass"});
    }
    return w;
}

}
