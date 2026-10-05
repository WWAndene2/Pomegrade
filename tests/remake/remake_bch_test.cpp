// The remake tooling's 3DS model reading: PICA 24-bit floats, command lists
// and shader uniforms, texture formats (tile order, RGB565, ETC1), and a
// small BCH built here to SPICA's layout (one model, material, mesh with a
// 16-bit index buffer, texture), read end to end through its relocation
// table. On the real game the same code reads Omega Ruby's map pieces and
// area texture packs (see Bch.h). Writing (BchWriter.h): the mesh's geometry
// replaced and read back.
#include "Bch.h"
#include "BchWriter.h"
#include "Gltf.h"
#include "PicaCommands.h"
#include "PicaTexture.h"
#include "synthetic_files.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace remake;
using namespace synthetic;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t Bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

// a command: its first parameter, then the header (register, all 4 mask bits, extra count, consecutive)
static void Cmd(std::vector<uint32_t>& w, uint16_t reg, std::vector<uint32_t> params, bool consecutive = false)
{
    w.push_back(params[0]);
    w.push_back(reg | 0xFu << 16 | (uint32_t)(params.size() - 1) << 20 | (consecutive ? 1u << 31 : 0));
    for (size_t i = 1; i < params.size(); i++) w.push_back(params[i]);
    if (w.size() & 1) w.push_back(0);
}
// a vertex shader float uniform, as 32-bit floats (index, then w, z, y, x)
static void Uniform(std::vector<uint32_t>& w, uint32_t index, float x, float y, float z, float wv)
{
    Cmd(w, 0x2C0, {index | 0x80000000u});
    Cmd(w, 0x2C1, {Bits(wv), Bits(z), Bits(y), Bits(x)});
}

// a BCH being built: sections, and the relocations naming every pointer in them
struct BchBuilder
{
    Bytes Contents, Strings, Commands, Raw;
    std::vector<uint32_t> Relocs;
    // the word at `at` in section `source` points at `value` in section `target`
    void Pointer(Bytes& sec, uint32_t source, size_t at, uint32_t target, uint32_t value)
    {
        Put32(sec, at, value);
        Relocs.push_back((target == 1 ? (uint32_t)at : (uint32_t)at >> 2) | target << 25 | source << 29);
    }
    uint32_t String(const std::string& s) { const uint32_t at = (uint32_t)Strings.size(); Strings.insert(Strings.end(), s.begin(), s.end()); Strings.push_back(0); return at; }
    Bytes Build()
    {
        Bytes relocs;
        for (uint32_t r : Relocs) Push32(relocs, r);
        Bytes out(0x44, 0);
        memcpy(out.data(), "BCH", 3); out[4] = out[5] = 0x21;
        std::vector<const Bytes*> secs = {&Contents, &Strings, &Commands, &Raw, nullptr, &relocs};
        uint32_t at = 0x44;
        for (size_t i = 0; i < secs.size(); i++)
        {
            const uint32_t len = secs[i] ? (uint32_t)secs[i]->size() : 0;
            Put32(out, 8 + i * 4, at); Put32(out, 0x20 + i * 4, len);
            if (secs[i]) { out.insert(out.end(), secs[i]->begin(), secs[i]->end()); at += len; }
        }
        return out;
    }
};

int main()
{
    // 24-bit floats: 1 sign, 7 exponent (bias 63), 16 mantissa bits
    {
        // a texture written as RGBA8 reads back the same (16 x 8: two tiles, every texel different)
        Bytes rgba(16 * 8 * 4);
        for (size_t i = 0; i < rgba.size(); i++) rgba[i] = (uint8_t)(i * 7 + 3);
        const Bytes stored = PicaTextureEncodeRgba8(rgba, 16, 8);
        check(stored.size() == PicaTextureLength(16, 8, 0) && PicaTextureDecode(stored, 16, 8, 0) == rgba, "an RGBA8 texture encodes and decodes back unchanged");
    }
    check(PicaFloat24(63u << 16) == 1.0f && PicaFloat24(1u << 23 | 64u << 16 | 0x8000) == -3.0f && PicaFloat24(0) == 0.0f, "PICA float24: 1, -3, 0");

    // commands: a multi-parameter one, a consecutive run, uniforms as 32-bit and as 24-bit floats
    std::vector<uint32_t> w;
    Cmd(w, 0x201, {0x11});
    Cmd(w, 0x203, {5, 6, 7}, true);   // 0x203, 0x204, 0x205
    Uniform(w, 7, 2.0f, 0.5f, 0.25f, 0.125f);
    // uniform 8 as 24-bit floats: x=1 (word2 low 24 bits), others 0
    Cmd(w, 0x2C0, {8});
    Cmd(w, 0x2C1, {0, 0, 63u << 16});
    const PicaCommands c = PicaCommands::Parse(w);
    check(c.Last(0x201) == 0x11 && c.Last(0x203) == 5 && c.Last(0x204) == 6 && c.Last(0x205) == 7 && c.Last(0x999) == 0, "PICA commands: values, a consecutive run");
    check(c.VertexUniforms[7][0] == 2.0f && c.VertexUniforms[7][1] == 0.5f && c.VertexUniforms[7][2] == 0.25f && c.VertexUniforms[7][3] == 0.125f, "PICA uniforms: 32-bit, x y z w");
    check(c.VertexUniforms[8][0] == 1.0f && c.VertexUniforms[8][3] == 0.0f, "PICA uniforms: 24-bit");
    bool refused = false;
    try { PicaCommands::Parse({1, 0x201u | 3u << 20}); } catch (const FormatError&) { refused = true; }
    check(refused, "PICA commands: a command past its list refused");

    // textures: RGBA8 (stored A, B, G, R), 8x8: the first texel stored is the bottom-left one
    Bytes rgba8(PicaTextureLength(8, 8, 0), 0);
    rgba8[0] = 0xFF; rgba8[1] = 0x30; rgba8[2] = 0x20; rgba8[3] = 0x10; // texel (0,0) from the bottom: R 10 G 20 B 30
    rgba8[4] = 0xFF; rgba8[7] = 0x99;                                   // texel (1,0): R 99
    rgba8[8] = 0xFF; rgba8[11] = 0x77;                                  // swizzle 2: texel (0,1): R 77
    const Bytes t = PicaTextureDecode(rgba8, 8, 8, 0);
    const uint8_t* bottomLeft = &t[(7 * 8 + 0) * 4];
    check(bottomLeft[0] == 0x10 && bottomLeft[1] == 0x20 && bottomLeft[2] == 0x30 && bottomLeft[3] == 0xFF && t[(7 * 8 + 1) * 4] == 0x99 && t[(6 * 8 + 0) * 4] == 0x77,
          "texture RGBA8: channel order, tile order, rows written top-down");
    check(PicaTextureLength(8, 8, 3) == 128 && PicaTextureLength(4, 4, 10) == 128 && PicaTextureLength(16, 16, 0) == 1024, "texture sizes rounded to 0x80");
    Bytes rgb565(128, 0);
    rgb565[0] = 0x00; rgb565[1] = 0xF8;                                  // pure red: R in the high 5 bits
    check(PicaTextureDecode(rgb565, 8, 8, 3)[(7 * 8) * 4] == 255 && PicaTextureDecode(rgb565, 8, 8, 3)[(7 * 8) * 4 + 2] == 0, "texture RGB565: red in the high bits");
    // ETC1: one 64-bit block per 4x4, stored little-endian: its last byte is red (Khronos byte 0),
    // individual mode (high nibble: the block's left half, low: its right half), table 0, all
    // pixel indices 0 (+2)
    Bytes etc(PicaTextureLength(8, 8, 12), 0);
    for (int b = 0; b < 4; b++) etc[b * 8 + 7] = 0xFF; // red 0xF in both halves of every block
    const Bytes e = PicaTextureDecode(etc, 8, 8, 12);
    check(e[0] == 255 && e[1] == 2 && e[2] == 2 && e[3] == 255 && e[(7 * 8 + 7) * 4] == 255, "texture ETC1: red base colour decodes red, whole texture covered");

    // a BCH: one model "m" (material "mat", texture "tex"), one mesh of one triangle, one texture
    BchBuilder b;
    b.Contents.assign(15 * 12, 0);                     // the 15 dictionaries
    const uint32_t modelList = (uint32_t)b.Contents.size(); b.Contents.resize(modelList + 4, 0);
    const uint32_t textureList = (uint32_t)b.Contents.size(); b.Contents.resize(textureList + 4, 0);
    const uint32_t model = (uint32_t)b.Contents.size(); b.Contents.resize(model + 0x98, 0);
    const uint32_t material = (uint32_t)b.Contents.size(); b.Contents.resize(material + 0x2C, 0);
    const uint32_t mesh = (uint32_t)b.Contents.size(); b.Contents.resize(mesh + 0x38, 0);
    const uint32_t sub = (uint32_t)b.Contents.size(); b.Contents.resize(sub + 0x34, 0);
    const uint32_t texture = (uint32_t)b.Contents.size(); b.Contents.resize(texture + 0x20, 0);
    b.Pointer(b.Contents, 0, 0, 0, modelList); Put32(b.Contents, 4, 1);
    b.Pointer(b.Contents, 0, 36, 0, textureList); Put32(b.Contents, 40, 1);
    b.Pointer(b.Contents, 0, modelList, 0, model);
    b.Pointer(b.Contents, 0, textureList, 0, texture);
    b.Pointer(b.Contents, 0, model + 0x34, 0, material); Put32(b.Contents, model + 0x38, 1);
    b.Pointer(b.Contents, 0, model + 0x40, 0, mesh); Put32(b.Contents, model + 0x44, 1);
    b.Pointer(b.Contents, 0, model + 0x84, 1, b.String("m"));
    b.Pointer(b.Contents, 0, material + 0x1C, 1, b.String("tex"));
    b.Pointer(b.Contents, 0, material + 0x28, 1, b.String("mat"));
    // vertices: position (3 floats), texcoord (2 floats): stride 20; indices 0, 1, 2 (16 bits)
    const uint32_t vertices = (uint32_t)b.Raw.size();
    const float vdata[3][5] = {{0, 0, 0, 0, 0}, {1, 0, 0, 1, 0}, {0, 0, 2, 0, 1}};
    for (auto& v : vdata) for (float f : v) Push32(b.Raw, Bits(f));
    const uint32_t indices = (uint32_t)b.Raw.size();
    Push16(b.Raw, 0); Push16(b.Raw, 1); Push16(b.Raw, 2); Push16(b.Raw, 0);
    const uint32_t texels = (uint32_t)b.Raw.size();
    b.Raw.insert(b.Raw.end(), rgb565.begin(), rgb565.end());
    // mesh commands: attribute formats (position: float x3 = 0xB, texcoord: float x2 = 0x7), buffer,
    // attribute order, stride 20 and 2 attributes, attribute names (position 0, texcoord0 4), scales
    // (uniform 7 x, 8 x) and a position offset (uniform 6) of x + 10
    std::vector<uint32_t> mc;
    Cmd(mc, 0x201, {0x7B});
    Cmd(mc, 0x202, {0});
    const size_t bufferWord = mc.size(); Cmd(mc, 0x203, {0});
    Cmd(mc, 0x204, {0x10});
    Cmd(mc, 0x205, {20u << 16 | 2u << 28});
    Cmd(mc, 0x242, {1});
    Cmd(mc, 0x2BB, {0x40});
    Uniform(mc, 6, 10, 0, 0, 0);
    Uniform(mc, 7, 1, 1, 1, 1);
    Uniform(mc, 8, 1, 1, 1, 1);
    std::vector<uint32_t> sc;
    const size_t indexWord = sc.size(); Cmd(sc, 0x227, {0});
    Cmd(sc, 0x228, {3});
    Cmd(sc, 0x25E, {0});
    std::vector<uint32_t> tc;
    Cmd(tc, 0x82, {8u << 16 | 8u});
    const size_t texWord = tc.size(); Cmd(tc, 0x85, {0});
    auto commands = [&](const std::vector<uint32_t>& words, size_t pointerWord, uint32_t target, uint32_t value) {
        const uint32_t at = (uint32_t)b.Commands.size();
        for (uint32_t x : words) Push32(b.Commands, x);
        b.Pointer(b.Commands, 2, at + pointerWord * 4, target, value);
        return at;
    };
    b.Pointer(b.Contents, 0, mesh + 8, 2, commands(mc, bufferWord, 6, vertices)); Put32(b.Contents, mesh + 12, (uint32_t)mc.size());
    b.Pointer(b.Contents, 0, mesh + 0x10, 0, sub); Put32(b.Contents, mesh + 0x14, 1);
    b.Pointer(b.Contents, 0, sub + 0x2C, 2, commands(sc, indexWord, 7, indices)); Put32(b.Contents, sub + 0x30, (uint32_t)sc.size());
    b.Pointer(b.Contents, 0, texture, 2, commands(tc, texWord, 5, texels)); Put32(b.Contents, texture + 4, (uint32_t)tc.size());
    b.Contents[texture + 0x18] = 3; // RGB565
    b.Pointer(b.Contents, 0, texture + 0x1C, 1, b.String("tex"));
    const Bytes file = b.Build();

    const Bch r = Bch::Read(file);
    check(r.Version == 0x21 && r.Models.size() == 1 && r.Models[0].Name == "m" && r.Textures.size() == 1, "BCH: header, relocation, a model and a texture found");
    if (r.Models.size() == 1 && r.Models[0].Meshes.size() == 1 && r.Textures.size() == 1)
    {
        const BchModel& m = r.Models[0];
        check(m.Materials.size() == 1 && m.Materials[0].Name == "mat" && m.Materials[0].Texture[0] == "tex", "BCH: material and its texture name");
        const BchMesh& me = m.Meshes[0];
        check(me.Vertices.size() == 3 && me.Triangles == std::vector<uint32_t>({0, 1, 2}), "BCH: 16-bit index buffer, one triangle, three vertices");
        check(me.Vertices[1].Position[0] == 11 && me.Vertices[2].Position[2] == 2 && me.Vertices[0].Position[0] == 10 && me.Vertices[2].TexCoord[1] == 1,
              "BCH: positions (with the uniform offset) and texture coordinates");
        const BchTexture& tx = r.Textures[0];
        check(tx.Name == "tex" && tx.Width == 8 && tx.Height == 8 && tx.Format == 3 && tx.Data == rgb565, "BCH: texture size, format, data from its commands");
        check(Slice(file, tx.DataOffset, tx.Data.size()) == tx.Data, "BCH: the texture data's offset in the file (to patch it in place)");
        std::vector<GltfPart> parts;
        std::vector<GltfMaterial> mats;
        const float origin[3] = {0, 0, 0};
        AppendBchModel(m, r.Textures, origin, parts, mats);
        check(parts.size() == 1 && mats.size() == 1 && !mats[0].Png.empty() && parts[0].Mesh.Vertices[2].TexCoord[1] == 8.0f,
              "BCH to glTF: textured, texture v kept (rows are stored bottom first and decoded in that order, so v indexes them directly)");
    }
    // writing: the mesh's triangle replaced by a quad (two triangles), positions given in the world (the
    // writer takes the mesh's offset of x + 10 off), then read back through the same reader
    BchGeometry quad;
    quad.Mesh = 0;
    const float corners[4][2] = {{10, 0}, {14, 0}, {14, 4}, {10, 4}};
    for (const auto& c : corners)
    {
        BchVertex v;
        v.Position[0] = c[0]; v.Position[2] = c[1];
        v.TexCoord[0] = (c[0] - 10) / 4; v.TexCoord[1] = c[1] / 4;
        quad.Vertices.push_back(v);
    }
    quad.Triangles = {0, 1, 2, 0, 2, 3};
    const Bytes written = BchReplaceGeometry(file, 0, {quad});
    const Bch back = Bch::Read(written);
    if (back.Models.size() == 1 && back.Models[0].Meshes.size() == 1 && back.Textures.size() == 1)
    {
        const BchMesh& me = back.Models[0].Meshes[0];
        bool same = me.Vertices.size() == 4;
        for (size_t i = 0; same && i < 4; i++)
            same = me.Vertices[i].Position[0] == quad.Vertices[i].Position[0] && me.Vertices[i].Position[2] == quad.Vertices[i].Position[2] &&
                   me.Vertices[i].TexCoord[0] == quad.Vertices[i].TexCoord[0] && me.Vertices[i].TexCoord[1] == quad.Vertices[i].TexCoord[1];
        check(same && me.Triangles == quad.Triangles, "BCH writer: a mesh's geometry replaced, read back the same (positions less the mesh's offset)");
        check(back.Models[0].Name == "m" && back.Models[0].Materials[0].Texture[0] == "tex" && back.Textures[0].Data == rgb565,
              "BCH writer: the model's name, material and texture kept");
        check(written.size() > file.size() && written.size() % 4 == 0 && Slice(written, 0, 8) == Slice(file, 0, 8), "BCH writer: the file grown, its header's start kept");
        check(Bch::Read(BchReplaceGeometry(written, 0, {quad})).Models[0].Meshes[0].Triangles == quad.Triangles, "BCH writer: a written file written again");
    }
    // the replaced buffers removed: a 400-vertex grid written, then the quad over it; the grid's
    // buffers (whole 0x80 blocks) leave the file, the texture after them still reads
    BchGeometry grid;
    for (int z = 0; z < 20; z++) for (int x = 0; x < 20; x++) { BchVertex v; v.Position[0] = 10.0f + x; v.Position[2] = (float)z; grid.Vertices.push_back(v); }
    for (int z = 0; z < 19; z++) for (int x = 0; x < 19; x++)
    {
        const uint32_t a = z * 20 + x;
        for (uint32_t i : {a, a + 20, a + 21, a, a + 21, a + 1}) grid.Triangles.push_back(i);
    }
    const Bytes big = BchReplaceGeometry(file, 0, {grid});
    const Bytes small = BchReplaceGeometry(big, 0, {quad});
    const Bch smallRead = Bch::Read(small);
    check(Bch::Read(big).Models[0].Meshes[0].Triangles == grid.Triangles && small.size() + 8000 < big.size() &&
              smallRead.Models[0].Meshes[0].Triangles == quad.Triangles && smallRead.Textures[0].Data == rgb565,
          "BCH writer: a replaced mesh's buffers removed (" + std::to_string(big.size()) + " -> " + std::to_string(small.size()) + " bytes), the texture kept");
    // a buffer something else still points into is kept: the texture pointed at the grid's index buffer, which
    // doesn't start on a 0x80 block; replacing the grid must leave those bytes where the texture finds them
    {
        Bytes shared = big;
        const BchSections sec = BchSections::Read(shared);
        const uint32_t indexAt = Bch::Read(shared).Models[0].Meshes[0].SubMeshes[0].IndexBuffer;
        const size_t texAt = Bch::Read(shared).Textures[0].DataOffset;
        bool repointed = false;
        for (const BchPointer& p : BchPointers(shared, sec))
            if (p.Target >= 4 && p.Target <= 8 && U32(shared, p.At) + sec.Raw == texAt) { Put32(shared, p.At, indexAt - sec.Raw); repointed = true; }
        const Bytes before = Bch::Read(shared).Textures[0].Data;
        const Bch after = Bch::Read(BchReplaceGeometry(shared, 0, {quad}));
        check(repointed && indexAt % 0x80 != 0 && after.Textures[0].Data == before && after.Models[0].Meshes[0].Triangles == quad.Triangles,
              "BCH writer: an old buffer another pointer reaches (short of its first 0x80 block) kept");
    }
    // a whole string replaced in place (a terrain model renamed for its new place): the model "m" becomes "n",
    // the material "mat" (which contains no whole "m") stays; a missing string or another length refused
    {
        const Bch moved = Bch::Read(BchReplaceString(file, "m", "n")); // "mat" ends with no "@m": kept
        check(moved.Models[0].Name == "n" && moved.Models[0].Materials[0].Name == "mat" && moved.Models[0].Meshes[0].Triangles == std::vector<uint32_t>({0, 1, 2}),
              "BCH writer: a model renamed in place, its material's name (holding the letter) kept");
        bool r1 = false, r2 = false;
        try { BchReplaceString(file, "m", "nn"); } catch (const FormatError&) { r1 = true; }
        try { BchReplaceString(file, "zz", "yy"); } catch (const FormatError&) { r2 = true; }
        check(r1 && r2, "BCH writer: a string of another length, or one the file lacks: refused");
    }
    // a material's texture renamed: the new name read back, the rest intact, the file still writable
    const Bytes renamed = BchSetTextureName(file, 0, 0, 0, "a_longer_texture_name");
    const Bch rn = Bch::Read(renamed);
    check(rn.Models[0].Materials[0].Texture[0] == "a_longer_texture_name" && rn.Models[0].Materials[0].Name == "mat" &&
              rn.Models[0].Meshes[0].Triangles == std::vector<uint32_t>({0, 1, 2}) && rn.Models[0].Meshes[0].Vertices[1].Position[0] == 11 &&
              rn.Textures[0].Name == "tex" && rn.Textures[0].Data == rgb565,
          "BCH writer: a material's texture renamed, the model's other names, mesh and texture kept");
    check(Bch::Read(BchReplaceGeometry(renamed, 0, {quad})).Models[0].Materials[0].Texture[0] == "a_longer_texture_name",
          "BCH writer: a renamed file's geometry replaced");
    refused = false;
    try { BchSetTextureName(file, 0, 0, 1, "x"); } catch (const FormatError&) { refused = true; }
    check(refused, "BCH writer: renaming a texture slot that names none refused");
    // a mesh with 8-bit indices (the relocation naming section 6): written with 16-bit ones
    Bytes narrow = file;
    {
        const BchSections sec = BchSections::Read(narrow);
        const std::vector<BchPointer> ps = BchPointers(narrow, sec);
        for (size_t i = 0; i < ps.size(); i++)
            if (ps[i].Target == 7) Put32(narrow, sec.Relocation + i * 4, (U32(narrow, sec.Relocation + i * 4) & ~(0xFu << 25)) | 6u << 25);
        narrow[sec.Raw + indices] = 0; narrow[sec.Raw + indices + 1] = 1; narrow[sec.Raw + indices + 2] = 2;
    }
    check(Bch::Read(narrow).Models[0].Meshes[0].Triangles == std::vector<uint32_t>({0, 1, 2}), "BCH: 8-bit index buffer read");
    check(Bch::Read(BchReplaceGeometry(narrow, 0, {quad})).Models[0].Meshes[0].Triangles == quad.Triangles, "BCH writer: 8-bit indices become 16-bit");
    refused = false;
    try { BchGeometry bad = quad; bad.Triangles = {0, 1, 4}; BchReplaceGeometry(file, 0, {bad}); } catch (const FormatError&) { refused = true; }
    bool refused2 = false;
    try { BchGeometry bad = quad; bad.Mesh = 1; BchReplaceGeometry(file, 0, {bad}); } catch (const FormatError&) { refused2 = true; }
    check(refused && refused2, "BCH writer: an index past the vertices, a mesh the model lacks: refused");

    refused = false;
    // the model list's entry (contents + 180), relocated by the contents' address: far past the end
    try { Bytes bad = file; Put32(bad, 0x44 + 15 * 12, 0x00FFFFFF); Bch::Read(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "BCH: a model pointer outside the file refused");

    {
        // the preview blends a material only when a mesh draws it in a layer above 0, as the game does; a layer-0 texture with
        // holes (leaves) is cut out, so it hides what is behind it
        BchModel model;
        BchMaterial leaves, band;
        leaves.Name = "leaves"; leaves.Texture[0] = "holes";
        band.Name = "band"; band.Texture[0] = "holes";
        model.Materials = {leaves, band};
        BchMesh opaque, blended;
        opaque.Material = 0; opaque.Layer = 0;
        blended.Material = 1; blended.Layer = 1;
        for (BchMesh* m : {&opaque, &blended}) { m->Vertices.resize(3); m->Triangles = {0, 1, 2}; }
        model.Meshes = {opaque, blended};
        BchTexture holes;
        holes.Name = "holes"; holes.Width = 8; holes.Height = 8; holes.Format = 0;
        holes.Data = Bytes(PicaTextureLength(8, 8, 0), 0); // RGBA8, every texel clear
        std::vector<GltfPart> parts;
        std::vector<GltfMaterial> mats;
        const float origin[3] = {0, 0, 0};
        AppendBchModel(model, {holes}, origin, parts, mats);
        check(mats.size() == 2 && !mats[0].AlphaBlend && mats[1].AlphaBlend, "a material blends in the preview by its mesh's layer, not by its texture's holes");
    }

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
