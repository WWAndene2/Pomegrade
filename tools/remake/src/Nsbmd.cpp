#include "Nsbmd.h"

#include "Png.h"

#include <algorithm>
#include <memory>

namespace remake
{

// the render program: which material each shape is drawn with
static void RunSbc(const Bytes& d, size_t at, size_t end, NsbmdModel& m)
{
    int material = -1;
    while (at < end)
    {
        const uint8_t op = U8(d, at);
        const uint8_t base = op & 0x1F;
        size_t params;
        switch (base)
        {
        case 0x00: params = 0; break;
        case 0x01: return;                                   // end
        case 0x02: params = 2; break;                        // visibility
        case 0x03: params = 1; break;                        // matrix restore
        case 0x04: params = 1; material = U8(d, at + 1); break; // bind material
        case 0x05:                                           // draw shape
            params = 1;
            if (U8(d, at + 1) < m.Shapes.size()) m.Shapes[U8(d, at + 1)].Material = material;
            break;
        case 0x06: params = op == 0x06 ? 3 : op == 0x66 ? 5 : 4; break; // node transform
        case 0x07: case 0x08: params = 1; break;
        case 0x09: params = 2 + (size_t)U8(d, at + 2) * 3; break;      // skinning
        case 0x0B: params = 0; break;                        // scale up/down
        case 0x0C: case 0x0D: params = 2; break;
        default: return; // unknown: stop; shapes keep what was bound so far
        }
        at += 1 + params;
    }
}

Nsbmd::Nsbmd(Bytes file) : Data(std::move(file))
{
    if (Text(Data, 0, 4) != "BMD0") throw FormatError("not an NSBMD (BMD0)");
    const size_t mdl0 = U32(Data, 16);
    if (Text(Data, mdl0, 4) != "MDL0") throw FormatError("NSBMD: no MDL0 block");
    for (const DictEntry& e : ReadDictionary(Data, mdl0 + 8))
    {
        NsbmdModel m;
        m.Name = e.Name;
        const size_t base = mdl0 + U32(Data, e.Data);
        const size_t sbc = base + U32(Data, base + 4), mat = base + U32(Data, base + 8), shp = base + U32(Data, base + 12);
        const int32_t scale = (int32_t)U32(Data, base + 0x1C);
        m.PosScale = scale > 0 ? scale / 4096.0f : 1.0f;
        for (const DictEntry& s : ReadDictionary(Data, shp))
        {
            const size_t at = shp + U32(Data, s.Data);
            NsbmdShape shape;
            shape.Name = s.Name;
            shape.DisplayList = Slice(Data, at + U32(Data, at + 8), U32(Data, at + 12));
            m.Shapes.push_back(std::move(shape));
        }
        const std::vector<DictEntry> mats = ReadDictionary(Data, mat + 4);
        for (const DictEntry& d : mats) m.Materials.push_back({d.Name, "", ""});
        // texture and palette name -> the materials using it
        auto pair = [&](size_t dict, bool texture) {
            for (const DictEntry& t : ReadDictionary(Data, dict))
            {
                const size_t list = mat + U16(Data, t.Data);
                const uint8_t n = U8(Data, t.Data + 2);
                for (uint8_t i = 0; i < n; i++)
                {
                    const uint8_t index = U8(Data, list + i);
                    if (index < m.Materials.size()) (texture ? m.Materials[index].Texture : m.Materials[index].Palette) = t.Name;
                }
            }
        };
        pair(mat + U16(Data, mat), true);
        pair(mat + U16(Data, mat + 2), false);
        // the program ends at opcode 0x01; the model's size bounds it, since the
        // blocks' order inside a model is not fixed
        RunSbc(Data, sbc, std::min(Data.size(), base + U32(Data, base)), m);
        ModelList.push_back(std::move(m));
    }
}

std::string ModelToGltf(const Nsbmd& file, size_t model, const Tex0* tex)
{
    const NsbmdModel& m = file.Models().at(model);
    std::unique_ptr<Tex0> own;
    const long at = Tex0::Find(file.File());
    if (at >= 0) { own = std::make_unique<Tex0>(file.File(), (size_t)at); tex = own.get(); }

    std::vector<GltfMaterial> materials;
    for (const NsbmdMaterial& nm : m.Materials)
    {
        GltfMaterial g;
        g.Name = nm.Name;
        if (tex)
            for (size_t t = 0; t < tex->Textures().size(); t++)
            {
                if (tex->Textures()[t].Name != nm.Texture) continue;
                int pal = -1;
                for (size_t p = 0; p < tex->Palettes().size(); p++)
                    if (tex->Palettes()[p] == nm.Palette) pal = (int)p;
                const TextureFormat& f = tex->Textures()[t].Format;
                try
                {
                    g.Png = EncodePng(f.Width, f.Height, tex->Decode(t, pal));
                    g.TexWidth = f.Width; g.TexHeight = f.Height;
                    g.AlphaBlend = f.Format == 1 || f.Format == 6;
                }
                catch (const FormatError&) {} // left untextured
            }
        materials.push_back(g);
    }
    std::vector<GltfPart> parts;
    for (const NsbmdShape& s : m.Shapes)
        parts.push_back({DecodeDisplayList(s.DisplayList), s.Material});
    return WriteGltf(parts, materials, m.PosScale);
}

}
