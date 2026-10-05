#include "TopView.h"
#include "BinLinker.h"
#include "Png.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <utility>

namespace remake
{

static constexpr int Tiles = 40;
static constexpr float TileUnits = 18;

static bool Has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

TopViewKind ClassifyMesh(const BchMaterial& m)
{
    const std::string names = m.Name + " " + m.Texture[0] + " " + m.Texture[1];
    if (Has(names, "chip_edge_tex") || Has(names, "chip_kusa_edge") || Has(names, "grass_edge")) return TopViewKind::Rim;
    if (Has(names, "decolate") || Has(names, "chip_alpha") || Has(names, "touka_alpha")) return TopViewKind::Blades;
    if (Has(names, "shadow")) return TopViewKind::Shadow;
    if (Has(names, "gake") || Has(names, "cliff")) return TopViewKind::Cliff;
    if (Has(names, "chip_wood")) return TopViewKind::Forest;
    if (Has(names, "wind") || Has(names, "light") || Has(names, "Comb")) return TopViewKind::Effect;
    if (Has(names, "river") || Has(names, "sea") || Has(names, "water") || Has(names, "lake")) return TopViewKind::Water;
    if (Has(names, "kusa_b") || Has(names, "pale")) return TopViewKind::Light;
    if (Has(names, "kusa") || Has(names, "grass") || Has(names, "ngrass")) return TopViewKind::Ground;
    if (Has(names, "soil") || Has(names, "sand") || Has(names, "road")) return TopViewKind::Path;
    if (Has(names, "house") || Has(names, "waku") || Has(names, "mado") || Has(names, "pokecen") || Has(names, "t101") || Has(names, "wall") || Has(names, "poke_gym"))
        return TopViewKind::Structure;
    return TopViewKind::Other;
}

void TopViewColour(TopViewKind kind, uint8_t rgb[3])
{
    static const uint8_t table[][3] = {{110, 190, 230}, {110, 100, 90}, {70, 100, 55}, {30, 120, 60}, {120, 220, 120}, {200, 170, 100}, {20, 60, 40}, {70, 130, 220}, {150, 150, 150}, {255, 255, 0}, {255, 0, 0}, {190, 90, 190}};
    std::memcpy(rgb, table[(int)kind], 3);
}

const char* TopViewName(TopViewKind kind)
{
    static const char* names[] = {"effect", "cliff", "forest floor", "ground", "lighter grass", "path", "shadow", "water", "structure", "grass blades", "forest rim", "other"};
    return names[(int)kind];
}

TopViewKind MeshKind(const BchModel& model, const BchMesh& mesh)
{
    const TopViewKind kind = ClassifyMesh(model.Materials[mesh.Material]);
    return kind == TopViewKind::Structure && mesh.Layer > 0 ? TopViewKind::Effect : kind;
}

namespace
{

struct Canvas
{
    int W, H;
    Bytes Rgba;
    Canvas(int w, int h) : W(w), H(h), Rgba((size_t)w * h * 4, 255) { for (size_t i = 0; i < Rgba.size(); i += 4) { Rgba[i] = 24; Rgba[i + 1] = 24; Rgba[i + 2] = 24; } }
    void Set(int x, int y, const uint8_t rgb[3], float alpha = 1)
    {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        uint8_t* p = &Rgba[((size_t)y * W + x) * 4];
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)std::lround(p[k] + (rgb[k] - p[k]) * alpha);
    }
};

void Triangle(Canvas& c, const float a[2], const float b[2], const float d[2], const uint8_t rgb[3], float alpha)
{
    const float area = (b[0] - a[0]) * (d[1] - a[1]) - (b[1] - a[1]) * (d[0] - a[0]);
    if (std::fabs(area) < 1e-6f) return;
    const int x0 = std::max(0, (int)std::floor(std::min({a[0], b[0], d[0]}))), x1 = std::min(c.W - 1, (int)std::ceil(std::max({a[0], b[0], d[0]})));
    const int y0 = std::max(0, (int)std::floor(std::min({a[1], b[1], d[1]}))), y1 = std::min(c.H - 1, (int)std::ceil(std::max({a[1], b[1], d[1]})));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            const float px = x + 0.5f, py = y + 0.5f;
            const float w0 = ((b[0] - px) * (d[1] - py) - (b[1] - py) * (d[0] - px)) / area;
            const float w1 = ((d[0] - px) * (a[1] - py) - (d[1] - py) * (a[0] - px)) / area;
            const float w2 = 1 - w0 - w1;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) c.Set(x, y, rgb, alpha);
        }
}

void Line(Canvas& c, const float a[2], const float b[2], const uint8_t rgb[3], int width)
{
    const int steps = std::max(1, (int)std::ceil(std::max(std::fabs(b[0] - a[0]), std::fabs(b[1] - a[1]))));
    for (int i = 0; i <= steps; i++)
    {
        const float x = a[0] + (b[0] - a[0]) * i / steps, y = a[1] + (b[1] - a[1]) * i / steps;
        for (int dy = -(width / 2); dy <= (width - 1) / 2; dy++)
            for (int dx = -(width / 2); dx <= (width - 1) / 2; dx++) c.Set((int)std::floor(x) + dx, (int)std::floor(y) + dy, rgb);
    }
}

void Dot(Canvas& c, const float p[2], int radius, const uint8_t rgb[3])
{
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            if (dx * dx + dy * dy <= radius * radius) c.Set((int)std::floor(p[0]) + dx, (int)std::floor(p[1]) + dy, rgb);
}

}

Bytes RenderTopView(const BchModel& model, const Bytes& tileBlock, const Bytes& doorBlock, const TopViewOptions& o)
{
    const int side = Tiles * std::max(4, o.PixelsPerTile);
    return EncodePng(side, side, RenderTopViewRgba(model, tileBlock, doorBlock, o));
}

Bytes RenderTopViewRgba(const BchModel& model, const Bytes& tileBlock, const Bytes& doorBlock, const TopViewOptions& o)
{
    const int px = std::max(4, o.PixelsPerTile), size = Tiles * px;
    Canvas canvas(size, size);
    // a piece spans -360..360 units: tile column = x / 18 + 20
    auto toPixel = [&](const BchVertex& v, float out[2]) { out[0] = (v.Position[0] / TileUnits + Tiles / 2) * px; out[1] = (v.Position[2] / TileUnits + Tiles / 2) * px; };
    for (int kind = 0; kind <= (int)TopViewKind::Other; kind++)
    {
        uint8_t rgb[3];
        TopViewColour((TopViewKind)kind, rgb);
        for (const BchMesh& mesh : model.Meshes)
        {
            if (mesh.Material >= model.Materials.size() || (int)MeshKind(model, mesh) != kind) continue;
            for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
            {
                float p[3][2];
                for (int k = 0; k < 3; k++) toPixel(mesh.Vertices[mesh.Triangles[t + k]], p[k]);
                // translucent layers are drawn translucent: a shadow darkens, an effect only tints
                Triangle(canvas, p[0], p[1], p[2], rgb, kind == (int)TopViewKind::Shadow ? 0.6f : kind == (int)TopViewKind::Effect ? 0.4f : 1.0f);
            }
        }
    }
    if (o.Tiles && tileBlock.size() >= 4 + Tiles * Tiles * 4)
        for (int r = 0; r < Tiles; r++)
            for (int c = 0; c < Tiles; c++)
            {
                uint32_t v = U32(tileBlock, 4 + (r * Tiles + c) * 4);
                v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; // a colour per value
                const uint8_t rgb[3] = {(uint8_t)(v & 255), (uint8_t)((v >> 8) & 255), (uint8_t)((v >> 16) & 255)};
                for (int y = 0; y < px; y++) for (int x = 0; x < px; x++) canvas.Set(c * px + x, r * px + y, rgb, 0.45f);
            }
    if (o.Grid)
        for (int i = 0; i <= Tiles; i++)
        {
            const uint8_t line[3] = {255, 255, 255};
            const float alpha = i % 5 == 0 ? 0.55f : 0.2f;
            for (int k = 0; k < size; k++) { canvas.Set(std::min(i * px, size - 1), k, line, alpha); canvas.Set(k, std::min(i * px, size - 1), line, alpha); }
        }
    if (o.Points)
    {
        const int width = std::max(1, px / 12), radius = std::max(1, px / 10);
        // a zone's border: the edges only one of its triangles has (vertices merged by position, to a tenth of a unit)
        for (const BchMesh& mesh : model.Meshes)
        {
            if (mesh.Material >= model.Materials.size()) continue;
            const TopViewKind kind = MeshKind(model, mesh);
            if (kind != TopViewKind::Light && kind != TopViewKind::Path) continue;
            auto key = [&](uint32_t i) { return std::make_pair((long)std::lround(mesh.Vertices[i].Position[0] * 10), (long)std::lround(mesh.Vertices[i].Position[2] * 10)); };
            std::map<std::pair<std::pair<long, long>, std::pair<long, long>>, std::pair<int, uint32_t>> edges; // edge -> (uses, a vertex pair)
            std::map<std::pair<std::pair<long, long>, std::pair<long, long>>, std::pair<uint32_t, uint32_t>> ends;
            for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
                for (int e = 0; e < 3; e++)
                {
                    const uint32_t i = mesh.Triangles[t + e], j = mesh.Triangles[t + (e + 1) % 3];
                    auto ki = key(i), kj = key(j);
                    if (ki == kj) continue;
                    const auto k = ki < kj ? std::make_pair(ki, kj) : std::make_pair(kj, ki);
                    edges[k].first++;
                    ends[k] = {i, j};
                }
            const uint8_t colour[3] = {(uint8_t)(kind == TopViewKind::Light ? 0 : 110), (uint8_t)(kind == TopViewKind::Light ? 110 : 60), (uint8_t)(kind == TopViewKind::Light ? 0 : 20)};
            for (const auto& [k, use] : edges)
            {
                if (use.first != 1) continue;
                float a[2], b[2];
                toPixel(mesh.Vertices[ends[k].first], a); toPixel(mesh.Vertices[ends[k].second], b);
                Line(canvas, a, b, colour, width + 1);
            }
        }
        // the blades' strip: its triangles' edges, then its tips and roots
        for (const BchMesh& mesh : model.Meshes)
        {
            if (mesh.Material >= model.Materials.size() || MeshKind(model, mesh) != TopViewKind::Blades) continue;
            const uint8_t orange[3] = {230, 120, 40}, red[3] = {220, 0, 0}, blue[3] = {0, 0, 220};
            for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
                for (int e = 0; e < 3; e++)
                {
                    float a[2], b[2];
                    toPixel(mesh.Vertices[mesh.Triangles[t + e]], a); toPixel(mesh.Vertices[mesh.Triangles[t + (e + 1) % 3]], b);
                    Line(canvas, a, b, orange, 1);
                }
            for (const BchVertex& v : mesh.Vertices)
            {
                const bool tip = std::fabs(v.TexCoord[1] - 0.302f) < 0.02f, root = std::fabs(v.TexCoord[1] - 0.496f) < 0.02f;
                if (!tip && !root) continue;
                float p[2];
                toPixel(v, p);
                Dot(canvas, p, radius, tip ? red : blue);
            }
        }
    }
    if (o.Doors && doorBlock.size() >= 4)
    {
        const uint32_t count = std::min<uint32_t>(U32(doorBlock, 0), (uint32_t)((doorBlock.size() - 4) / 44));
        for (uint32_t k = 0; k < count; k++)
        {
            const size_t e = 4 + (size_t)k * 44;
            float x, z;
            uint32_t bx = U32(doorBlock, e + 28), bz = U32(doorBlock, e + 36);
            std::memcpy(&x, &bx, 4); std::memcpy(&z, &bz, 4);
            // doors carry world coordinates: the tile within the piece is the position modulo the piece's 40 tiles
            const int col = ((int)std::floor(x / TileUnits) % Tiles + Tiles) % Tiles, row = ((int)std::floor(z / TileUnits) % Tiles + Tiles) % Tiles;
            const uint8_t red[3] = {255, 40, 40};
            for (int y = px / 4; y < px - px / 4; y++) for (int xx = px / 4; xx < px - px / 4; xx++) canvas.Set(col * px + xx, row * px + y, red);
        }
    }
    return canvas.Rgba;
}

Bytes RenderTopViewOfPiece(const Bytes& gr, const TopViewOptions& options)
{
    const BinLinker piece = BinLinker::Read(gr, "GR");
    const Bch bch = Bch::Read(piece.Files.at(1));
    if (bch.Models.empty()) throw FormatError("the piece has no terrain model");
    return RenderTopView(bch.Models[0], piece.Files.at(0), piece.Files.size() > 3 ? piece.Files[3] : Bytes(), options);
}

}
