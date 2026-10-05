// Builders of synthetic DS files for the remake tests, to the formats'
// documented layout (no game data): little-endian writers, a Nitro
// dictionary, and an NSBMD holding one quad model.
#ifndef REMAKE_TESTS_SYNTHETIC_FILES_H
#define REMAKE_TESTS_SYNTHETIC_FILES_H

#include "Bytes.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace synthetic
{
using remake::Bytes;

inline void Put16(Bytes& b, size_t at, uint16_t v) { b[at] = v & 0xFF; b[at + 1] = v >> 8; }
inline void Put32(Bytes& b, size_t at, uint32_t v) { for (int i = 0; i < 4; i++) b[at + i] = (v >> (8 * i)) & 0xFF; }
inline void Push32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }
inline void Push16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
inline const uint8_t* Px(const Bytes& rgba, uint32_t w, uint32_t x, uint32_t y) { return &rgba[((size_t)y * w + x) * 4]; }
inline bool Is(const uint8_t* p, int r, int g, int b, int a) { return p[0] == r && p[1] == g && p[2] == b && p[3] == a; }

// a Nitro dictionary of the given entries (unit-sized data each) and names
inline Bytes Dict(const std::vector<Bytes>& entries, const std::vector<std::string>& names)
{
    const uint8_t n = (uint8_t)entries.size();
    const uint16_t unit = entries.empty() ? 4 : (uint16_t)entries[0].size();
    Bytes d = {0, n, 0, 0};
    // the tree's size counts from the dictionary's start (as in Platinum's files)
    Push16(d, 8); Push16(d, (uint16_t)(4 + 8 + 4 * n)); Push32(d, 0x17F);
    for (int i = 0; i < n; i++) Push32(d, 0);
    Push16(d, unit); Push16(d, (uint16_t)(4 + unit * n));
    for (const Bytes& e : entries) d.insert(d.end(), e.begin(), e.end());
    for (const std::string& s : names) { Bytes nm(16, 0); memcpy(nm.data(), s.data(), std::min<size_t>(16, s.size())); d.insert(d.end(), nm.begin(), nm.end()); }
    Put16(d, 2, (uint16_t)d.size());
    return d;
}
inline Bytes U32Entry(uint32_t v) { Bytes b; Push32(b, v); return b; }

// a NARC of the given members, no names
inline Bytes MakeNarc(const std::vector<Bytes>& members)
{
    Bytes btaf(12 + members.size() * 8, 0), gmif;
    gmif = {'G', 'M', 'I', 'F', 0, 0, 0, 0};
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

// an NSBMD with one model "pokecenter": one shape, a unit quad in X/Y drawn
// with material "mat0" (paired with texture "tex" and palette "tex_pl"),
// position scale 2; tex0, when not empty, is appended as its TEX0 block
inline Bytes QuadNsbmd(const Bytes& tex0)
{
    Bytes dl; Push32(dl, 0x40); Push32(dl, 1);
    for (int i = 0; i < 4; i++) { Push32(dl, 0x25); Push32(dl, (uint32_t)(((i / 2) << 12) << 16 | ((i % 2) << 12))); }
    Bytes model(0x40, 0);
    // shapes block: dictionary, then the shape (8 header bytes, list offset, size)
    const size_t shp = model.size();
    Bytes shape(16, 0); Put32(shape, 8, 16); Put32(shape, 12, (uint32_t)dl.size()); shape.insert(shape.end(), dl.begin(), dl.end());
    Bytes sdict = Dict({U32Entry(0)}, {"shape0"});
    Put32(sdict, sdict.size() - 16 - 4, (uint32_t)sdict.size()); // the shape follows the dictionary
    model.insert(model.end(), sdict.begin(), sdict.end()); model.insert(model.end(), shape.begin(), shape.end());
    // materials block: pair offsets, dictionary, the texture and palette pairings
    const size_t mat = model.size();
    Bytes mblock(4, 0);
    const Bytes mdict = Dict({U32Entry(0)}, {"mat0"});
    mblock.insert(mblock.end(), mdict.begin(), mdict.end());
    const size_t list = mblock.size(); mblock.push_back(0); mblock.push_back(0); mblock.push_back(0); mblock.push_back(0); // material index 0
    auto pairEntry = [&]() { Bytes e; Push16(e, (uint16_t)list); e.push_back(1); e.push_back(0); return e; };
    Put16(mblock, 0, (uint16_t)mblock.size()); const Bytes td = Dict({pairEntry()}, {"tex"}); mblock.insert(mblock.end(), td.begin(), td.end());
    Put16(mblock, 2, (uint16_t)mblock.size()); const Bytes pd = Dict({pairEntry()}, {"tex_pl"}); mblock.insert(mblock.end(), pd.begin(), pd.end());
    model.insert(model.end(), mblock.begin(), mblock.end());
    // render program: bind material 0, draw shape 0, end
    const size_t sbc = model.size(); model.insert(model.end(), {0x04, 0, 0x05, 0, 0x01});
    Put32(model, 4, (uint32_t)sbc); Put32(model, 8, (uint32_t)mat); Put32(model, 12, (uint32_t)shp); Put32(model, 0x1C, 4096 * 2);
    Put32(model, 0, (uint32_t)model.size());
    Bytes mdl0(8, 0); memcpy(mdl0.data(), "MDL0", 4);
    const Bytes mdlDict = Dict({U32Entry(0)}, {"pokecenter"});
    const size_t modelAt = 8 + mdlDict.size();
    Bytes d2 = mdlDict; Put32(d2, d2.size() - 16 - 4, (uint32_t)modelAt);
    mdl0.insert(mdl0.end(), d2.begin(), d2.end()); mdl0.insert(mdl0.end(), model.begin(), model.end());
    Put32(mdl0, 4, (uint32_t)mdl0.size());
    Bytes bmd(24, 0); memcpy(bmd.data(), "BMD0", 4); Put16(bmd, 14, tex0.empty() ? 1 : 2); Put32(bmd, 16, 24);
    bmd.insert(bmd.end(), mdl0.begin(), mdl0.end());
    if (!tex0.empty()) { Put32(bmd, 20, (uint32_t)bmd.size()); bmd.insert(bmd.end(), tex0.begin(), tex0.end()); }
    return bmd;
}

}

#endif // REMAKE_TESTS_SYNTHETIC_FILES_H
