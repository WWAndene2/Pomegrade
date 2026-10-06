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

#ifndef GPU3D_H
#define GPU3D_H

#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Savestate.h"
#include "FIFO.h"

namespace melonDS
{
class GPU;

struct Vertex
{
    s32 Position[4];
    s32 Color[3];
    s16 TexCoords[2];

    bool Clipped;

    // final vertex attributes.
    // allows them to be reused in polygon strips.

    s32 FinalPosition[2];
    s32 FinalColor[3];

    // hi-res position (4-bit fractional part)
    // TODO maybe: hi-res color? (that survives clipping)
    s32 HiresPosition[2];

    // polygon multiplier (Pomegrade): view-space position and normal, captured
    // when the vertex is submitted. Not part of the hardware state.
    float ViewPosition[4];
    float ViewNormal[3];
    // high-precision geometry (Pomegrade): screen position without the
    // hardware's integer rounding, in native pixels. Not part of the hardware state.
    float PreciseScreen[2];
    bool HasViewNormal;
    bool LitColor; // colour computed by the DS lighting from that normal
    bool Orthographic; // projected without perspective (2D-like 3D: menus, HUDs)
    float Specular;    // how shiny the material is, 0..1 (its specular colour, when lit)
    s16 ModelPosition[3]; // as the game sent it, before any matrix (polygon multiplier: edge identity)
    // polygon multiplier: a sub-polygon vertex's place in its triangle, the
    // barycentric weights of the triangle's 2nd and 3rd corners as fractions
    // i/n, j/n packed i | j << 4 | n << 8; 0 when unknown (cut by clipping, or
    // not a sub-polygon's). For frame generation across subdivisions.
    u16 GridPoint;

    void DoSavestate(Savestate* file) noexcept;
};

struct Polygon
{
    Vertex* Vertices[10];
    u32 NumVertices;

    s32 FinalZ[10];
    s32 FinalW[10];
    bool WBuffer;

    u32 Attr;
    u32 TexParam;
    u32 TexPalette;

    bool Degenerate;

    bool FacingView;
    bool Translucent;

    bool IsShadowMask;
    bool IsShadow;

    int Type; // 0=regular 1=line

    u32 VTop, VBottom; // vertex indices
    s32 YTop, YBottom; // Y coords
    s32 XTop, XBottom; // associated X coords

    u32 SortKey;

    // polygon multiplier (Pomegrade): this polygon's sub-polygons in its bank's
    // sub-polygon storage (count 0 = drawn as is). Not part of the hardware state.
    u32 SubPolygonStart;
    u32 SubPolygonCount;

    // frame generation (Pomegrade): the same polygon from one frame to the
    // next. The game's submission order (counting polygons culled or clipped
    // away) << 8, low byte: 0xFF, or the sub-polygon's number. Not part of the
    // hardware state.
    u32 FrameId;
    // frame generation (Pomegrade): a sub-polygon's subdivision, its
    // triangle's level and its edges' (adaptive multiplier, 4 bits each), and
    // which triangle of its polygon it divides (bits 16-19); sub-polygons of
    // two frames are the same piece only when it matches. 0 for the others.
    // Not part of the hardware state.
    u32 Subdivision;

    // inspector (Pomegrade): the ARM9 call site (R[15] form, bit 0: Thumb)
    // and the hash of the display list this polygon was drawn from, 0 when
    // unknown. Not part of the hardware state.
    u32 CallSite;
    u32 Caller; // LR at that store: the return address into the caller
    u32 ListHash;

    void DoSavestate(Savestate* file) noexcept;
};

class Renderer3D;
class NDS;

// Polygon multiplier (Pomegrade) storage, per RAM bank: allocated in blocks as
// needed (blocks never move, renderers keep pointers into them). The limit only
// guards memory (a full 2048-quad scene at x64 fits); a polygon that would go
// beyond it is drawn as is.
constexpr u32 SubBlockSize = 4096;
constexpr u32 MaxSubPolygons = 262144;
constexpr u32 MaxSubVertices = MaxSubPolygons * 4;

class GPU3D
{
public:
    GPU3D(melonDS::NDS& nds, std::unique_ptr<Renderer3D>&& renderer = nullptr) noexcept;
    ~GPU3D() noexcept = default;
    void Reset() noexcept;

    void DoSavestate(Savestate* file) noexcept;

    void SetEnabled(bool geometry, bool rendering) noexcept;

    void ExecuteCommand() noexcept;

    s32 CyclesToRunFor() const noexcept;
    void Run() noexcept;
    void CheckFIFOIRQ() noexcept;
    void CheckFIFODMA() noexcept;

    void VCount144(GPU& gpu) noexcept;
    void VBlank() noexcept;
    void VCount215(GPU& gpu) noexcept;

    void RestartFrame(GPU& gpu) noexcept;
    void Stop(const GPU& gpu) noexcept;

    void SetRenderXPos(u16 xpos) noexcept;
    [[nodiscard]] u16 GetRenderXPos() const noexcept { return RenderXPos; }
    u32* GetLine(int line) noexcept;

    void WriteToGXFIFO(u32 val) noexcept;

    // Polygon multiplier (Pomegrade): each lit polygon is drawn as
    // level*level curved sub-triangles (quads: twice that). 1 = off.
    void SetPolygonMultiplier(int level) noexcept;
    [[nodiscard]] int GetPolygonMultiplier() const noexcept { return PolygonMultiplierLevel; }
    // Adaptive level: the renderer's output pixels per DS pixel. Each edge is
    // then subdivided only as finely as it shows at that resolution (see
    // PolygonMultiplier::EdgeLevel), up to the level above. 0 = every polygon
    // at that level.
    void SetPolygonMultiplierScale(int outputScale) noexcept { PolygonMultiplierScale = outputScale; }

    // Polygon limit removed (Pomegrade): polygons past the hardware's 2048
    // polygons / 6144 vertices are still drawn. The game sees the hardware
    // behaviour (overflow flag, counters, timing) either way.
    void SetUnlimitedPolygons(bool enable) noexcept;

    // View-space position and normal per vertex (Pomegrade), for the polygon
    // multiplier and the renderer's lighting effects.
    void SetViewDataCapture(bool enable) noexcept { ViewDataRequested = enable; }
    [[nodiscard]] bool CaptureViewData() const noexcept { return PolygonMultiplierLevel > 1 || ViewDataRequested; }

    // High colour (Pomegrade): vertex colours keep the precision of the DS
    // lighting instead of being rounded to 5 bits per channel. Meant for the
    // OpenGL renderer, whose compositor then blends in 8 bits.
    void SetHighColor(bool enable) noexcept { HighColor = enable; }

    // Polygons for the renderers: the hardware list, or the same list with
    // multiplied polygons replaced by their sub-polygons.
    [[nodiscard]] Polygon** GetRenderPolygons() noexcept
    { return RenderMultiplied ? MultipliedRenderPolygons.data() : RenderPolygonRAM.data(); }
    [[nodiscard]] u32 GetRenderNumPolygons() const noexcept
    { return RenderMultiplied ? MultipliedRenderNumPolygons : RenderNumPolygons; }

    [[nodiscard]] bool IsRendererAccelerated() const noexcept;
    [[nodiscard]] Renderer3D& GetCurrentRenderer() noexcept { return *CurrentRenderer; }
    [[nodiscard]] const Renderer3D& GetCurrentRenderer() const noexcept { return *CurrentRenderer; }
    void SetCurrentRenderer(std::unique_ptr<Renderer3D>&& renderer) noexcept;

    u8 Read8(u32 addr) noexcept;
    u16 Read16(u32 addr) noexcept;
    u32 Read32(u32 addr) noexcept;
    void Write8(u32 addr, u8 val) noexcept;
    void Write16(u32 addr, u16 val) noexcept;
    void Write32(u32 addr, u32 val) noexcept;
    void Blit(const GPU& gpu) noexcept;
private:
    melonDS::NDS& NDS;
    typedef union
    {
        u64 _contents;
        struct
        {
            u32 Param;
            u8 Command;
            u8 Unused;
            // Pomegrade (inspector): who wrote this command (Inspector
            // source tag, 0: none). Fits in the entry's former padding
            u16 Source;
        };

    } CmdFIFOEntry;

    // inspector (Pomegrade): source tag of the command being executed
    u16 CurCommandSource = 0;

    void UpdateClipMatrix() noexcept;
    void ResetRenderingState() noexcept;
    void AddCycles(s32 num) noexcept;
    void NextVertexSlot() noexcept;
    void StallPolygonPipeline(s32 delay, s32 nonstalldelay) noexcept;
    void SubmitPolygon() noexcept;
    void SubmitVertex() noexcept;
    void ComputeScreenPosition(Vertex* vtx) const noexcept;
    void FinalizePolygon(Polygon* poly, int nverts) const noexcept;
    void MultiplyPolygon(Polygon* parent, int nverts) noexcept;
    void BuildMultipliedRenderList() noexcept;
    Vertex* NewSubVertex() noexcept;
    Polygon* NewSubPolygon() noexcept;
    void ClearSubPolygons() noexcept;
    void CalculateLighting() noexcept;
    s32 LightVertex(const s32* normaltrans, u8* color, s32* precisecolor = nullptr) const noexcept;
    void BoxTest(const u32* params) noexcept;
    void PosTest() noexcept;
    void VecTest(u32 param) noexcept;
    void CmdFIFOWrite(const CmdFIFOEntry& entry) noexcept;
    CmdFIFOEntry CmdFIFORead() noexcept;
    void FinishWork(s32 cycles) noexcept;
    void VertexPipelineSubmitCmd() noexcept
    {
        // vertex commands 0x24, 0x25, 0x26, 0x27, 0x28
        if (!(VertexSlotsFree & 0x1)) NextVertexSlot();
        else                          AddCycles(1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed6() noexcept
    {
        // commands 0x20, 0x30, 0x31, 0x72 that can run 6 cycles after a vertex
        if (VertexPipeline > 2) AddCycles((VertexPipeline - 2) + 1);
        else                    AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed8() noexcept
    {
        // commands 0x29, 0x2A, 0x2B, 0x33, 0x34, 0x41, 0x60, 0x71 that can run 8 cycles after a vertex
        if (VertexPipeline > 0) AddCycles(VertexPipeline + 1);
        else                    AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed4() noexcept
    {
        // all other commands can run 4 cycles after a vertex
        // no need to do much here since that is the minimum
        AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    std::unique_ptr<Renderer3D> CurrentRenderer = nullptr;

    u16 RenderXPos = 0;

public:
    FIFO<CmdFIFOEntry, 256> CmdFIFO {};
    FIFO<CmdFIFOEntry, 4> CmdPIPE {};

    FIFO<CmdFIFOEntry, 64> CmdStallQueue {};

    u32 ZeroDotWLimit = 0xFFFFFF;

    u32 GXStat = 0;

    u32 ExecParams[32] {};
    u32 ExecParamCount = 0;

    s32 CycleCount = 0;
    s32 VertexPipeline = 0;
    s32 NormalPipeline = 0;
    s32 PolygonPipeline = 0;
    s32 VertexSlotCounter = 0;
    u32 VertexSlotsFree = 0;

    u32 NumPushPopCommands = 0;
    u32 NumTestCommands = 0;


    u32 MatrixMode = 0;

    s32 ProjMatrix[16] {};
    s32 PosMatrix[16] {};
    s32 VecMatrix[16] {};
    s32 TexMatrix[16] {};

    s32 ClipMatrix[16] {};
    bool ClipMatrixDirty = false;

    u32 Viewport[6] {};

    s32 ProjMatrixStack[16] {};
    s32 PosMatrixStack[32][16] {};
    s32 VecMatrixStack[32][16] {};
    s32 TexMatrixStack[16] {};
    s32 ProjMatrixStackPointer = 0;
    s32 PosMatrixStackPointer = 0;
    s32 TexMatrixStackPointer = 0;

    u32 NumCommands = 0;
    u32 CurCommand = 0;
    u32 ParamCount = 0;
    u32 TotalParams = 0;

    bool GeometryEnabled = false;
    bool RenderingEnabled = false;

    u32 DispCnt = 0;
    u8 AlphaRefVal = 0;
    u8 AlphaRef = 0;

    u16 ToonTable[32] {};
    u16 EdgeTable[8] {};

    u32 FogColor = 0;
    u32 FogOffset = 0;
    u8 FogDensityTable[32] {};

    u32 ClearAttr1 = 0;
    u32 ClearAttr2 = 0;

    u32 RenderDispCnt = 0;
    u8 RenderAlphaRef = 0;

    u16 RenderToonTable[32] {};
    u16 RenderEdgeTable[8] {};

    u32 RenderFogColor = 0;
    u32 RenderFogOffset = 0;
    u32 RenderFogShift = 0;
    u8 RenderFogDensityTable[34] {};

    u32 RenderClearAttr1 = 0;
    u32 RenderClearAttr2 = 0;

    bool RenderFrameIdentical = false; // not part of the hardware state, don't serialize

    bool AbortFrame = false;

    u64 Timestamp = 0;


    u32 PolygonMode = 0;
    s16 CurVertex[3] {};
    u8 VertexColor[3] {};
    s16 TexCoords[2] {};
    s16 RawTexCoords[2] {};
    s16 Normal[3] {};

    s16 LightDirection[4][3] {};
    s32 SpecRecip[4] {};
    u8 LightColor[4][3] {};
    u8 MatDiffuse[3] {};
    u8 MatAmbient[3] {};
    u8 MatSpecular[3] {};
    u8 MatEmission[3] {};

    bool UseShininessTable = false;
    u8 ShininessTable[128] {};

    u32 PolygonAttr = 0;
    u32 CurPolygonAttr = 0;

    u32 TexParam = 0;
    u32 TexPalette = 0;

    s32 PosTestResult[4] {};
    s16 VecTestResult[3] {};

    Vertex TempVertexBuffer[4] {};
    u32 VertexNum = 0;
    u32 VertexNumInPoly = 0;
    u32 NumConsecutivePolygons = 0;
    Polygon* LastStripPolygon = nullptr;
    u32 NumOpaquePolygons = 0;

    Vertex VertexRAM[6144 * 2] {};
    Polygon PolygonRAM[2048 * 2] {};

    Vertex* CurVertexRAM = nullptr;
    Polygon* CurPolygonRAM = nullptr;
    u32 NumVertices = 0;
    u32 NumPolygons = 0;
    u32 CurRAMBank = 0;

    std::array<Polygon*,2048> RenderPolygonRAM {};
    u32 RenderNumPolygons = 0;

    u32 FlushRequest = 0;
    u32 FlushAttributes = 0;
    u32 ScrolledLine[256]; // not part of the hardware state, don't serialize

    // polygon multiplier (Pomegrade), not part of the hardware state, don't serialize
    int PolygonMultiplierLevel = 1;
    int PolygonMultiplierScale = 0;
    float CurViewNormal[3] {};
    bool CurViewNormalValid = false;
    bool CurColorFromLighting = false; // vertex colour last set by a normal command
    std::vector<std::unique_ptr<Vertex[]>> SubVertexBlocks[2];
    std::vector<std::unique_ptr<Polygon[]>> SubPolygonBlocks[2];
    u32 NumSubVertices = 0;   // in the current bank
    u32 NumSubPolygons = 0;
    std::vector<Polygon*> MultipliedRenderPolygons;
    bool UnlimitedPolygons = false;
    bool ViewDataRequested = false;
    u32 PolygonSubmitCount = 0; // polygons the game sent this frame, see Polygon::FrameId
    // Polygon multiplier, geometry fidelity from neighbours: share of the
    // curvature kept along each edge, from the angle between the two faces
    // sharing it (PolygonMultiplier::DihedralKeep). Edges are identified by
    // their corners' model positions and texture, so they are the same edge
    // from frame to frame. Faces seen this frame are paired in EdgeFaces; the
    // angles found apply from the next frame (EdgeKeep), so the two polygons
    // of an edge always use the same value within a frame (no cracks).
    struct EdgeFace
    {
        float Normal[3] {}; // the face waiting for its pair
        float Keep = 1;     // from the first pair
        u8 Faces = 0;       // faces seen this frame
        bool Conflict = false; // pairs that disagree
    };
    std::unordered_map<u64, EdgeFace> EdgeFaces;
    std::unordered_map<u64, float> EdgeKeep;
    void UpdateEdgeKeep() noexcept;
    void RegisterEdgeFaces(int nverts) noexcept;
    void RecordShadowCaster(int nverts);
    std::vector<float> ShadowCasters; // the frame being submitted
    [[nodiscard]] u64 EdgeKey(int corner, int nverts) const noexcept;
    [[nodiscard]] u64 EdgeKeyOf(int corner, int other) const noexcept;
    // Adaptive multiplier level, per edge (same keys as EdgeKeep): the levels
    // of the previous frame, read only while a frame is submitted so both
    // polygons of an edge start from the same one (no cracks), and this
    // frame's, for the next (see PolygonMultiplier::SteadyEdgeLevel).
    std::unordered_map<u64, u8> EdgeLevelsShown, EdgeLevelsNext;
    // Edges of the polygons drawn without being multiplied (flat, unlit, out
    // of room...), keyed by their corners' model positions only (a neighbour
    // with another texture shares the edge too): the previous frame's, read
    // while a frame is submitted, and this frame's. A multiplied polygon keeps
    // such an edge straight and unsubdivided, as its neighbour draws it, or a
    // crack opens between the two.
    std::unordered_set<u64> StraightEdgesShown, StraightEdgesNext;
    void RecordStraightEdges(int nverts) noexcept;
    [[nodiscard]] u64 EdgePositionKey(int corner, int other) const noexcept;
    s16 RenderLightDirection[4][3] {}; // light directions (view space) at the end of the rendered frame
    // Pomegrade: what casts shadows in the rendered frame (see RecordShadowCaster):
    // opaque lit triangles as the game submitted them, 12 floats each: the
    // three corners' view-space xyz, then the face's normal (its vertex normals' average)
    std::vector<float> RenderShadowCasters;
    // the perspective projection and viewport most of the rendered frame's vertices used
    s32 RenderProjMatrix[16] {};
    u32 RenderViewport[6] {};
    // while the frame is submitted: the projection in use and its vertex count,
    // and the one with the most vertices so far
    s32 FrameProjMatrix[16] {};
    u32 FrameViewport[6] {};
    u32 FrameProjVertices = 0;
    s32 BestProjMatrix[16] {};
    u32 BestViewport[6] {};
    u32 BestProjVertices = 0;
    bool FrameProjectionCheck = true; // a matrix or the viewport changed since the last check
    bool HighColor = false;
    s32 VertexColorPrecise[3] {}; // current vertex colour, 5.12 fixed point (high colour)
    std::vector<Polygon*> ExtraPolygons[2]; // past the hardware limit, per bank, in the sub-polygon storage
    u32 MultipliedRenderNumPolygons = 0;
    bool RenderMultiplied = false;
};

class Renderer3D
{
public:
    virtual ~Renderer3D() = default;

    Renderer3D(const Renderer3D&) = delete;
    Renderer3D& operator=(const Renderer3D&) = delete;

    virtual void Reset(GPU& gpu) = 0;

    // This "Accelerated" flag currently communicates if the framebuffer should
    // be allocated differently and other little misc handlers. Ideally there
    // are more detailed "traits" that we can ask of the Renderer3D type
    const bool Accelerated;

    virtual void VCount144(GPU& gpu) {};
    virtual void Stop(const GPU& gpu) {}
    virtual void RenderFrame(GPU& gpu) = 0;
    virtual void RestartFrame(GPU& gpu) {};
    virtual u32* GetLine(int line) = 0;
    virtual void Blit(const GPU& gpu) {};

    virtual void SetupAccelFrame() {}
    virtual void PrepareCaptureFrame() {}
    virtual void SetOutputTexture(int buffer, u32 texture) {}
    virtual void BindOutputTexture(int buffer) {}

    virtual bool NeedsShaderCompile() { return false; }
    virtual void ShaderCompileStep(int& current, int& count) {}

protected:
    Renderer3D(bool Accelerated);
};

}

#endif
