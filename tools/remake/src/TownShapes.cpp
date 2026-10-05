#include "TownShapes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>

namespace remake
{

namespace
{

constexpr float Level = 0.5f;

std::vector<float> BoxBlur(const std::vector<float>& in, int nx, int ny, int radius)
{
    if (radius <= 0) return in;
    std::vector<float> tmp(in.size()), out(in.size());
    const float w = 1.0f / (2 * radius + 1);
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
        {
            float s = 0;
            for (int k = -radius; k <= radius; k++) s += in[(size_t)j * nx + std::clamp(i + k, 0, nx - 1)];
            tmp[(size_t)j * nx + i] = s * w;
        }
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
        {
            float s = 0;
            for (int k = -radius; k <= radius; k++) s += tmp[(size_t)std::clamp(j + k, 0, ny - 1) * nx + i];
            out[(size_t)j * nx + i] = s * w;
        }
    return out;
}

// a polygon of at most six vertices into triangles, counter-clockwise as the ground's quads (a, e, d: negative area
// in (X, Z), see TownBuilder), by ear clipping
void Triangulate(std::vector<ShapePoint> poly, std::vector<std::array<ShapePoint, 3>>& out)
{
    auto area = [](const ShapePoint& a, const ShapePoint& b, const ShapePoint& c) { return (b.X - a.X) * (c.Z - a.Z) - (b.Z - a.Z) * (c.X - a.X); };
    float total = 0;
    for (size_t i = 0; i < poly.size(); i++) total += poly[i].X * poly[(i + 1) % poly.size()].Z - poly[(i + 1) % poly.size()].X * poly[i].Z;
    if (std::fabs(total) < 1e-6f) return;
    if (total > 0) std::reverse(poly.begin(), poly.end()); // a negative area: the ground's winding
    while (poly.size() > 3)
    {
        bool clipped = false;
        for (size_t i = 0; i < poly.size() && !clipped; i++)
        {
            const ShapePoint &a = poly[(i + poly.size() - 1) % poly.size()], &b = poly[i], &c = poly[(i + 1) % poly.size()];
            if (area(a, b, c) >= -1e-6f) continue; // a reflex corner
            bool empty = true;
            for (const ShapePoint& p : poly)
            {
                if (&p == &a || &p == &b || &p == &c) continue;
                if (area(a, b, p) < 0 && area(b, c, p) < 0 && area(c, a, p) < 0) { empty = false; break; }
            }
            if (!empty) continue;
            out.push_back({a, b, c});
            poly.erase(poly.begin() + i);
            clipped = true;
        }
        if (!clipped) return; // degenerate: left out
    }
    if (poly.size() == 3 && std::fabs(area(poly[0], poly[1], poly[2])) > 1e-6f) out.push_back({poly[0], poly[1], poly[2]});
}

struct Chord { int64_t From, To; ShapePoint A, B, Inside; };

}

ZoneShape SmoothZone(const std::vector<std::vector<bool>>& mask, float cellSize, float originX, float originZ, int blur)
{
    ZoneShape shape;
    const int rows = (int)mask.size(), cols = rows ? (int)mask[0].size() : 0;
    if (!rows || !cols) return shape;
    const int F = 2, nx = cols * F, ny = rows * F;
    const float fs = cellSize / F;
    std::vector<float> g((size_t)nx * ny);
    for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) g[(size_t)j * nx + i] = mask[j / F][i / F] ? 1.0f : 0.0f;
    g = BoxBlur(BoxBlur(g, nx, ny, blur), nx, ny, blur);
    auto G = [&](int i, int j) { return g[(size_t)std::clamp(j, 0, ny - 1) * nx + std::clamp(i, 0, nx - 1)]; };
    // the lattice corners of the sub-cells hold the mean of the four sub-cells around them
    std::vector<float> corner((size_t)(nx + 1) * (ny + 1));
    for (int j = 0; j <= ny; j++) for (int i = 0; i <= nx; i++) corner[(size_t)j * (nx + 1) + i] = 0.25f * (G(i - 1, j - 1) + G(i, j - 1) + G(i - 1, j) + G(i, j));
    auto C = [&](int i, int j) { return corner[(size_t)j * (nx + 1) + i]; };
    auto at = [&](int i, int j) { return ShapePoint{originX + i * fs, originZ + j * fs}; };

    std::vector<Chord> chords;
    for (int j = 0; j < ny; j++)
    {
        int run = -1; // a run of whole sub-cells in this row, one quad
        auto flush = [&](int end) {
            if (run < 0) return;
            const ShapePoint a = at(run, j), b = at(end, j), d = at(end, j + 1), e = at(run, j + 1);
            shape.Fill.push_back({a, e, d});
            shape.Fill.push_back({a, d, b});
            run = -1;
        };
        for (int i = 0; i < nx; i++)
        {
            // corners clockwise from the top left, and the lattice edge each crossing lies on
            const int ci[4] = {i, i + 1, i + 1, i}, cj[4] = {j, j, j + 1, j + 1};
            float v[4]; bool in[4]; ShapePoint p[4];
            int count = 0;
            for (int k = 0; k < 4; k++) { v[k] = C(ci[k], cj[k]); in[k] = v[k] >= Level; p[k] = at(ci[k], cj[k]); count += in[k]; }
            if (count == 4) { if (run < 0) run = i; continue; }
            flush(i);
            if (count == 0) continue;
            // edge k joins corner k and k+1: horizontal edges 0 and 2, vertical 1 and 3
            auto edgeId = [&](int k) -> int64_t {
                switch (k) {
                case 0: return ((int64_t)j * (nx + 1) + i) * 4 + 0;
                case 1: return ((int64_t)j * (nx + 1) + i + 1) * 4 + 1;
                case 2: return ((int64_t)(j + 1) * (nx + 1) + i) * 4 + 0;
                default: return ((int64_t)j * (nx + 1) + i) * 4 + 1;
                }
            };
            auto cross = [&](int k) {
                const int n = (k + 1) % 4;
                const float t = (Level - v[k]) / (v[n] - v[k]);
                return ShapePoint{p[k].X + (p[n].X - p[k].X) * t, p[k].Z + (p[n].Z - p[k].Z) * t};
            };
            struct Poly { std::vector<ShapePoint> V; int64_t ExitEdge = -1, EnterEdge = -1; ShapePoint Exit, Enter; };
            std::vector<Poly> polys;
            const bool saddle = count == 2 && in[0] == in[2];
            if (saddle)
            {
                const bool connected = 0.25f * (v[0] + v[1] + v[2] + v[3]) >= Level;
                const int a = in[0] ? 0 : 1, c2 = a + 2; // the two inside corners
                // enter crossing before a (edge a-1), exit after a (edge a), enter before c (edge c-1), exit after c (edge c)
                auto E = [&](int k) { return cross(((k % 4) + 4) % 4); };
                auto Id = [&](int k) { return edgeId(((k % 4) + 4) % 4); };
                if (connected)
                {
                    Poly h; h.V = {E(a - 1), p[a], E(a), E(c2 - 1), p[c2], E(c2)};
                    polys.push_back(h);
                    // two chords: exit a -> enter c, exit c -> enter a
                    chords.push_back({Id(a), Id(c2 - 1), E(a), E(c2 - 1), {}});
                    chords.push_back({Id(c2), Id(a - 1), E(c2), E(a - 1), {}});
                }
                else
                {
                    polys.push_back({{E(a - 1), p[a], E(a)}});
                    polys.push_back({{E(c2 - 1), p[c2], E(c2)}});
                    chords.push_back({Id(a), Id(a - 1), E(a), E(a - 1), {}});
                    chords.push_back({Id(c2), Id(c2 - 1), E(c2), E(c2 - 1), {}});
                }
            }
            else
            {
                int start = 0;
                for (int k = 0; k < 4; k++) if (in[k] && !in[(k + 3) % 4]) start = k;
                Poly h;
                const int before = (start + 3) % 4;
                h.V.push_back(cross(before));
                h.EnterEdge = edgeId(before); h.Enter = h.V.back();
                int k = start;
                while (in[k]) { h.V.push_back(p[k]); k = (k + 1) % 4; }
                h.V.push_back(cross((k + 3) % 4));
                h.ExitEdge = edgeId((k + 3) % 4); h.Exit = h.V.back();
                polys.push_back(h);
                chords.push_back({h.ExitEdge, h.EnterEdge, h.Exit, h.Enter, {}});
            }
            for (const Poly& poly : polys) Triangulate(poly.V, shape.Fill);
            // the inside's centre, to tell the chord's outward side
            for (size_t c = chords.size() - (saddle ? 2 : 1); c < chords.size(); c++)
            {
                ShapePoint m{0, 0}; int n = 0;
                for (const Poly& poly : polys) for (const ShapePoint& q : poly.V) { m.X += q.X; m.Z += q.Z; n++; }
                chords[c].Inside = {m.X / n, m.Z / n};
            }
        }
        flush(nx);
    }

    // chains: a chord ends on the lattice edge the next one starts from
    std::map<int64_t, size_t> startAt;
    for (size_t i = 0; i < chords.size(); i++) startAt[chords[i].From] = i;
    std::vector<bool> used(chords.size(), false);
    auto outward = [&](const Chord& c) {
        float dx = c.B.X - c.A.X, dz = c.B.Z - c.A.Z;
        const float len = std::hypot(dx, dz);
        if (len < 1e-6f) return ShapePoint{0, 0};
        ShapePoint n{dz / len, -dx / len};
        const float side = n.X * (c.Inside.X - (c.A.X + c.B.X) / 2) + n.Z * (c.Inside.Z - (c.A.Z + c.B.Z) / 2);
        if (side > 0) { n.X = -n.X; n.Z = -n.Z; }
        return n;
    };
    // open chains begin at a chord no other chord leads to
    std::map<int64_t, size_t> leadsTo;
    for (size_t i = 0; i < chords.size(); i++) leadsTo[chords[i].To] = i;
    auto follow = [&](size_t first) {
        ShapeChain chain;
        std::vector<ShapePoint> segmentNormals;
        size_t at = first;
        while (true)
        {
            used[at] = true;
            chain.Points.push_back(chords[at].A);
            segmentNormals.push_back(outward(chords[at]));
            const auto next = startAt.find(chords[at].To);
            if (next == startAt.end()) { chain.Points.push_back(chords[at].B); break; }
            if (used[next->second]) { chain.Closed = next->second == first; if (!chain.Closed) chain.Points.push_back(chords[at].B); break; }
            at = next->second;
        }
        // a point's normal: the mean of its neighbouring chords' (the first and last keep their own at an open end)
        const size_t n = chain.Points.size();
        for (size_t i = 0; i < n; i++)
        {
            ShapePoint a = i > 0 ? segmentNormals[i - 1] : (chain.Closed ? segmentNormals.back() : segmentNormals[0]);
            ShapePoint b = i < segmentNormals.size() ? segmentNormals[i] : (chain.Closed ? segmentNormals[0] : segmentNormals.back());
            ShapePoint m{a.X + b.X, a.Z + b.Z};
            const float len = std::hypot(m.X, m.Z);
            chain.Normals.push_back(len > 1e-6f ? ShapePoint{m.X / len, m.Z / len} : b);
        }
        // a lattice corner on the cut gives chords of no length: their repeated points are dropped
        ShapeChain clean;
        clean.Closed = chain.Closed;
        for (size_t i = 0; i < chain.Points.size(); i++)
        {
            if (!clean.Points.empty() && std::hypot(chain.Points[i].X - clean.Points.back().X, chain.Points[i].Z - clean.Points.back().Z) < 1e-4f) continue;
            clean.Points.push_back(chain.Points[i]);
            clean.Normals.push_back(chain.Normals[i]);
        }
        shape.Chains.push_back(std::move(clean));
    };
    for (size_t i = 0; i < chords.size(); i++) if (!used[i] && !leadsTo.count(chords[i].From)) follow(i);
    for (size_t i = 0; i < chords.size(); i++) if (!used[i]) follow(i);
    return shape;
}

}
