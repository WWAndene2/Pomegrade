#include "OrasMeasure.h"
#include "Bch.h"
#include "BinLinker.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "TopView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace remake
{

namespace
{

constexpr int Kinds = (int)TopViewKind::Other + 1;

Bytes Plain(const Bytes& d) { return IsLzCompressed(d) ? LzDecompress(d) : d; }

std::string F(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    return buffer;
}

float Frac(float v) { return std::fabs(v - std::round(v)); } // distance to the nearest lattice line, in tiles

struct Shape
{
    size_t Vertices = 0, BoundaryEdges = 0, BoundaryVertices = 0;
    size_t OnLattice = 0, OnHalf = 0, Axis = 0, Diagonal = 0, OneTile = 0, Arcs = 0, ArcCandidates = 0;
    double JitterSum = 0, JitterMax = 0;
    size_t Levels = 0;               // distinct heights
    size_t Multiple18 = 0, Counted = 0; // vertices whose height is a multiple of 18 (a tile), of those counted
};

struct Totals
{
    size_t Meshes = 0, Pieces = 0;
    Shape Sum;
};

void Accumulate(Totals& t, const Shape& s)
{
    t.Meshes++;
    t.Sum.Vertices += s.Vertices; t.Sum.BoundaryEdges += s.BoundaryEdges; t.Sum.BoundaryVertices += s.BoundaryVertices;
    t.Sum.OnLattice += s.OnLattice; t.Sum.OnHalf += s.OnHalf; t.Sum.Axis += s.Axis; t.Sum.Diagonal += s.Diagonal; t.Sum.OneTile += s.OneTile;
    t.Sum.Arcs += s.Arcs; t.Sum.ArcCandidates += s.ArcCandidates;
    t.Sum.JitterSum += s.JitterSum; t.Sum.JitterMax = std::max(t.Sum.JitterMax, s.JitterMax);
    t.Sum.Multiple18 += s.Multiple18; t.Sum.Counted += s.Counted;
}

// the boundary of a mesh (edges used by one triangle, vertices welded by position to 0.05 tile) against the tile lattice
Shape Measure(const BchMesh& mesh, std::array<size_t, 10>* offsetHistogram)
{
    Shape s;
    std::map<std::pair<long, long>, int> id;
    std::vector<std::pair<float, float>> points;
    std::vector<int> weld(mesh.Vertices.size());
    std::set<long> heights;
    for (size_t i = 0; i < mesh.Vertices.size(); i++)
    {
        const float x = mesh.Vertices[i].Position[0] / 18 + 20, z = mesh.Vertices[i].Position[2] / 18 + 20, y = mesh.Vertices[i].Position[1];
        const auto key = std::make_pair(std::lround(x * 20), std::lround(z * 20));
        auto it = id.find(key);
        if (it == id.end()) { it = id.emplace(key, (int)points.size()).first; points.push_back({x, z}); heights.insert(std::lround(y * 10)); s.Counted++; if (std::fabs(y / 18 - std::round(y / 18)) < 0.01f) s.Multiple18++; }
        weld[i] = it->second;
    }
    s.Vertices = points.size();
    s.Levels = heights.size();
    std::map<std::pair<int, int>, int> edges;
    for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
        for (int k = 0; k < 3; k++)
        {
            const int a = weld[mesh.Triangles[t + k]], b = weld[mesh.Triangles[t + (k + 1) % 3]];
            if (a != b) edges[{std::min(a, b), std::max(a, b)}]++;
        }
    std::set<int> boundary;
    for (const auto& [e, n] : edges)
    {
        if (n != 1) continue;
        s.BoundaryEdges++;
        boundary.insert(e.first); boundary.insert(e.second);
        const float dx = points[e.second].first - points[e.first].first, dz = points[e.second].second - points[e.first].second;
        const double length = std::hypot(dx, dz);
        double angle = std::fabs(std::atan2(dz, dx)) * 180 / M_PI; // 0..180
        angle = std::fmod(angle, 90.0);
        if (angle < 15 || angle > 75) s.Axis++; else if (std::fabs(angle - 45) < 15) s.Diagonal++;
        if (std::fabs(length - 1) < 0.2) s.OneTile++;
    }
    s.BoundaryVertices = boundary.size();
    for (int v : boundary)
    {
        const float x = points[v].first, z = points[v].second;
        const float dx = Frac(x), dz = Frac(z);
        if (std::fabs(x * 2 - std::round(x * 2)) < 0.3f && std::fabs(z * 2 - std::round(z * 2)) < 0.3f) s.OnHalf++; // the half-tile lattice (includes the tile's)
        if (dx < 0.15f && dz < 0.15f)
        {
            s.OnLattice++;
            const double j = std::hypot(dx, dz);
            s.JitterSum += j; s.JitterMax = std::max(s.JitterMax, j);
        }
        else if (std::fabs(dx - 0.36f) < 0.1f && std::fabs(dz - 0.36f) < 0.1f) s.Arcs++;
        if (offsetHistogram && std::min(dx, dz) < 0.12f) (*offsetHistogram)[std::min<size_t>(9, (size_t)(std::max(dx, dz) / 0.05f))]++;
    }
    return s;
}

// the surface under a tile's centre: the highest of the ground-like meshes covering it
struct Surface { float Height = -1e9f; int Kind = -1; };

void CoverTiles(const BchModel& model, std::vector<Surface>& tiles)
{
    for (const BchMesh& mesh : model.Meshes)
    {
        if (mesh.Material >= model.Materials.size()) continue;
        const TopViewKind kind = MeshKind(model, mesh);
        if (kind != TopViewKind::Ground && kind != TopViewKind::Light && kind != TopViewKind::Path && kind != TopViewKind::Water &&
            kind != TopViewKind::Forest && kind != TopViewKind::Cliff && kind != TopViewKind::Structure) continue;
        for (size_t t = 0; t + 2 < mesh.Triangles.size(); t += 3)
        {
            const BchVertex* v[3] = {&mesh.Vertices[mesh.Triangles[t]], &mesh.Vertices[mesh.Triangles[t + 1]], &mesh.Vertices[mesh.Triangles[t + 2]]};
            float x[3], z[3];
            for (int k = 0; k < 3; k++) { x[k] = v[k]->Position[0] / 18 + 20; z[k] = v[k]->Position[2] / 18 + 20; }
            const float d = (z[1] - z[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (z[0] - z[2]);
            if (std::fabs(d) < 1e-6f) continue;
            const int c0 = std::max(0, (int)std::floor(std::min({x[0], x[1], x[2]}) - 0.5f)), c1 = std::min(39, (int)std::ceil(std::max({x[0], x[1], x[2]}) - 0.5f));
            const int r0 = std::max(0, (int)std::floor(std::min({z[0], z[1], z[2]}) - 0.5f)), r1 = std::min(39, (int)std::ceil(std::max({z[0], z[1], z[2]}) - 0.5f));
            for (int r = r0; r <= r1; r++)
                for (int c = c0; c <= c1; c++)
                {
                    const float px = c + 0.5f, pz = r + 0.5f;
                    const float l0 = ((z[1] - z[2]) * (px - x[2]) + (x[2] - x[1]) * (pz - z[2])) / d;
                    const float l1 = ((z[2] - z[0]) * (px - x[2]) + (x[0] - x[2]) * (pz - z[2])) / d;
                    const float l2 = 1 - l0 - l1;
                    if (l0 < -1e-4f || l1 < -1e-4f || l2 < -1e-4f) continue;
                    const float y = l0 * v[0]->Position[1] + l1 * v[1]->Position[1] + l2 * v[2]->Position[1];
                    Surface& s = tiles[r * 40 + c];
                    if (y > s.Height) { s.Height = y; s.Kind = (int)kind; }
                }
        }
    }
}

}

std::string MeasureGame(N3dsRom& game, const std::string& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream shapes(directory + "/zone_shapes.tsv");
    shapes << "piece\tmesh\tkind\tvertices\tboundary_edges\tboundary_vertices\ton_lattice\ton_half_lattice\taxis_edges\tdiagonal_edges\tone_tile_edges\tarc_middles\tjitter_mean\tjitter_max\theight_levels\tmultiple_of_18\n";
    Totals totals[Kinds];
    std::array<size_t, 10> offsets[Kinds] = {};
    std::map<uint32_t, std::array<size_t, Kinds>> surfaces;
    std::map<uint32_t, std::set<size_t>> surfacePieces;
    const Garc pieces(game.Read("a/0/3/9"));
    size_t read = 0;
    for (size_t i = 0; i < pieces.Count(); i++)
    {
        try
        {
            const Bytes raw = Plain(pieces.Sub(i));
            if (raw.size() < 2 || raw[0] != 'G' || raw[1] != 'R') continue;
            const BinLinker gr = BinLinker::Read(raw, "GR");
            const Bch bch = Bch::Read(gr.Files.at(1));
            if (bch.Models.empty()) continue;
            const BchModel& model = bch.Models[0];
            read++;
            std::set<int> seen;
            for (size_t m = 0; m < model.Meshes.size(); m++)
            {
                const BchMesh& mesh = model.Meshes[m];
                if (mesh.Material >= model.Materials.size() || mesh.Triangles.empty()) continue;
                const int kind = (int)MeshKind(model, mesh);
                const Shape s = Measure(mesh, &offsets[kind]);
                Accumulate(totals[kind], s);
                seen.insert(kind);
                shapes << i << '\t' << m << '\t' << TopViewName((TopViewKind)kind) << '\t' << s.Vertices << '\t' << s.BoundaryEdges << '\t' << s.BoundaryVertices << '\t' << s.OnLattice << '\t' << s.OnHalf << '\t'
                       << s.Axis << '\t' << s.Diagonal << '\t' << s.OneTile << '\t' << s.Arcs << '\t' << F("%.3f\t%.3f", s.BoundaryVertices ? s.JitterSum / std::max<size_t>(1, s.OnLattice) : 0.0, s.JitterMax) << '\t'
                       << s.Levels << '\t' << s.Multiple18 << '\n';
            }
            for (int k : seen) totals[k].Pieces++;
            const Bytes& tileBlock = gr.Files.at(0);
            if (tileBlock.size() >= 4 + 1600 * 4)
            {
                std::vector<Surface> under(1600);
                CoverTiles(model, under);
                for (int t = 0; t < 1600; t++)
                {
                    const uint32_t value = U32(tileBlock, 4 + t * 4);
                    auto& row = surfaces[value];
                    row[under[t].Kind < 0 ? Kinds - 1 : under[t].Kind]++;
                    surfacePieces[value].insert(i);
                }
            }
        }
        catch (const std::exception&) {}
    }
    std::ofstream tiles(directory + "/tile_surfaces.tsv");
    tiles << "value\tpieces\ttiles";
    for (int k = 0; k < Kinds; k++) tiles << '\t' << TopViewName((TopViewKind)k);
    tiles << "\n";
    for (const auto& [value, row] : surfaces)
    {
        size_t total = 0;
        for (size_t n : row) total += n;
        tiles << F("%08X", value) << '\t' << surfacePieces[value].size() << '\t' << total;
        for (size_t n : row) tiles << '\t' << n;
        tiles << '\n';
    }
    std::ofstream offsetFile(directory + "/outline_offsets.tsv");
    offsetFile << "kind\t0-0.05\t0.05-0.1\t0.1-0.15\t0.15-0.2\t0.2-0.25\t0.25-0.3\t0.3-0.35\t0.35-0.4\t0.4-0.45\t0.45+\n";
    std::string summary = F("%zu pieces measured; per kind of mesh (boundary of each mesh against the tile lattice):\n", read);
    summary += "kind              pieces meshes  bnd.verts on-lattice on-half   axis-edges diag-edges 1-tile-edges arcs jitter(mean/max)  heights x18\n";
    for (int k = 0; k < Kinds; k++)
    {
        const Totals& t = totals[k];
        offsetFile << TopViewName((TopViewKind)k);
        for (size_t n : offsets[k]) offsetFile << '\t' << n;
        offsetFile << '\n';
        if (!t.Meshes) continue;
        auto pct = [](size_t a, size_t b) { return b ? 100.0 * a / b : 0.0; };
        summary += F("%-17s %6zu %6zu %10zu %9.1f%% %7.1f%% %9.1f%% %9.1f%% %10.1f%% %5zu  %.3f/%.3f %8.1f%%\n", TopViewName((TopViewKind)k), t.Pieces, t.Meshes, t.Sum.BoundaryVertices, pct(t.Sum.OnLattice, t.Sum.BoundaryVertices), pct(t.Sum.OnHalf, t.Sum.BoundaryVertices),
                     pct(t.Sum.Axis, t.Sum.BoundaryEdges), pct(t.Sum.Diagonal, t.Sum.BoundaryEdges), pct(t.Sum.OneTile, t.Sum.BoundaryEdges), t.Sum.Arcs,
                     t.Sum.OnLattice ? t.Sum.JitterSum / t.Sum.OnLattice : 0.0, t.Sum.JitterMax, pct(t.Sum.Multiple18, t.Sum.Counted));
    }
    return summary + F("written to %s (zone_shapes.tsv, tile_surfaces.tsv, outline_offsets.tsv)\n", directory.c_str());
}

}
