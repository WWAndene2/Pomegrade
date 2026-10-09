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
    p.bounce = 1.0f;
    p.contact_shadow = 1.0f;
    p.rim = 1.0f;
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
    if (!v.remaster_ao.GetValue()) p.ao = p.bounce = p.contact_shadow = 0; // the light pass
    if (!v.remaster_outline.GetValue()) p.rim = 0;
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
layout(binding = 7) uniform sampler2D bright;
layout(binding = 8) uniform sampler2D light;
// cr, dr: the frame's and the depth's rectangle in their textures (u from x to y, v from z to w) for this pass's
// coordinates; texel: 1 / width, 1 / height of the pass, whether there is a depth, the stats' smallest mipmap
uniform vec4 cr;
uniform vec4 dr;
uniform vec4 texel;
uniform vec4 p0; // grading, ao, sky fill, sky light
uniform vec4 p1; // outline, glow, aerial, far blur
uniform vec4 p2; // vibrance, contrast, adaptive contrast, rim
uniform vec4 p3; // bounce, contact shadow, traced light (gl_pathtracer.h), 0
uniform vec4 p4; // the traced sun's direction across the screen (x, y), whether known, 0
vec3 Frame(vec2 p) {
    return texture(frame, vec2(mix(cr.x, cr.y, p.x), mix(cr.z, cr.w, p.y))).rgb;
}
float Luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}
float RawDepth(vec2 p) {
    return texture(depth, vec2(mix(dr.x, dr.y, p.x), mix(dr.z, dr.w, p.y))).r;
}
float far_is_big;
// 0 near, 1 far, whichever way the game's depth runs (the screen's top, sky and far scenery, against its bottom)
void FindDepthDirection() {
    far_is_big = RawDepth(vec2(0.95, 0.5)) >= RawDepth(vec2(0.05, 0.5)) ? 1.0 : 0.0;
}
float Far(vec2 p) {
    float d = RawDepth(p);
    return far_is_big > 0.5 ? d : 1.0 - d;
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
// The light pass, at half the frame's size: what the offline renders path-traced, from the depth buffer and the frame,
// live. Occlusion on 12 taps of three rings; contact shadows, a march of 8 steps toward the sun (up the screen, as the
// surface shading assumes: the post-process has no light direction), a step whose depth rises above the ray shadows;
// the coloured bounce, the colour of the nearby surfaces that face the pixel (nearer ones on the occluding side),
// averaged (compose.py: its tint, blurred to the low frequencies, kept subtle). Out: rgb the bounce, a the light.
const char* const LIGHT_FRAG = R"(
void main() {
    vec2 p = frag_tex_coord;
    vec2 t = texel.xy * 2.0; // the frame's texels: this pass is at half size
    if (texel.z < 0.5) {
        color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    FindDepthDirection();
    float f0 = Far(p);
    float scale = 1.0 / max(1.0 - f0, 0.002);
    float occ = 0.0;
    vec3 bounce = vec3(0.0);
    float weight = 0.0;
    for (int k = 0; k < 12; k++) {
        float a = float(k) * 2.39996 + 0.5; // golden angle
        float r = k < 4 ? 5.0 : k < 8 ? 11.0 : 20.0;
        vec2 o = vec2(cos(a), sin(a)) * r * t;
        float d = (f0 - Far(p + o)) * scale;
        float near_ = clamp(d * 40.0, 0.0, 1.0) * (1.0 - smoothstep(0.08, 0.25, d));
        occ += near_;
        float w = 0.4 + near_;
        bounce += Frame(p + o) * w;
        weight += w;
    }
    float ao = 1.0 - occ / 12.0;
    float lit = 1.0;
    for (int i = 1; i <= 8; i++) {
        vec2 q = p + vec2(float(i) * 2.5 * t.x, 0.0);
        float rise = (f0 - Far(q)) * scale - float(i) * 0.004; // the ray climbs toward the sun as it goes
        lit = min(lit, 1.0 - clamp(rise * 30.0, 0.0, 1.0) * (1.0 - smoothstep(0.1, 0.3, rise)));
    }
    float light = mix(1.0, ao, p0.y) * mix(1.0, 0.55 + 0.45 * lit, p3.y);
    color = vec4(bounce / weight, light);
}
)";

const char* const MAIN_FRAG = R"(
// the light pass, denoised: a 3x3 joint bilateral filter guided by the depth (compose.py's guided filter on the path
// tracer's grain: shadow edges and corners kept)
vec4 Light(vec2 p, float f0, float scale) {
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 q = p + vec2(float(x), float(y)) * texel.xy * 2.0;
            float w = exp(-abs(Far(q) - f0) * scale * 60.0) * (x == 0 && y == 0 ? 2.0 : 1.0);
            sum += texture(light, q) * w;
            wsum += w;
        }
    }
    return sum / wsum;
}
void main() {
    // kept a texel inside the frame: the outermost row read past what was drawn (no depth there, so the aerial
    // perspective lit a thin bright line along the top: full render, 9 October)
    vec2 p = clamp(frag_tex_coord, texel.xy, 1.0 - texel.xy);
    vec2 t = texel.xy;
    vec3 c = Frame(p);
    float lum = Luma(c);
    float step_ = smoothstep(0.60, 0.78, lum); // the v7 two-tone step: 0 shade, 1 sun
    bool has_depth = texel.z > 0.5;

    float ao = 1.0, f0 = 0.0, up = 0.0, edge = 0.0, rim = 0.0;
    vec3 tint = vec3(1.0);
    if (has_depth) {
        FindDepthDirection();
        f0 = Far(p);
        float scale = 1.0 / max(1.0 - f0, 0.002);
        vec4 lp = Light(p, f0, scale);
        ao = lp.a;
        // the bounce's tint: the surroundings' colour against their luminance, clamped as compose.py does
        tint = clamp(lp.rgb / max(Luma(lp.rgb), 0.03), 0.6, 1.6);
        // the normal from the depth: a surface whose depth grows up the screen faces up (the ground, roofs)
        float fu = Far(p + vec2(t.x, 0.0)), fd = Far(p - vec2(t.x, 0.0));
        float fr = Far(p + vec2(0.0, t.y)), fl = Far(p - vec2(0.0, t.y));
        up = clamp((fu - fd) * scale * 60.0, 0.0, 1.0);
        edge = smoothstep(0.02, 0.06, max(max(abs(fu - f0), abs(fd - f0)), max(abs(fr - f0), abs(fl - f0))) * scale);
        // rim (layers.py): a silhouette edge turned to the sun: something much farther just beyond it on the sun's side.
        // The sun's side is the traced sun's direction on screen when the path tracer runs, else up the screen
        vec2 toward = p4.z > 0.5 ? p4.xy : vec2(1.0, 0.0);
        float beyond = max(Far(p + toward * 2.0 * t), Far(p + toward * 3.0 * t)) - f0;
        rim = smoothstep(0.02, 0.1, beyond * scale);
    }

    // two-tone light: a cool lavender shade with a cool lift (shade shows on saturated green), a warm sun
    vec3 shade = vec3(0.86, 0.84, 1.0), sun = vec3(1.05, 1.0, 0.93);
    if (p3.z > 0.5) {
        // the path-traced light (gl_pathtracer.h): a white surface's light, open ground in the sun 1, laid out as the
        // depth; composed as compose.py does, in linear light
        vec4 traced = texture(light, vec2(mix(dr.x, dr.y, p.x), mix(dr.z, dr.w, p.y)));
        float l = Luma(traced.rgb);
        step_ = smoothstep(0.60, 0.78, l);
        // the shade's tone measured on the owner's reference render (9 October): its shade keeps 0.34, 0.43 and 0.50 of
        // the sun's red, green and blue, a deep blue-green; compose.py's (0.55, 0.57, 0.68) came out grey-blue and light
        // here (0.58, 0.43, 0.82 of the sun's)
        // the tone applies in linear light but the reference was measured on the shown (sRGB) image: a linear factor k
        // shows as k^(1/2.2) (0.52 showed as 0.74, the measured blue). Matching the reference's shown 0.34, 0.43, 0.50
        // from the measured 0.47, 0.43, 0.74: red 0.36 (0.34 / 0.47)^2.2, green kept, blue 0.52 (0.50 / 0.74)^2.2
        vec3 tone = mix(vec3(0.18, 0.45, 0.22), vec3(1.04, 1.0, 0.94), step_);
        tone *= 0.8 + 0.2 * smoothstep(0.1, 0.45, l);                // deep corners a little darker, never black
        tone *= 1.0 - (1.0 - pow(traced.a, 1.3)) * (0.55 - 0.4 * step_); // traced occlusion
        // the bounce's colour, at 30 %: the traced light also holds the sky's blue, which compose.py's tint (from the
        // bounce alone) did not; in full it turned the shade blue (0.76 of the sun's blue against the reference's 0.50)
        vec3 tint = mix(vec3(1.0), clamp(traced.rgb / max(l, 0.03), 0.6, 1.6), 0.3);
        tone *= 1.0 + 0.3 * (tint - 1.0) * (1.0 - 0.5 * step_);
        vec3 base = pow(c, vec3(2.2));
        float bl = Luma(base);
        // the lifts halved: they greyed the shade against the reference
        vec3 lin = base * tone + (vec3(0.006, 0.01, 0.035) * (1.0 - step_) + vec3(0.03, 0.015, 0.0) * step_) * (0.4 + bl);
        lin += vec3(0.010, 0.018, 0.045) * traced.a * (1.0 - step_) * (0.4 + bl); // sky fill
        c = pow(max(lin, 0.0), vec3(1.0 / 2.2));
        ao = 1.0; // applied
    } else {
        c = mix(c, c * mix(shade, sun, step_) + vec3(0.012, 0.016, 0.034) * (1.0 - step_), p0.x);
        // the light pass (occlusion and contact shadows), v9's way: strong in the shade, weak in the sun
        c *= 1.0 - (1.0 - pow(ao, 1.3)) * (0.55 - 0.4 * step_);
        // coloured bounce, stronger in the shade (compose.py: 1 + 0.3 (tint - 1)(1 - 0.5 step))
        c *= 1.0 + 0.3 * (tint - 1.0) * (1.0 - 0.5 * step_) * p3.x;
    }
    // warm rim on the silhouettes facing the sun (layers.py's colour and weight)
    c += rim * vec3(0.28, 0.2, 0.1) * (0.3 + Luma(c)) * p2.w;
    // v9's sky fill in the open shade (the traced light's own above), and the sky's light on surfaces facing up
    c += vec3(0.010, 0.018, 0.045) * (1.0 - step_) * ao * p0.z * (1.0 - p3.z);
    // (the traced light holds the sky already: added again, it turned the traced shade blue against the reference)
    c += vec3(0.03, 0.045, 0.07) * up * p0.w * (1.0 - p3.z);
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
    // the textures' lines and contrast: an unsharp mask on the luminance (the frame against its 1-pixel cross), so painted
    // outlines and blades keep their weight after the light's grading; off the far, blurred scenery
    {
        vec3 cross_ = (Frame(p + vec2(t.x, 0.0)) + Frame(p - vec2(t.x, 0.0)) + Frame(p + vec2(0.0, t.y)) +
                       Frame(p - vec2(0.0, t.y))) * 0.25;
        float detail = Luma(Frame(p)) - Luma(cross_);
        float keep = has_depth ? 1.0 - smoothstep(0.80, 1.0, f0) : 1.0;
        c += detail * 0.6 * keep * p1.x;
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
