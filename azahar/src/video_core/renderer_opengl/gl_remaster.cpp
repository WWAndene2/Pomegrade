// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "video_core/pica/regs_lcd.h"
#include "video_core/remaster.h"
#include "video_core/renderer_opengl/gl_remaster.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

namespace RM = VideoCore::Remaster;

static constexpr u32 StatsSize = 64;

static u32 Levels(u32 w, u32 h) {
    return 1 + static_cast<u32>(std::floor(std::log2(static_cast<float>(std::max(w, h)))));
}

// a texture of w x h with `levels` mipmaps and its framebuffer (level 0)
static void MakeTarget(OGLTexture& texture, OGLFramebuffer& fbo, GLenum format, u32 w, u32 h, u32 levels) {
    texture.Release();
    fbo.Release();
    texture.Create();
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexStorage2D(GL_TEXTURE_2D, static_cast<GLsizei>(levels), format, static_cast<GLsizei>(w),
                   static_cast<GLsizei>(h));
    glBindTexture(GL_TEXTURE_2D, 0);
    fbo.Create();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.handle);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.handle, 0);
}

void RemasterGL::Resize(u32 w, u32 h) {
    width = w;
    height = h;
    MakeTarget(stats, stats_fbo, GL_RGBA8, StatsSize, StatsSize, Levels(StatsSize, StatsSize));
    const u32 bw = std::max<u32>(1, w / 4), bh = std::max<u32>(1, h / 4);
    MakeTarget(bright, bright_fbo, GL_RGBA8, bw, bh, Levels(bw, bh));
    MakeTarget(result, result_fbo, GL_RGBA8, w, h, 1);
    if (main_program.handle == 0) {
        const std::string vert = RM::Source(RM::FULLSCREEN_VERT);
        stats_program.Create(vert, RM::Source(RM::STATS_FRAG));
        bright_program.Create(vert, RM::Source(RM::BRIGHT_FRAG));
        main_program.Create(vert, RM::Source(RM::MAIN_FRAG));
        vao.Create();
        // a depth texture is sampled with nearest filtering: OpenGL ES does not filter depth linearly
        for (auto [sampler, filter] : {std::pair{&linear, GL_LINEAR}, std::pair{&nearest, GL_NEAREST}}) {
            sampler->Create();
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER,
                                filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, filter);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    }
}

void RemasterGL::Pass(const OGLProgram& program, GLuint fbo, u32 w, u32 h) {
    OpenGLState state = OpenGLState::GetCurState();
    state.draw.draw_framebuffer = fbo;
    state.draw.shader_program = program.handle;
    state.draw.vertex_array = vao.handle;
    state.viewport = {0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h)};
    state.Apply();
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void RemasterGL::Apply(ScreenInfo& screen_info) {
    const RM::Params p = RM::Current();
    if (!p.enabled || screen_info.display_width == 0 || screen_info.display_height == 0) {
        return;
    }
    if (screen_info.display_width != width || screen_info.display_height != height) {
        const OpenGLState saved = OpenGLState::GetCurState();
        Resize(screen_info.display_width, screen_info.display_height);
        saved.Apply();
    }
    const OpenGLState saved = OpenGLState::GetCurState();
    OpenGLState state = saved;
    state.blend.enabled = false;
    state.depth.test_enabled = false;
    state.cull.enabled = false;
    state.scissor.enabled = false;
    state.stencil.test_enabled = false;
    state.color_mask = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    state.texture_units[0] = {screen_info.display_texture, linear.handle};
    state.texture_units[1] = {screen_info.depth_texture, nearest.handle};
    state.texture_units[2] = {stats.handle, linear.handle};
    state.texture_units[3] = {bright.handle, linear.handle};
    state.Apply();

    // the frame's rectangle as DrawSingleScreen reads it: u along display_texcoords' top -> bottom, v along left -> right
    const auto& tc = screen_info.display_texcoords;
    const float cr[4] = {tc.top, tc.bottom, tc.left, tc.right};
    const auto& d = screen_info.depth_rect;
    const float stats_level = static_cast<float>(Levels(StatsSize, StatsSize) - 1);
    const float texel[4] = {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height),
                            screen_info.depth_texture != 0 ? 1.0f : 0.0f, stats_level};
    const float p0[4] = {p.grading, p.ao, p.sky_fill, p.sky_light};
    const float p1[4] = {p.outline, p.glow, p.aerial, p.far_blur};
    const float p2[4] = {p.vibrance, p.contrast, p.adaptive_contrast ? 1.0f : 0.0f, 0.0f};
    for (const OGLProgram* program : {&stats_program, &bright_program, &main_program}) {
        glUseProgram(program->handle);
        glUniform4fv(glGetUniformLocation(program->handle, "cr"), 1, cr);
        glUniform4fv(glGetUniformLocation(program->handle, "dr"), 1, d.data());
        glUniform4fv(glGetUniformLocation(program->handle, "texel"), 1, texel);
        glUniform4fv(glGetUniformLocation(program->handle, "p0"), 1, p0);
        glUniform4fv(glGetUniformLocation(program->handle, "p1"), 1, p1);
        glUniform4fv(glGetUniformLocation(program->handle, "p2"), 1, p2);
    }
    Pass(stats_program, stats_fbo.handle, StatsSize, StatsSize);
    Pass(bright_program, bright_fbo.handle, std::max<u32>(1, width / 4), std::max<u32>(1, height / 4));
    // the stats and highlights are bound on units 2 and 3 (the tracked state): their mipmaps made there, no binding changed
    for (GLenum unit : {GL_TEXTURE2, GL_TEXTURE3}) {
        glActiveTexture(unit);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glActiveTexture(GL_TEXTURE0);
    Pass(main_program, result_fbo.handle, width, height);
    saved.Apply();

    // the screen now shows the remastered frame, the whole of its texture in the same orientation
    screen_info.display_texture = result.handle;
    screen_info.display_texcoords = Common::Rectangle<float>(0.0f, 0.0f, 1.0f, 1.0f);
}

} // namespace OpenGL
