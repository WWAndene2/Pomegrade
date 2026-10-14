// SPDX-FileCopyrightText: Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: Rich colours, as the 3DS core's (azahar vulkan_present.frag, PomegradeDeband): smooths the steps that
// low-precision textures and buffers leave in gradients. Four samples at a random distance (1 to 6 native pixels)
// and angle around the pixel; when all of them are within a band's height of it (1.25 / 32) the pixel takes their
// average, so real edges and detail keep their sharpness; the result is dithered to 8 bits so the smoothing shows.

#version 460

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D input_texture;

// interleaved gradient noise (Jimenez 2014): stable in mediump, no sin() of large values
float Noise(vec2 position, float seed) {
    position += seed * vec2(47.0, 17.0);
    return fract(52.9829189 * fract(dot(position, vec2(0.06711056, 0.00583715))));
}

void main() {
    const float threshold = 1.25 / 32.0;
    vec4 centre = texture(input_texture, frag_tex_coord);
    // one native Switch pixel: the internal resolution's scale (games render 1280 pixels wide at 1x)
    vec2 size = vec2(textureSize(input_texture, 0));
    vec2 texel = max(1.0, size.x / 1280.0) / size;
    vec2 position = gl_FragCoord.xy;
    float angle = Noise(position, 0.0) * 6.2831853;
    float radius = mix(1.0, 6.0, Noise(position, 1.7));
    vec2 offset = vec2(cos(angle), sin(angle)) * radius * texel;
    vec3 a = texture(input_texture, frag_tex_coord + offset).rgb;
    vec3 b = texture(input_texture, frag_tex_coord - offset).rgb;
    vec3 c = texture(input_texture, frag_tex_coord + vec2(-offset.y, offset.x)).rgb;
    vec3 d = texture(input_texture, frag_tex_coord + vec2(offset.y, -offset.x)).rgb;
    vec3 difference = max(max(abs(a - centre.rgb), abs(b - centre.rgb)), max(abs(c - centre.rgb), abs(d - centre.rgb)));
    vec3 smoothed = mix((a + b + c + d) * 0.25, centre.rgb, step(vec3(threshold), difference));
    // triangular dither of +-1 step of 8 bits
    float dither = (Noise(position, 3.1) + Noise(position, 5.3) - 1.0) / 255.0;
    color = vec4(clamp(smoothed + dither, 0.0, 1.0), centre.a);
}
