// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <tuple>
#include "video_core/pica/regs_lcd.h"
#include "video_core/remaster.h"
#include "video_core/renderer_opengl/gl_pathtracer.h"
#include "video_core/renderer_opengl/gl_remaster.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

namespace RM = VideoCore::Remaster;

static constexpr u32 StatsSize = 64;

static u32 Levels(u32 w, u32 h) {
    return 1 + static_cast<u32>(std::floor(std::log2(static_cast<float>(std::max(w, h)))));
}

// a texture of w x h with `levels` mipmaps and its framebuffer (level 0). Bound outside OpenGLState's
// tracking, as the texture runtime does: the texture on its temporary unit 15; Apply puts the draw
// framebuffer back
static void MakeTarget(OGLTexture& texture, OGLFramebuffer& fbo, GLenum format, u32 w, u32 h, u32 levels) {
    texture.Release();
    fbo.Release();
    texture.Create();
    glActiveTexture(GL_TEXTURE15);
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
    MakeTarget(light, light_fbo, GL_RGBA8, std::max<u32>(1, w / 2), std::max<u32>(1, h / 2), 1);
    MakeTarget(result, result_fbo, GL_RGBA8, w, h, 1);
    if (main_program.handle == 0) {
        const std::string vert = RM::Source(RM::FULLSCREEN_VERT);
        stats_program.Create(vert, RM::Source(RM::STATS_FRAG));
        bright_program.Create(vert, RM::Source(RM::BRIGHT_FRAG));
        light_program.Create(vert, RM::Source(RM::LIGHT_FRAG));
        main_program.Create(vert, RM::Source(RM::MAIN_FRAG));
        vao.Create();
        // a depth texture is sampled with nearest filtering: OpenGL ES does not filter depth linearly.
        // The frame has no mipmaps: a mipmap filter on it makes it incomplete, read as black (seen
        // headless, 9 October), so only the stats and highlights use `mipmapped`
        for (auto [sampler, min, mag] :
             {std::tuple{&linear, GL_LINEAR, GL_LINEAR}, std::tuple{&mipmapped, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR},
              std::tuple{&nearest, GL_NEAREST, GL_NEAREST}}) {
            sampler->Create();
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER, min);
            glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, mag);
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

void RemasterGL::Apply(ScreenInfo& screen_info, PathTracerGL& tracer) {
    const RM::Params p = RM::Current();
    if (!p.enabled || screen_info.display_width == 0 || screen_info.display_height == 0) {
        return;
    }
    // the draw framebuffer actually bound, put back at the end: a frontend may bind its own outside
    // OpenGLState (libretro's SetupFramebuffer), and restoring the tracked one sent the present to a
    // stale framebuffer, both screens black (seen headless, 9 October)
    GLint bound_framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound_framebuffer);
    if (screen_info.display_width != width || screen_info.display_height != height) {
        const OpenGLState saved = OpenGLState::GetCurState();
        Resize(screen_info.display_width, screen_info.display_height);
        saved.Apply();
    }
    // the frame's light, path-traced when the preset asks it (before this pass's state is set: it keeps its own)
    const PathTracerGL::Result traced_frame = PathTracerGL::Enabled()
                                                  ? tracer.Trace(screen_info, screen_info.display_texture)
                                                  : PathTracerGL::Result{};
    const GLuint traced = traced_frame.light;
    // a traced frame's depth is the path tracer's own G-buffer (the game's may be drawn over by then: ORAS draws both
    // screens with one depth buffer)
    const GLuint depth_texture = traced != 0 ? traced_frame.distance : screen_info.depth_texture;
    const OpenGLState saved = OpenGLState::GetCurState();
    OpenGLState state = saved;
    state.blend.enabled = false;
    state.depth.test_enabled = false;
    state.cull.enabled = false;
    state.scissor.enabled = false;
    state.stencil.test_enabled = false;
    state.color_mask = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    // TextureUnit is {texture_2d, target, sampler}: named, so the sampler never lands in the target
    const auto unit = [](GLuint texture, GLuint sampler) {
        return OpenGLState::TextureUnit{
            .texture_2d = texture, .target = GL_TEXTURE_2D, .sampler = sampler};
    };
    state.texture_units[0] = unit(screen_info.display_texture, linear.handle);
    state.texture_units[1] = unit(depth_texture, nearest.handle);
    state.texture_units[2] = unit(stats.handle, mipmapped.handle);
    // OpenGLState tracks 3 texture units: the highlights go on the colour buffer's unit, 7, their
    // sampler bound there for these passes only
    state.color_buffer.texture_2d = bright.handle;
    state.Apply();
    glBindSampler(TextureUnits::TextureColorBuffer.id, mipmapped.handle);

    // the frame's rectangle as DrawSingleScreen reads it: u along display_texcoords' top -> bottom, v along left -> right
    const auto& tc = screen_info.display_texcoords;
    const float cr[4] = {tc.top, tc.bottom, tc.left, tc.right};
    const auto& d = traced != 0 ? screen_info.target_rect : screen_info.depth_rect;
    const float stats_level = static_cast<float>(Levels(StatsSize, StatsSize) - 1);
    const float texel[4] = {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height),
                            depth_texture != 0 ? 1.0f : 0.0f, stats_level};
    const float p0[4] = {p.grading, p.ao, p.sky_fill, p.sky_light};
    const float p1[4] = {p.outline, p.glow, p.aerial, p.far_blur};
    const float p2[4] = {p.vibrance, p.contrast, p.adaptive_contrast ? 1.0f : 0.0f, p.rim};
    const float p3[4] = {p.bounce, p.contact_shadow, traced != 0 ? 1.0f : 0.0f, 0.0f};
    // the traced sun's direction in this pass's coordinates (the target's texture coordinates through d), for the rim
    float p4[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (traced != 0 && traced_frame.sun_up > 0.0f) {
        const float sx = traced_frame.sun_on_screen[0] / (d[1] - d[0]), sy = traced_frame.sun_on_screen[1] / (d[3] - d[2]);
        const float l = std::sqrt(sx * sx + sy * sy);
        if (l > 1e-6f) p4[0] = sx / l, p4[1] = sy / l, p4[2] = 1.0f;
    }
    // glProgramUniform: no glUseProgram behind the tracked state's back
    for (const OGLProgram* program : {&stats_program, &bright_program, &light_program, &main_program}) {
        const GLuint h = program->handle;
        glProgramUniform4fv(h, glGetUniformLocation(h, "cr"), 1, cr);
        glProgramUniform4fv(h, glGetUniformLocation(h, "dr"), 1, d.data());
        glProgramUniform4fv(h, glGetUniformLocation(h, "texel"), 1, texel);
        glProgramUniform4fv(h, glGetUniformLocation(h, "p0"), 1, p0);
        glProgramUniform4fv(h, glGetUniformLocation(h, "p1"), 1, p1);
        glProgramUniform4fv(h, glGetUniformLocation(h, "p2"), 1, p2);
        glProgramUniform4fv(h, glGetUniformLocation(h, "p3"), 1, p3);
        glProgramUniform4fv(h, glGetUniformLocation(h, "p4"), 1, p4);
    }
    Pass(stats_program, stats_fbo.handle, StatsSize, StatsSize);
    Pass(bright_program, bright_fbo.handle, std::max<u32>(1, width / 4), std::max<u32>(1, height / 4));
    // the stats and highlights are bound on units 2 and 7 (the tracked state): their mipmaps made there, no binding changed
    for (const GLenum unit : {GLenum{GL_TEXTURE2}, TextureUnits::TextureColorBuffer.Enum()}) {
        glActiveTexture(unit);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glActiveTexture(GL_TEXTURE0);
    if (traced == 0) {
        Pass(light_program, light_fbo.handle, std::max<u32>(1, width / 2), std::max<u32>(1, height / 2));
    }
    // the light on unit 8, which OpenGLState does not use (it tracks 0-7): bound for the main pass only
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, traced != 0 ? traced : light.handle);
    glBindSampler(8, linear.handle);
    Pass(main_program, result_fbo.handle, width, height);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindSampler(8, 0);
    glBindSampler(TextureUnits::TextureColorBuffer.id, 0);
    saved.Apply();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(bound_framebuffer));

    // the screen now shows the remastered frame, the whole of its texture in the same orientation
    screen_info.display_texture = result.handle;
    screen_info.display_texcoords = Common::Rectangle<float>(0.0f, 0.0f, 1.0f, 1.0f);
}

} // namespace OpenGL
