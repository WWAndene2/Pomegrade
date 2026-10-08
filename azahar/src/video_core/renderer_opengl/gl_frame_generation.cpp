// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "video_core/frame_generation.h"
#include "video_core/renderer_opengl/gl_frame_generation.h"
#include "video_core/renderer_opengl/gl_texture_mailbox.h"

namespace OpenGL {

namespace FG = VideoCore::FrameGeneration;

// a texture of `format` and its framebuffer, filtered linearly and clamped (raw GL: this runs on the presentation thread,
// whose state OpenGLState doesn't track)
static void MakeTarget(OGLTexture& texture, OGLFramebuffer& fbo, GLenum format, u32 width, u32 height) {
    texture.Release();
    fbo.Release();
    texture.Create();
    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    fbo.Create();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.handle);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.handle, 0);
}

static u32 Small(u32 size) {
    return std::max<u32>(1, (size + 7) / 8);
}

void FrameGeneratorGL::Resize(u32 w, u32 h) {
    width = w;
    height = h;
    for (int i = 0; i < 2; i++) {
        MakeTarget(images[i], image_fbos[i], GL_RGBA8, w, h);
    }
    MakeTarget(luma, luma_fbo, GL_RG8, Small(w), Small(h));
    MakeTarget(motion, motion_fbo, GL_RGBA16F, Small(w), Small(h));
    MakeTarget(result, result_fbo, GL_RGBA8, w, h);
    if (warp.handle == 0) {
        const std::string vert = FG::Source(FG::FULLSCREEN_VERT, false);
        downsample.Create(vert, FG::Source(FG::DOWNSAMPLE_FRAG, false));
        estimate.Create(vert, FG::Source(FG::MOTION_FRAG, false));
        warp.Create(vert, FG::Source(FG::WARP_FRAG, false));
        vao.Create();
    }
    newest = -1;
    have_two = false;
}

// one pass of the interpolation into `fbo` (w x h): `program` with images A, B and `aux` bound and its two parameters
static void Pass(const OGLProgram& program, GLuint fbo, u32 w, u32 h, GLuint a, GLuint b, GLuint aux,
                 const float p0[4], const float p1[4]) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    glUseProgram(program.handle);
    glUniform4fv(glGetUniformLocation(program.handle, "p0"), 1, p0);
    glUniform4fv(glGetUniformLocation(program.handle, "p1"), 1, p1);
    const GLuint textures[3] = {a, b, aux};
    for (GLuint unit = 0; unit < 3; unit++) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textures[unit]);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void FrameGeneratorGL::Generate(float t, u32 out_width, u32 out_height) {
    const GLuint a = images[1 - newest].handle, b = images[newest].handle;
    const float sw = static_cast<float>(Small(width)), sh = static_cast<float>(Small(height));
    const float p0[4] = {t, 1.0f / sw, 1.0f / sh, 0.0f};
    const float p1[4] = {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height), 0.0f, 0.0f};
    GLint draw_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(vao.handle);
    Pass(downsample, luma_fbo.handle, Small(width), Small(height), a, b, 0, p0, p1);
    Pass(estimate, motion_fbo.handle, Small(width), Small(height), a, b, luma.handle, p0, p1);
    Pass(warp, result_fbo.handle, width, height, a, b, motion.handle, p0, p1);
    glBindVertexArray(0);
    glUseProgram(0);
    for (GLuint unit = 0; unit < 3; unit++) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_fbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, result_fbo.handle);
    glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
                      static_cast<GLint>(out_width), static_cast<GLint>(out_height), GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
}

bool FrameGeneratorGL::Present(Frontend::Frame* frame, u32 out_width, u32 out_height) {
    if (!FG::Generates()) {
        newest = -1;
        have_two = false;
        return false;
    }
    const auto now = std::chrono::steady_clock::now();
    if (frame->width != width || frame->height != height) {
        // Resize binds its own framebuffers: the screen's is put back for the caller
        GLint screen_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &screen_fbo);
        Resize(frame->width, frame->height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
    }
    if (newest < 0 || frame->serial != serial) {
        // a distinct frame: copied over A, which becomes B
        const int next = newest < 0 ? 0 : 1 - newest;
        GLint draw_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, frame->present.handle);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, image_fbos[next].handle);
        glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
                          static_cast<GLint>(width), static_cast<GLint>(height), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_fbo));
        if (newest >= 0) {
            // the interval between the two frames' rendering, as the emulation made them
            interval_s = static_cast<double>(frame->time_us - time_b_us) / 1e6;
            have_two = true;
        }
        time_b_us = frame->time_us;
        serial = frame->serial;
        newest = next;
        arrival = now;
    }
    const u32 images_between = have_two ? FG::ImagesBetween(interval_s) : 0;
    if (images_between == 0) {
        return false;
    }
    // the position between A and B of this refresh, on the steps of the images to show: A as B comes, then the generated
    // ones, B once its interval has gone by
    const double elapsed = std::chrono::duration<double>(now - arrival).count();
    const double steps = static_cast<double>(images_between + 1);
    const double t = std::floor(elapsed / interval_s * steps) / steps;
    if (t >= 1.0) {
        return false;
    }
    if (t <= 0.0) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, image_fbos[1 - newest].handle);
        glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0,
                          static_cast<GLint>(out_width), static_cast<GLint>(out_height), GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
        return true;
    }
    Generate(static_cast<float>(t), out_width, out_height);
    return true;
}

} // namespace OpenGL
