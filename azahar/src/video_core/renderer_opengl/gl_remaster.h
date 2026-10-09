// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

struct ScreenInfo;

// Pomegrade: Remaster (VideoCore::Remaster) for OpenGL: the top screen's frame, once per emulated frame, through three
// passes (stats and highlights at reduced size with their mipmaps, then the remastered frame) into a texture of its own,
// which the screen then shows in place of the frame
class RemasterGL {
public:
    /// Remasters screen_info's frame when the preset is on; screen_info then names the remastered texture
    void Apply(ScreenInfo& screen_info);

private:
    void Resize(u32 width, u32 height);
    void Pass(const OGLProgram& program, GLuint fbo, u32 w, u32 h);
    u32 width = 0, height = 0;
    OGLTexture stats, bright, light, result;
    OGLFramebuffer stats_fbo, bright_fbo, light_fbo, result_fbo;
    OGLProgram stats_program, bright_program, light_program, main_program;
    OGLSampler linear, mipmapped, nearest;
    OGLVertexArray vao;
};

} // namespace OpenGL
