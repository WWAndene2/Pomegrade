#include "TownShapes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>

namespace remake
{

// a small fixed shift of a lattice point, up to spread / 2 of a cell either way: a hash of the point
static ShapePoint Jitter(int i, int j, float cell, float spread)
{
    uint32_t h = (uint32_t)(i * 73856093) ^ (uint32_t)(j * 19349663);
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    const float x = ((h & 0xFFFF) / 65535.0f - 0.5f) * spread, z = (((h >> 16) & 0xFFFF) / 65535.0f - 0.5f) * spread;
    return {x * cell, z * cell};
}

ZoneShape StairZone(const std::vector<std::vector<bool>>& mask, float cellSize, float originX, float originZ, bool roundTips, float jitter)
{
    ZoneShape shape;
    const int H = (int)mask.size();
    const int W = H ? (int)mask[0].size() : 0;
    if (!W) return shape;
    auto in = [&](int c, int r) { return mask[std::clamp(r, 0, H - 1)][std::clamp(c, 0, W - 1)]; };
    auto point = [&](int i, int j) {
        const ShapePoint s = Jitter(i, j, cellSize, jitter);
        return ShapePoint{originX + i * cellSize + s.X, originZ + j * cellSize + s.Z};
    };

    // the fill: two triangles a cell, as the ground's quads (a, e, d / a, d, b)
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
        {
            if (!mask[r][c]) continue;
            const ShapePoint a = point(c, r), b = point(c + 1, r), d = point(c + 1, r + 1), e = point(c, r + 1);
            shape.Fill.push_back({a, e, d});
            shape.Fill.push_back({a, d, b});
        }

    // boundary edges between a cell in the zone and a cell out of it, the zone on the right of the direction of travel
    struct Edge { int From[2], To[2], Normal[2]; };
    std::vector<Edge> edges;
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++)
        {
            if (!mask[r][c]) continue;
            if (r > 0 && !mask[r - 1][c]) edges.push_back({{c, r}, {c + 1, r}, {0, -1}});
            if (c < W - 1 && !mask[r][c + 1]) edges.push_back({{c + 1, r}, {c + 1, r + 1}, {1, 0}});
            if (r < H - 1 && !mask[r + 1][c]) edges.push_back({{c + 1, r + 1}, {c, r + 1}, {0, 1}});
            if (c > 0 && !mask[r][c - 1]) edges.push_back({{c, r + 1}, {c, r}, {-1, 0}});
        }
    std::map<std::pair<int, int>, std::vector<size_t>> from;
    for (size_t i = 0; i < edges.size(); i++) from[{edges[i].From[0], edges[i].From[1]}].push_back(i);
    std::vector<bool> used(edges.size(), false);
    std::set<std::pair<int, int>> ends;
    for (const Edge& e : edges) ends.insert({e.To[0], e.To[1]});
    // chains that run off the mask's border (a zone touching it) start where no edge arrives; the rest are loops
    std::vector<size_t> starts;
    for (size_t i = 0; i < edges.size(); i++) if (!ends.count({edges[i].From[0], edges[i].From[1]})) starts.push_back(i);
    for (size_t i = 0; i < edges.size(); i++) starts.push_back(i);
    for (size_t start : starts)
    {
        if (used[start]) continue;
        std::vector<size_t> loop;
        size_t at = start;
        while (true)
        {
            used[at] = true;
            loop.push_back(at);
            const auto it = from.find({edges[at].To[0], edges[at].To[1]});
            if (it == from.end()) break;
            size_t next = edges.size();
            for (size_t k : it->second) if (!used[k]) { next = k; break; }
            if (next == edges.size()) break;
            at = next;
        }
        ShapeChain chain;
        const size_t n = loop.size();
        chain.Closed = edges[loop.back()].To[0] == edges[loop[0]].From[0] && edges[loop.back()].To[1] == edges[loop[0]].From[1];
        // the point at a lattice point between two edges (or at an open chain's end, with its one edge's normal)
        auto add = [&](int i, int j, const Edge* before, const Edge* after) {
            ShapePoint p = point(i, j);
            const Edge& one = before ? *before : *after;
            ShapePoint normal{(float)one.Normal[0], (float)one.Normal[1]};
            const bool corner = before && after && (before->Normal[0] != after->Normal[0] || before->Normal[1] != after->Normal[1]);
            if (corner)
            {
                normal = {(float)(before->Normal[0] + after->Normal[0]), (float)(before->Normal[1] + after->Normal[1])};
                const float length = std::hypot(normal.X, normal.Z);
                if (length > 1e-4f) { normal.X /= length; normal.Z /= length; }
                else normal = {(float)after->Normal[0], (float)after->Normal[1]};
                if (roundTips)
                {
                    // cells around the lattice point: (i-1, j-1), (i, j-1), (i-1, j), (i, j); three in the zone: the fourth's tip is cut
                    const bool cell[4] = {in(i - 1, j - 1), in(i, j - 1), in(i - 1, j), in(i, j)};
                    if (cell[0] + cell[1] + cell[2] + cell[3] == 3)
                    {
                        const int gap = !cell[0] ? 0 : !cell[1] ? 1 : !cell[2] ? 2 : 3;
                        p = {originX + i * cellSize + 0.36f * cellSize * ((gap & 1) ? 1.0f : -1.0f), originZ + j * cellSize + 0.36f * cellSize * ((gap & 2) ? 1.0f : -1.0f)};
                    }
                }
            }
            chain.Points.push_back(p);
            chain.Normals.push_back(normal);
        };
        for (size_t k = 0; k < n; k++)
        {
            const Edge* before = (k > 0 || chain.Closed) ? &edges[loop[(k + n - 1) % n]] : nullptr;
            add(edges[loop[k]].From[0], edges[loop[k]].From[1], before, &edges[loop[k]]);
        }
        if (!chain.Closed) add(edges[loop.back()].To[0], edges[loop.back()].To[1], &edges[loop.back()], nullptr);
        if (chain.Points.size() >= 2) shape.Chains.push_back(std::move(chain));
    }
    return shape;
}

ShapeChain RoundCorners(const ShapeChain& chain, float radius, int steps)
{
    const size_t n = chain.Points.size();
    if (n < 3) return chain;
    ShapeChain out;
    out.Closed = chain.Closed;
    std::vector<ShapePoint> side; // the original normal each new point takes its side from
    auto add = [&](ShapePoint p, ShapePoint nrm) { out.Points.push_back(p); side.push_back(nrm); };
    auto at = [&](size_t i) { return chain.Points[i % n]; };
    for (size_t i = 0; i < n; i++)
    {
        const bool end = !chain.Closed && (i == 0 || i == n - 1);
        if (end) { add(chain.Points[i], chain.Normals[i]); continue; }
        const ShapePoint p = chain.Points[i], a = at(i + n - 1), b = at(i + 1);
        float ax = p.X - a.X, az = p.Z - a.Z, bx = b.X - p.X, bz = b.Z - p.Z;
        const float la = std::hypot(ax, az), lb = std::hypot(bx, bz);
        if (la < 1e-4f || lb < 1e-4f) { add(p, chain.Normals[i]); continue; }
        ax /= la; az /= la; bx /= lb; bz /= lb;
        if (ax * bx + az * bz > std::cos(10.0f * 3.14159265f / 180)) { add(p, chain.Normals[i]); continue; }
        const float t = std::min(radius, std::min(la, lb) / 2);
        const ShapePoint s{p.X - ax * t, p.Z - az * t}, e{p.X + bx * t, p.Z + bz * t};
        for (int k = 0; k <= steps; k++)
        {
            const float u = (float)k / steps, w0 = (1 - u) * (1 - u), w1 = 2 * u * (1 - u), w2 = u * u;
            add({w0 * s.X + w1 * p.X + w2 * e.X, w0 * s.Z + w1 * p.Z + w2 * e.Z}, chain.Normals[i]);
        }
    }
    // normals from the curve's direction (central differences), on the side the original normals point to
    const size_t m = out.Points.size();
    for (size_t i = 0; i < m; i++)
    {
        const bool open = !out.Closed;
        const ShapePoint prev = (open && i == 0) ? out.Points[i] : out.Points[(i + m - 1) % m];
        const ShapePoint next = (open && i == m - 1) ? out.Points[i] : out.Points[(i + 1) % m];
        float nx = next.Z - prev.Z, nz = -(next.X - prev.X);
        const float l = std::hypot(nx, nz);
        if (l < 1e-6f) { out.Normals.push_back(side[i]); continue; }
        nx /= l; nz /= l;
        if (nx * side[i].X + nz * side[i].Z < 0) { nx = -nx; nz = -nz; }
        out.Normals.push_back({nx, nz});
    }
    return out;
}

}
