// Copyright 2022-2024 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (push_constant, std140) uniform DrawInfo {
    mat4 modelview_matrix;
    vec4 i_resolution;
    vec4 o_resolution;
    int screen_id_l;
    int screen_id_r;
    int layer;
    int reverse_interlaced;
    // Pomegrade: Rich colours and Deep black (see PresentUniformData)
    int rich_colours;
    float deep_black_threshold;
};

layout (set = 0, binding = 0) uniform sampler2D screen_textures[3];

vec4 GetScreenAt(int screen_id, vec2 coord) {
#ifdef ARRAY_DYNAMIC_INDEX
    return texture(screen_textures[screen_id], coord);
#else
    switch (screen_id) {
    case 0:
        return texture(screen_textures[0], coord);
    case 1:
        return texture(screen_textures[1], coord);
    case 2:
        return texture(screen_textures[2], coord);
    }
#endif
}

vec4 GetScreen(int screen_id) {
    return GetScreenAt(screen_id, frag_tex_coord);
}

ivec2 GetScreenSize(int screen_id) {
#ifdef ARRAY_DYNAMIC_INDEX
    return textureSize(screen_textures[screen_id], 0);
#else
    switch (screen_id) {
    case 0:
        return textureSize(screen_textures[0], 0);
    case 1:
        return textureSize(screen_textures[1], 0);
    case 2:
        return textureSize(screen_textures[2], 0);
    }
#endif
}

#define POMEGRADE_SAMPLE(coord) GetScreenAt(screen_id_l, coord).rgb

// Pomegrade: Rich colours and Deep black (keep in sync with the other present shader,
// opengl_present.frag / vulkan_present.frag)
const vec3 POMEGRADE_LUMA = vec3(0.2126, 0.7152, 0.0722);

// interleaved gradient noise (Jimenez 2014): stable in mediump, no sin() of large values
float PomegradeNoise(vec2 position, float seed) {
    position += seed * vec2(47.0, 17.0);
    return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

// Rich colours: smooths the steps that 16-bit textures and buffers leave in gradients. Four
// samples at a random distance (1 to 6 native pixels) and angle around the pixel; when all of
// them are within a band's height of it (the 5 and 6-bit steps of 16-bit colour, 1/32 and 1/64)
// the pixel takes their average, so real edges and detail keep their sharpness. The result,
// computed in floating point, is dithered to the screen's 8 bits so the smoothing shows.
vec3 PomegradeDeband(vec3 centre, vec2 coord, vec2 texel, vec2 position) {
    const float threshold = 1.25 / 32.0;
    float angle = PomegradeNoise(position, 0.0) * 6.2831853;
    float radius = mix(1.0, 6.0, PomegradeNoise(position, 1.7));
    vec2 offset = vec2(cos(angle), sin(angle)) * radius * texel;
    vec3 a = POMEGRADE_SAMPLE(coord + offset);
    vec3 b = POMEGRADE_SAMPLE(coord - offset);
    vec3 c = POMEGRADE_SAMPLE(coord + vec2(-offset.y, offset.x));
    vec3 d = POMEGRADE_SAMPLE(coord + vec2(offset.y, -offset.x));
    vec3 difference = max(max(abs(a - centre), abs(b - centre)), max(abs(c - centre), abs(d - centre)));
    vec3 smoothed = mix((a + b + c + d) * 0.25, centre, step(vec3(threshold), difference));
    // triangular dither of +-1 step of 8 bits
    float dither = (PomegradeNoise(position, 3.1) + PomegradeNoise(position, 5.3) - 1.0) / 255.0;
    return clamp(smoothed + dither, 0.0, 1.0);
}

// Deep black: shades under the scene's threshold fade to true black
vec3 PomegradeDeepBlack(vec3 colour, float threshold) {
    return colour * smoothstep(threshold * 0.5, threshold, dot(colour, POMEGRADE_LUMA));
}

void main() {
    color = GetScreen(screen_id_l);
    if (rich_colours != 0) {
        // one native 3DS pixel: the internal resolution's scale (screens are 240 texels wide)
        vec2 texel = max(1.0, i_resolution.x / 240.0) / vec2(GetScreenSize(screen_id_l));
        color.rgb = PomegradeDeband(color.rgb, frag_tex_coord, texel, gl_FragCoord.xy);
    }
    if (deep_black_threshold > 0.0) {
        color.rgb = PomegradeDeepBlack(color.rgb, deep_black_threshold);
    }
}
