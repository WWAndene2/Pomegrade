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
    // Pomegrade: smooth texture filtering, colour only (see TextureLookup_Filtered)
    void SetTextureFilter(bool enable) noexcept { ShaderConfig.uTextureFilter = enable ? 1 : 0; }
    // Deeper blacks on OLED screens (Pomegrade): see GPU_SceneColour.h
    void SetOledBlacks(bool enable) { CurGLCompositor.SetOledBlacks(enable); }
    // Frame generation (Pomegrade): see RenderIntermediateFrame
    void SetFrameGeneration(bool enable) noexcept;
    // Renders an image between the 3D frame last shown and the one rendered
    // after it (the DS renders a frame ahead), at position (0 = the frame
    // shown, 1 = the next; 0.5 halfway, 0.25/0.5/0.75 for three images): each
    // polygon present in both, with the same texture and shape, has its
    // vertices placed there (screen position, depth, colour, texture
    // coordinates). The pairing is made once for all the images between the
    // same two frames. Composited
    // with the 2D layers of the frame just finished, into outputTexture.
    // Returns false (nothing rendered) when there is nothing to interpolate.
    // The renderer's own image is left as it was: display capture and the
    // next frame see the DS render.
    bool RenderIntermediateFrame(GPU& gpu, u32 outputTexture, float position = 0.5f);
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

    bool BuildRenderShaders();

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
    void RenderScene(GPU& gpu, Polygon** renderpolys, u32 numrenderpolys, bool intermediate);

    // frame generation: copies of the polygons of the last two 3D frames
    struct FrameSnapshot
    {
        std::vector<Polygon> Polygons;
        std::vector<Vertex> Vertices;
        std::vector<u32> Order; // indices in Polygon::FrameId order (the game's submission order)
        void Take(Polygon** polys, u32 count);
    };
    bool FrameGeneration {};
    FrameSnapshot Snapshots[2]; // previous, current
    FrameSnapshot Intermediate;
    // the previous frame's counterparts of sub-polygons subdivided differently
    // there (adaptive multiplier level), resampled from its subdivision
    FrameSnapshot Resampled;
    bool ResamplePrevious(const Polygon& cur, const Polygon* const* prevPieces, u32 count, Polygon& out);
    // each polygon of the current snapshot paired with the previous one's
    // (null: none), once per pair of frames for all the images between them
    bool PairSnapshots();
    std::vector<const Polygon*> Match;
    u32 SnapshotVersion = 0, PairedVersion = ~0u;
    bool Paired = false;
    std::vector<Polygon*> IntermediateList;
    GLuint BackupColorTex {};
    GLuint CopyFramebuffers[2] {};
    int BackupW {}, BackupH {};

    // Pomegrade: a polygon's Z-buffer depth as a plane on screen (output
    // units), when its rounded vertices need it; and its depth at a position
    bool DepthPlane(const Polygon* poly, double plane[3]) const;
    static u32 PlaneDepth(const double plane[3], u32 x, u32 y) noexcept;

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
        u32 uTextureFilter;         // int        306 / 1   Pomegrade: TextureLookup_Filtered
        u32 __pad1;                 // the block's size stays a multiple of 16 bytes
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

    // inspector (Pomegrade): set while polygons are drawn in its colours
    const class Inspector* ViewInspector = nullptr;
    u32 Framebuffer[256*192] {};
};
}
#endif