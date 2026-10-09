// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Pica {
struct LightingRegs;
}

namespace OpenGL {

struct ScreenInfo;

// Pomegrade: a path tracer for the 3D scene, every frame, as the offline renders of tools/remake/render did it
// (three-gpu-pathtracer: rays through a BVH of the scene, the sun's shadows, the sky, light bounced between surfaces,
// then denoised), live in the emulator. The 3DS gives each lit vertex its position and normal in the camera's space (the
// fragment lighting's view vector and normal quaternion): the rasterizer hands every lit draw of a frame here (drawn with
// the software vertex shader while this runs, so the vertices are on the CPU), and the frame's triangles become a BVH
// (pathtrace_bvh.h). The same triangles, rasterised again from the game's own clip positions, give each pixel of the
// frame its position and normal (a G-buffer laid out as the game's depth buffer, so it lines up with the displayed frame
// as Remaster's depth does). Per pixel, at half the frame's size:
// - a ray toward the sun: real shadows, cast by whatever stands in the way, the characters included;
// - two rays over the hemisphere: the sky's light where they escape, occlusion where they hit near, and the light
//   bounced off the surface they hit (its colour read from the frame where it is on screen, lit by the sun if a
//   second ray reaches it);
// - the result, a white surface's light (as pt.html renders white surfaces), scaled so open ground in the sun is 1,
//   is denoised by three passes of an edge-aware a-trous filter guided by the G-buffer's positions and normals (the
//   offline median and guided filters' part), and handed to Remaster, which composes it as compose.py does (v9).
// The sun follows the clock (the 3DS clock is the device's): it rises in the east at 6, crosses the south and sets in
// the west at 18; the moon lights the night. Up is the normal of the frame's largest flat area (the ground), north the
// camera's forward direction along it. The game's own time-of-day colours stay in the frame underneath.
class PathTracerGL {
public:
    PathTracerGL();
    ~PathTracerGL();

    /// Whether the current Remaster preset traces the light (v9 and custom)
    static bool Enabled();

    /// A vertex as the software vertex shader outputs it: clip position, the lighting's view vector and normal quaternion
    struct InputVertex {
        float clip[4];
        float view[3];
        float normquat[4];
    };

    /// A lit draw: its triangles (three vertices each), drawn into the render target whose colour buffer is at `target`
    /// through `viewport` (x, y, width, height, in the target's scaled pixels)
    void Capture(PAddr target, const std::array<GLint, 4>& viewport, std::span<const InputVertex> vertices);

    struct Result {
        GLuint light = 0;    ///< the denoised light (rgb the light, a the occlusion), half the target's size; 0: none
        GLuint distance = 0; ///< the G-buffer's distance to the camera, 0 near to 1 far (1 where nothing was drawn)
    };
    /// Traces the frame shown on the screen, both textures laid out as its render target (screen_info.target_rect)
    Result Trace(const ScreenInfo& screen_info, GLuint frame_texture);

    /// The render target at `target` was copied out to `display`: the draws made into it so far are that display's frame
    /// (kept until the next copy to it), and the next draw into the target starts a new frame. Taken at the copy, not at
    /// the screen's refresh: games draw ahead, and ORAS draws both screens into one target, one after the other
    void Complete(PAddr target, PAddr display);

private:
    struct Vertex {
        float clip[4];
        float position[3];
        float normal[3];
    };
    struct Batch {
        std::array<GLint, 4> viewport;
        std::size_t first, count; ///< vertices in the frame's list
    };
    struct Frame {
        std::vector<Vertex> vertices;
        std::vector<Batch> batches;
    };

    void Resize(u32 width, u32 height);
    void Upload(const Frame& frame);

    std::unordered_map<PAddr, std::shared_ptr<Frame>> frames;    ///< being drawn, by render target
    std::unordered_map<PAddr, bool> copied;                       ///< a target's frame was copied out: next draw starts anew
    std::unordered_map<PAddr, std::shared_ptr<Frame>> completed; ///< the frame each display shows
    u32 width = 0, height = 0;
    u32 frame_number = 0;

    OGLTexture gbuffer_position, gbuffer_normal, gbuffer_distance;
    OGLRenderbuffer gbuffer_depth;
    OGLFramebuffer gbuffer_fbo;
    std::array<OGLTexture, 2> light;
    std::array<OGLFramebuffer, 2> light_fbo;
    OGLProgram gbuffer_program, trace_program, denoise_program;
    OGLVertexArray gbuffer_vao, fullscreen_vao;
    OGLBuffer gbuffer_vbo, nodes_buffer, triangles_buffer;
    OGLTexture nodes_texture, triangles_texture;
    OGLSampler nearest, linear;
    std::size_t node_count = 0;

    // the scene's frame of reference in view space, and the sun from the clock
    float up[3] = {0, 1, 0}, sun[3] = {0, 1, 0}, sun_colour[3] = {1, 1, 1};
    float sky_top[3] = {0, 0, 0}, sky_horizon[3] = {0, 0, 0};
    float view_to_clip[16] = {};
    std::array<GLint, 4> main_viewport{};
};

} // namespace OpenGL
