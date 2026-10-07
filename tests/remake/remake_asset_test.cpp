// Asset customization (AssetEdit.h): a model file's texture edited in place, in its own format, the rest of the file
// untouched; recolouring and resampling
#include "AssetEdit.h"
#include "Bch.h"
#include "BchTextureFile.h"
#include "PicaTexture.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

// an image of one colour, RGBA
static Bytes Flat(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    Bytes p((size_t)w * h * 4);
    for (size_t i = 0; i < p.size(); i += 4) { p[i] = r; p[i + 1] = g; p[i + 2] = b; p[i + 3] = a; }
    return p;
}

int main()
{
    // a texture file as the game has them: an ETC1 red texture and an RGBA4 grey one
    const Bytes red = Flat(16, 16, 200, 40, 40), grey = Flat(8, 8, 136, 136, 136);
    const Bytes file = BchWriteTextureFile({{"body", 16, 16, 12, PicaTextureEncode(red, 16, 16, 12)},
                                           {"eye", 8, 8, 4, PicaTextureEncode(grey, 8, 8, 4)}});
    const Bch before = Bch::Read(file);

    // red turned 120 degrees round the colour wheel: green
    const Bytes edited = BchEditTexture(file, "body", [](Bytes& rgba, uint32_t, uint32_t) { Recolour(rgba, 120, 1, 1); });
    const Bch after = Bch::Read(edited);
    check(edited.size() == file.size(), "the file keeps its size");
    const Bytes body = PicaTextureDecode(after.Textures[0].Data, 16, 16, 12);
    printf("  body's first pixel %u %u %u\n", body[0], body[1], body[2]);
    check(body[1] > 150 && body[0] < 90 && body[2] < 90, "the red texture is green after a 120-degree hue turn");
    check(after.Textures[0].Format == 12 && after.Textures[0].Width == 16 && after.Textures[0].Name == "body", "it keeps its name, size and format");
    check(after.Textures[1].Data == before.Textures[1].Data, "the other texture is untouched");
    // outside the edited texture's bytes the file is identical
    size_t outside = 0;
    for (size_t i = 0; i < file.size(); i++)
    {
        const size_t at = before.Textures[0].DataOffset;
        if ((i < at || i >= at + before.Textures[0].Data.size()) && file[i] != edited[i]) outside++;
    }
    check(outside == 0, "no byte outside the texture's data changes");

    // only one colour of a texture: a half red, half blue texture, its red parts made yellow (hue +60 around red)
    Bytes two = Flat(16, 16, 220, 30, 30);
    for (size_t i = 0; i < two.size() / 2; i += 4) { two[i] = 30; two[i + 1] = 30; two[i + 2] = 220; }
    Recolour(two, 60, 1, 1, 0, 30);
    check(two[0] == 30 && two[2] == 220, "a colour outside the range (blue) is kept");
    const uint8_t* r = &two[two.size() - 4];
    check(r[0] > 200 && r[1] > 200 && r[2] < 60, "the red parts are yellow");
    // greys have no hue: kept when one colour changes
    Bytes g = Flat(8, 8, 128, 128, 128);
    Recolour(g, 90, 1, 1, 0, 30);
    check(g[0] == 128 && g[1] == 128 && g[2] == 128, "a grey is kept when one colour changes");
    // saturation 0: grey; brightness 0.5: half as bright
    Bytes s = Flat(8, 8, 200, 100, 50);
    Recolour(s, 0, 0, 0.5);
    check(s[0] == s[1] && s[1] == s[2] && std::abs(s[0] - 100) <= 1, "saturation 0 and brightness x0.5 give a grey of half the value");

    // resampling: a 2x2 image to 4x4 keeps its corners' colours, a flat image stays flat
    Bytes small = Flat(2, 2, 0, 0, 0);
    small[4] = 255; // top-right pixel red
    const Bytes big = Resample(small, 2, 2, 4, 4);
    check(big[3 * 4] == 255 && big[0] == 0, "resampled 2x2 to 4x4: the corners keep their colours");
    check(Resample(Flat(8, 8, 10, 20, 30), 8, 8, 16, 4) == Flat(16, 4, 10, 20, 30), "a flat image resampled stays flat");

    bool threw = false;
    try { BchEditTexture(file, "none", [](Bytes&, uint32_t, uint32_t) {}); } catch (const FormatError&) { threw = true; }
    check(threw, "a texture the file lacks is refused");

    puts(ok ? "ALL OK" : "FAILURES");
    return ok ? 0 : 1;
}
