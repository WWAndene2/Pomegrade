// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <limits>
#include <numeric>
#include "video_core/pathtrace_bvh.h"

namespace VideoCore::PathTrace {

namespace {

constexpr std::size_t LeafSize = 4;

struct Box {
    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
             -std::numeric_limits<float>::max()};
    void Add(const Vec3& p) {
        for (int k = 0; k < 3; k++) {
            min[k] = std::min(min[k], p[k]);
            max[k] = std::max(max[k], p[k]);
        }
    }
};

struct Builder {
    const std::vector<Vec3>& corners;
    std::vector<Vec3> centres;
    std::vector<std::size_t> order; ///< triangle indices, reordered so each leaf's run is contiguous
    Bvh& out;

    void Node(std::size_t node, std::size_t first, std::size_t count) {
        Box box, centre_box;
        for (std::size_t i = first; i < first + count; i++) {
            const std::size_t t = order[i];
            for (int c = 0; c < 3; c++) {
                box.Add(corners[t * 3 + c]);
            }
            centre_box.Add(centres[t]);
        }
        auto& lo = out.nodes[node * 2];
        auto& hi = out.nodes[node * 2 + 1];
        lo = {box.min[0], box.min[1], box.min[2], 0.0f};
        hi = {box.max[0], box.max[1], box.max[2], 0.0f};
        int axis = 0;
        for (int k = 1; k < 3; k++) {
            if (centre_box.max[k] - centre_box.min[k] > centre_box.max[axis] - centre_box.min[axis]) {
                axis = k;
            }
        }
        if (count <= LeafSize || centre_box.max[axis] - centre_box.min[axis] <= 0.0f) {
            lo[3] = static_cast<float>(first);
            hi[3] = static_cast<float>(count);
            return;
        }
        const std::size_t half = count / 2;
        std::nth_element(order.begin() + first, order.begin() + first + half,
                         order.begin() + first + count, [&](std::size_t a, std::size_t b) {
                             return centres[a][axis] < centres[b][axis];
                         });
        const std::size_t left = out.nodes.size() / 2;
        out.nodes.resize(out.nodes.size() + 4);
        out.nodes[node * 2][3] = static_cast<float>(left);
        Node(left, first, half);
        Node(left + 1, first + half, count - half);
    }
};

} // Anonymous namespace

Bvh Build(const std::vector<Vec3>& corners) {
    Bvh bvh;
    const std::size_t count = corners.size() / 3;
    if (count == 0) {
        return bvh;
    }
    Builder b{corners, std::vector<Vec3>(count), std::vector<std::size_t>(count), bvh};
    for (std::size_t t = 0; t < count; t++) {
        for (int k = 0; k < 3; k++) {
            b.centres[t][k] =
                (corners[t * 3][k] + corners[t * 3 + 1][k] + corners[t * 3 + 2][k]) / 3.0f;
        }
    }
    std::iota(b.order.begin(), b.order.end(), std::size_t{0});
    bvh.nodes.reserve(count * 2);
    bvh.nodes.resize(2);
    b.Node(0, 0, count);
    bvh.triangles.reserve(count * 3);
    for (const std::size_t t : b.order) {
        for (int c = 0; c < 3; c++) {
            const Vec3& p = corners[t * 3 + c];
            bvh.triangles.push_back({p[0], p[1], p[2], 0.0f});
        }
    }
    return bvh;
}

} // namespace VideoCore::PathTrace
