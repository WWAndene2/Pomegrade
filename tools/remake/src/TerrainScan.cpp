#include "TerrainScan.h"
#include "GxDisplayList.h"
#include "Nsbmd.h"
#include "NitroCompression.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }

std::string BaseMaterialName(const std::string& name)
{
    size_t end = name.size();
    while (end > 0 && std::isdigit((unsigned char)name[end - 1])) end--;
    if (end < name.size() && end >= 3 && name.compare(end - 3, 3, "_lm") == 0) return name.substr(0, end - 3);
    return name;
}

namespace
{

struct DecodedMaterial
{
    std::string Name;
    uint32_t Width = 0, Height = 0;
    Bytes Rgba; // empty: untextured
};

// each of a model's materials with its texture decoded, paired as the glTF export pairs them
std::vector<DecodedMaterial> DecodeMaterials(const Nsbmd& file, const NsbmdModel& model, const Tex0* fallback)
{
    std::unique_ptr<Tex0> own;
    const Tex0* tex = fallback;
    const long at = Tex0::Find(file.File());
    if (at >= 0) { own = std::make_unique<Tex0>(file.File(), (size_t)at); tex = own.get(); }
    std::vector<DecodedMaterial> out;
    for (const NsbmdMaterial& nm : model.Materials)
    {
        DecodedMaterial d;
        d.Name = BaseMaterialName(nm.Name);
        if (tex)
            for (size_t t = 0; t < tex->Textures().size(); t++)
            {
                if (tex->Textures()[t].Name != nm.Texture) continue;
                int pal = -1;
                for (size_t p = 0; p < tex->Palettes().size(); p++)
                    if (tex->Palettes()[p] == nm.Palette) pal = (int)p;
                const TextureFormat& f = tex->Textures()[t].Format;
                try { d.Rgba = tex->Decode(t, pal); d.Width = f.Width; d.Height = f.Height; }
                catch (const FormatError&) {} // left untextured
            }
        out.push_back(std::move(d));
    }
    return out;
}

}

TerrainScan TerrainScan::Run(const PlatinumWorld& plat, int left, int top, int tiles, int r)
{
    TerrainScan scan;
    scan.Left = left; scan.Top = top; scan.Tiles = tiles; scan.R = r;
    const int n = tiles * r;
    scan.Samples.resize((size_t)n * n);
    const WorldMap& world = plat.World;
    const float tile = NdsTileUnits;
    const float cellSize = tile * LandTiles;
    const float step = 1.0f / r; // in tiles

    // one cell more on each side: a building stands over its cell's edge
    const int firstCol = std::max(0, left / (int)LandTiles - 1), lastCol = std::min((int)world.Matrix.Width - 1, (left + tiles - 1) / (int)LandTiles + 1);
    const int firstRow = std::max(0, top / (int)LandTiles - 1), lastRow = std::min((int)world.Matrix.Height - 1, (top + tiles - 1) / (int)LandTiles + 1);
    for (int cy = firstRow; cy <= lastRow; cy++)
        for (int cx = firstCol; cx <= lastCol; cx++)
        {
            const size_t c = world.Matrix.Cell(cx, cy);
            if (!world.Cells[c] || world.Cells[c]->Model.empty()) continue;
            // one model placed at an origin (model units, from the matrix's corner), its textures from the cell's area
            auto scanModel = [&](const Nsbmd& file, const Tex0* fallback, const float origin[3]) {
                if (file.Models().empty()) return;
                const NsbmdModel& model = file.Models()[0];
                const std::vector<DecodedMaterial> mats = DecodeMaterials(file, model, fallback);
                for (const NsbmdShape& shape : model.Shapes)
                {
                    if (shape.Material < 0 || (size_t)shape.Material >= mats.size()) continue;
                    const DecodedMaterial& mat = mats[shape.Material];
                    const GxMesh mesh = DecodeDisplayList(shape.DisplayList);
                    auto pos = [&](const GxVertex& v, float* out) {
                        for (int k = 0; k < 3; k++) out[k] = (v.Position[k] * model.PosScale + origin[k]) / tile;
                    };
                    for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
                    {
                        const GxVertex* v[3] = {&mesh.Vertices[mesh.Triangles[t]], &mesh.Vertices[mesh.Triangles[t + 1]], &mesh.Vertices[mesh.Triangles[t + 2]]};
                        float p[3][3];
                        for (int k = 0; k < 3; k++) pos(*v[k], p[k]);
                        const float d = (p[1][2] - p[2][2]) * (p[0][0] - p[2][0]) + (p[2][0] - p[1][0]) * (p[0][2] - p[2][2]);
                        if (std::fabs(d) < 1e-9f) continue;
                        const float x0 = std::min({p[0][0], p[1][0], p[2][0]}), x1 = std::max({p[0][0], p[1][0], p[2][0]});
                        const float z0 = std::min({p[0][2], p[1][2], p[2][2]}), z1 = std::max({p[0][2], p[1][2], p[2][2]});
                        // the samples (centres of 1/R tiles) inside the triangle's box, in the window. The geometry
                        // sits on the sample lattice, so a sample can lie exactly on a triangle's edge: the box excludes
                        // its minimum edge and includes its maximum, as the prototype that the first phone tests used did
                        const int c0 = std::max((int)std::floor(x0 / step - 0.5f) + 1, left * r), c1 = std::min((int)std::floor(x1 / step - 0.5f), (left + tiles) * r - 1);
                        const int r0 = std::max((int)std::floor(z0 / step - 0.5f) + 1, top * r), r1 = std::min((int)std::floor(z1 / step - 0.5f), (top + tiles) * r - 1);
                        for (int gr = r0; gr <= r1; gr++)
                            for (int gc = c0; gc <= c1; gc++)
                            {
                                const float x = (gc + 0.5f) * step, z = (gr + 0.5f) * step;
                                const float l1 = ((p[1][2] - p[2][2]) * (x - p[2][0]) + (p[2][0] - p[1][0]) * (z - p[2][2])) / d;
                                const float l2 = ((p[2][2] - p[0][2]) * (x - p[2][0]) + (p[0][0] - p[2][0]) * (z - p[2][2])) / d;
                                const float l3 = 1 - l1 - l2;
                                if (std::min({l1, l2, l3}) < -1e-6f) continue;
                                TerrainSample& s = scan.Samples[(size_t)(gr - top * r) * n + (gc - left * r)];
                                s.Materials.insert(mat.Name);
                                if (mat.Rgba.empty()) continue;
                                const float y = l1 * p[0][1] + l2 * p[1][1] + l3 * p[2][1];
                                // the texel under the sample; the texture repeats
                                const float u = (l1 * v[0]->TexCoord[0] + l2 * v[1]->TexCoord[0] + l3 * v[2]->TexCoord[0]) / mat.Width;
                                const float tv = (l1 * v[0]->TexCoord[1] + l2 * v[1]->TexCoord[1] + l3 * v[2]->TexCoord[1]) / mat.Height;
                                const float fu = u - std::floor(u), fv = tv - std::floor(tv);
                                const size_t px = std::min<size_t>((size_t)(fu * mat.Width), mat.Width - 1), py = std::min<size_t>((size_t)(fv * mat.Height), mat.Height - 1);
                                const uint8_t* texel = &mat.Rgba[(py * mat.Width + px) * 4];
                                if (texel[3] < 128) continue;
                                // the highest surface wins; at equal height the larger (R, G, B), so the result does not
                                // depend on the order the triangles are met in
                                const uint8_t* now = s.Rgb;
                                const bool lower = s.HasColour && (y < s.Height || (y == s.Height && std::lexicographical_compare(texel, texel + 3, now, now + 3)));
                                if (lower || (s.HasColour && y == s.Height && std::equal(texel, texel + 3, now))) continue;
                                s.HasColour = true; s.Height = y;
                                s.Rgb[0] = texel[0]; s.Rgb[1] = texel[1]; s.Rgb[2] = texel[2];
                            }
                    }
                }
            };
            const float centre[3] = {(cx + 0.5f) * cellSize, 0, (cy + 0.5f) * cellSize};
            scanModel(Nsbmd(world.Cells[c]->Model), plat.CellTex[c].Map, centre);
            for (const LandBuilding& b : world.Cells[c]->Buildings)
            {
                if (b.Model >= plat.BuildingModels.Count()) continue;
                try
                {
                    const float at[3] = {centre[0] + b.Position[0] * tile, centre[1] + b.Position[1] * tile, centre[2] + b.Position[2] * tile};
                    scanModel(Nsbmd(Plain(plat.BuildingModels.Member(b.Model))), plat.CellTex[c].Buildings, at);
                }
                catch (const FormatError&) {} // a building model that does not read is left out
            }
        }
    return scan;
}

}
