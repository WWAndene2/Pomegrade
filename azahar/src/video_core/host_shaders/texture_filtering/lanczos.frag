// Copyright Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

//? #version 330
precision highp float;

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 frag_color;

layout(binding = 0) uniform sampler2D input_texture;

// Pomegrade: Lanczos-3 texture upscaling, the sharpest of the classic interpolations, for
// photo-like textures (skies, faces) where pixel-art scalers look painted. 6x6 texels weighted by
// sinc(x) * sinc(x / 3), then clamped to the 2x2 texels around the point (anti-ringing: no halos
// around edges).

const float PI = 3.14159265;

float Lanczos(float x) {
    x = abs(x);
    if (x < 1e-4) {
        return 1.0;
    }
    if (x >= 3.0) {
        return 0.0;
    }
    float px = PI * x;
    return 3.0 * sin(px) * sin(px / 3.0) / (px * px);
}

void main() {
    ivec2 size = textureSize(input_texture, 0);
    vec2 position = tex_coord * vec2(size) - 0.5;
    vec2 base = floor(position);
    vec2 f = position - base;

    vec4 sum = vec4(0.0);
    float weight_sum = 0.0;
    vec4 low = vec4(1e9);
    vec4 high = vec4(-1e9);
    for (int y = -2; y <= 3; y++) {
        float wy = Lanczos(float(y) - f.y);
        for (int x = -2; x <= 3; x++) {
            float w = Lanczos(float(x) - f.x) * wy;
            ivec2 texel = clamp(ivec2(base) + ivec2(x, y), ivec2(0), size - 1);
            vec4 colour = texelFetch(input_texture, texel, 0);
            sum += colour * w;
            weight_sum += w;
            if (x >= 0 && x <= 1 && y >= 0 && y <= 1) {
                low = min(low, colour);
                high = max(high, colour);
            }
        }
    }
    frag_color = clamp(sum / weight_sum, low, high);
}
