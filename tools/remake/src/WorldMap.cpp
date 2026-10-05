#include "WorldMap.h"

#include "NitroCompression.h"
#include "Nsbmd.h"
#include "Png.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace remake
{

static Bytes Plain(const Bytes& d)
{
    if (!IsLzCompressed(d)) return d;
    try { return LzDecompress(d); }
    catch (const FormatError&) { return d; }
}

WorldMap::WorldMap(MapMatrix matrix, const Narc& lands) : Matrix(std::move(matrix))
{
    Cells.resize(Matrix.LandData.size());
    for (size_t i = 0; i < Cells.size(); i++)
    {
        const int id = Matrix.LandData[i];
        if (id < 0) continue;
        if ((size_t)id >= lands.Count()) { Errors.push_back("cell " + std::to_string(i) + ": land data " + std::to_string(id) + " missing"); continue; }
        try { Cells[i] = LandData::Read(Plain(lands.Member(id))); }
        catch (const FormatError& e) { Errors.push_back("land data " + std::to_string(id) + ": " + e.what()); }
    }
}

std::string WorldMap::Json() const
{
    std::ostringstream o;
    o << "{\n  \"name\": \"" << Matrix.Name << "\", \"width\": " << Matrix.Width << ", \"height\": " << Matrix.Height << ",\n  \"cells\": [";
    bool first = true;
    for (uint32_t y = 0; y < Matrix.Height; y++)
        for (uint32_t x = 0; x < Matrix.Width; x++)
        {
            const size_t c = Matrix.Cell(x, y);
            if (Matrix.LandData[c] < 0) continue;
            o << (first ? "" : ",") << "\n    {\"x\": " << x << ", \"y\": " << y << ", \"landData\": " << Matrix.LandData[c]
              << ", \"header\": " << Matrix.Headers[c] << ", \"heightLayer\": " << Matrix.Heights[c];
            first = false;
            if (const auto& l = Cells[c])
            {
                size_t solid = 0;
                std::map<int, int> behaviours;
                for (uint16_t p : l->Permissions) { solid += LandData::Solid(p); behaviours[p & 0xFF]++; }
                o << ", \"solidTiles\": " << solid << ", \"behaviours\": {";
                bool b1 = true;
                for (auto& [b, n] : behaviours) { o << (b1 ? "" : ", ") << "\"" << b << "\": " << n; b1 = false; }
                o << "}, \"modelBytes\": " << l->Model.size() << ", \"heightBytes\": " << l->Heights.size() << ", \"buildings\": [";
                for (size_t b = 0; b < l->Buildings.size(); b++)
                {
                    const LandBuilding& x = l->Buildings[b];
                    o << (b ? ", " : "") << "{\"model\": " << x.Model << ", \"position\": [" << x.Position[0] << ", " << x.Position[1] << ", " << x.Position[2] << "]}";
                }
                o << "]";
            }
            else o << ", \"error\": true";
            o << "}";
        }
    o << "\n  ],\n  \"errors\": [";
    for (size_t i = 0; i < Errors.size(); i++) o << (i ? ", " : "") << "\"" << Errors[i] << "\"";
    o << "]\n}\n";
    return o.str();
}

Bytes WorldMap::CollisionPng() const
{
    const uint32_t w = Matrix.Width * LandTiles, h = Matrix.Height * LandTiles;
    Bytes rgba((size_t)w * h * 4);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++)
        {
            uint8_t* p = &rgba[((size_t)y * w + x) * 4];
            const auto& l = Cells[Matrix.Cell(x / LandTiles, y / LandTiles)];
            p[3] = 255;
            if (!l) { p[0] = p[1] = p[2] = 128; continue; }
            const uint16_t perm = l->Permissions[(y % LandTiles) * LandTiles + x % LandTiles];
            if (LandData::Solid(perm)) { p[0] = p[1] = p[2] = 32; continue; }
            // behaviour 0 (plain ground) white, the others a stable colour each
            const uint32_t b = perm & 0xFF, k = b * 2654435761u;
            p[0] = b ? 96 + (k >> 8 & 127) : 255; p[1] = b ? 96 + (k >> 16 & 127) : 255; p[2] = b ? 96 + (k >> 24 & 127) : 255;
        }
    return EncodePng(w, h, rgba);
}

std::string WorldMap::Gltf(const Tex0* tex, const Narc* buildings, const Tex0* buildingTex, float* cellSizeOut, float scale,
                           const std::vector<CellTextures>* perCell) const
{
    // the terrain models, read once, and the cell size they span
    std::map<size_t, Nsbmd> terrain;
    float widest = 0;
    for (size_t c = 0; c < Cells.size(); c++)
    {
        if (!Cells[c] || Cells[c]->Model.empty()) continue;
        try
        {
            Nsbmd m(Cells[c]->Model);
            if (m.Models().empty()) continue;
            std::vector<GltfPart> parts;
            std::vector<GltfMaterial> mats;
            const float origin[3] = {0, 0, 0};
            AppendModel(m, 0, nullptr, origin, parts, mats);
            float lo = 1e30f, hi = -1e30f;
            for (const GltfPart& p : parts)
                for (const GxVertex& v : p.Mesh.Vertices) { lo = std::min(lo, v.Position[0]); hi = std::max(hi, v.Position[0]); }
            if (hi > lo) widest = std::max(widest, hi - lo);
            terrain.emplace(c, std::move(m));
        }
        catch (const FormatError&) {} // reported by the JSON's model size; the scene skips it
    }
    const float cell = widest > 0 ? std::exp2(std::round(std::log2(widest))) : 1.0f;
    if (cellSizeOut) *cellSizeOut = cell;

    std::vector<GltfPart> parts;
    std::vector<GltfMaterial> materials;
    std::map<uint32_t, std::optional<Nsbmd>> buildingModels;
    for (auto& [c, m] : terrain)
    {
        const uint32_t x = (uint32_t)(c % Matrix.Width), y = (uint32_t)(c / Matrix.Width);
        const float centre[3] = {(x + 0.5f) * cell, 0, (y + 0.5f) * cell};
        const CellTextures own = perCell && c < perCell->size() ? (*perCell)[c] : CellTextures{};
        try { AppendModel(m, 0, own.Map ? own.Map : tex, centre, parts, materials); } catch (const FormatError&) {}
        if (!buildings) continue;
        for (const LandBuilding& b : Cells[c]->Buildings)
        {
            auto it = buildingModels.find(b.Model);
            if (it == buildingModels.end())
            {
                std::optional<Nsbmd> bm;
                try { if (b.Model < buildings->Count()) bm.emplace(Plain(buildings->Member(b.Model))); } catch (const FormatError&) {}
                it = buildingModels.emplace(b.Model, std::move(bm)).first;
            }
            if (!it->second || it->second->Models().empty()) continue;
            // positions are in tiles from the cell's centre (see LandBuilding)
            const float tile = cell / LandTiles;
            const float at[3] = {centre[0] + b.Position[0] * tile, centre[1] + b.Position[1] * tile, centre[2] + b.Position[2] * tile};
            try { AppendModel(*it->second, 0, own.Buildings ? own.Buildings : buildingTex, at, parts, materials); } catch (const FormatError&) {}
        }
    }
    return WriteGltf(parts, materials, scale);
}

}
