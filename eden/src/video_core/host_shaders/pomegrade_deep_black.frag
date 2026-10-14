// SPDX-FileCopyrightText: Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: Deep black, as the 3DS core's (azahar vulkan_present.frag, PomegradeDeepBlack): near-black shades fade
// to true black (pixels off on OLED screens). The 3DS core measures each scene's black level to set the threshold;
// here it is fixed, 6 % luminance (fading from 3 %), the level the 3DS measurement settles on in dark scenes.

#version 460

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D input_texture;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);
const float THRESHOLD = 0.06;

void main() {
    vec4 colour = texture(input_texture, frag_tex_coord);
    color = vec4(colour.rgb * smoothstep(THRESHOLD * 0.5, THRESHOLD, dot(colour.rgb, LUMA)), colour.a);
}
