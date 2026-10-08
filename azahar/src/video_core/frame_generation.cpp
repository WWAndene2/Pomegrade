// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cmath>
#include "common/settings.h"
#include "video_core/frame_generation.h"

namespace VideoCore::FrameGeneration {

namespace {
std::atomic<float> display_hz{0.0f};
std::atomic<bool> display_hot{false};
} // namespace

Mode CurrentMode() {
    return static_cast<Mode>(std::min<u32>(Settings::values.frame_rate_mode.GetValue(), 5));
}

void SetDisplay(float refresh_hz, bool hot) {
    display_hz = refresh_hz;
    display_hot = hot;
}

bool ShowsHalf() {
    return CurrentMode() == Mode::Fps30;
}

// the most images shown per second the mode allows, 0 when it generates none
static float Cap() {
    switch (CurrentMode()) {
    case Mode::Fps60Smooth:
        return 60.0f;
    case Mode::Fps120:
        return 120.0f;
    case Mode::Fps240:
        return 240.0f;
    case Mode::Adaptive:
        return display_hot ? 0.0f : 240.0f;
    default:
        return 0.0f;
    }
}

bool Generates() {
    return Cap() > 0.0f;
}

u32 ImagesBetween(double interval_s) {
    const float cap = Cap();
    if (cap <= 0.0f || interval_s <= 0.0) {
        return 0;
    }
    const float screen = display_hz > 1.0f ? display_hz.load() : 60.0f;
    const double slots = std::round(interval_s * std::min(cap, screen));
    // 7 at most: a game paused or loading (long intervals) shows its image as is
    return slots >= 2.0 && slots <= 8.0 ? static_cast<u32>(slots) - 1 : 0;
}

// The bindings and parameters of both renderers: Vulkan a descriptor set and push constants, OpenGL binding points and
// two uniforms (Source() puts POMEGRADE_VULKAN 1 or 0 first; VULKAN itself is predefined by the Vulkan GLSL compiler)
static constexpr char COMMON[] = R"(
#if POMEGRADE_VULKAN
#define TEXTURE(n) layout(set = 0, binding = n) uniform sampler2D
layout(push_constant, std140) uniform Params {
    vec4 p0;
    vec4 p1;
} params;
#define P0 params.p0
#define P1 params.p1
#else
#ifdef GL_ES
precision highp float;
precision highp sampler2D;
#endif
#define TEXTURE(n) layout(binding = n) uniform sampler2D
uniform vec4 p0;
uniform vec4 p1;
#define P0 p0
#define P1 p1
#endif
layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;
TEXTURE(0) image_a;
TEXTURE(1) image_b;
TEXTURE(2) aux;
)";

const char* const DOWNSAMPLE_FRAG = R"(
float Luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}
void main() {
    // four bilinear taps a quarter of a small texel from the centre: the average of about 4x4 screen pixels around it
    vec2 q = 0.25 * P0.yz;
    vec2 uv = frag_tex_coord;
    float a = Luma(texture(image_a, uv + vec2(-q.x, -q.y)).rgb) + Luma(texture(image_a, uv + vec2(q.x, -q.y)).rgb) +
              Luma(texture(image_a, uv + vec2(-q.x, q.y)).rgb) + Luma(texture(image_a, uv + vec2(q.x, q.y)).rgb);
    float b = Luma(texture(image_b, uv + vec2(-q.x, -q.y)).rgb) + Luma(texture(image_b, uv + vec2(q.x, -q.y)).rgb) +
              Luma(texture(image_b, uv + vec2(-q.x, q.y)).rgb) + Luma(texture(image_b, uv + vec2(q.x, q.y)).rgb);
    color = vec4(0.25 * a, 0.25 * b, 0.0, 1.0);
}
)";

const char* const MOTION_FRAG = R"(
// the luma of A (r) and B (g) at 1/8 size is `aux`; the search covers +-4 small texels (+-32 screen pixels)
const int R = 4;
float Cost(vec2 uv, vec2 ts, vec2 v, float t) {
    float c = 0.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 o = vec2(float(i), float(j));
            float a = texture(aux, uv + (o - t * v) * ts).r;
            float b = texture(aux, uv + (o + (1.0 - t) * v) * ts).g;
            c += abs(a - b);
        }
    }
    // a small preference for small motions: flat areas, where every motion matches, stay still
    return c + 0.002 * dot(v, v);
}
void main() {
    float t = P0.x;
    vec2 ts = P0.yz;
    vec2 uv = frag_tex_coord;
    float best = 1e9;
    vec2 bv = vec2(0.0);
    for (int y = -R; y <= R; y++) {
        for (int x = -R; x <= R; x++) {
            vec2 v = vec2(float(x), float(y));
            float c = Cost(uv, ts, v, t);
            if (c < best) {
                best = c;
                bv = v;
            }
        }
    }
    // below a small texel: a parabola through the costs on each side of the best motion, along x then y
    float cl = Cost(uv, ts, bv - vec2(1.0, 0.0), t), cr = Cost(uv, ts, bv + vec2(1.0, 0.0), t);
    float cd = Cost(uv, ts, bv - vec2(0.0, 1.0), t), cu = Cost(uv, ts, bv + vec2(0.0, 1.0), t);
    float dx = cl + cr - 2.0 * best, dy = cd + cu - 2.0 * best;
    vec2 sub = vec2(dx > 1e-5 ? 0.5 * (cl - cr) / dx : 0.0, dy > 1e-5 ? 0.5 * (cd - cu) / dy : 0.0);
    color = vec4(bv + clamp(sub, -0.5, 0.5), best / 9.0, 1.0);
}
)";

const char* const WARP_FRAG = R"(
void main() {
    float t = P0.x;
    vec2 uv = frag_tex_coord;
    // the motion in screen texture coordinates (`aux` holds it in small texels)
    vec2 v = texture(aux, uv).xy * P0.yz;
    vec3 a = texture(image_a, uv - t * v).rgb;
    vec3 b = texture(image_b, uv + (1.0 - t) * v).rgb;
    vec3 blend = mix(a, b, t);
    // where the two moved images disagree, a blend would show a ghost of both: the nearer one instead
    float k = smoothstep(0.12, 0.3, length(a - b));
    color = vec4(mix(blend, t < 0.5 ? a : b, k), 1.0);
}
)";

const char* const FULLSCREEN_VERT = R"(
#if POMEGRADE_VULKAN
#define VERTEX gl_VertexIndex
#else
#define VERTEX gl_VertexID
#endif
layout(location = 0) out vec2 frag_tex_coord;
void main() {
    vec2 p = vec2(float((VERTEX << 1) & 2), float(VERTEX & 2));
    frag_tex_coord = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

std::string Source(const char* body, bool vulkan) {
    std::string s = vulkan ? "#version 450 core\n#define POMEGRADE_VULKAN 1\n" : "#define POMEGRADE_VULKAN 0\n";
    // the vertex shader has no fragment outputs or samplers
    if (body != FULLSCREEN_VERT) {
        s += COMMON;
    } else if (!vulkan) {
        s += "#ifdef GL_ES\nprecision highp float;\n#endif\n";
    }
    s += body;
    return s;
}

} // namespace VideoCore::FrameGeneration
