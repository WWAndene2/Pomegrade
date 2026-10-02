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

#ifdef OGLRENDERER_ENABLED
#include "GPU3D.h"
#include "GPU_OpenGL.h"
#include "GPU3D_OpenGL_HDTextures.h"
#include "OpenGLSupport.h"

namespace melonDS
{
class GPU;

class GLRenderer : public Renderer3D
{
public:
    ~GLRenderer() override;
    void Reset(GPU& gpu) override;

    void SetRenderSettings(bool betterpolygons, int scale) noexcept;
    void SetBetterPolygons(bool betterpolygons) noexcept;
    // High-precision geometry (Pomegrade): vertices placed at sub-pixel
    // positions instead of being snapped to whole pixels of the output.
    void SetHighPrecision(bool enable) noexcept { HighPrecision = enable; }
    // High colour (Pomegrade): see GLCompositor::SetHighColor and GPU3D::SetHighColor
    void SetHighColor(bool enable) noexcept { CurGLCompositor.SetHighColor(enable); }
    // Native texture upscaling (Pomegrade): see GLHDTextures::SetUpscaleFactor
    void SetTextureUpscale(int factor) { HDTextures.SetUpscaleFactor(factor); }
    // Scene-adaptive colour (Pomegrade): see GPU_SceneColour.h
    void SetAdaptiveColours(bool enable) { CurGLCompositor.SetAdaptiveColours(enable); }
    void SetOledBlacks(bool enable) { CurGLCompositor.SetOledBlacks(enable); }
    // Pseudo ray tracing (Pomegrade): lighting effects computed from the
    // view-space position and normal of each opaque pixel, see RenderLighting.
    // Display only: the frame the game can capture stays the DS render.
    void SetAmbientOcclusion(bool enable) noexcept { AmbientOcclusion = enable; }
    void SetScaleFactor(int scale) noexcept;
    [[nodiscard]] bool GetBetterPolygons() const noexcept { return BetterPolygons; }
    [[nodiscard]] int GetScaleFactor() const noexcept { return ScaleFactor; }

    void VCount144(GPU& gpu) override {};
    void RenderFrame(GPU& gpu) override;
    void Stop(const GPU& gpu) override;
    u32* GetLine(int line) override;

    void SetupAccelFrame() override;
    void PrepareCaptureFrame() override;
    void Blit(const GPU& gpu) override;

    void SetOutputTexture(int buffer, u32 texture) override;
    void BindOutputTexture(int buffer) override;

    static std::unique_ptr<GLRenderer> New() noexcept;
private:
    // Used by New()
    GLRenderer(GLCompositor&& compositor) noexcept;

    // GL version requirements
    // * texelFetch: 3.0 (GLSL 1.30)     (3.2/1.50 for MS)
    // * UBO: 3.1

    struct RendererPolygon
    {
        Polygon* PolyData;

        u32 NumIndices;
        u32 IndicesOffset;
        GLuint PrimType;

        u32 NumEdgeIndices;
        u32 EdgeIndicesOffset;

        u32 RenderKey;

        u32 HDTexture; // packed HD atlas location, see GLHDTextures::Lookup
    };

    GLCompositor CurGLCompositor;
    // buffers grow past the hardware's needs for the polygon multiplier (Pomegrade), see EnsureCapacity
    std::vector<RendererPolygon> PolygonList = std::vector<RendererPolygon>(2048);

    bool BuildRenderShader(u32 flags, const std::string& vs, const std::string& fs);
    void UseRenderShader(u32 flags);
    void SetupPolygon(RendererPolygon* rp, Polygon* polygon) const;
    u32* SetupVertex(const Polygon* poly, int vid, const Vertex* vtx, u32 vtxattr, u32 hdTexture, u32* vptr) const;
    void LookupHDTextures(GPU& gpu, int npolys);
    void BuildPolygons(RendererPolygon* polygons, int npolys);
    void EnsureCapacity(Polygon** polygons, u32 npolys);
    int RenderSinglePolygon(int i) const;
    int RenderPolygonBatch(int i) const;
    int RenderPolygonEdgeBatch(int i) const;
    void RenderSceneChunk(const GPU3D& gpu3d, int y, int h);

    bool LightingEnabled() const noexcept { return AmbientOcclusion; }
    float* SetupViewVertex(const Vertex* vtx, float* gptr) const;
    float* SetupViewCenterVertex(const Polygon* poly, float* gptr) const;
    void SetupLightingTargets();
    void RenderLighting();

    enum
    {
        RenderFlag_WBuffer     = 0x01,
        RenderFlag_Trans       = 0x02,
        RenderFlag_ShadowMask  = 0x04,
        RenderFlag_Edge        = 0x08,
    };


    GLuint ClearShaderPlain {};

    GLuint RenderShader[16] {};
    GLuint CurShaderID = -1;

    GLuint FinalPassEdgeShader {};
    GLuint FinalPassFogShader {};

    // std140 compliant structure
    struct
    {
        float uScreenSize[2];       // vec2       0 / 2
        u32 uDispCnt;               // int        2 / 1
        u32 __pad0;
        float uToonColors[32][4];   // vec4[32]   4 / 128
        float uEdgeColors[8][4];    // vec4[8]    132 / 32
        float uFogColor[4];         // vec4       164 / 4
        float uFogDensity[34][4];   // float[34]  168 / 136
        u32 uFogOffset;             // int        304 / 1
        u32 uFogShift;              // int        305 / 1
        u32 _pad1[2];               // int        306 / 2
    } ShaderConfig {};

    GLuint ShaderConfigUBO {};
    int NumFinalPolys {}, NumOpaqueFinalPolys {};

    GLuint ClearVertexBufferID = 0, ClearVertexArrayID {};
    GLint ClearUniformLoc[4] {};

    // vertex buffer
    // * XYZW: 4x16bit
    // * RGBA: 4x8bit
    // * ST: 2x16bit
    // * polygon data: 3x32bit (polygon attrib, texture VRAM offset, (texture attrib (low 16 bit), texture palette (high 16 bit))
    //
    // polygon attributes:
    // * bit4-7, 11, 14-15, 24-29: POLYGON_ATTR
    // * bit16-20: Z shift
    // * bit8: front-facing (?)
    // * bit9: W-buffering (?)

    GLuint VertexBufferID {};
    static constexpr u32 VertexSize = 8; // in u32s
    std::vector<u32> VertexBuffer = std::vector<u32>(10240 * VertexSize);

    GLHDTextures HDTextures;
    u32 NumVertices {};

    GLuint VertexArrayID {};
    GLuint IndexBufferID {};
    // 32-bit indices: the polygon multiplier goes beyond 65536 vertices.
    // Triangle indices first, then edge indices from EdgeIndicesOffset.
    std::vector<u32> IndexBuffer = std::vector<u32>(2048 * 40);
    u32 NumIndices {}, NumEdgeIndices {};

    u32 EdgeIndicesOffset = 2048 * 30;

    GLuint TexMemID {};
    GLuint TexPalMemID {};

    int ScaleFactor {};
    bool BetterPolygons {};
    bool HighPrecision {};
    // sub-pixel steps per output pixel in vertex positions (16-bit, up to 4096 px)
    u32 SubpixelScale() const noexcept { return HighPrecision ? (ScaleFactor <= 8 ? 16 : 8) : 1; }
    int ScreenW {}, ScreenH {};

    GLuint ColorBufferTex {}, DepthBufferTex {}, AttrBufferTex {};
    GLuint DownScaleBufferTex {};
    GLuint PixelbufferID {};

    GLuint MainFramebuffer {}, DownscaleFramebuffer {};

    // lighting effects (Pomegrade)
    bool AmbientOcclusion {};
    bool LightingActive {};   // view data is being rendered this frame
    bool LightingDone {};     // this frame's lit image is in LightingTex
    bool ViewDataCaptured {}; // GPU3D captured view data while the rendered frame was submitted
    bool LightingSupported = true; // the render targets work on this GPU
    // per vertex, beside VertexBuffer: view position xyz + perspective flag,
    // view normal xyz + has-normal flag
    GLuint ViewVertexBufferID {};
    static constexpr u32 ViewVertexSize = 8; // in floats
    std::vector<float> ViewVertexBuffer = std::vector<float>(10240 * ViewVertexSize);
    GLuint ViewPositionTex {}, ViewNormalTex {}, AOTex {}, LightingTex {};
    GLuint AOFramebuffer {}, LightingFramebuffer {};
    GLuint LightingAOShader {}, LightingComposeShader {};
    GLint LightingAORadiusLoc = -1;
    int LightingTargetsW {}, LightingTargetsH {};
    u32 Framebuffer[256*192] {};
};
}
#endif