// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

// Pomegrade: the bounding volume hierarchy the path tracer (renderer_opengl/gl_pathtracer.h) traces its rays through,
// built on the CPU once per frame from the frame's own triangles, in the camera's (view) space. Laid out for a shader's
// texture buffer of vec4s:
// - nodes, two vec4 each: (min.xyz, a), (max.xyz, b); a leaf has b > 0: its b triangles from triangle a; an inner node has
//   b = 0 and its two children at nodes a and a + 1;
// - triangles, three vec4 each: the corners (xyz, w unused), in the order the leaves name them.
// Split at the median of the triangles' centres along the longest axis of their bounds, up to 4 triangles a leaf: an
// O(n log n) build (a frame of a 3DS game holds some tens of thousands of triangles) whose trees trace well enough.
namespace VideoCore::PathTrace {

using Vec3 = std::array<float, 3>;

struct Bvh {
    std::vector<std::array<float, 4>> nodes;     ///< two per node, the root first
    std::vector<std::array<float, 4>> triangles; ///< three per triangle
};

/// The hierarchy of the triangles (three corners each, in order); empty when there are none
Bvh Build(const std::vector<Vec3>& corners);

} // namespace VideoCore::PathTrace
