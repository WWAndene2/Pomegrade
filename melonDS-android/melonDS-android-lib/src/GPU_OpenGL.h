/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#pragma once

#include "OpenGLSupport.h"
#include "GPU_SceneColour.h"

#include <array>
#include <optional>

namespace melonDS
{
class GPU;
struct RenderSettings;
class GLRenderer;
class Renderer3D;
class GLCompositor
{
public:
    static std::optional<GLCompositor> New() noexcept;
    GLCompositor(const GLCompositor&) = delete;
    GLCompositor& operator=(const GLCompositor&) = delete;
    GLCompositor(GLCompositor&&) noexcept;
    GLCompositor& operator=(GLCompositor&&) noexcept;
    ~GLCompositor();

    void SetScaleFactor(int scale) noexcept;
    // High colour (Pomegrade): blend the 3D and 2D layers in 8 bits per channel
    void SetHighColor(bool enable) noexcept { HighColor = enable; }
    // Scene-adaptive colour (Pomegrade), see GPU_SceneColour.h
    void SetAdaptiveColours(bool enable) noexcept { SceneColourState.Adaptive = enable; }
    void SetOledBlacks(bool enable) noexcept { SceneColourState.Oled = enable; }
    [[nodiscard]] int GetScaleFactor() const noexcept { return Scale; }

    void Stop(const GPU& gpu) noexcept;
    void RenderFrame(const GPU& gpu, Renderer3D& renderer) noexcept;
    // Frame generation (Pomegrade): composites the renderer's current 3D image
    // with the 2D layers of the frame just finished (already uploaded by its
    // RenderFrame), into texture. The scene
    // colour parameters are used as they are (they advance once per DS frame).
    void RenderIntermediateFrame(const GPU& gpu, Renderer3D& renderer, GLuint texture) noexcept;
    void SetOutputTexture(int buf, GLuint texture);
    void BindOutputTexture(int buf);
private:
    GLCompositor(GLuint CompShader) noexcept;
    int Scale = 0;
    int ScreenH = 0, ScreenW = 0;

    GLuint CompShader {};
    GLuint CompScaleLoc = 0;
    GLuint CompHighColorLoc = 0;
    bool HighColor = false;

    // scene-adaptive colour: each frame a small copy of the unadjusted image is
    // read back asynchronously (used the next frame) to update the parameters
    struct SceneColourPass
    {
        static constexpr int Width = 64, Height = 96; // both screens
        bool Adaptive = false, Oled = false;
        SceneColourParams Current, Target;
        GLuint Tex = 0, FB = 0, PBO = 0;
        bool Pending = false;
        GLuint AdjustLoc = 0, LevelsLoc = 0, SaturationLoc = 0, OledLoc = 0;
    } SceneColourState;
    void DeleteSceneColourTargets() noexcept;
    void RunSceneColourPass() noexcept;
    // srcbuf: the 2D layers to upload, -1 = those already in the input texture
    void Composite(const GPU& gpu, Renderer3D& renderer, GLuint framebuffer, int srcbuf, bool advanceSceneColour) noexcept;

    GLuint CompVertexBufferID = 0;
    GLuint CompVertexArrayID = 0;

    struct CompVertex
    {
        std::array<float, 2> Position {};
        std::array<float, 2> Texcoord {};
    };
    std::array<CompVertex, 2*3*2> CompVertices {};

    GLuint CompScreenInputTex = 0;
    std::array<GLuint, 2> CompScreenOutputTex {};
    std::array<GLuint, 2> CompScreenOutputFB {};
    GLuint IntermediateFB = 0; // frame generation output
};

}
