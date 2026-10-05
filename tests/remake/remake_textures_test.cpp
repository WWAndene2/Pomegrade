// The remake tooling's writer of texture-only BCH files (BchTextureFile): textures written and read back, the sections' alignment, the
// relocation count, and the name tree's lookup rule. The same writer reproduces all 439 texture files of the game byte for byte (checked on
// the real game by `remake_tool oras-verify`, which needs the owner's dump).
#include "BchTextureFile.h"
#include "PicaTexture.h"

#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static BchTextureSource Texture(const std::string& name, uint32_t w, uint32_t h, uint8_t format, uint8_t seed)
{
    BchTextureSource t;
    t.Name = name; t.Width = w; t.Height = h; t.Format = format;
    t.Data.resize(PicaTextureLength(w, h, format));
    for (size_t i = 0; i < t.Data.size(); i++) t.Data[i] = (uint8_t)(seed + i * 7);
    return t;
}

static int Bit(const std::string& name, uint32_t i) { return i >> 3 >= name.size() ? 0 : ((uint8_t)name[i >> 3] >> (i & 7)) & 1; }

int main()
{
    const std::vector<BchTextureSource> in = {Texture("chip_kusa", 128, 128, 12, 1), Texture("chip_kusa_b", 128, 128, 12, 2), Texture("door", 64, 32, 13, 3),
                                              Texture("in", 8, 8, 3, 4), Texture("t101_01", 256, 128, 13, 5), Texture("t101_02", 256, 256, 12, 6), Texture("kage", 16, 16, 12, 7)};
    const Bytes file = BchWriteTextureFile(in);
    check(Bch::Is(file) && file[4] == 0x21, "a BCH of version 0x21");
    const Bch out = Bch::Read(file);
    bool same = out.Textures.size() == in.size();
    for (size_t i = 0; same && i < in.size(); i++)
        same = out.Textures[i].Name == in[i].Name && out.Textures[i].Width == in[i].Width && out.Textures[i].Height == in[i].Height &&
               out.Textures[i].Format == in[i].Format && out.Textures[i].Data == in[i].Data;
    check(same, "every texture reads back with its name, size, format and data, in order");

    const uint32_t commands = U32(file, 0x14), raw = U32(file, 0x18), relocations = U32(file, 0x1C), relocationLength = U32(file, 0x34 + 0);
    (void)relocations;
    check(commands % 16 == 0 && raw % 0x80 == 0 && U32(file, 0x2C) % 0x80 == 0, "commands aligned to 16, raw data to 0x80 and padded to 0x80");
    check(U32(file, 0x30) == 0 && U32(file, 0x38) == 12 * in.size() && U32(file, 0x3C) == 0 && U16(file, 0x40) == 1 && U16(file, 0x42) == 3 * in.size(),
          "header: no extra raw section, 12 per texture uninitialised, flag 1, 3 per texture addresses");
    check(relocationLength == 4 * (9 * in.size() + 16), "9 * count + 16 relocation words");

    // the name tree (descriptor 3 of the contents, at 0x44 + 36): every name is found by the documented lookup
    const uint32_t contents = U32(file, 0x08);
    const uint32_t treeAt = contents + U32(file, contents + 3 * 12 + 8);
    auto node = [&](uint32_t i, uint32_t& ref, uint32_t& left, uint32_t& right, uint32_t& name) {
        const uint32_t at = treeAt + i * 12;
        ref = U32(file, at); left = U16(file, at + 4); right = U16(file, at + 6); name = U32(file, at + 8);
    };
    bool found = true;
    const uint32_t strings = U32(file, 0x0C);
    for (size_t k = 0; k < in.size(); k++)
    {
        uint32_t ref, l, r, nm, pref, pl, pr, pnm;
        uint32_t prev = 0, cur;
        node(0, pref, pl, pr, pnm);
        cur = pl;
        node(cur, ref, l, r, nm);
        while (pref > ref)
        {
            prev = cur;
            cur = Bit(in[k].Name, ref) ? r : l;
            pref = ref;
            node(cur, ref, l, r, nm);
        }
        (void)prev;
        if (Text(file, strings + nm, 256) != in[k].Name) found = false;
    }
    check(found, "the name tree finds every name (bit i = (name[i / 8] >> (i % 8)) & 1, walking while the reference bit decreases)");

    bool twice = false;
    try { BchWriteTextureFile({Texture("a", 8, 8, 3, 1), Texture("a", 8, 8, 3, 2)}); } catch (const FormatError&) { twice = true; }
    check(twice, "the same name twice is refused");
    bool empty = false;
    try { BchWriteTextureFile({}); } catch (const FormatError&) { empty = true; }
    check(empty, "no texture is refused");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
