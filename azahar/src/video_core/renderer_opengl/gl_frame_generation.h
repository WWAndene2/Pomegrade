// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Frontend {
struct Frame;
}

namespace OpenGL {

// Pomegrade: images generated between two distinct frames for the presentation thread (VideoCore::FrameGeneration),
// called at each screen refresh by TryPresent. It keeps copies of the last two distinct frames (A, B) and shows, at a
// refresh, the image at its time between them: A when B has just come, B once B's interval has gone by, generated
// images in between. Its objects are made on the presentation thread's context (framebuffers aren't shared).
class FrameGeneratorGL {
public:
    /// Draws into the bound draw framebuffer (out_width x out_height) the image to show now for `frame`, the
    /// mailbox's latest; returns false when there is nothing to generate (the caller shows the frame as usual)
    bool Present(Frontend::Frame* frame, u32 out_width, u32 out_height);

private:
    void Resize(u32 width, u32 height);
    void Generate(float t, u32 out_width, u32 out_height);

    u32 width = 0, height = 0;
    OGLTexture images[2];
    OGLFramebuffer image_fbos[2];
    OGLTexture luma, motion, result;
    OGLFramebuffer luma_fbo, motion_fbo, result_fbo;
    OGLProgram downsample, estimate, warp;
    OGLVertexArray vao;
    int newest = -1; ///< index in images of B, -1 before the first frame
    bool have_two = false;
    u64 serial = 0;  ///< B's serial (Frontend::Frame::serial)
    s64 time_b_us = 0; ///< when the emulation made B (Frontend::Frame::time_us)
    double interval_s = 0.0;
    std::chrono::steady_clock::time_point arrival; ///< when B came to the presentation thread
};

} // namespace OpenGL
