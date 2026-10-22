// Layout files (Bclim.h, Darc.h, Layout.h): a BCLIM written and read back, a DARC of files in folders written and read
// back, a BCLYT built here to the section layout described. No game data.
#include "Bclim.h"
#include "Darc.h"
#include "Layout.h"
#include "OrasXySequence.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void P16(Bytes& b, uint16_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }
static void P32(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }
static void PF(Bytes& b, float f) { uint32_t u; std::memcpy(&u, &f, 4); P32(b, u); }
static void PS(Bytes& b, const std::string& s, size_t n) { for (size_t i = 0; i < n; i++) b.push_back(i < s.size() ? (uint8_t)s[i] : 0); }

int main()
{
    // BCLIM: 20 x 10 RGBA8, stored 32 x 16
    Bytes rgba;
    for (int i = 0; i < 20 * 10; i++) { rgba.push_back((uint8_t)i); rgba.push_back((uint8_t)(i * 3)); rgba.push_back((uint8_t)(255 - i)); rgba.push_back(200); }
    const Bytes clim = ClimImage::WriteRgba8(20, 10, rgba);
    const ClimImage c = ClimImage::Read(clim);
    check(c.Width == 20 && c.Height == 10 && c.StoredWidth == 32 && c.StoredHeight == 16 && c.Format == 9, "BCLIM size, rounded storage and format read back");
    check(c.Rgba() == rgba, "BCLIM pixels read back");

    // an L8 image 20 x 8 stored 32 x 8: 256 bytes, the same length as 20 x 8 rounded up to 0x80, so the length alone
    // does not decide; 20 is no whole number of 8x8 tiles, so the stored size is the power of two
    {
        Bytes l8(256);
        for (size_t i = 0; i < l8.size(); i++) l8[i] = (uint8_t)i;
        Bytes f = l8;
        for (char ch : std::string("CLIM")) f.push_back((uint8_t)ch);
        P16(f, 0xFEFF); P16(f, 0x14); P32(f, 0x02020000); P32(f, (uint32_t)(256 + 0x28)); P16(f, 1); P16(f, 0);
        for (char ch : std::string("imag")) f.push_back((uint8_t)ch);
        P32(f, 0x10); P16(f, 20); P16(f, 8); P32(f, 0); P32(f, 256);
        const ClimImage s = ClimImage::Read(f);
        check(s.StoredWidth == 32 && s.StoredHeight == 8 && s.Rgba().size() == 20 * 8 * 4, "a size not in whole tiles is read as stored at powers of two");
    }

    // DARC
    const std::vector<DarcFile> files = {{"blyt/title.bclyt", {1, 2, 3}}, {"blyt/menu.bclyt", {4}}, {"timg/logo.bclim", clim}, {"root.bin", {9, 9}}};
    const Bytes darc = WriteDarc(files);
    auto same = [](const std::vector<DarcFile>& got, const std::vector<DarcFile>& want) {
        bool ok = got.size() == want.size();
        for (size_t i = 0; ok && i < want.size(); i++) ok = got[i].Path == want[i].Path && got[i].Data == want[i].Data;
        return ok;
    };
    // read back folder by folder: the root's files, then each folder's in order of first appearance
    check(same(ReadDarc(darc), {files[3], files[0], files[1], files[2]}), "DARC files and folders read back, folder by folder");
    // a folder's files given apart (a/x, b/y, a/z) stay in their own folders
    const std::vector<DarcFile> apart = {{"a/x", {1}}, {"b/y", {2}}, {"a/z", {3}}};
    check(same(ReadDarc(WriteDarc(apart)), {apart[0], apart[2], apart[1]}), "DARC folders kept apart when their files are interleaved");
    // laid out as the game's: the data offset the table's end rounded to 8, a BCLIM at 0x80, any other file at 8
    {
        auto u32 = [&](size_t at) { return (uint32_t)(darc[at] | darc[at + 1] << 8 | darc[at + 2] << 16 | (uint32_t)darc[at + 3] << 24); };
        const uint32_t table = u32(16), tableSize = u32(20), nodes = u32(table + 8);
        bool aligned = u32(24) == ((table + tableSize + 7) & ~7u);
        for (uint32_t i = 0; i < nodes; i++)
            if (!(u32(table + 12 * i) >> 24)) aligned = aligned && u32(table + 12 * i + 4) % (u32(table + 12 * i + 8) == clim.size() ? 0x80 : 8) == 0;
        check(aligned, "DARC laid out as the game's (data offset at 8, BCLIM at 0x80, other files at 8)");
    }

    // BCLYT: lyt1, txl1 (one texture), pas1, pic1, pae1
    Bytes body;
    PS(body, "lyt1", 4); P32(body, 20); body.push_back(1); PS(body, "", 3); PF(body, 400); PF(body, 240);
    PS(body, "txl1", 4); P32(body, 28); P16(body, 1); P16(body, 0); P32(body, 4); PS(body, "logo.bclim", 12);
    PS(body, "pas1", 4); P32(body, 8);
    PS(body, "pic1", 4); P32(body, 0x4C); body.push_back(1); body.push_back(4); body.push_back(255); body.push_back(0);
    PS(body, "N_Logo", 16); PS(body, "", 8);
    for (float v : {10.f, -20.f, 0.f, 0.f, 0.f, 45.f, 1.f, 1.f, 256.f, 128.f}) PF(body, v);
    PS(body, "pae1", 4); P32(body, 8);
    Bytes lyt;
    PS(lyt, "CLYT", 4); P16(lyt, 0xFEFF); P16(lyt, 0x14); P32(lyt, 0x02020000); P32(lyt, (uint32_t)(0x14 + body.size())); P16(lyt, 5); P16(lyt, 0);
    lyt.insert(lyt.end(), body.begin(), body.end());
    const std::string text = DescribeLayout(lyt);
    check(text.find("screen 400 x 240, origin centre") != std::string::npos, "lyt1 screen size");
    check(text.find("txl1: 1 logo.bclim") != std::string::npos, "txl1 texture names");
    check(text.find("    pic1 N_Logo: at (10, -20, 0), rotation (0, 0, 45), scale (1, 1), size 256 x 128, alpha 255") != std::string::npos,
          "a pane nested under pas1, with its transform");
    {
        // XySequenceToOras: X's command 0x97 (16 bytes of arguments) is Omega Ruby's 0xA0 (20: zeros added), X's 0x9 stays 0x9;
        // the header, the records' frames and the end mark kept, the record handed over with Omega Ruby's number
        Bytes xSizes(4 * 186, 0), oSizes(4 * 213, 0);
        xSizes[4 * 0x97] = 16; oSizes[4 * 0xA0] = 20; xSizes[4 * 0x9] = 8; oSizes[4 * 0x9] = 8;
        Bytes x = {'S', 'E', 'S', 'D', 2, 0, 0, 0, 0, 0, 0, 0, 0xE6, 0, 0, 0};
        const auto record = [&](uint32_t start, uint16_t command, uint8_t fill, size_t n) {
            for (int k = 0; k < 4; k++) x.push_back((uint8_t)(start >> (8 * k)));
            for (int k = 0; k < 4; k++) x.push_back((uint8_t)(start >> (8 * k)));
            x.insert(x.end(), {0, 0, 0, 0, (uint8_t)command, (uint8_t)(command >> 8)});
            x.insert(x.end(), n, fill);
        };
        record(0, 0x97, 3, 16);
        record(29, 0x9, 7, 8);
        x.insert(x.end(), {0xFF, 0xFF, 0xFF, 0xFF});
        std::vector<int> seen;
        const Bytes o = XySequenceToOras(x, xSizes, oSizes, [&](uint16_t c, Bytes& a) { seen.push_back(c); if (c == 0xA0) a[0] = 41; });
        check(o.size() == x.size() + 4 && o[0x1C] == 0xA0 && o[0x1E] == 41 && o[0x1F] == 3 && o[0x2E] == 0 && o[0x31] == 0 &&
              o[0x32] == 29 && o[0x3E] == 0x9 && o[0x40] == 7 && o[o.size() - 1] == 0xFF && seen == std::vector<int>{0xA0, 0x9},
              "an X/Y sequence renumbered, an argument grown with zeros, a record changed by the caller");
        check(XySequenceCommand(0x14) == 0x15 && XySequenceCommand(0x6C) == 0x75 && XySequenceCommand(0xB8) == 0xD3, "X/Y's sequence commands as Omega Ruby's");
    }
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
