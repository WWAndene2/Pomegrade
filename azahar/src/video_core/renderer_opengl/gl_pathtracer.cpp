// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ctime>
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_page.h"
#include "video_core/pathtrace_bvh.h"
#include "video_core/pica/regs_lcd.h"
#include "video_core/remaster.h"
#include "video_core/renderer_opengl/gl_pathtracer.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

namespace PT = VideoCore::PathTrace;

namespace {

// texture units OpenGLState never binds (it tracks 0-7; 15 is the texture runtime's temporary unit)
constexpr GLuint UnitPosition = 8, UnitNormal = 9, UnitFrame = 10, UnitNodes = 11, UnitTriangles = 12;

constexpr char GBUFFER_VERT[] = R"(
layout(location = 0) in vec4 clip;
layout(location = 1) in vec3 position;
layout(location = 2) in vec3 normal;
out vec3 v_position;
out vec3 v_normal;
void main() {
    v_position = position;
    v_normal = normal;
    gl_Position = vec4(clip.x, clip.y, -clip.z, clip.w); // as the rasterizer's vertex shaders
}
)";

// the nearest surface by its distance (the game's own depth mapping is not needed)
constexpr char GBUFFER_FRAG[] = R"(
#ifdef GL_ES
precision highp float;
#endif
in vec3 v_position;
in vec3 v_normal;
layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out float g_distance; // for Remaster's depth effects: 0 near, 1 far
void main() {
    g_distance = clamp(length(v_position) / 3000.0, 0.0, 0.999);
    g_position = vec4(v_position, 1.0);
    vec3 n = normalize(v_normal);
    if (dot(n, -v_position) < 0.0) n = -n; // toward the camera
    g_normal = vec4(n, 0.0);
    gl_FragDepth = clamp(length(v_position) * 1e-5, 0.0, 1.0);
}
)";

constexpr char FULLSCREEN_VERT[] = R"(
layout(location = 0) out vec2 frag_tex_coord;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    frag_tex_coord = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Shared by the trace and denoise passes. The passes run in the G-buffer's own layout: their
// coordinate is the G-buffer's texture coordinate.
constexpr char COMMON[] = R"(
#ifdef GL_ES
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp samplerBuffer;
#endif
layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;
layout(binding = 8) uniform sampler2D g_position;
layout(binding = 9) uniform sampler2D g_normal;
)";

constexpr char TRACE_FRAG[] = R"(
layout(binding = 10) uniform sampler2D frame;
layout(binding = 11) uniform samplerBuffer nodes;
layout(binding = 12) uniform samplerBuffer triangles;
uniform vec3 sun;          // toward the sun, view space
uniform vec3 sun_colour;
uniform vec3 up;
uniform vec3 sky_top;
uniform vec3 sky_horizon;
uniform mat4 view_to_clip;
uniform vec4 viewport;     // the main draw's viewport in the G-buffer, normalised (x, y, width, height)
uniform vec4 dr;           // the G-buffer (depth) rectangle the frame shows: s from x to y, t from z to w
uniform vec4 cr;           // the frame's rectangle in its texture, for the same coordinates
uniform float seed;
uniform float reference;   // the light of open ground in the sun, to scale by

uint state_;
float Random() {
    state_ = state_ * 747796405u + 2891336453u;
    uint w = ((state_ >> ((state_ >> 28u) + 4u)) ^ state_) * 277803737u;
    return float((w >> 22u) ^ w) * (1.0 / 4294967296.0);
}

// nearest hit closer than t_max: its distance (t_max when none) and triangle
float Hit(vec3 o, vec3 d, float t_max, out int hit_tri) {
    hit_tri = -1;
    vec3 inv = 1.0 / mix(d, vec3(1e-8), lessThan(abs(d), vec3(1e-8)));
    int stack[32];
    int top = 0;
    stack[top++] = 0;
    float best = t_max;
    while (top > 0) {
        int n = stack[--top];
        vec4 lo = texelFetch(nodes, n * 2), hi = texelFetch(nodes, n * 2 + 1);
        vec3 t0 = (lo.xyz - o) * inv, t1 = (hi.xyz - o) * inv;
        vec3 tn = min(t0, t1), tf = max(t0, t1);
        float near_ = max(max(tn.x, tn.y), max(tn.z, 0.0)), far_ = min(min(tf.x, tf.y), min(tf.z, best));
        if (near_ > far_) continue;
        if (hi.w > 0.0) {
            int first = int(lo.w), count = int(hi.w);
            for (int i = 0; i < count; i++) {
                int t = (first + i) * 3;
                vec3 a = texelFetch(triangles, t).xyz, b = texelFetch(triangles, t + 1).xyz,
                     c = texelFetch(triangles, t + 2).xyz;
                // Moller-Trumbore
                vec3 e1 = b - a, e2 = c - a, pv = cross(d, e2);
                float det = dot(e1, pv);
                if (abs(det) < 1e-9) continue;
                float id = 1.0 / det;
                vec3 tv = o - a;
                float u = dot(tv, pv) * id;
                if (u < 0.0 || u > 1.0) continue;
                vec3 qv = cross(tv, e1);
                float v = dot(d, qv) * id;
                if (v < 0.0 || u + v > 1.0) continue;
                float th = dot(e2, qv) * id;
                if (th > 1e-3 && th < best) {
                    best = th;
                    hit_tri = first + i;
                }
            }
        } else if (top < 30) {
            int left = int(lo.w);
            stack[top++] = left;
            stack[top++] = left + 1;
        }
    }
    return best;
}

vec3 Sky(vec3 d) {
    float h = dot(d, up);
    return h >= 0.0 ? mix(sky_horizon, sky_top, pow(h, 1.0 / 1.6)) : sky_horizon * 0.35; // below: the ground's glow
}

vec3 ToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

// the frame's colour where a point lies on screen (a surface's colour for its bounce), else a neutral grey
vec3 Albedo(vec3 p) {
    vec4 c = view_to_clip * vec4(p, 1.0);
    if (c.w <= 0.0) return vec3(0.35);
    vec2 ndc = c.xy / c.w;
    if (any(greaterThan(abs(ndc), vec2(1.0)))) return vec3(0.35);
    vec2 st = viewport.xy + (ndc * 0.5 + 0.5) * viewport.zw;
    vec2 q = vec2((st.x - dr.x) / (dr.y - dr.x), (st.y - dr.z) / (dr.w - dr.z));
    if (any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) return vec3(0.35);
    return ToLinear(texture(frame, vec2(mix(cr.x, cr.y, q.x), mix(cr.z, cr.w, q.y))).rgb);
}

vec3 Cosine(vec3 n) {
    float r1 = Random(), r2 = Random();
    float phi = 6.2831853 * r1, s = sqrt(r2);
    vec3 a = abs(n.x) < 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 t = normalize(cross(a, n)), b = cross(n, t);
    return normalize(t * cos(phi) * s + b * sin(phi) * s + n * sqrt(1.0 - r2));
}

void main() {
    vec4 gp = texture(g_position, frag_tex_coord);
    if (gp.w < 0.5) {
        color = vec4(1.0, 1.0, 1.0, 1.0); // no surface traced here: the frame as it is
        return;
    }
    ivec2 px = ivec2(gl_FragCoord.xy);
    state_ = uint(px.x) * 1973u + uint(px.y) * 9277u + uint(seed) * 26699u;
    vec3 p = gp.xyz, n = texture(g_normal, frag_tex_coord).xyz;
    float scale = length(p);
    vec3 o = p + n * (0.02 + scale * 0.002);
    int tri;
    // the sun: a shadow ray toward a point of the sun's disc (a different one each pixel and frame), so the shadow's
    // edge is sharp where it touches what casts it and widens with the distance to it (a real penumbra), the
    // denoiser averaging the points; the disc is drawn 3 degrees wide, wider than the real 0.5, as the offline
    // renders' soft sun
    vec3 st = normalize(cross(abs(sun.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0), sun)), sb = cross(sun, st);
    float sr = sqrt(Random()) * 0.026, sa = 6.2831853 * Random();
    vec3 sun_ray = normalize(sun + (st * cos(sa) + sb * sin(sa)) * sr);
    float lit = Hit(o, sun_ray, 1e5, tri) < 1e5 ? 0.0 : 1.0;
    vec3 light = sun_colour * max(dot(n, sun), 0.0) * lit;
    // the sky and the bounce: three rays over the hemisphere, cosine-weighted (a white surface sends back their mean)
    const int Samples = 3;
    const float Reach = 600.0, Occlusion = 40.0;
    vec3 gathered = vec3(0.0);
    float open_ = 0.0;
    for (int s = 0; s < Samples; s++) {
        vec3 d = Cosine(n);
        float t = Hit(o, d, Reach, tri);
        if (tri < 0) {
            gathered += Sky(d);
            open_ += 1.0;
            continue;
        }
        open_ += smoothstep(1.5, Occlusion, t);
        vec3 h = o + d * t;
        int at = tri * 3;
        vec3 hn = normalize(cross(texelFetch(triangles, at + 1).xyz - texelFetch(triangles, at).xyz,
                                  texelFetch(triangles, at + 2).xyz - texelFetch(triangles, at).xyz));
        if (dot(hn, d) > 0.0) hn = -hn;
        vec3 ho = h + hn * (0.02 + length(h) * 0.002);
        int tri2;
        float sun_at = max(dot(hn, sun), 0.0);
        if (sun_at > 0.0 && Hit(ho, sun, 1e5, tri2) < 1e5) sun_at = 0.0;
        gathered += Albedo(h) * (sun_colour * sun_at + 0.5 * (sky_top + sky_horizon));
    }
    light += gathered / float(Samples);
    color = vec4(light / reference, open_ / float(Samples));
}
)";

// a-trous wavelet filter (Dammertz et al.): a 5x5 B3 kernel spread by `step`, its weights cut where the G-buffer's
// positions or normals differ (shadow edges and corners kept)
constexpr char DENOISE_FRAG[] = R"(
layout(binding = 13) uniform sampler2D source;
uniform float step_;
uniform vec2 texel;
void main() {
    vec2 p = frag_tex_coord;
    vec4 gp = texture(g_position, p);
    vec4 c0 = texture(source, p);
    if (gp.w < 0.5) {
        color = c0;
        return;
    }
    vec3 n0 = texture(g_normal, p).xyz;
    float scale = 1.0 / max(length(gp.xyz), 1.0);
    const float k[3] = float[3](0.375, 0.25, 0.0625);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            vec2 q = p + vec2(float(x), float(y)) * texel * step_;
            vec4 gq = texture(g_position, q);
            if (gq.w < 0.5) continue;
            float wn = pow(max(dot(n0, texture(g_normal, q).xyz), 0.0), 32.0);
            float wp = exp(-length(gq.xyz - gp.xyz) * scale * 40.0);
            float w = k[abs(x)] * k[abs(y)] * wn * wp;
            sum += texture(source, q) * w;
            wsum += w;
        }
    }
    color = wsum > 0.0 ? sum / wsum : c0;
}
)";

// temporal accumulation: the history kept where the pixel shows the same point as last frame (its view-space position
// within 1 % of its distance), else the frame's own light
constexpr char TEMPORAL_FRAG[] = R"(
layout(binding = 13) uniform sampler2D current;
layout(binding = 14) uniform sampler2D history_;
layout(binding = 10) uniform sampler2D previous;
uniform float first;
void main() {
    vec2 p = frag_tex_coord;
    vec4 now = texture(current, p);
    vec4 gp = texture(g_position, p), pp = texture(previous, p);
    bool same = first < 0.5 && gp.w > 0.5 && pp.w > 0.5 && length(gp.xyz - pp.xyz) < 0.01 * length(gp.xyz) + 0.05;
    color = same ? mix(now, texture(history_, p), 0.8) : now;
}
)";

void Normalise(float v[3]) {
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) {
        v[0] /= l, v[1] /= l, v[2] /= l;
    }
}

void Srgb(float out[3], u32 hex, float power) {
    for (int k = 0; k < 3; k++) {
        out[k] = std::pow(static_cast<float>((hex >> (16 - 8 * k)) & 0xFF) / 255.0f, 2.2f) * power;
    }
}

// 4x4 least squares, rows of m: clip = M (view, 1), from every vertex of the frame (each row its own system)
bool FitProjection(const std::vector<std::array<float, 8>>& samples, float m[16]) {
    double a[4][4] = {}, b[4][4] = {};
    for (const auto& s : samples) {
        const double v[4] = {s[4], s[5], s[6], 1.0};
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                a[i][j] += v[i] * v[j];
            }
            for (int r = 0; r < 4; r++) {
                b[r][i] += v[i] * s[r];
            }
        }
    }
    for (int r = 0; r < 4; r++) {
        double x[4][5];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) x[i][j] = a[i][j];
            x[i][4] = b[r][i];
        }
        for (int c = 0; c < 4; c++) {
            int pivot = c;
            for (int i = c + 1; i < 4; i++) {
                if (std::abs(x[i][c]) > std::abs(x[pivot][c])) pivot = i;
            }
            if (std::abs(x[pivot][c]) < 1e-12) return false;
            for (int j = 0; j < 5; j++) std::swap(x[c][j], x[pivot][j]);
            for (int i = 0; i < 4; i++) {
                if (i == c) continue;
                const double f = x[i][c] / x[c][c];
                for (int j = c; j < 5; j++) x[i][j] -= f * x[c][j];
            }
        }
        // column-major for glUniformMatrix4fv: element (row r, column c) at c * 4 + r
        for (int c = 0; c < 4; c++) m[c * 4 + r] = static_cast<float>(x[c][4] / x[c][c]);
    }
    return true;
}

} // Anonymous namespace

PathTracerGL::PathTracerGL() = default;
PathTracerGL::~PathTracerGL() = default;

bool PathTracerGL::Enabled() {
    return VideoCore::Remaster::SurfaceMode() == 2;
}

void PathTracerGL::Capture(PAddr target, const std::array<GLint, 4>& viewport,
                           std::span<const InputVertex> vertices) {
    if (target == 0 || vertices.size() < 3) {
        return;
    }
    auto& slot = frames[target];
    if (!slot || copied[target]) {
        slot = std::make_shared<Frame>();
        copied[target] = false;
    }
    Frame& frame = *slot;
    const std::size_t first = frame.vertices.size();
    for (const auto& v : vertices) {
        Vertex out;
        for (int k = 0; k < 4; k++) out.clip[k] = v.clip[k];
        // the lighting's view vector runs from the surface to the camera: the surface is at its opposite
        for (int k = 0; k < 3; k++) out.position[k] = -v.view[k];
        // the normal: the quaternion turning +z (as the fragment lighting's quaternion_rotate does)
        float q[4] = {v.normquat[0], v.normquat[1], v.normquat[2], v.normquat[3]};
        const float ql = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (ql > 0) for (float& c : q) c /= ql;
        out.normal[0] = 2.0f * (q[0] * q[2] + q[3] * q[1]);
        out.normal[1] = 2.0f * (q[1] * q[2] - q[3] * q[0]);
        out.normal[2] = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
        frame.vertices.push_back(out);
    }
    frame.batches.push_back({viewport, first, vertices.size()});
}

void PathTracerGL::Complete(PAddr target, PAddr display) {
    const auto it = frames.find(target);
    if (it == frames.end() || !it->second) {
        return;
    }
    completed[display] = it->second;
    copied[target] = true;
}

void PathTracerGL::Resize(u32 w, u32 h) {
    width = w;
    height = h;
    const auto make = [](OGLTexture& texture, GLenum format, u32 tw, u32 th) {
        texture.Release();
        texture.Create();
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, texture.handle);
        glTexStorage2D(GL_TEXTURE_2D, 1, format, static_cast<GLsizei>(tw), static_cast<GLsizei>(th));
    };
    make(gbuffer_position, GL_RGBA32F, w, h);
    make(gbuffer_normal, GL_RGBA16F, w, h);
    make(gbuffer_distance, GL_R32F, w, h);
    make(previous_position, GL_RGBA32F, w, h);
    have_history = false;
    gbuffer_depth.Release();
    gbuffer_depth.Create();
    glBindRenderbuffer(GL_RENDERBUFFER, gbuffer_depth.handle);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(w),
                          static_cast<GLsizei>(h));
    gbuffer_fbo.Release();
    gbuffer_fbo.Create();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer_fbo.handle);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gbuffer_position.handle, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gbuffer_normal.handle, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gbuffer_distance.handle, 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gbuffer_depth.handle);
    const GLenum targets[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, targets);
    const u32 hw = w, hh = h; // full size: the sun's shadows stay sharp
    for (int i = 0; i < 2; i++) {
        make(light[i], GL_RGBA16F, hw, hh);
        make(history[i], GL_RGBA16F, hw, hh);
        history_fbo[i].Release();
        history_fbo[i].Create();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, history_fbo[i].handle);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history[i].handle, 0);
        light_fbo[i].Release();
        light_fbo[i].Create();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, light_fbo[i].handle);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, light[i].handle, 0);
    }
    if (trace_program.handle == 0) {
        gbuffer_program.Create(GBUFFER_VERT, GBUFFER_FRAG);
        trace_program.Create(FULLSCREEN_VERT, std::string(COMMON) + TRACE_FRAG);
        denoise_program.Create(FULLSCREEN_VERT, std::string(COMMON) + DENOISE_FRAG);
        temporal_program.Create(FULLSCREEN_VERT, std::string(COMMON) + TEMPORAL_FRAG);
        gbuffer_vao.Create();
        fullscreen_vao.Create();
        gbuffer_vbo.Create();
        nodes_buffer.Create();
        triangles_buffer.Create();
        nodes_texture.Create();
        triangles_texture.Create();
        for (auto [sampler, filter] : {std::pair{&nearest, GL_NEAREST}, std::pair{&linear, GL_LINEAR}}) {
            sampler->Create();
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER, filter);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, filter);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

void PathTracerGL::Upload(const Frame& frame) {
    // the triangles in view space, their BVH
    std::vector<PT::Vec3> corners;
    corners.reserve(frame.vertices.size());
    for (const Vertex& v : frame.vertices) {
        corners.push_back({v.position[0], v.position[1], v.position[2]});
    }
    const PT::Bvh bvh = PT::Build(corners);
    node_count = bvh.nodes.size() / 2;
    const auto upload = [](OGLBuffer& buffer, OGLTexture& texture, GLuint unit,
                           const std::vector<std::array<float, 4>>& data) {
        glBindBuffer(GL_TEXTURE_BUFFER, buffer.handle);
        glBufferData(GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(data[0])), data.data(),
                     GL_STREAM_DRAW);
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_BUFFER, texture.handle);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buffer.handle);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    };
    upload(nodes_buffer, nodes_texture, UnitNodes, bvh.nodes);
    upload(triangles_buffer, triangles_texture, UnitTriangles, bvh.triangles);

    // up: the normal of the frame's largest flat area (the ground); first every face toward the camera, then those
    // near that mean, so the walls' share drops out
    float mean[3] = {}, ground[3] = {};
    std::vector<std::array<float, 4>> faces; // normal, area
    faces.reserve(corners.size() / 3);
    for (std::size_t t = 0; t + 2 < corners.size(); t += 3) {
        const auto &a = corners[t], &b = corners[t + 1], &c = corners[t + 2];
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const float area = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (area <= 0) continue;
        for (float& x : n) x /= area;
        if (n[0] * -a[0] + n[1] * -a[1] + n[2] * -a[2] < 0) for (float& x : n) x = -x;
        faces.push_back({n[0], n[1], n[2], area});
        for (int k = 0; k < 3; k++) mean[k] += n[k] * area;
    }
    Normalise(mean);
    for (const auto& f : faces) {
        if (f[0] * mean[0] + f[1] * mean[1] + f[2] * mean[2] > 0.8f) {
            for (int k = 0; k < 3; k++) ground[k] += f[k] * f[3];
        }
    }
    Normalise(ground);
    if (ground[0] != 0 || ground[1] != 0 || ground[2] != 0) std::copy(ground, ground + 3, up);

    // north: the camera's forward (-z) along the ground; east to its right
    float north[3] = {up[2] * up[0], up[2] * up[1], -1.0f + up[2] * up[2]};
    Normalise(north);
    float east[3] = {north[1] * up[2] - north[2] * up[1], north[2] * up[0] - north[0] * up[2],
                     north[0] * up[1] - north[1] * up[0]};
    Normalise(east);

    // the sun from the game's clock (the emulated 3DS's, as the game reads it for its own day and night: the device's
    // clock differs when the emulator's clock is set otherwise, and the game then lit its night under a day sun):
    // rises in the east at 6, crosses the north (behind the scene, as the camera looks) and sets in the west at 18, 55
    // degrees up at noon; between 18 and 6 the moon, 35 degrees up in the north, dim and blue (pt.html's colours). Over
    // the north so the shadows fall toward the camera, as the game's painted ones do (its art direction): over the south
    // the full render's shadows fell behind the houses, out of sight; straight over the north the facades facing the
    // camera were all in shade, darker than the reference. So its path leans 40 % to the north (9 October)
    const u64 seconds = Core::System::GetInstance().Kernel().GetSharedPageHandler().GetSystemTimeSince2000() / 1000;
    const float hour = static_cast<float>(seconds % 86400) / 3600.0f;
    const bool day = hour >= 6.0f && hour < 18.0f;
    const float pi = 3.14159265f;
    float elevation, along;
    if (day) {
        along = (hour - 6.0f) / 12.0f;
        elevation = std::max(4.0f, 55.0f * std::sin(pi * along)) * pi / 180.0f;
        const float warm = std::clamp(elevation / (30.0f * pi / 180.0f), 0.0f, 1.0f);
        float low[3], high[3];
        Srgb(low, 0xff8c48, 2.2f);
        Srgb(high, 0xfff5e6, 3.0f);
        for (int k = 0; k < 3; k++) sun_colour[k] = low[k] + (high[k] - low[k]) * warm;
        Srgb(sky_top, warm > 0.5f ? 0x6aa6ec : 0x4a5f9a, 1.0f);
        Srgb(sky_horizon, warm > 0.5f ? 0xf2dcc0 : 0xf09a60, 1.0f);
    } else {
        along = 0.5f;
        elevation = 35.0f * pi / 180.0f;
        Srgb(sun_colour, 0x9db4ff, 0.6f);
        Srgb(sky_top, 0x1a2440, 1.0f);
        Srgb(sky_horizon, 0x2a3350, 1.0f);
    }
    for (int k = 0; k < 3; k++) {
        const float horizontal = std::cos(pi * along) * east[k] + 0.4f * std::sin(pi * along) * north[k];
        sun[k] = std::cos(elevation) * horizontal + std::sin(elevation) * up[k];
    }
    Normalise(sun);

    // the camera's projection, for where a bounce lands on screen
    std::vector<std::array<float, 8>> samples;
    const std::size_t stride = std::max<std::size_t>(1, frame.vertices.size() / 4096);
    for (std::size_t i = 0; i < frame.vertices.size(); i += stride) {
        const Vertex& v = frame.vertices[i];
        samples.push_back({v.clip[0], v.clip[1], v.clip[2], v.clip[3], v.position[0], v.position[1], v.position[2], 0});
    }
    if (!FitProjection(samples, view_to_clip)) std::fill(std::begin(view_to_clip), std::end(view_to_clip), 0.0f);
    // the viewport most of the frame's triangles were drawn through
    std::size_t most = 0;
    for (const Batch& b : frame.batches) {
        if (b.count > most) most = b.count, main_viewport = b.viewport;
    }
}

PathTracerGL::Result PathTracerGL::Trace(const ScreenInfo& screen_info, GLuint frame_texture) {
    const auto it = completed.find(screen_info.display_address);
    if (it == completed.end() || !it->second || it->second->vertices.empty() || screen_info.target_width == 0) {
        return {};
    }
    const Frame& frame = *it->second;
    const OpenGLState saved = OpenGLState::GetCurState();
    if (screen_info.target_width != width || screen_info.target_height != height) {
        Resize(screen_info.target_width, screen_info.target_height);
    }
    Upload(frame);

    // the G-buffer: the frame's lit triangles again, through their own viewports
    OpenGLState state = saved;
    state.blend.enabled = false;
    state.cull.enabled = false;
    state.stencil.test_enabled = false;
    state.scissor.enabled = false;
    state.color_mask = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    state.depth.test_enabled = true;
    state.depth.test_func = GL_LESS;
    state.depth.write_mask = GL_TRUE;
    state.draw.draw_framebuffer = gbuffer_fbo.handle;
    state.draw.shader_program = gbuffer_program.handle;
    state.draw.vertex_array = gbuffer_vao.handle;
    state.draw.vertex_buffer = gbuffer_vbo.handle;
    state.viewport = {0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)};
    state.Apply();
    const GLfloat clear_colour[4] = {0, 0, 0, 0};
    glClearBufferfv(GL_COLOR, 0, clear_colour);
    glClearBufferfv(GL_COLOR, 1, clear_colour);
    const GLfloat far_[4] = {1, 1, 1, 1};
    glClearBufferfv(GL_COLOR, 2, far_);
    const GLfloat far_depth = 1.0f;
    glClearBufferfv(GL_DEPTH, 0, &far_depth);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(frame.vertices.size() * sizeof(Vertex)),
                 frame.vertices.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, clip)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    for (const Batch& b : frame.batches) {
        state.viewport = {b.viewport[0], b.viewport[1], b.viewport[2], b.viewport[3]};
        state.Apply();
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(b.first), static_cast<GLsizei>(b.count));
    }

    // the light, traced, then denoised
    const u32 hw = width, hh = height;
    const auto bind = [](GLuint unit, GLuint texture, GLuint sampler) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindSampler(unit, sampler);
    };
    bind(UnitPosition, gbuffer_position.handle, nearest.handle);
    bind(UnitNormal, gbuffer_normal.handle, nearest.handle);
    bind(UnitFrame, frame_texture, linear.handle);
    state.depth.test_enabled = false;
    state.draw.vertex_array = fullscreen_vao.handle;
    state.draw.shader_program = trace_program.handle;
    state.draw.draw_framebuffer = light_fbo[0].handle;
    state.viewport = {0, 0, static_cast<GLsizei>(hw), static_cast<GLsizei>(hh)};
    state.Apply();
    const GLuint tp = trace_program.handle;
    // open ground in the sun: the sun on the ground plus the sky's mean
    float reference = std::max(0.0f, sun[0] * up[0] + sun[1] * up[1] + sun[2] * up[2]);
    float ref_rgb[3];
    for (int k = 0; k < 3; k++) ref_rgb[k] = sun_colour[k] * reference + 0.5f * (sky_top[k] + sky_horizon[k]);
    reference = 0.2126f * ref_rgb[0] + 0.7152f * ref_rgb[1] + 0.0722f * ref_rgb[2];
    const auto& tc = screen_info.display_texcoords;
    const auto& d = screen_info.target_rect;
    const float vp[4] = {static_cast<float>(main_viewport[0]) / static_cast<float>(width),
                         static_cast<float>(main_viewport[1]) / static_cast<float>(height),
                         static_cast<float>(main_viewport[2]) / static_cast<float>(width),
                         static_cast<float>(main_viewport[3]) / static_cast<float>(height)};
    glProgramUniform3fv(tp, glGetUniformLocation(tp, "sun"), 1, sun);
    glProgramUniform3fv(tp, glGetUniformLocation(tp, "sun_colour"), 1, sun_colour);
    glProgramUniform3fv(tp, glGetUniformLocation(tp, "up"), 1, up);
    glProgramUniform3fv(tp, glGetUniformLocation(tp, "sky_top"), 1, sky_top);
    glProgramUniform3fv(tp, glGetUniformLocation(tp, "sky_horizon"), 1, sky_horizon);
    glProgramUniformMatrix4fv(tp, glGetUniformLocation(tp, "view_to_clip"), 1, GL_FALSE, view_to_clip);
    glProgramUniform4fv(tp, glGetUniformLocation(tp, "viewport"), 1, vp);
    glProgramUniform4fv(tp, glGetUniformLocation(tp, "dr"), 1, d.data());
    glProgramUniform4f(tp, glGetUniformLocation(tp, "cr"), tc.top, tc.bottom, tc.left, tc.right);
    glProgramUniform1f(tp, glGetUniformLocation(tp, "seed"), static_cast<float>(frame_number++ % 4096));
    glProgramUniform1f(tp, glGetUniformLocation(tp, "reference"), std::max(reference, 1e-3f));
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const GLuint dp = denoise_program.handle;
    glProgramUniform2f(dp, glGetUniformLocation(dp, "texel"), 1.0f / static_cast<float>(hw),
                       1.0f / static_cast<float>(hh));
    state.draw.shader_program = dp;
    int from = 0;
    for (const float step : {1.0f, 2.0f}) {
        bind(13, light[from].handle, nearest.handle);
        state.draw.draw_framebuffer = light_fbo[1 - from].handle;
        state.Apply();
        glProgramUniform1f(dp, glGetUniformLocation(dp, "step_"), step);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        from = 1 - from;
    }
    // accumulated over frames
    const GLuint tmp = temporal_program.handle;
    bind(13, light[from].handle, nearest.handle);
    bind(14, history[history_index].handle, nearest.handle);
    bind(UnitFrame, previous_position.handle, nearest.handle);
    state.draw.shader_program = tmp;
    state.draw.draw_framebuffer = history_fbo[1 - history_index].handle;
    state.Apply();
    glProgramUniform1f(tmp, glGetUniformLocation(tmp, "first"), have_history ? 0.0f : 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    history_index = 1 - history_index;
    have_history = true;
    glCopyImageSubData(gbuffer_position.handle, GL_TEXTURE_2D, 0, 0, 0, 0, previous_position.handle, GL_TEXTURE_2D, 0,
                       0, 0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1);
    for (GLuint unit : {UnitPosition, UnitNormal, UnitFrame, GLuint{13}, GLuint{14}}) bind(unit, 0, 0);
    glActiveTexture(GL_TEXTURE0);
    saved.Apply();
    Result result{history[history_index].handle, gbuffer_distance.handle};
    // the sun's direction on screen: a point of the scene and the same point moved toward the sun, both projected
    {
        double centre[3] = {};
        const std::size_t step_ = std::max<std::size_t>(1, frame.vertices.size() / 512);
        std::size_t count = 0;
        for (std::size_t i = 0; i < frame.vertices.size(); i += step_, count++) {
            for (int k = 0; k < 3; k++) centre[k] += frame.vertices[i].position[k];
        }
        const auto project = [&](const float q[3], float out[2]) {
            float c[4];
            for (int r = 0; r < 4; r++) {
                c[r] = view_to_clip[r] * q[0] + view_to_clip[4 + r] * q[1] + view_to_clip[8 + r] * q[2] +
                       view_to_clip[12 + r];
            }
            const float w = std::abs(c[3]) > 1e-6f ? c[3] : 1e-6f;
            out[0] = vp[0] + (c[0] / w * 0.5f + 0.5f) * vp[2];
            out[1] = vp[1] + (c[1] / w * 0.5f + 0.5f) * vp[3];
        };
        float a[3], b[3], pa[2], pb[2];
        const float scale = 0.05f * static_cast<float>(std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] +
                                                                  centre[2] * centre[2]) / std::max<std::size_t>(count, 1));
        for (int k = 0; k < 3; k++) {
            a[k] = static_cast<float>(centre[k] / std::max<std::size_t>(count, 1));
            b[k] = a[k] + sun[k] * std::max(scale, 1.0f);
        }
        project(a, pa);
        project(b, pb);
        const float dx = pb[0] - pa[0], dy = pb[1] - pa[1], l = std::sqrt(dx * dx + dy * dy);
        if (l > 1e-6f) result.sun_on_screen[0] = dx / l, result.sun_on_screen[1] = dy / l;
        result.sun_up = sun[0] * up[0] + sun[1] * up[1] + sun[2] * up[2];
    }
    return result;
}

} // namespace OpenGL
