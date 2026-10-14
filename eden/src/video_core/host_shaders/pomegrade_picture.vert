// SPDX-FileCopyrightText: Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: the full-screen triangle of the picture passes (Rich colours, Deep black), without push constants

#version 460

const vec2 vertices[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));

layout (location = 0) out vec2 frag_tex_coord;

void main() {
    vec2 vertex = vertices[gl_VertexIndex];
    gl_Position = vec4(vertex, 0.0, 1.0);
    frag_tex_coord = (vertex + 1.0) / 2.0;
}
