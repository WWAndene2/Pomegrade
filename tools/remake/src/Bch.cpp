#include "Bch.h"

#include "PicaCommands.h"
#include "PicaTexture.h"
#include "Png.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace remake
{

namespace
{

// the file once relocated: absolute pointers, bounds-checked reads
struct Reader
{
    Bytes D;
    uint32_t P(size_t at) const { return U32(D, at); }
    uint16_t H(size_t at) const { return U16(D, at); }
    float F(size_t at) const { const uint32_t v = U32(D, at); float f; memcpy(&f, &v, 4); return f; }
    std::string Str(uint32_t at) const { return at ? Text(D, at, 256) : std::string(); }
    std::vector<uint32_t> Words(uint32_t at, uint32_t count) const
    {
        if ((uint64_t)at + (uint64_t)count * 4 > D.size()) throw FormatError("BCH: a command list runs past the file");
        std::vector<uint32_t> w(count);
        for (uint32_t i = 0; i < count; i++) w[i] = U32(D, at + i * 4);
        return w;
    }
};

}

Bch Bch::Read(const Bytes& data)
{
    if (!Is(data)) throw FormatError("not a BCH");
    Bch out;
    out.Version = data[4];
    const bool ext = out.Version >= 0x21; // the RawExt section exists from version 0x21
    const size_t a = 8, n = ext ? 6 : 5;  // addresses: contents, strings, commands, raw data, [raw ext], relocation
    const uint32_t contents = U32(data, a), strings = U32(data, a + 4), commands = U32(data, a + 8), raw = U32(data, a + 12);
    const uint32_t rawExt = ext ? U32(data, a + 16) : 0, reloc = U32(data, a + 4 * (n - 1));
    const uint32_t relocLength = U32(data, a + 4 * n + 4 * (n - 1));

    // relocation: each word names a pointer (its section and its offset there, in words except for
    // pointers to strings) and the section it points into, whose address is added to it
    Reader r{data};
    auto base = [&](uint32_t section) -> uint32_t {
        switch (section)
        {
        case 0: return contents;
        case 1: return strings;
        case 2: case 3: return commands;
        case 4: case 5: case 6: case 8: return raw;
        case 7: return raw | 0x80000000u;          // 16-bit index buffers: flagged in the address
        case 9: case 10: case 11: case 13: return rawExt;
        case 12: return rawExt | 0x80000000u;
        case 14: return 0;                          // "base address": from the file's start
        }
        throw FormatError("BCH: unknown relocation section " + std::to_string(section));
    };
    for (uint32_t off = 0; off + 4 <= relocLength; off += 4)
    {
        const uint32_t v = U32(data, reloc + off);
        uint32_t ptr = v & 0x1FFFFFF, target = (v >> 25) & 0xF;
        const uint32_t source = v >> 29;
        // older versions numbered the sections differently (SPICA's GetLegacyRelocDiff)
        if (out.Version > 7 && out.Version < 0x21 && target >= 6) target -= 1;
        else if (out.Version < 7 && target >= 3) target += 1;
        if (target != 1) ptr <<= 2;
        const size_t at = (size_t)base(source) + ptr;
        if (source == 7 || at + 4 > r.D.size()) throw FormatError("BCH: a relocation outside the file");
        const uint32_t now = U32(r.D, at) + base(target);
        for (int k = 0; k < 4; k++) r.D[at + k] = (uint8_t)(now >> (8 * k));
    }

    // contents: 15 dictionaries (values pointer, count, name tree); models are the first, textures the fourth
    const uint32_t modelList = r.P(contents), modelCount = r.P(contents + 4);
    const uint32_t textureList = r.P(contents + 36), textureCount = r.P(contents + 40);
    if (modelCount > 4096 || textureCount > 65536) throw FormatError("BCH: implausible model or texture count");

    for (uint32_t t = 0; t < textureCount; t++)
    {
        const uint32_t at = r.P(textureList + t * 4);
        // texture: 3 command lists (pointer, count), u8 format, u8 mipmaps, (aligned) name
        BchTexture tex;
        tex.Format = r.D.at(at + 0x18);
        tex.Name = r.Str(r.P(at + 0x1C));
        const PicaCommands cmd = PicaCommands::Parse(r.Words(r.P(at), r.P(at + 4)));
        const uint32_t dim = cmd.Last(0x82);
        tex.Height = dim & 0x7FF; tex.Width = (dim >> 16) & 0x7FF;
        const uint32_t address = cmd.Last(0x85);
        if (address && tex.Width && tex.Height && tex.Format < 14)
        {
            tex.Data = Slice(r.D, address, PicaTextureLength(tex.Width, tex.Height, tex.Format));
            tex.DataOffset = address;
        }
        out.Textures.push_back(std::move(tex));
    }

    for (uint32_t m = 0; m < modelCount; m++)
    {
        const uint32_t at = r.P(modelList + m * 4);
        // model: u8 flags, u8 bone scaling, u16 silhouettes, 3x4 matrix, materials dictionary (0x34),
        // meshes (0x40), 4 mesh layers, [sub-mesh cullings], skeleton dictionary, node visibility,
        // name (0x84 from version 7, without the cullings 8 earlier)
        BchModel model;
        const uint32_t nameAt = out.Version >= 7 ? 0x84 : 0x7C;
        model.Name = r.Str(r.P(at + nameAt));
        const uint32_t matList = r.P(at + 0x34), matCount = r.P(at + 0x38);
        const uint32_t meshList = r.P(at + 0x40), meshCount = r.P(at + 0x44);
        if (matCount > 4096 || meshCount > 65536) throw FormatError("BCH: implausible material or mesh count");
        for (uint32_t i = 0; i < matCount; i++)
        {
            // material (inline, 0x2C from version 0x21): parameters, 3 textures, texture commands, texture
            // mappers, 3 texture names, its name
            const uint32_t mat = matList + i * (ext ? 0x2C : 0x2C);
            if (!ext) throw FormatError("BCH: materials before version 0x21 are not read");
            BchMaterial bm;
            for (int k = 0; k < 3; k++) bm.Texture[k] = r.Str(r.P(mat + 0x1C + k * 4));
            bm.Name = r.Str(r.P(mat + 0x28));
            model.Materials.push_back(bm);
        }
        for (uint32_t i = 0; i < meshCount; i++)
        {
            // mesh (inline, 0x38): u16 material, u8 flags, u16 node, u16 key (layer: bits 8-9),
            // enable commands, sub-meshes, disable commands, centre, ...
            const uint32_t mesh = meshList + i * 0x38;
            BchMesh bm;
            bm.Material = r.H(mesh);
            bm.Layer = (r.H(mesh + 6) >> 8) & 3;
            const PicaCommands cmd = PicaCommands::Parse(r.Words(r.P(mesh + 8), r.P(mesh + 12)));
            uint64_t formats = 0, attributes = 0, permutation = 0;
            uint32_t buffer = 0, stride = 0, total = 0, fixedIndex = 0;
            uint32_t fixed[12][3] = {};
            for (const PicaCommand& c : cmd.List)
            {
                const uint32_t p = c.Params[0];
                switch (c.Register)
                {
                case 0x201: formats |= p; break;
                case 0x202: formats |= (uint64_t)p << 32; break;
                case 0x203: buffer = p; break;
                case 0x204: attributes |= p; break;
                case 0x205: attributes |= (uint64_t)(p & 0xFFFF) << 32; stride = (p >> 16) & 0xFF; break;
                case 0x232: fixedIndex = p % 12; break;
                case 0x233: case 0x234: case 0x235: fixed[fixedIndex][c.Register - 0x233] = p; break;
                case 0x242: total = p + 1; break;
                case 0x2BB: permutation |= p; break;
                case 0x2BC: permutation |= (uint64_t)p << 32; break;
                }
            }
            const float* u6 = cmd.VertexUniforms[6];
            const float* u7 = cmd.VertexUniforms[7];
            const float* u8 = cmd.VertexUniforms[8];
            struct Attribute { int Name, Format, Elements; float Scale; };
            std::vector<Attribute> attrs;
            float fixedColour[4] = {1, 1, 1, 1};
            bool hasFixedColour = false;
            for (uint32_t k = 0; k < total && k < 12; k++)
            {
                if ((formats >> (48 + k)) & 1)
                {
                    if (((permutation >> (k * 4)) & 0xF) == 3) // a colour for every vertex
                    {
                        const uint32_t* w = fixed[k];
                        fixedColour[0] = PicaFloat24(w[2] & 0xFFFFFF);
                        fixedColour[1] = PicaFloat24((w[2] >> 24) | ((w[1] & 0xFFFF) << 8));
                        fixedColour[2] = PicaFloat24((w[1] >> 16) | ((w[0] & 0xFF) << 16));
                        fixedColour[3] = PicaFloat24(w[0] >> 8);
                        hasFixedColour = true;
                    }
                    continue;
                }
                const int slot = (int)((attributes >> (k * 4)) & 0xF);
                const int name = (int)((permutation >> (slot * 4)) & 0xF), fmt = (int)((formats >> (slot * 4)) & 0xF);
                float scale = 1;
                switch (name)
                {
                case 0: scale = u7[0]; break; case 1: scale = u7[1]; break; case 2: scale = u7[2]; break; case 3: scale = u7[3]; break;
                case 4: scale = u8[0]; break; case 5: scale = u8[1]; break; case 6: scale = u8[2]; break; case 8: scale = u8[3]; break;
                }
                attrs.push_back({name, fmt & 3, (fmt >> 2) + 1, scale});
            }

            // sub-meshes (inline, 0x34): u8 skinning, u16 bone count, 20 bone indices, commands
            uint32_t maxIndex = 0;
            std::vector<std::pair<std::vector<uint32_t>, uint32_t>> lists; // indices, primitive mode
            const uint32_t subList = r.P(mesh + 0x10), subCount = r.P(mesh + 0x14);
            if (subCount > 65536) throw FormatError("BCH: implausible sub-mesh count");
            for (uint32_t s = 0; s < subCount; s++)
            {
                const uint32_t sub = subList + s * 0x34;
                const PicaCommands sc = PicaCommands::Parse(r.Words(r.P(sub + 0x2C), r.P(sub + 0x30)));
                const uint32_t ib = sc.Last(0x227), count = sc.Last(0x228), mode = sc.Last(0x25E) >> 8;
                const bool wide = ib >> 31;
                std::vector<uint32_t> idx(count);
                for (uint32_t k = 0; k < count; k++) idx[k] = wide ? r.H((ib & 0x7FFFFFFF) + k * 2) : r.D.at((ib & 0x7FFFFFFF) + k);
                for (uint32_t v : idx) maxIndex = std::max(maxIndex, v);
                lists.push_back({std::move(idx), mode});
            }
            if (!stride || lists.empty()) { model.Meshes.push_back(bm); continue; }

            // vertices: each attribute 2-byte aligned unless bytes (SPICA's AlignStream)
            const uint32_t vertexCount = maxIndex + 1;
            if ((uint64_t)buffer + (uint64_t)vertexCount * stride > r.D.size()) throw FormatError("BCH: a vertex buffer runs past the file");
            bm.Vertices.resize(vertexCount);
            for (uint32_t v = 0; v < vertexCount; v++)
            {
                size_t at = buffer + (size_t)v * stride;
                BchVertex& out = bm.Vertices[v];
                if (hasFixedColour) std::copy(fixedColour, fixedColour + 4, out.Colour);
                for (const Attribute& attr : attrs)
                {
                    if (attr.Format >= 2) at += at & 1;
                    float e[4] = {0, 0, 0, 1};
                    for (int k = 0; k < attr.Elements; k++)
                        switch (attr.Format)
                        {
                        case 0: e[k] = (int8_t)r.D.at(at); at += 1; break;
                        case 1: e[k] = r.D.at(at); at += 1; break;
                        case 2: e[k] = (int16_t)r.H(at); at += 2; break;
                        case 3: e[k] = r.F(at); at += 4; break;
                        }
                    for (int k = 0; k < 4; k++) e[k] *= attr.Scale;
                    switch (attr.Name)
                    {
                    case 0: for (int k = 0; k < 3; k++) out.Position[k] = e[k] + u6[k]; break;
                    case 1: std::copy(e, e + 3, out.Normal); break;
                    case 3: std::copy(e, e + 4, out.Colour); break;
                    case 4: std::copy(e, e + 2, out.TexCoord); break;
                    }
                }
            }
            // triangles from triangle lists, strips and fans
            for (const auto& [idx, mode] : lists)
            {
                if (mode == 0)
                    for (size_t k = 0; k + 2 < idx.size(); k += 3) { bm.Triangles.push_back(idx[k]); bm.Triangles.push_back(idx[k + 1]); bm.Triangles.push_back(idx[k + 2]); }
                else if (mode == 1)
                    for (size_t k = 0; k + 2 < idx.size(); k++)
                    {
                        if (idx[k] == idx[k + 1] || idx[k + 1] == idx[k + 2] || idx[k] == idx[k + 2]) continue; // degenerate joins
                        if (k & 1) { bm.Triangles.push_back(idx[k + 1]); bm.Triangles.push_back(idx[k]); bm.Triangles.push_back(idx[k + 2]); }
                        else { bm.Triangles.push_back(idx[k]); bm.Triangles.push_back(idx[k + 1]); bm.Triangles.push_back(idx[k + 2]); }
                    }
                else if (mode == 2)
                    for (size_t k = 1; k + 1 < idx.size(); k++) { bm.Triangles.push_back(idx[0]); bm.Triangles.push_back(idx[k]); bm.Triangles.push_back(idx[k + 1]); }
            }
            model.Meshes.push_back(std::move(bm));
        }
        out.Models.push_back(std::move(model));
    }
    return out;
}

void AppendBchModel(const BchModel& model, const std::vector<BchTexture>& textures, const float offset[3],
                    std::vector<GltfPart>& parts, std::vector<GltfMaterial>& materials)
{
    const int first = (int)materials.size();
    for (const BchMaterial& m : model.Materials)
    {
        GltfMaterial g;
        g.Name = m.Name;
        // the colour texture: slot 0, unless it holds "projection_dummy" (a projected shadow's
        // placeholder: ORAS's trees and grass edges put their own texture in slot 1)
        std::string colour = m.Texture[0];
        if (colour.empty() || colour == "projection_dummy") colour = !m.Texture[1].empty() ? m.Texture[1] : colour;
        for (const BchTexture& t : textures)
        {
            if (t.Name != colour || t.Data.empty()) continue;
            try
            {
                const Bytes rgba = PicaTextureDecode(t.Data, t.Width, t.Height, t.Format);
                g.Png = EncodePng(t.Width, t.Height, rgba);
                g.TexWidth = t.Width; g.TexHeight = t.Height;
                for (size_t p = 3; p < rgba.size(); p += 4) if (rgba[p] < 255) { g.AlphaBlend = true; break; }
            }
            catch (const FormatError&) {} // left untextured
            break;
        }
        materials.push_back(g);
    }
    for (const BchMesh& mesh : model.Meshes)
    {
        if (mesh.Triangles.empty()) continue;
        GltfPart part;
        part.Material = mesh.Material < model.Materials.size() ? first + mesh.Material : -1;
        const GltfMaterial* mat = part.Material >= 0 ? &materials[part.Material] : nullptr;
        for (const BchVertex& v : mesh.Vertices)
        {
            GxVertex g;
            for (int k = 0; k < 3; k++) { g.Position[k] = v.Position[k] + offset[k]; g.Normal[k] = v.Normal[k]; }
            // the glTF writer divides texel coordinates by the texture's size; the GPU's v runs bottom-up
            g.TexCoord[0] = v.TexCoord[0] * (mat ? mat->TexWidth : 1);
            g.TexCoord[1] = (1.0f - v.TexCoord[1]) * (mat ? mat->TexHeight : 1);
            auto c5 = [](float c) { return (uint16_t)std::clamp((int)std::lround(c * 31.0f), 0, 31); };
            g.Colour = (uint16_t)(c5(v.Colour[0]) | c5(v.Colour[1]) << 5 | c5(v.Colour[2]) << 10);
            part.Mesh.Vertices.push_back(g);
        }
        part.Mesh.Triangles = mesh.Triangles;
        parts.push_back(std::move(part));
    }
}

}
