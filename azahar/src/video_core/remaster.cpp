// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "video_core/remaster.h"

namespace VideoCore::Remaster {

Params Current() {
    Params p;
    const u32 preset = Settings::values.remaster_preset.GetValue();
    if (preset == 0) {
        return p;
    }
    p.enabled = true;
    // v7 (ORAS_RENDER.md "v7 values"): the two-tone step, vibrance 1.28, the layers (sky light, contours, glow, aerial
    // perspective, far blur, contrast 0.7); its occlusion came with the path-traced light, here a lighter screen-space one
    p.grading = 1.0f;
    p.ao = 0.5f;
    p.sky_light = 1.0f;
    p.outline = 1.0f;
    p.glow = 1.0f;
    p.aerial = 1.0f;
    p.far_blur = 1.0f;
    p.vibrance = 1.28f;
    p.contrast = 0.7f;
    if (preset == 1) {
        return p;
    }
    // v9: the occlusion at full strength, the sky fill, the adaptive contrast (the texture relief, off in v9, is not done)
    p.ao = 1.0f;
    p.sky_fill = 1.0f;
    p.adaptive_contrast = true;
    if (preset == 2) {
        return p;
    }
    // custom: v9 with each effect on its switch
    const auto& v = Settings::values;
    if (!v.remaster_grading.GetValue()) p.grading = 0;
    if (!v.remaster_ao.GetValue()) p.ao = 0;
    if (!v.remaster_sky.GetValue()) p.sky_fill = p.sky_light = 0;
    if (!v.remaster_outline.GetValue()) p.outline = 0;
    if (!v.remaster_glow.GetValue()) p.glow = 0;
    if (!v.remaster_aerial.GetValue()) p.aerial = 0;
    if (!v.remaster_far_blur.GetValue()) p.far_blur = 0;
    if (!v.remaster_vibrance.GetValue()) p.vibrance = 1.0f;
    if (!v.remaster_contrast.GetValue()) p.contrast = 0, p.adaptive_contrast = false;
    return p;
}

int SurfaceMode() {
    const u32 preset = Settings::values.remaster_preset.GetValue();
    if (Settings::values.graphics_api.GetValue() != Settings::GraphicsAPI::OpenGL) {
        return -1; // the shading is written for the OpenGL shader generator only
    }
    return preset == 0 ? -1 : preset == 1 ? 0 : 2;
}

static constexpr char COMMON[] = R"(
#ifdef GL_ES
precision highp float;
precision highp sampler2D;
#endif
layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D frame;
layout(binding = 1) uniform sampler2D depth;
layout(binding = 2) uniform sampler2D stats;
layout(binding = 3) uniform sampler2D bright;
// cr, dr: the frame's and the depth's rectangle in their textures (u from x to y, v from z to w) for this pass's
// coordinates; texel: 1 / width, 1 / height of the pass, whether there is a depth, the stats' smallest mipmap
uniform vec4 cr;
uniform vec4 dr;
uniform vec4 texel;
uniform vec4 p0; // grading, ao, sky fill, sky light
uniform vec4 p1; // outline, glow, aerial, far blur
uniform vec4 p2; // vibrance, contrast, adaptive contrast, 0
vec3 Frame(vec2 p) {
    return texture(frame, vec2(mix(cr.x, cr.y, p.x), mix(cr.z, cr.w, p.y))).rgb;
}
float Luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}
)";

const char* const STATS_FRAG = R"(
void main() {
    float l = Luma(Frame(frag_tex_coord));
    color = vec4(l, l * l, 0.0, 1.0);
}
)";

const char* const BRIGHT_FRAG = R"(
void main() {
    vec3 c = Frame(frag_tex_coord);
    color = vec4(c * smoothstep(0.72, 1.0, Luma(c)), 1.0);
}
)";

// The pass's u runs along the screen's height (the 3DS framebuffer is stored turned): up the screen is +u, right is +v.
// The depth's direction (which end is far) differs between games: the screen's top (sky, the far scenery) against its
// bottom (the ground near the player) tells it, per frame.
const char* const MAIN_FRAG = R"(
float RawDepth(vec2 p) {
    return texture(depth, vec2(mix(dr.x, dr.y, p.x), mix(dr.z, dr.w, p.y))).r;
}
float far_is_big;
// 0 near, 1 far, whichever way the game's depth runs
float Far(vec2 p) {
    float d = RawDepth(p);
    return far_is_big > 0.5 ? d : 1.0 - d;
}
void main() {
    vec2 p = frag_tex_coord;
    vec2 t = texel.xy;
    vec3 c = Frame(p);
    float lum = Luma(c);
    float step_ = smoothstep(0.60, 0.78, lum); // the v7 two-tone step: 0 shade, 1 sun
    bool has_depth = texel.z > 0.5;

    float ao = 1.0, f0 = 0.0, up = 0.0, edge = 0.0;
    if (has_depth) {
        far_is_big = RawDepth(vec2(0.95, 0.5)) >= RawDepth(vec2(0.05, 0.5)) ? 1.0 : 0.0;
        f0 = Far(p);
        // occlusion: 8 taps on two rings; a tap nearer than the pixel occludes it, a much nearer one (another object in
        // front) not; the depth's precision falls with distance, so the differences are scaled by how far the pixel is
        float scale = 1.0 / max(1.0 - f0, 0.002);
        float occ = 0.0;
        for (int k = 0; k < 8; k++) {
            float a = float(k) * 0.785398 + 0.39;
            float r = (k < 4 ? 6.0 : 14.0);
            vec2 o = vec2(cos(a), sin(a)) * r * t;
            float d = (f0 - Far(p + o)) * scale;
            occ += clamp(d * 40.0, 0.0, 1.0) * (1.0 - smoothstep(0.08, 0.25, d));
        }
        ao = 1.0 - occ / 8.0;
        // the normal from the depth: a surface whose depth grows up the screen faces up (the ground, roofs)
        float fu = Far(p + vec2(t.x, 0.0)), fd = Far(p - vec2(t.x, 0.0));
        float fr = Far(p + vec2(0.0, t.y)), fl = Far(p - vec2(0.0, t.y));
        up = clamp((fu - fd) * scale * 60.0, 0.0, 1.0);
        edge = smoothstep(0.02, 0.06, max(max(abs(fu - f0), abs(fd - f0)), max(abs(fr - f0), abs(fl - f0))) * scale);
    }

    // two-tone light: a cool lavender shade with a cool lift (shade shows on saturated green), a warm sun
    vec3 shade = vec3(0.86, 0.84, 1.0), sun = vec3(1.05, 1.0, 0.93);
    c = mix(c, c * mix(shade, sun, step_) + vec3(0.012, 0.016, 0.034) * (1.0 - step_), p0.x);
    // occlusion, v9's way: strong in the shade, weak in the sun
    c *= 1.0 - (1.0 - pow(ao, 1.3)) * (0.55 - 0.4 * step_) * p0.y;
    // v9's sky fill in the open shade, and the sky's light on surfaces facing up
    c += vec3(0.010, 0.018, 0.045) * (1.0 - step_) * ao * p0.z;
    c += vec3(0.03, 0.045, 0.07) * up * p0.w;
    // contours on depth breaks
    c *= 1.0 - 0.35 * edge * p1.x;
    // glow: the highlights blurred by their mipmaps
    vec3 g = textureLod(bright, p, 2.0).rgb * 0.6 + textureLod(bright, p, 3.5).rgb * 0.4;
    c += g * 0.35 * p1.y;
    if (has_depth) {
        // aerial perspective and far blur on the farthest scenery
        float far_ = smoothstep(0.80, 1.0, f0);
        vec3 blur = (Frame(p + vec2(1.5, 0.0) * t) + Frame(p - vec2(1.5, 0.0) * t) +
                     Frame(p + vec2(0.0, 1.5) * t) + Frame(p - vec2(0.0, 1.5) * t)) * 0.25;
        c = mix(c, blur * mix(shade, sun, step_), far_ * 0.5 * p1.w);
        c = mix(c, vec3(0.78, 0.86, 1.0) * max(Luma(c), 0.6), far_ * 0.18 * p1.z);
    }
    // vibrance: the dull colours raised more than the vivid ones
    float l2 = Luma(c), sat = max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
    c = mix(vec3(l2), c, 1.0 + (p2.x - 1.0) * (1.0 - sat));
    // contrast: an S-curve, fixed or following the scene's luminance spread (its stats' smallest mipmap)
    float k = p2.y;
    if (p2.z > 0.5) {
        vec2 m = textureLod(stats, vec2(0.5), texel.w).rg;
        float spread = sqrt(max(m.y - m.x * m.x, 1e-6));
        k = clamp(0.7 * 0.2 / spread, 0.4, 1.0);
    }
    c = clamp(c, 0.0, 1.0);
    c = mix(c, c * c * (3.0 - 2.0 * c), k * 0.4);
    color = vec4(c, 1.0);
}
)";

const char* const FULLSCREEN_VERT = R"(
#ifdef GL_ES
precision highp float;
#endif
layout(location = 0) out vec2 frag_tex_coord;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    frag_tex_coord = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

std::string Source(const char* body) {
    if (body == FULLSCREEN_VERT) {
        return body;
    }
    return std::string(COMMON) + body;
}

} // namespace VideoCore::Remaster
