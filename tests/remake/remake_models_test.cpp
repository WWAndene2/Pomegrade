// The remake tooling's model and texture side: the seven DS texture formats
// decoded against pixels worked out by hand from the hardware's rules; GX
// command lists (every vertex encoding, the four primitive types, the joint
// a vertex follows); the PNG and glTF writers; and GARC, NSBTX and NSBMD
// files built here to their documented layout. The hardware-defined parts
// are checked against known answers; the file layouts only for being read
// back consistently (they still have to be checked on real game files).
#include "Garc.h"
#include "Gltf.h"
#include "GxDisplayList.h"
#include "Nsbmd.h"
#include "Nsbtx.h"
#include "NitroTexture.h"
#include "Png.h"

#include <cmath>
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
static void Push32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }
static void Push16(Bytes& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static const uint8_t* Px(const Bytes& rgba, uint32_t w, uint32_t x, uint32_t y) { return &rgba[((size_t)y * w + x) * 4]; }
static bool Is(const uint8_t* p, int r, int g, int b, int a) { return p[0] == r && p[1] == g && p[2] == b && p[3] == a; }

// a Nitro dictionary of the given entries (unit-sized data each) and names
static Bytes Dict(const std::vector<Bytes>& entries, const std::vector<std::string>& names)
{
    const uint8_t n = (uint8_t)entries.size();
    const uint16_t unit = entries.empty() ? 4 : (uint16_t)entries[0].size();
    Bytes d = {0, n, 0, 0};
    Push16(d, 8); Push16(d, (uint16_t)(8 + 4 * n)); Push32(d, 0x17F);
    for (int i = 0; i < n; i++) Push32(d, 0);
    Push16(d, unit); Push16(d, (uint16_t)(4 + unit * n));
    for (const Bytes& e : entries) d.insert(d.end(), e.begin(), e.end());
    for (const std::string& s : names) { Bytes nm(16, 0); memcpy(nm.data(), s.data(), std::min<size_t>(16, s.size())); d.insert(d.end(), nm.begin(), nm.end()); }
    Put16(d, 2, (uint16_t)d.size());
    return d;
}
static Bytes U32Entry(uint32_t v) { Bytes b; Push32(b, v); return b; }

int main()
{
    // --- texture formats (hardware rules) ---
    uint8_t rgb[3];
    Bgr555ToRgb(0x7FFF, rgb); bool white = rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255;
    Bgr555ToRgb(16, rgb);
    check(white && rgb[0] == 132, "BGR555 widened as the DS does (31 -> 255, 16 -> 132)");
    const Bytes pal4 = {0x1F, 0x00, 0xE0, 0x03, 0x00, 0x7C, 0xFF, 0x7F}; // red, green, blue, white
    {
        Bytes texels(16, 0); // 8x8, 2 bits: row 0 = 0,1,2,3,0,1,2,3
        texels[0] = 0b11100100; texels[1] = 0b11100100;
        const Bytes out = DecodeTexture({2, 8, 8, true}, texels, pal4);
        check(Is(Px(out, 8, 0, 0), 255, 0, 0, 0) && Is(Px(out, 8, 1, 0), 0, 255, 0, 255) && Is(Px(out, 8, 2, 0), 0, 0, 255, 255) && Is(Px(out, 8, 7, 0), 255, 255, 255, 255),
              "4 colours: 2-bit texels low bits first, colour 0 transparent");
    }
    {
        Bytes texels(32, 0); texels[0] = 0x21; // 16 colours: texel 0 = 1, texel 1 = 2
        const Bytes out = DecodeTexture({3, 8, 8, false}, texels, pal4);
        check(Is(Px(out, 8, 0, 0), 0, 255, 0, 255) && Is(Px(out, 8, 1, 0), 0, 0, 255, 255) && Px(out, 8, 2, 0)[3] == 255, "16 colours: 4-bit texels, colour 0 opaque when not flagged");
    }
    {
        Bytes texels(64, 3);
        const Bytes out = DecodeTexture({4, 8, 8, false}, texels, pal4);
        check(Is(Px(out, 8, 5, 5), 255, 255, 255, 255), "256 colours: 8-bit texels");
    }
    {
        Bytes texels(64, 0); texels[0] = (7 << 5) | 3; texels[1] = (3 << 5) | 1; texels[2] = 1;
        const Bytes out = DecodeTexture({1, 8, 8, false}, texels, pal4);
        check(Is(Px(out, 8, 0, 0), 255, 255, 255, 255) && Px(out, 8, 1, 0)[3] == 109 && Px(out, 8, 2, 0)[3] == 0, "A3I5: index 5 bits, alpha 3 bits (7 -> 255, 3 -> 109, 0 -> 0)");
        Bytes t6(64, 0); t6[0] = (31 << 3) | 2; t6[1] = (16 << 3) | 1;
        const Bytes out6 = DecodeTexture({6, 8, 8, false}, t6, pal4);
        check(Is(Px(out6, 8, 0, 0), 0, 0, 255, 255) && Px(out6, 8, 1, 0)[3] == 132, "A5I3: index 3 bits, alpha 5 bits");
    }
    {
        Bytes texels(128, 0); Put16(texels, 0, 0x801F); Put16(texels, 2, 0x001F);
        const Bytes out = DecodeTexture({7, 8, 8, false}, texels, {});
        check(Is(Px(out, 8, 0, 0), 255, 0, 0, 255) && Px(out, 8, 1, 0)[3] == 0, "direct colour: BGR555, alpha bit 15");
    }
    {
        // 8x8 = 4 blocks; every texel of block b uses code b (0..3 in turn: 0,1,2,3 per row)
        Bytes texels(16, 0);
        for (int b = 0; b < 4; b++) Put32(texels, b * 4, 0b11100100 * 0x01010101u);
        Bytes info(8, 0);
        Put16(info, 0, (0 << 14) | 0);  // mode 0: c2 = colour 2, c3 transparent
        Put16(info, 2, (1u << 14) | 0); // mode 1: c2 = (c0 + c1) / 2
        Put16(info, 4, (2u << 14) | 0); // mode 2: c3 = colour 3
        Put16(info, 6, (3u << 14) | 0); // mode 3: c2 = (5 c0 + 3 c1) / 8
        const Bytes out = DecodeTexture({5, 8, 8, false}, texels, pal4, info);
        // red (31,0,0) and green (0,31,0): mode 1 c2 = (15,15,0) -> 123; mode 3 c2 = (19,11,0) -> 156, 90
        check(Is(Px(out, 8, 2, 0), 0, 0, 255, 255) && Px(out, 8, 3, 0)[3] == 0 &&
              Is(Px(out, 8, 6, 0), 123, 123, 0, 255) && Px(out, 8, 7, 0)[3] == 0 &&
              Is(Px(out, 8, 3, 4), 255, 255, 255, 255) &&
              Is(Px(out, 8, 6, 4), 156, 90, 0, 255),
              "4x4 compressed: the four modes' third and fourth colours");
    }
    bool refused = false;
    try { DecodeTexture({4, 8, 8, false}, Bytes(10, 0), pal4); } catch (const FormatError&) { refused = true; }
    check(refused, "texture: too little data refused");

    // --- display lists ---
    {
        Bytes dl;
        // BEGIN(tris) COLOR TEXCOORD NORMAL ; VTX_16 VTX_10 VTX_DIFF MTX_RESTORE ; END
        Push32(dl, 0x21222040); Push32(dl, 0); Push32(dl, 0x001F); Push32(dl, (32u << 16) | 16); Push32(dl, 511u << 20); // normal z = +511/512
        Push32(dl, 0x14282423);
        Push32(dl, (0x1000u << 16) | 0x0800); Push32(dl, 0xF000); // (0.5, 1, -1)
        Push32(dl, (64u) | (0u << 10) | (0u << 20));               // VTX_10: x = 64 << 6 = 4096 -> 1.0
        Push32(dl, 8u | (0x3F8u << 10));                           // VTX_DIFF: x += 8, y -= 8
        Push32(dl, 5);                                             // MTX_RESTORE 5
        Push32(dl, 0x00000041);
        const GxMesh m = DecodeDisplayList(dl);
        check(m.Vertices.size() == 3 && m.Triangles.size() == 3, "display list: one triangle from three vertex commands");
        check(m.Vertices[0].Position[0] == 0.5f && m.Vertices[0].Position[1] == 1.0f && m.Vertices[0].Position[2] == -1.0f, "VTX_16: 4.12 positions, signed");
        check(m.Vertices[1].Position[0] == 1.0f && m.Vertices[1].Position[1] == 0.0f, "VTX_10: 10-bit, 6 fraction bits");
        check(std::fabs(m.Vertices[2].Position[0] - (1.0f + 8 / 4096.0f)) < 1e-6f && std::fabs(m.Vertices[2].Position[1] + 8 / 4096.0f) < 1e-6f, "VTX_DIFF: signed 10-bit steps from the last vertex");
        check(m.Vertices[0].Colour == 0x1F && m.Vertices[0].TexCoord[0] == 1.0f && m.Vertices[0].TexCoord[1] == 2.0f && std::fabs(m.Vertices[0].Normal[2] - 511 / 512.0f) < 1e-6f,
              "COLOR, TEXCOORD (12.4), NORMAL (1.0.9) carried by the vertices");
        check(m.Vertices[2].Joint == -1 && m.Commands == 9, "MTX_RESTORE applies from the next vertex; 9 commands counted");
    }
    {
        auto list = [](int prim, int verts) {
            Bytes dl; Push32(dl, 0x40); Push32(dl, (uint32_t)prim);
            for (int i = 0; i < verts; i++) { Push32(dl, 0x25); Push32(dl, (uint32_t)(i << 12)); }
            return DecodeDisplayList(dl);
        };
        const GxMesh quads = list(1, 4), strip = list(2, 4), qstrip = list(3, 6);
        check(quads.Triangles == std::vector<uint32_t>({0, 1, 2, 0, 2, 3}), "quads: two triangles each");
        check(strip.Triangles == std::vector<uint32_t>({0, 1, 2, 2, 1, 3}), "triangle strip: every other one reversed");
        check(qstrip.Triangles == std::vector<uint32_t>({0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4}), "quad strip: v0 v1 v3 v2 per quad");
    }
    refused = false;
    try { Bytes dl; Push32(dl, 0x00000099); DecodeDisplayList(dl); } catch (const FormatError&) { refused = true; }
    check(refused, "display list: unknown command refused");
    refused = false;
    try { Bytes dl; Push32(dl, 0x00000023); Push32(dl, 0); DecodeDisplayList(dl); } catch (const FormatError&) { refused = true; }
    check(refused, "display list: parameters cut short refused");

    // --- PNG and glTF ---
    const Bytes png = EncodePng(8, 8, Bytes(8 * 8 * 4, 200));
    WriteFile("test.png", png);
    check(png.size() > 33 && png[1] == 'P' && png[16] == 0 && png[19] == 8 && png[23] == 8, "PNG: signature and 8x8 header");
    {
        Bytes dl; Push32(dl, 0x40); Push32(dl, 1);
        const int xy[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (auto& v : xy) { Push32(dl, 0x2522); Push32(dl, (uint32_t)((v[1] * 128) << 16 | (v[0] * 128))); Push32(dl, (uint32_t)((v[1] << 12) << 16 | (v[0] << 12))); }
        GltfMaterial mat; mat.Name = "m"; mat.Png = png; mat.TexWidth = 8; mat.TexHeight = 8;
        const std::string g = WriteGltf({{DecodeDisplayList(dl), 0}}, {mat});
        WriteFile("test.gltf", Bytes(g.begin(), g.end()));
        check(g.find("\"POSITION\"") != std::string::npos && g.find("\"count\":6,\"type\":\"SCALAR\"") != std::string::npos &&
              g.find("data:image/png;base64,") != std::string::npos && g.find("\"max\":[1,1,0]") != std::string::npos,
              "glTF: a textured quad (6 indices, bounds, embedded PNG)");
    }

    // --- GARC (version 4) ---
    {
        Bytes a = {1, 2, 3, 4}, b = {5, 6}, c = {7, 8, 9, 10};
        Bytes data = a; data.insert(data.end(), b.begin(), b.end()); data.resize(8); data.insert(data.end(), c.begin(), c.end());
        Bytes fatb; fatb.insert(fatb.end(), {'B','T','A','F'}); Push32(fatb, 0); Push32(fatb, 2);
        const size_t e0 = fatb.size() - 12; Push32(fatb, 1); Push32(fatb, 0); Push32(fatb, 4); Push32(fatb, 4);
        const size_t e1 = fatb.size() - 12; Push32(fatb, 3); Push32(fatb, 4); Push32(fatb, 6); Push32(fatb, 2); Push32(fatb, 8); Push32(fatb, 12); Push32(fatb, 4);
        Put32(fatb, 4, (uint32_t)fatb.size());
        Bytes fato; fato.insert(fato.end(), {'O','T','A','F'}); Push32(fato, 0); Push16(fato, 2); Push16(fato, 0xFFFF); Push32(fato, (uint32_t)e0); Push32(fato, (uint32_t)e1);
        Put32(fato, 4, (uint32_t)fato.size());
        Bytes fimb; fimb.insert(fimb.end(), {'B','M','I','F'}); Push32(fimb, 12); Push32(fimb, (uint32_t)data.size());
        Bytes g(0x1C, 0); memcpy(g.data(), "CRAG", 4); Put32(g, 4, 0x1C); Put16(g, 8, 0xFEFF); Put16(g, 0x0A, 0x0400); Put32(g, 0x0C, 4);
        g.insert(g.end(), fato.begin(), fato.end()); g.insert(g.end(), fatb.begin(), fatb.end()); g.insert(g.end(), fimb.begin(), fimb.end());
        Put32(g, 0x10, (uint32_t)g.size());
        g.insert(g.end(), data.begin(), data.end());
        Put32(g, 0x14, (uint32_t)g.size());
        const Garc garc(g);
        check(garc.Version() == 0x400 && garc.Count() == 2 && garc.Sub(0) == a && garc.Has(1, 0) && garc.Has(1, 1) && garc.Sub(1, 0) == b && garc.Sub(1, 1) == c,
              "GARC v4: two entries, the second with two sub-files");
        refused = false;
        try { Bytes bad = g; Put16(bad, 0x0A, 0x0500); Garc x(bad); } catch (const FormatError&) { refused = true; }
        check(refused, "GARC: unknown version refused");
    }

    // --- NSBTX and NSBMD, to the documented layout ---
    // TEX0: one 8x8 4-colour texture "tex" (row 0 = 0..3 twice) and palette "tex_pl"
    Bytes tex0(0x3C, 0);
    memcpy(tex0.data(), "TEX0", 4);
    {
        const Bytes tdict = Dict({[] { Bytes e; Push32(e, (2u << 26) | (0u << 20) | (0u << 23) | 0); Push32(e, 0); return e; }()}, {"tex"});
        const Bytes pdict = Dict({[] { Bytes e; Push16(e, 0); Push16(e, 0); return e; }()}, {"tex_pl"});
        Put16(tex0, 0x0E, (uint16_t)tex0.size()); tex0.insert(tex0.end(), tdict.begin(), tdict.end());
        Put32(tex0, 0x34, (uint32_t)tex0.size()); tex0.insert(tex0.end(), pdict.begin(), pdict.end());
        while (tex0.size() % 8) tex0.push_back(0);
        Put32(tex0, 0x14, (uint32_t)tex0.size()); Bytes texels(16, 0); texels[0] = 0b11100100; texels[1] = 0b11100100; tex0.insert(tex0.end(), texels.begin(), texels.end());
        Put32(tex0, 0x38, (uint32_t)tex0.size()); tex0.insert(tex0.end(), pal4.begin(), pal4.end());
        Put32(tex0, 4, (uint32_t)tex0.size());
    }
    Bytes btx(20, 0); memcpy(btx.data(), "BTX0", 4); Put16(btx, 14, 1); Put32(btx, 16, 20);
    btx.insert(btx.end(), tex0.begin(), tex0.end());
    {
        const long at = Tex0::Find(btx);
        const Tex0 t(btx, (size_t)at);
        const Bytes out = t.Decode(0);
        check(t.Textures().size() == 1 && t.Textures()[0].Name == "tex" && t.Palettes()[0] == "tex_pl" && t.DefaultPalette(0) == 0 &&
              Is(Px(out, 8, 1, 0), 0, 255, 0, 255), "NSBTX: texture and palette by name, decoded");
    }
    // NSBMD: one model, one shape (a quad), one material using "tex"/"tex_pl"
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
        Bytes bmd(24, 0); memcpy(bmd.data(), "BMD0", 4); Put16(bmd, 14, 2); Put32(bmd, 16, 24);
        bmd.insert(bmd.end(), mdl0.begin(), mdl0.end());
        Put32(bmd, 20, (uint32_t)bmd.size()); bmd.insert(bmd.end(), tex0.begin(), tex0.end());
        // the TEX0 offsets are relative to its block: unchanged by its new position
        WriteFile("test.nsbmd", bmd);
        const Nsbmd n(bmd);
        const NsbmdModel& m = n.Models().at(0);
        check(m.Name == "pokecenter" && m.Shapes.size() == 1 && m.Materials.size() == 1 && m.Materials[0].Texture == "tex" && m.Materials[0].Palette == "tex_pl" &&
              m.Shapes[0].Material == 0 && m.PosScale == 2.0f, "NSBMD: model, shape, material paired with texture and palette, SBC binding, position scale");
        const std::string g = ModelToGltf(n, 0);
        WriteFile("test_model.gltf", Bytes(g.begin(), g.end()));
        check(g.find("\"count\":6,\"type\":\"SCALAR\"") != std::string::npos && g.find("\"max\":[2,2,0]") != std::string::npos &&
              g.find("baseColorTexture") != std::string::npos, "NSBMD to glTF: the quad scaled x2, textured from the model's own TEX0");
    }

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
