#include "GPU3D_WorldFrame.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace melonDS
{

bool FitRigidMotion(const std::vector<float>& a, const std::vector<float>& b, float r[9], float t[3])
{
    const size_t n = a.size() / 3;
    if (n < 3 || b.size() != a.size()) return false;
    double ca[3] = {}, cb[3] = {};
    for (size_t i = 0; i < n; i++)
        for (int k = 0; k < 3; k++) { ca[k] += a[i * 3 + k]; cb[k] += b[i * 3 + k]; }
    for (int k = 0; k < 3; k++) { ca[k] /= n; cb[k] /= n; }
    // cross-covariance S[j][k] = sum (a_j - ca_j)(b_k - cb_k)
    double s[3][3] = {};
    for (size_t i = 0; i < n; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                s[j][k] += (a[i * 3 + j] - ca[j]) * (b[i * 3 + k] - cb[k]);
    // Horn: the rotation's quaternion is the eigenvector of N with the largest eigenvalue
    const double N[4][4] = {
        {s[0][0] + s[1][1] + s[2][2], s[1][2] - s[2][1], s[2][0] - s[0][2], s[0][1] - s[1][0]},
        {s[1][2] - s[2][1], s[0][0] - s[1][1] - s[2][2], s[0][1] + s[1][0], s[2][0] + s[0][2]},
        {s[2][0] - s[0][2], s[0][1] + s[1][0], -s[0][0] + s[1][1] - s[2][2], s[1][2] + s[2][1]},
        {s[0][1] - s[1][0], s[2][0] + s[0][2], s[1][2] + s[2][1], -s[0][0] - s[1][1] + s[2][2]}};
    // power iteration on N + shift I (the shift makes every eigenvalue positive, the largest stays largest)
    double shift = 0;
    for (auto& row : N) for (double v : row) shift += std::fabs(v);
    if (shift <= 0) return false;
    double q[4] = {1, 0, 0, 0};
    for (int it = 0; it < 64; it++)
    {
        double nq[4];
        for (int i = 0; i < 4; i++)
        {
            nq[i] = shift * q[i];
            for (int j = 0; j < 4; j++) nq[i] += N[i][j] * q[j];
        }
        const double l = std::sqrt(nq[0] * nq[0] + nq[1] * nq[1] + nq[2] * nq[2] + nq[3] * nq[3]);
        if (l <= 0) return false;
        for (int i = 0; i < 4; i++) q[i] = nq[i] / l;
    }
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double m[9] = {w * w + x * x - y * y - z * z, 2 * (x * y - w * z), 2 * (x * z + w * y),
                         2 * (x * y + w * z), w * w - x * x + y * y - z * z, 2 * (y * z - w * x),
                         2 * (x * z - w * y), 2 * (y * z + w * x), w * w - x * x - y * y + z * z};
    for (int i = 0; i < 9; i++) r[i] = (float)m[i];
    for (int k = 0; k < 3; k++)
        t[k] = (float)(cb[k] - (m[k * 3] * ca[0] + m[k * 3 + 1] * ca[1] + m[k * 3 + 2] * ca[2]));
    return true;
}

void WorldFrame::Reset() noexcept
{
    const float id[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::copy(id, id + 9, ToWorld);
}

void WorldFrame::ViewToWorld(const float in[3], float out[3]) const noexcept
{
    for (int i = 0; i < 3; i++) out[i] = ToWorld[i * 3] * in[0] + ToWorld[i * 3 + 1] * in[1] + ToWorld[i * 3 + 2] * in[2];
}

void WorldFrame::WorldToView(const float in[3], float out[3]) const noexcept
{
    for (int i = 0; i < 3; i++) out[i] = ToWorld[i] * in[0] + ToWorld[3 + i] * in[1] + ToWorld[6 + i] * in[2];
}

bool WorldFrame::Update(const std::vector<float>& points)
{
    const size_t n = points.size() / 3;
    const bool first = Previous.size() < 48 || n < 16;
    if (first)
    {
        Reset();
        Previous = points;
        return true;
    }
    // the scene's scale: the median distance of its vertices
    std::vector<float> dist(n);
    for (size_t i = 0; i < n; i++)
        dist[i] = std::sqrt(points[i * 3] * points[i * 3] + points[i * 3 + 1] * points[i * 3 + 1] + points[i * 3 + 2] * points[i * 3 + 2]);
    std::nth_element(dist.begin(), dist.begin() + n / 2, dist.end());
    const float scale = std::max(dist[n / 2], 1e-6f);

    // the previous frame's vertices in a grid, for their nearest neighbours
    const float cell = 0.05f * scale;
    auto key = [](int x, int y, int z) { return ((int64_t)(x & 0x1FFFFF) << 42) | ((int64_t)(y & 0x1FFFFF) << 21) | (int64_t)(z & 0x1FFFFF); };
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    const size_t m = Previous.size() / 3;
    for (size_t i = 0; i < m; i++)
        grid[key((int)std::floor(Previous[i * 3] / cell), (int)std::floor(Previous[i * 3 + 1] / cell),
                 (int)std::floor(Previous[i * 3 + 2] / cell))].push_back((uint32_t)i);

    // up to 512 of this frame's vertices, spread over the list
    const size_t stride = std::max<size_t>(1, n / 512);
    float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, T[3] = {0, 0, 0}; // this view -> the previous one
    std::vector<float> from, to;
    size_t samples = 0;
    float rms = 0;
    // between two frames the view moves little: a search radius shrinking from a tenth of the scene
    for (float radius : {0.10f, 0.06f, 0.03f, 0.02f, 0.02f})
    {
        const float rad = radius * scale;
        const int reach = (int)std::ceil(rad / cell);
        from.clear(); to.clear();
        samples = 0;
        double err = 0;
        for (size_t i = 0; i < n; i += stride)
        {
            samples++;
            const float* p = &points[i * 3];
            float q[3];
            for (int k = 0; k < 3; k++) q[k] = R[k * 3] * p[0] + R[k * 3 + 1] * p[1] + R[k * 3 + 2] * p[2] + T[k];
            const int cx = (int)std::floor(q[0] / cell), cy = (int)std::floor(q[1] / cell), cz = (int)std::floor(q[2] / cell);
            float best = rad * rad;
            int found = -1;
            for (int dz = -reach; dz <= reach; dz++)
                for (int dy = -reach; dy <= reach; dy++)
                    for (int dx = -reach; dx <= reach; dx++)
                    {
                        auto it = grid.find(key(cx + dx, cy + dy, cz + dz));
                        if (it == grid.end()) continue;
                        for (uint32_t j : it->second)
                        {
                            const float* o = &Previous[j * 3];
                            const float d = (o[0] - q[0]) * (o[0] - q[0]) + (o[1] - q[1]) * (o[1] - q[1]) + (o[2] - q[2]) * (o[2] - q[2]);
                            if (d < best) { best = d; found = (int)j; }
                        }
                    }
            if (found < 0) continue;
            from.insert(from.end(), p, p + 3);
            to.insert(to.end(), &Previous[found * 3], &Previous[found * 3] + 3);
            err += best;
        }
        if (from.size() < 3 * 16 || !FitRigidMotion(from, to, R, T)) break;
        rms = (float)std::sqrt(err / (from.size() / 3));
    }
    Previous = points;
    // accepted: most of the scenery found again, within 0.3 % of the scene's size. Else a new view: start again
    if (from.size() / 3 < samples / 2 || rms > 0.003f * scale)
    {
        Reset();
        return true;
    }
    float next[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            next[i * 3 + j] = ToWorld[i * 3] * R[j] + ToWorld[i * 3 + 1] * R[3 + j] + ToWorld[i * 3 + 2] * R[6 + j];
    std::copy(next, next + 9, ToWorld);
    return false;
}

}
