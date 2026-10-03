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

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include "NDS.h"
#include "GPU.h"
#include "FIFO.h"
#include "GPU3D_Soft.h"
#include "Platform.h"
#include "GPU3D.h"
#include "GPU3D_PolygonMultiplier.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// 3D engine notes
//
// vertex/polygon RAM is filled when a complete polygon is defined, after it's been culled and clipped
// 04000604 reads from bank used by renderer
// bank used by renderer is emptied at scanline ~192
// banks are swapped at scanline ~194
// TODO: needs more investigation. it's weird.
//
// clipping rules:
// * if a shared vertex in a strip is clipped, affected polygons are converted into single polygons
//   strip is resumed at the first eligible polygon
//
// clipping exhibits oddities on the real thing. bad precision? fancy algorithm? TODO: investigate.
//
// vertex color precision:
// * vertex colors are kept at 5-bit during clipping. makes for shitty results.
// * vertex colors are converted to 9-bit before drawing, as such:
//   if (x > 0) x = (x << 4) + 0xF
//   the added bias affects interpolation.
//
// depth buffer:
// Z-buffering mode: val = ((Z * 0x800 * 0x1000) / W) + 0x7FFEFF (nope, wrong. TODO update)
// W-buffering mode: val = W
//
// formula for clear depth: (GBAtek is wrong there)
// clearZ = (val * 0x200) + 0x1FF;
//
// alpha is 5-bit
//
// matrix push/pop on the position matrix are always applied to the vector matrix too, even in position-only mode
// store/restore too, probably (TODO: confirm)
// (the idea is that each position matrix has an associated vector matrix)
//
// TODO: check if translate works on the vector matrix? seems pointless
//
// viewport Y coordinates are upside-down
//
// several registers are latched upon VBlank, the renderer uses the latched registers
// latched registers include:
// DISP3DCNT
// alpha test ref value
// fog color, offset, density table
// toon table
// edge table
// clear attributes
//
// TODO: check how DISP_1DOT_DEPTH works and whether it's latched
//
// TODO: emulate GPU hanging
// * when calling BEGIN with an incomplete polygon defined
// * probably same with BOXTEST
// * when sending vertices immediately after a BOXTEST
//
// TODO: test results should probably not be presented immediately, even if we set the busy flag


// command execution notes
//
// timings given by GBAtek are for individual commands
// actual display lists have different timing characteristics
// * vertex pipeline: individual vertex commands are able to execute in parallel
//   with certain other commands
// * similarly, the normal command can execute in parallel with a subsequent vertex
// * polygon pipeline: each vertex which completes a polygon takes longer to run
//   and imposes rules on when further vertex commands can run
//   (one every 9-cycle time slot during polygon setup)
//   polygon setup time is 27 cycles for a triangle and 36 for a quad
//   except: only one time slot is taken if the polygon is rejected by culling/clipping
// * additionally, some commands (BEGIN, LIGHT_VECTOR, BOXTEST) stall the polygon pipeline


const u8 CmdNumParams[256] =
{
    // 0x00
    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10
    1, 0, 1, 1, 1, 0, 16, 12, 16, 12, 9, 3, 3,
    0, 0, 0,
    // 0x20
    1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0,
    // 0x30
    1, 1, 1, 1, 32,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x40
    1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60
    1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70
    3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80+
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void MatrixLoadIdentity(s32* m);

GPU3D::GPU3D(melonDS::NDS& nds, std::unique_ptr<Renderer3D>&& renderer) noexcept :
    NDS(nds),
    CurrentRenderer(renderer ? std::move(renderer) : std::make_unique<SoftRenderer>())
{
}

void Vertex::DoSavestate(Savestate* file) noexcept
{
    file->VarArray(Position, sizeof(Position));
    file->VarArray(Color, sizeof(Color));
    file->VarArray(TexCoords, sizeof(TexCoords));

    file->Bool32(&Clipped);

    file->VarArray(FinalPosition, sizeof(FinalPosition));
    file->VarArray(FinalColor, sizeof(FinalColor));
    file->VarArray(HiresPosition, sizeof(HiresPosition));
}

void GPU3D::SetCurrentRenderer(std::unique_ptr<Renderer3D>&& renderer) noexcept
{
    CurrentRenderer = std::move(renderer);
    CurrentRenderer->Reset(NDS.GPU);
}

void GPU3D::ResetRenderingState() noexcept
{
    RenderNumPolygons = 0;

    RenderMultiplied = false;
    MultipliedRenderNumPolygons = 0;
    ClearSubPolygons();

    RenderDispCnt = 0;
    RenderAlphaRef = 0;

    memset(RenderEdgeTable, 0, 8*2);
    memset(RenderToonTable, 0, 32*2);

    RenderFogColor = 0;
    RenderFogOffset = 0;
    RenderFogShift = 0;
    memset(RenderFogDensityTable, 0, 34);

    RenderClearAttr1 = 0x3F000000;
    RenderClearAttr2 = 0x00007FFF;
}

void GPU3D::Reset() noexcept
{
    PolygonSubmitCount = 0;
    EdgeFaces.clear();
    EdgeKeep.clear();

    CmdFIFO.Clear();
    CmdPIPE.Clear();

    CmdStallQueue.Clear();

    ZeroDotWLimit = 0xFFFFFF;

    GXStat = 0;

    memset(ExecParams, 0, 32*4);
    ExecParamCount = 0;

    CycleCount = 0;
    VertexPipeline = 0;
    NormalPipeline = 0;
    PolygonPipeline = 0;
    VertexSlotCounter = 0;
    VertexSlotsFree = 1;

    NumPushPopCommands = 0;
    NumTestCommands = 0;

    MatrixMode = 0;

    MatrixLoadIdentity(ProjMatrix);
    MatrixLoadIdentity(PosMatrix);
    MatrixLoadIdentity(VecMatrix);
    MatrixLoadIdentity(TexMatrix);

    ClipMatrixDirty = true;
    UpdateClipMatrix();

    memset(Viewport, 0, sizeof(Viewport));

    memset(ProjMatrixStack, 0, 16*4);
    memset(PosMatrixStack, 0, 31 * 16*4);
    memset(VecMatrixStack, 0, 31 * 16*4);
    memset(TexMatrixStack, 0, 16*4);

    ProjMatrixStackPointer = 0;
    PosMatrixStackPointer = 0;
    TexMatrixStackPointer = 0;

    NumCommands = 0;
    CurCommand = 0;
    ParamCount = 0;
    TotalParams = 0;

    GeometryEnabled = false;
    RenderingEnabled = false;

    DispCnt = 0;
    AlphaRefVal = 0;
    AlphaRef = 0;

    memset(ToonTable, 0, sizeof(ToonTable));
    memset(EdgeTable, 0, sizeof(EdgeTable));

    // TODO: confirm initial polyid/color/fog values
    FogOffset = 0;
    FogColor = 0;
    memset(FogDensityTable, 0, sizeof(FogDensityTable));

    ClearAttr1 = 0x3F000000;
    ClearAttr2 = 0x00007FFF;

    ResetRenderingState();

    AbortFrame = false;

    Timestamp = 0;

    PolygonMode = 0;
    memset(CurVertex, 0, sizeof(CurVertex));
    memset(VertexColor, 0, sizeof(VertexColor));
    memset(TexCoords, 0, sizeof(TexCoords));
    memset(RawTexCoords, 0, sizeof(RawTexCoords));
    memset(Normal, 0, sizeof(Normal));

    memset(LightDirection, 0, sizeof(LightDirection));
    memset(LightColor, 0, sizeof(LightColor));
    memset(MatDiffuse, 0, sizeof(MatDiffuse));
    memset(MatAmbient, 0, sizeof(MatAmbient));
    memset(MatSpecular, 0, sizeof(MatSpecular));
    memset(MatEmission, 0, sizeof(MatSpecular));

    UseShininessTable = false;
    // Shininess table seems to be uninitialized garbage, at least on n3dsxl hw?
    // Also doesn't seem to be cleared properly unless the system is fully powered off?
    memset(ShininessTable, 0, sizeof(ShininessTable));

    PolygonAttr = 0;
    CurPolygonAttr = 0;

    TexParam = 0;
    TexPalette = 0;

    memset(PosTestResult, 0, 4*4);
    memset(VecTestResult, 0, 2*3);

    memset(TempVertexBuffer, 0, sizeof(TempVertexBuffer));
    VertexNum = 0;
    VertexNumInPoly = 0;
    NumConsecutivePolygons = 0;
    LastStripPolygon = nullptr;
    NumOpaquePolygons = 0;

    CurVertexRAM = &VertexRAM[0];
    CurPolygonRAM = &PolygonRAM[0];
    NumVertices = 0;
    NumPolygons = 0;
    CurRAMBank = 0;

    FlushRequest = 0;
    FlushAttributes = 0;

    RenderXPos = 0;

    if (CurrentRenderer)
        CurrentRenderer->Reset(NDS.GPU);
}

void GPU3D::DoSavestate(Savestate* file) noexcept
{
    file->Section("GP3D");

    SoftRenderer* softRenderer = dynamic_cast<SoftRenderer*>(CurrentRenderer.get());
    if (softRenderer && softRenderer->IsThreaded())
    {
        softRenderer->SetupRenderThread(NDS.GPU);
    }

    CmdFIFO.DoSavestate(file);
    CmdPIPE.DoSavestate(file);

    file->Var32(&NumCommands);
    file->Var32(&CurCommand);
    file->Var32(&ParamCount);
    file->Var32(&TotalParams);

    file->Var32(&NumPushPopCommands);
    file->Var32(&NumTestCommands);

    file->Var32(&DispCnt);
    file->Var8(&AlphaRefVal);
    file->Var8(&AlphaRef);

    file->VarArray(ToonTable, 32*2);
    file->VarArray(EdgeTable, 8*2);

    file->Var32(&FogColor);
    file->Var32(&FogOffset);
    file->VarArray(FogDensityTable, 32);

    file->Var32(&ClearAttr1);
    file->Var32(&ClearAttr2);

    file->Var32(&RenderDispCnt);
    file->Var8(&RenderAlphaRef);

    file->VarArray(RenderToonTable, 32*2);
    file->VarArray(RenderEdgeTable, 8*2);

    file->Var32(&RenderFogColor);
    file->Var32(&RenderFogOffset);
    file->Var32(&RenderFogShift);
    file->VarArray(RenderFogDensityTable, 34);

    file->Var32(&RenderClearAttr1);
    file->Var32(&RenderClearAttr2);

    file->Var16(&RenderXPos);

    file->Var32(&ZeroDotWLimit);

    file->Var32(&GXStat);

    file->VarArray(ExecParams, 32*4);
    file->Var32(&ExecParamCount);
    file->Var32((u32*)&CycleCount);
    file->Var64(&Timestamp);

    file->Var32(&MatrixMode);

    file->VarArray(ProjMatrix, 16*4);
    file->VarArray(PosMatrix, 16*4);
    file->VarArray(VecMatrix, 16*4);
    file->VarArray(TexMatrix, 16*4);

    file->VarArray(ProjMatrixStack, 16*4);
    file->VarArray(PosMatrixStack, 32*16*4);
    file->VarArray(VecMatrixStack, 32*16*4);
    file->VarArray(TexMatrixStack, 16*4);

    file->Var32((u32*)&ProjMatrixStackPointer);
    file->Var32((u32*)&PosMatrixStackPointer);
    file->Var32((u32*)&TexMatrixStackPointer);

    file->VarArray(Viewport, sizeof(Viewport));

    file->VarArray(PosTestResult, 4*4);
    file->VarArray(VecTestResult, 2*3);

    file->Var32(&VertexNum);
    file->Var32(&VertexNumInPoly);
    file->Var32(&NumConsecutivePolygons);

    for (Vertex& vtx : TempVertexBuffer)
    {
        vtx.DoSavestate(file);
    }

    if (file->Saving)
    {
        // an extra polygon (polygon limit removed) isn't saved: no strip to continue
        bool inram = LastStripPolygon >= &PolygonRAM[0] && LastStripPolygon < &PolygonRAM[2048*2];
        u32 index = inram ? (u32)(LastStripPolygon - &PolygonRAM[0]) : UINT32_MAX;
        file->Var32(&index);
    }
    else
    {
        u32 index = UINT32_MAX;
        file->Var32(&index);
        LastStripPolygon = (index == UINT32_MAX) ? nullptr : &PolygonRAM[index];
    }

    file->Var32(&CurRAMBank);
    file->Var32(&NumVertices);
    file->Var32(&NumPolygons);
    file->Var32(&NumOpaquePolygons);

    file->Var32(&FlushRequest);
    file->Var32(&FlushAttributes);

    for (Vertex& vtx : VertexRAM)
    {
        vtx.DoSavestate(file);
    }

    for(int i = 0; i < 2048*2; i++)
    {
        Polygon* poly = &PolygonRAM[i];

        // this is a bit ugly, but eh
        // we can't save the pointers as-is, that's a bad idea
        if (file->Saving)
        {
            for (int j = 0; j < 10; j++)
            {
                Vertex* ptr = poly->Vertices[j];
                u32 index = ptr ? (u32)(ptr - &VertexRAM[0]) : UINT32_MAX;
                file->Var32(&index);
            }
        }
        else
        {
            for (int j = 0; j < 10; j++)
            {
                u32 index = UINT32_MAX;
                file->Var32(&index);
                poly->Vertices[j] = index == UINT32_MAX ? nullptr : &VertexRAM[index];
            }
        }

        file->Var32(&poly->NumVertices);

        file->VarArray(poly->FinalZ, sizeof(s32)*10);
        file->VarArray(poly->FinalW, sizeof(s32)*10);
        file->Bool32(&poly->WBuffer);

        file->Var32(&poly->Attr);
        file->Var32(&poly->TexParam);
        file->Var32(&poly->TexPalette);

        file->Bool32(&poly->FacingView);
        file->Bool32(&poly->Translucent);

        file->Bool32(&poly->IsShadowMask);
        file->Bool32(&poly->IsShadow);

        if (file->IsAtLeastVersion(4, 1))
            file->Var32((u32*)&poly->Type);
        else
            poly->Type = 0;

        file->Var32(&poly->VTop);
        file->Var32(&poly->VBottom);
        file->Var32((u32*)&poly->YTop);
        file->Var32((u32*)&poly->YBottom);
        file->Var32((u32*)&poly->XTop);
        file->Var32((u32*)&poly->XBottom);

        file->Var32(&poly->SortKey);

        if (!file->Saving)
        {
            poly->Degenerate = false;

            for (u32 j = 0; j < poly->NumVertices; j++)
            {
                if (poly->Vertices[j]->Position[3] == 0)
                    poly->Degenerate = true;
            }

            if (poly->YBottom > 192) poly->Degenerate = true;
        }
    }

    CmdStallQueue.DoSavestate(file);

    file->Var32((u32*)&VertexPipeline);
    file->Var32((u32*)&NormalPipeline);
    file->Var32((u32*)&PolygonPipeline);
    file->Var32((u32*)&VertexSlotCounter);
    file->Var32(&VertexSlotsFree);

    if (!file->Saving)
    {
        ClipMatrixDirty = true;
        UpdateClipMatrix();

        CurVertexRAM = &VertexRAM[CurRAMBank ? 6144 : 0];
        CurPolygonRAM = &PolygonRAM[CurRAMBank ? 2048 : 0];

        // polygon multiplier: sub-polygons aren't saved, render the hardware list
        RenderMultiplied = false;
        ClearSubPolygons();
    }

    file->Var32(&RenderNumPolygons);
    if (file->Saving)
    {
        for (const Polygon* p : RenderPolygonRAM)
        {
            u32 index = p ? (p - &PolygonRAM[0]) : UINT32_MAX;

            file->Var32(&index);
        }
    }
    else
    {
        for (int i = 0; i < RenderPolygonRAM.size(); ++i)
        {
            u32 index = UINT32_MAX;
            file->Var32(&index);

            RenderPolygonRAM[i] = index == UINT32_MAX ? nullptr : &PolygonRAM[index];
        }
    }

    file->VarArray(CurVertex, sizeof(s16)*3);
    file->VarArray(VertexColor, sizeof(u8)*3);
    file->VarArray(TexCoords, sizeof(s16)*2);
    file->VarArray(RawTexCoords, sizeof(s16)*2);
    file->VarArray(Normal, sizeof(s16)*3);

    file->VarArray(LightDirection, sizeof(s16)*4*3);
    file->VarArray(LightColor, sizeof(u8)*4*3);
    file->VarArray(MatDiffuse, sizeof(u8)*3);
    file->VarArray(MatAmbient, sizeof(u8)*3);
    file->VarArray(MatSpecular, sizeof(u8)*3);
    file->VarArray(MatEmission, sizeof(u8)*3);

    file->Bool32(&UseShininessTable);
    file->VarArray(ShininessTable, 128*sizeof(u8));

    file->Bool32(&AbortFrame);
    file->Bool32(&GeometryEnabled);
    file->Bool32(&RenderingEnabled);
    file->Var32(&PolygonMode);
    file->Var32(&PolygonAttr);
    file->Var32(&CurPolygonAttr);
    file->Var32(&TexParam);
    file->Var32(&TexPalette);
    RenderFrameIdentical = false;
    if (softRenderer && softRenderer->IsThreaded())
    {
        softRenderer->EnableRenderThread();
    }
}



void GPU3D::SetEnabled(bool geometry, bool rendering) noexcept
{
    GeometryEnabled = geometry;
    RenderingEnabled = rendering;

    if (!rendering) ResetRenderingState();
}



void MatrixLoadIdentity(s32* m)
{
    m[0] = 0x1000; m[1] = 0;      m[2] = 0;       m[3] = 0;
    m[4] = 0;      m[5] = 0x1000; m[6] = 0;       m[7] = 0;
    m[8] = 0;      m[9] = 0;      m[10] = 0x1000; m[11] = 0;
    m[12] = 0;     m[13] = 0;     m[14] = 0;      m[15] = 0x1000;
}

void MatrixLoad4x4(s32* m, s32* s)
{
    memcpy(m, s, 16*4);
}

void MatrixLoad4x3(s32* m, s32* s)
{
    m[0] = s[0];  m[1] = s[1];  m[2] = s[2];    m[3] = 0;
    m[4] = s[3];  m[5] = s[4];  m[6] = s[5];    m[7] = 0;
    m[8] = s[6];  m[9] = s[7];  m[10] = s[8];   m[11] = 0;
    m[12] = s[9]; m[13] = s[10]; m[14] = s[11]; m[15] = 0x1000;
}

void MatrixMult4x4(s32* m, s32* s)
{
    s32 tmp[16];
    memcpy(tmp, m, 16*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8] + (s64)s[3]*tmp[12]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9] + (s64)s[3]*tmp[13]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10] + (s64)s[3]*tmp[14]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11] + (s64)s[3]*tmp[15]) >> 12;

    m[4] = ((s64)s[4]*tmp[0] + (s64)s[5]*tmp[4] + (s64)s[6]*tmp[8] + (s64)s[7]*tmp[12]) >> 12;
    m[5] = ((s64)s[4]*tmp[1] + (s64)s[5]*tmp[5] + (s64)s[6]*tmp[9] + (s64)s[7]*tmp[13]) >> 12;
    m[6] = ((s64)s[4]*tmp[2] + (s64)s[5]*tmp[6] + (s64)s[6]*tmp[10] + (s64)s[7]*tmp[14]) >> 12;
    m[7] = ((s64)s[4]*tmp[3] + (s64)s[5]*tmp[7] + (s64)s[6]*tmp[11] + (s64)s[7]*tmp[15]) >> 12;

    m[8] = ((s64)s[8]*tmp[0] + (s64)s[9]*tmp[4] + (s64)s[10]*tmp[8] + (s64)s[11]*tmp[12]) >> 12;
    m[9] = ((s64)s[8]*tmp[1] + (s64)s[9]*tmp[5] + (s64)s[10]*tmp[9] + (s64)s[11]*tmp[13]) >> 12;
    m[10] = ((s64)s[8]*tmp[2] + (s64)s[9]*tmp[6] + (s64)s[10]*tmp[10] + (s64)s[11]*tmp[14]) >> 12;
    m[11] = ((s64)s[8]*tmp[3] + (s64)s[9]*tmp[7] + (s64)s[10]*tmp[11] + (s64)s[11]*tmp[15]) >> 12;

    m[12] = ((s64)s[12]*tmp[0] + (s64)s[13]*tmp[4] + (s64)s[14]*tmp[8] + (s64)s[15]*tmp[12]) >> 12;
    m[13] = ((s64)s[12]*tmp[1] + (s64)s[13]*tmp[5] + (s64)s[14]*tmp[9] + (s64)s[15]*tmp[13]) >> 12;
    m[14] = ((s64)s[12]*tmp[2] + (s64)s[13]*tmp[6] + (s64)s[14]*tmp[10] + (s64)s[15]*tmp[14]) >> 12;
    m[15] = ((s64)s[12]*tmp[3] + (s64)s[13]*tmp[7] + (s64)s[14]*tmp[11] + (s64)s[15]*tmp[15]) >> 12;
}

void MatrixMult4x3(s32* m, s32* s)
{
    s32 tmp[16];
    memcpy(tmp, m, 16*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11]) >> 12;

    m[4] = ((s64)s[3]*tmp[0] + (s64)s[4]*tmp[4] + (s64)s[5]*tmp[8]) >> 12;
    m[5] = ((s64)s[3]*tmp[1] + (s64)s[4]*tmp[5] + (s64)s[5]*tmp[9]) >> 12;
    m[6] = ((s64)s[3]*tmp[2] + (s64)s[4]*tmp[6] + (s64)s[5]*tmp[10]) >> 12;
    m[7] = ((s64)s[3]*tmp[3] + (s64)s[4]*tmp[7] + (s64)s[5]*tmp[11]) >> 12;

    m[8] = ((s64)s[6]*tmp[0] + (s64)s[7]*tmp[4] + (s64)s[8]*tmp[8]) >> 12;
    m[9] = ((s64)s[6]*tmp[1] + (s64)s[7]*tmp[5] + (s64)s[8]*tmp[9]) >> 12;
    m[10] = ((s64)s[6]*tmp[2] + (s64)s[7]*tmp[6] + (s64)s[8]*tmp[10]) >> 12;
    m[11] = ((s64)s[6]*tmp[3] + (s64)s[7]*tmp[7] + (s64)s[8]*tmp[11]) >> 12;

    m[12] = ((s64)s[9]*tmp[0] + (s64)s[10]*tmp[4] + (s64)s[11]*tmp[8] + (s64)0x1000*tmp[12]) >> 12;
    m[13] = ((s64)s[9]*tmp[1] + (s64)s[10]*tmp[5] + (s64)s[11]*tmp[9] + (s64)0x1000*tmp[13]) >> 12;
    m[14] = ((s64)s[9]*tmp[2] + (s64)s[10]*tmp[6] + (s64)s[11]*tmp[10] + (s64)0x1000*tmp[14]) >> 12;
    m[15] = ((s64)s[9]*tmp[3] + (s64)s[10]*tmp[7] + (s64)s[11]*tmp[11] + (s64)0x1000*tmp[15]) >> 12;
}

void MatrixMult3x3(s32* m, s32* s)
{
    s32 tmp[12];
    memcpy(tmp, m, 12*4);

    // m = s*m
    m[0] = ((s64)s[0]*tmp[0] + (s64)s[1]*tmp[4] + (s64)s[2]*tmp[8]) >> 12;
    m[1] = ((s64)s[0]*tmp[1] + (s64)s[1]*tmp[5] + (s64)s[2]*tmp[9]) >> 12;
    m[2] = ((s64)s[0]*tmp[2] + (s64)s[1]*tmp[6] + (s64)s[2]*tmp[10]) >> 12;
    m[3] = ((s64)s[0]*tmp[3] + (s64)s[1]*tmp[7] + (s64)s[2]*tmp[11]) >> 12;

    m[4] = ((s64)s[3]*tmp[0] + (s64)s[4]*tmp[4] + (s64)s[5]*tmp[8]) >> 12;
    m[5] = ((s64)s[3]*tmp[1] + (s64)s[4]*tmp[5] + (s64)s[5]*tmp[9]) >> 12;
    m[6] = ((s64)s[3]*tmp[2] + (s64)s[4]*tmp[6] + (s64)s[5]*tmp[10]) >> 12;
    m[7] = ((s64)s[3]*tmp[3] + (s64)s[4]*tmp[7] + (s64)s[5]*tmp[11]) >> 12;

    m[8] = ((s64)s[6]*tmp[0] + (s64)s[7]*tmp[4] + (s64)s[8]*tmp[8]) >> 12;
    m[9] = ((s64)s[6]*tmp[1] + (s64)s[7]*tmp[5] + (s64)s[8]*tmp[9]) >> 12;
    m[10] = ((s64)s[6]*tmp[2] + (s64)s[7]*tmp[6] + (s64)s[8]*tmp[10]) >> 12;
    m[11] = ((s64)s[6]*tmp[3] + (s64)s[7]*tmp[7] + (s64)s[8]*tmp[11]) >> 12;
}

void MatrixScale(s32* m, s32* s)
{
    m[0] = ((s64)s[0]*m[0]) >> 12;
    m[1] = ((s64)s[0]*m[1]) >> 12;
    m[2] = ((s64)s[0]*m[2]) >> 12;
    m[3] = ((s64)s[0]*m[3]) >> 12;

    m[4] = ((s64)s[1]*m[4]) >> 12;
    m[5] = ((s64)s[1]*m[5]) >> 12;
    m[6] = ((s64)s[1]*m[6]) >> 12;
    m[7] = ((s64)s[1]*m[7]) >> 12;

    m[8] = ((s64)s[2]*m[8]) >> 12;
    m[9] = ((s64)s[2]*m[9]) >> 12;
    m[10] = ((s64)s[2]*m[10]) >> 12;
    m[11] = ((s64)s[2]*m[11]) >> 12;
}

void MatrixTranslate(s32* m, s32* s)
{
    m[12] += ((s64)s[0]*m[0] + (s64)s[1]*m[4] + (s64)s[2]*m[8]) >> 12;
    m[13] += ((s64)s[0]*m[1] + (s64)s[1]*m[5] + (s64)s[2]*m[9]) >> 12;
    m[14] += ((s64)s[0]*m[2] + (s64)s[1]*m[6] + (s64)s[2]*m[10]) >> 12;
    m[15] += ((s64)s[0]*m[3] + (s64)s[1]*m[7] + (s64)s[2]*m[11]) >> 12;
}

void GPU3D::UpdateClipMatrix() noexcept
{
    if (!ClipMatrixDirty) return;
    ClipMatrixDirty = false;
    FrameProjectionCheck = true; // Pomegrade: see SubmitVertex

    memcpy(ClipMatrix, ProjMatrix, 16*4);
    MatrixMult4x4(ClipMatrix, PosMatrix);
}



void GPU3D::AddCycles(s32 num) noexcept
{
    CycleCount += num;

    if (VertexPipeline > 0)
    {
        if (VertexPipeline > num) VertexPipeline -= num;
        else                      VertexPipeline = 0;
    }

    if (PolygonPipeline > 0)
    {
        if (PolygonPipeline > num)
        {
            PolygonPipeline -= num;
            VertexSlotCounter += num;
            while (VertexSlotCounter > 9)
            {
                VertexSlotCounter -= 9;
                VertexSlotsFree >>= 1;
            }
        }
        else
        {
            PolygonPipeline = 0;
            VertexSlotCounter = 0;
            VertexSlotsFree = 0x1;
        }
    }
}

void GPU3D::NextVertexSlot() noexcept
{
    s32 num = (9 - VertexSlotCounter) + 1;

    for (;;)
    {
        CycleCount += num;

        if (VertexPipeline > 0)
        {
            if (VertexPipeline > num) VertexPipeline -= num;
            else                      VertexPipeline = 0;
        }

        if (PolygonPipeline > 0)
        {
            if (PolygonPipeline > num)
            {
                PolygonPipeline -= num;
                VertexSlotCounter = 1;
                VertexSlotsFree >>= 1;
                if (VertexSlotsFree & 0x1)
                {
                    VertexSlotsFree &= ~0x1;
                    break;
                }
                else
                {
                    num = 9;
                    continue;
                }
            }
            else
            {
                PolygonPipeline = 0;
                VertexSlotCounter = 0;
                VertexSlotsFree = 1;
                break;
            }
        }
    }
}

void GPU3D::StallPolygonPipeline(s32 delay, s32 nonstalldelay) noexcept
{
    if (PolygonPipeline > 0)
    {
        CycleCount += PolygonPipeline + delay;

        // can be safely assumed those two will go to zero
        VertexPipeline = 0;
        NormalPipeline = 0;

        PolygonPipeline = 0;
        VertexSlotCounter = 0;
        VertexSlotsFree = 1;
    }
    else
    {
        if (VertexPipeline > nonstalldelay)
            AddCycles((VertexPipeline - nonstalldelay) + 1);
        else
            AddCycles(NormalPipeline + 1);
    }
}



template<int comp, s32 plane, bool attribs>
void ClipSegment(Vertex* outbuf, Vertex* vin, Vertex* vout)
{
    s64 factor_num = vin->Position[3] - (plane*vin->Position[comp]);
    s32 factor_den = factor_num - (vout->Position[3] - (plane*vout->Position[comp]));

#define INTERPOLATE(var)  { outbuf->var = (vin->var + ((vout->var - vin->var) * factor_num) / factor_den); }

    if (comp != 0) INTERPOLATE(Position[0]);
    if (comp != 1) INTERPOLATE(Position[1]);
    if (comp != 2) INTERPOLATE(Position[2]);
    INTERPOLATE(Position[3]);
    outbuf->Position[comp] = plane*outbuf->Position[3];

    if (attribs)
    {
        INTERPOLATE(Color[0]);
        INTERPOLATE(Color[1]);
        INTERPOLATE(Color[2]);

        INTERPOLATE(TexCoords[0]);
        INTERPOLATE(TexCoords[1]);

        // Pomegrade view-space data (not hardware state): same parameter, linear
        // in clip space is linear in view space
        float t = (float)factor_num / factor_den;
        for (int i = 0; i < 4; i++)
            outbuf->ViewPosition[i] = vin->ViewPosition[i] + (vout->ViewPosition[i] - vin->ViewPosition[i]) * t;
        for (int i = 0; i < 3; i++)
            outbuf->ViewNormal[i] = vin->ViewNormal[i] + (vout->ViewNormal[i] - vin->ViewNormal[i]) * t;
        outbuf->HasViewNormal = vin->HasViewNormal && vout->HasViewNormal;
        outbuf->LitColor = vin->LitColor && vout->LitColor;
        outbuf->Orthographic = vin->Orthographic;
        outbuf->Specular = vin->Specular + (vout->Specular - vin->Specular) * t;
    }

    outbuf->Clipped = true;

#undef INTERPOLATE
}

template<int comp, bool attribs>
int ClipAgainstPlane(const GPU3D& gpu, Vertex* vertices, int nverts, int clipstart)
{
    Vertex temp[10];
    int prev, next;
    int c = clipstart;

    if (clipstart == 2)
    {
        temp[0] = vertices[0];
        temp[1] = vertices[1];
    }

    for (int i = clipstart; i < nverts; i++)
    {
        prev = i-1; if (prev < 0) prev = nverts-1;
        next = i+1; if (next >= nverts) next = 0;

        Vertex vtx = vertices[i];
        if (vtx.Position[comp] > vtx.Position[3])
        {
            if ((comp == 2) && (!(gpu.CurPolygonAttr & (1<<12)))) return 0;

            Vertex* vprev = &vertices[prev];
            if (vprev->Position[comp] <= vprev->Position[3])
            {
                ClipSegment<comp, 1, attribs>(&temp[c], &vtx, vprev);
                c++;
            }

            Vertex* vnext = &vertices[next];
            if (vnext->Position[comp] <= vnext->Position[3])
            {
                ClipSegment<comp, 1, attribs>(&temp[c], &vtx, vnext);
                c++;
            }
        }
        else
            temp[c++] = vtx;
    }

    nverts = c; c = clipstart;
    for (int i = clipstart; i < nverts; i++)
    {
        prev = i-1; if (prev < 0) prev = nverts-1;
        next = i+1; if (next >= nverts) next = 0;

        Vertex vtx = temp[i];
        if (vtx.Position[comp] < -vtx.Position[3])
        {
            Vertex* vprev = &temp[prev];
            if (vprev->Position[comp] >= -vprev->Position[3])
            {
                ClipSegment<comp, -1, attribs>(&vertices[c], &vtx, vprev);
                c++;
            }

            Vertex* vnext = &temp[next];
            if (vnext->Position[comp] >= -vnext->Position[3])
            {
                ClipSegment<comp, -1, attribs>(&vertices[c], &vtx, vnext);
                c++;
            }
        }
        else
            vertices[c++] = vtx;
    }

    // checkme
    for (int i = 0; i < c; i++)
    {
        Vertex* vtx = &vertices[i];

        vtx->Color[0] &= ~0xFFF; vtx->Color[0] += 0xFFF;
        vtx->Color[1] &= ~0xFFF; vtx->Color[1] += 0xFFF;
        vtx->Color[2] &= ~0xFFF; vtx->Color[2] += 0xFFF;
    }

    return c;
}

template<bool attribs>
int ClipPolygon(GPU3D& gpu, Vertex* vertices, int nverts, int clipstart)
{
    // clip.
    // for each vertex:
    // if it's outside, check if the previous and next vertices are inside
    // if so, place a new vertex at the edge of the view volume

    // TODO: check for 1-dot polygons
    // TODO: the hardware seems to use a different algorithm. it reacts differently to vertices with W=0
    // some vertices that should get Y=-0x1000 get Y=0x1000 for some reason on hardware. it doesn't make sense.
    // clipping seems to process the Y plane before the X plane.

    // Z clipping
    nverts = ClipAgainstPlane<2, attribs>(gpu, vertices, nverts, clipstart);

    // Y clipping
    nverts = ClipAgainstPlane<1, attribs>(gpu, vertices, nverts, clipstart);

    // X clipping
    nverts = ClipAgainstPlane<0, attribs>(gpu, vertices, nverts, clipstart);

    return nverts;
}

bool ClipCoordsEqual(Vertex* a, Vertex* b)
{
    return a->Position[0] == b->Position[0] &&
           a->Position[1] == b->Position[1] &&
           a->Position[2] == b->Position[2] &&
           a->Position[3] == b->Position[3];
}

static void ComputeFinalColor(Vertex* vtx, bool highcolor) noexcept
{
    if (highcolor)
    {
        // high colour: Color is 5.12 fixed point without the hardware's bias;
        // map 0..31 to the same 0..511 range, keeping the fraction
        for (int i = 0; i < 3; i++)
            vtx->FinalColor[i] = std::clamp((s32)(((s64)vtx->Color[i] * 511) / (31 << 12)), 0, 511);
        return;
    }

    vtx->FinalColor[0] = vtx->Color[0] >> 12;
    if (vtx->FinalColor[0]) vtx->FinalColor[0] = ((vtx->FinalColor[0] << 4) + 0xF);
    vtx->FinalColor[1] = vtx->Color[1] >> 12;
    if (vtx->FinalColor[1]) vtx->FinalColor[1] = ((vtx->FinalColor[1] << 4) + 0xF);
    vtx->FinalColor[2] = vtx->Color[2] >> 12;
    if (vtx->FinalColor[2]) vtx->FinalColor[2] = ((vtx->FinalColor[2] << 4) + 0xF);
}

void GPU3D::FinalizePolygon(Polygon* poly, int nverts) const noexcept
{
    // determine bounds of the polygon
    // also determine the W shift and normalize W
    // normalization works both ways
    // (ie two W's that span 12 bits or less will be brought to 16 bits)

    u32 vtop = 0, vbot = 0;
    s32 ytop = 192, ybot = 0;
    s32 xtop = 256, xbot = 0;
    u32 wsize = 0;

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = poly->Vertices[i];

        if (vtx->FinalPosition[1] < ytop)
        {
            xtop = vtx->FinalPosition[0];
            ytop = vtx->FinalPosition[1];
            vtop = i;
        }
        if (vtx->FinalPosition[1] > ybot || (vtx->FinalPosition[1] == ybot && vtx->FinalPosition[0] > xbot))
        {
            xbot = vtx->FinalPosition[0];
            ybot = vtx->FinalPosition[1];
            vbot = i;
        }

        u32 w = (u32)vtx->Position[3];
        if (w == 0) poly->Degenerate = true;

        while ((w >> wsize) && (wsize < 32))
            wsize += 4;
    }

    poly->VTop = vtop; poly->VBottom = vbot;
    poly->YTop = ytop; poly->YBottom = ybot;
    poly->XTop = xtop; poly->XBottom = xbot;

    if (ybot > 192) poly->Degenerate = true;

    poly->SortKey = (ybot << 8) | ytop;
    if (poly->Translucent) poly->SortKey |= 0x10000;

    poly->WBuffer = (FlushAttributes & 0x2);

    for (int i = 0; i < nverts; i++)
    {
        Vertex* vtx = poly->Vertices[i];
        s32 w, wshifted;

        // W is normalized, such that all the polygon's W values fit within 16 bits
        // the viewport transform for X/Y/Z uses the original W values, but
        // when W-buffering is used, the normalized W is used
        // W normalization is applied to separate polygons, even within strips

        if (wsize < 16)
        {
            w = vtx->Position[3] << (16 - wsize);
            wshifted = w >> (16 - wsize);
        }
        else
        {
            w = vtx->Position[3] >> (wsize - 16);
            wshifted = w << (wsize - 16);
        }

        s32 z;
        if (FlushAttributes & 0x2)
            z = wshifted;
        else if (vtx->Position[3])
            z = ((((s64)vtx->Position[2] * 0x4000) / vtx->Position[3]) + 0x3FFF) * 0x200;
        else
            z = 0x7FFE00;

        // checkme (Z<0 shouldn't be possible, but Z>0xFFFFFF is possible)
        if (z < 0) z = 0;
        else if (z > 0xFFFFFF) z = 0xFFFFFF;

        poly->FinalZ[i] = z;
        poly->FinalW[i] = w;
    }
}

void GPU3D::ComputeScreenPosition(Vertex* vtx) const noexcept
{
    // W is truncated to 24 bits at this point
    // if this W is zero, the polygon isn't rendered
    vtx->Position[3] &= 0x00FFFFFF;

    // viewport transform
    // note: the DS performs these divisions using a 32-bit divider
    // thus, if W is greater than 0xFFFF, some precision is sacrificed
    // to make the numbers fit into the divider
    u32 posX, posY;
    u32 w = vtx->Position[3];
    if (w == 0)
    {
        posX = 0;
        posY = 0;
    }
    else
    {
        posX = vtx->Position[0] + w;
        posY = -vtx->Position[1] + w;
        u32 den = w;

        if (w > 0xFFFF)
        {
            posX >>= 1;
            posY >>= 1;
            den  >>= 1;
        }

        den <<= 1;
        posX = ((posX * Viewport[4]) / den) + Viewport[0];
        posY = ((posY * Viewport[5]) / den) + Viewport[3];
    }

    vtx->FinalPosition[0] = posX & 0x1FF;
    vtx->FinalPosition[1] = posY & 0xFF;

    // high-precision geometry: the same viewport transform, without rounding
    if (w != 0)
    {
        vtx->PreciseScreen[0] = (float)((double)(vtx->Position[0] + (s32)w) * Viewport[4] / (2.0 * w) + Viewport[0]);
        vtx->PreciseScreen[1] = (float)((double)(-vtx->Position[1] + (s32)w) * Viewport[5] / (2.0 * w) + Viewport[3]);
    }
    else
    {
        vtx->PreciseScreen[0] = 0;
        vtx->PreciseScreen[1] = 0;
    }

    // hi-res positions
    // to consider: only do this when using the GL renderer? apply the aforementioned quirk to this?
    if (w != 0)
    {
        posX = ((((s64)(vtx->Position[0] + w) * Viewport[4]) << 4) / (((s64)w) << 1)) + (Viewport[0] << 4);
        posY = ((((s64)(-vtx->Position[1] + w) * Viewport[5]) << 4) / (((s64)w) << 1)) + (Viewport[3] << 4);

        vtx->HiresPosition[0] = posX & 0x1FFF;
        vtx->HiresPosition[1] = posY & 0xFFF;
    }
}

void GPU3D::SubmitPolygon() noexcept
{
    Vertex clippedvertices[10];
    Vertex* reusedvertices[2];
    int clipstart = 0;
    int lastpolyverts = 0;

    int nverts = PolygonMode & 0x1 ? 4:3;
    const int srcverts = nverts;
    int prev, next;

    // submitting a polygon starts the polygon pipeline
    // noting that for now we are only reserving one vertex slot
    // further slots only get reserved if the polygon makes it through culling/clipping
    PolygonPipeline = 8;
    VertexSlotCounter = 1;
    VertexSlotsFree = 0b11110;

    // polygon multiplier: every face counts for the angles between faces,
    // including those culled or clipped away below
    if (PolygonMultiplierLevel > 1)
        RegisterEdgeFaces(nverts);
    if (ViewDataRequested)
        RecordShadowCaster(nverts);
    const u32 frameId = (PolygonSubmitCount++ << 8) | 0xFF;

    // culling
    // TODO: work out how it works on the real thing
    // the normalization part is a wild guess

    Vertex *v0, *v1, *v2, *v3;
    s64 normalX, normalY, normalZ;
    s64 dot;

    v0 = &TempVertexBuffer[0];
    v1 = &TempVertexBuffer[1];
    v2 = &TempVertexBuffer[2];
    v3 = &TempVertexBuffer[3];

    normalX = ((s64)(v0->Position[1]-v1->Position[1]) * (v2->Position[3]-v1->Position[3]))
        - ((s64)(v0->Position[3]-v1->Position[3]) * (v2->Position[1]-v1->Position[1]));
    normalY = ((s64)(v0->Position[3]-v1->Position[3]) * (v2->Position[0]-v1->Position[0]))
        - ((s64)(v0->Position[0]-v1->Position[0]) * (v2->Position[3]-v1->Position[3]));
    normalZ = ((s64)(v0->Position[0]-v1->Position[0]) * (v2->Position[1]-v1->Position[1]))
        - ((s64)(v0->Position[1]-v1->Position[1]) * (v2->Position[0]-v1->Position[0]));

    while ((((normalX>>31) ^ (normalX>>63)) != 0) ||
           (((normalY>>31) ^ (normalY>>63)) != 0) ||
           (((normalZ>>31) ^ (normalZ>>63)) != 0))
    {
        normalX >>= 4;
        normalY >>= 4;
        normalZ >>= 4;
    }

    dot = ((s64)v1->Position[0] * normalX) + ((s64)v1->Position[1] * normalY) + ((s64)v1->Position[3] * normalZ);

    bool facingview = (dot <= 0);

    if (dot < 0)
    {
        if (!(CurPolygonAttr & (1<<7)))
        {
            LastStripPolygon = NULL;
            return;
        }
    }
    else if (dot > 0)
    {
        if (!(CurPolygonAttr & (1<<6)))
        {
            LastStripPolygon = NULL;
            return;
        }
    }

    // for strips, check whether we can attach to the previous polygon
    // this requires two original vertices shared with the previous polygon, and that
    // the two polygons be of the same type

    if (PolygonMode >= 2 && LastStripPolygon)
    {
        int id0, id1;
        if (PolygonMode == 2)
        {
            if (NumConsecutivePolygons & 1)
            {
                id0 = 2;
                id1 = 1;
            }
            else
            {
                id0 = 0;
                id1 = 2;
            }

            lastpolyverts = 3;
        }
        else
        {
            id0 = 3;
            id1 = 2;

            lastpolyverts = 4;
        }

        if (LastStripPolygon->NumVertices == lastpolyverts &&
            !LastStripPolygon->Vertices[id0]->Clipped &&
            !LastStripPolygon->Vertices[id1]->Clipped)
        {
            reusedvertices[0] = LastStripPolygon->Vertices[id0];
            reusedvertices[1] = LastStripPolygon->Vertices[id1];

            clippedvertices[0] = *reusedvertices[0];
            clippedvertices[1] = *reusedvertices[1];

            clipstart = 2;
        }
    }

    for (int i = clipstart; i < nverts; i++)
        clippedvertices[i] = TempVertexBuffer[i];

    // detect lines, for the OpenGL renderer

    int polytype = 0;
    if (nverts == 3)
    {
        if (ClipCoordsEqual(&clippedvertices[0], &clippedvertices[1]) ||
            ClipCoordsEqual(&clippedvertices[0], &clippedvertices[2]) ||
            ClipCoordsEqual(&clippedvertices[1], &clippedvertices[2]))
        {
            polytype = 1;
        }
    }
    else if (nverts == 4)
    {
        // TODO
    }

    // clipping

    nverts = ClipPolygon<true>(*this, clippedvertices, nverts, clipstart);
    if (nverts == 0)
    {
        LastStripPolygon = NULL;
        return;
    }

    // reject the polygon if it's not going to fit in polygon/vertex RAM

    // Pomegrade: with the polygon limit removed, such a polygon is still drawn,
    // from extra storage. The game sees the hardware behaviour either way: the
    // overflow flag, the RAM counters and the pipeline timing of a rejected polygon.
    bool extra = false;
    if (NumPolygons >= 2048 || NumVertices+nverts > 6144)
    {
        DispCnt |= (1<<13);
        if (!UnlimitedPolygons || NumSubPolygons >= MaxSubPolygons || NumSubVertices + nverts > MaxSubVertices)
        {
            LastStripPolygon = NULL;
            return;
        }
        extra = true;
    }

    // compute screen coordinates

    for (int i = clipstart; i < nverts; i++)
        ComputeScreenPosition(&clippedvertices[i]);

    // zero-dot W check:
    // * if the polygon's vertices all have the same screen coordinates, it is considered to be zero-dot
    // * if all the vertices have a W greater than the threshold defined in register 0x04000610,
    //   the polygon is rejected, unless bit13 in the polygon attributes is set

    if (!(CurPolygonAttr & (1<<13)))
    {
        bool zerodot = true;
        bool allbehind = true;

        for (int i = 0; i < nverts; i++)
        {
            Vertex* vtx = &clippedvertices[i];

            if (vtx->FinalPosition[0] != clippedvertices[0].FinalPosition[0] ||
                vtx->FinalPosition[1] != clippedvertices[0].FinalPosition[1])
            {
                zerodot = false;
                break;
            }

            if (vtx->Position[3] <= ZeroDotWLimit)
            {
                allbehind = false;
                break;
            }
        }

        if (zerodot && allbehind)
        {
            LastStripPolygon = NULL;
            return;
        }
    }

    // build the actual polygon

    if (extra)
    {
        // timing of a rejected polygon: nothing more
    }
    else if (nverts == 4)
    {
        PolygonPipeline = 35;
        VertexSlotCounter = 1;
        if (PolygonMode & 0x2) VertexSlotsFree = 0b11100;
        else                   VertexSlotsFree = 0b11110;
    }
    else
    {
        PolygonPipeline = 26;
        VertexSlotCounter = 1;
        if (PolygonMode & 0x2) VertexSlotsFree = 0b1000;
        else                   VertexSlotsFree = 0b1110;
    }

    Polygon* poly = extra ? NewSubPolygon() : &CurPolygonRAM[NumPolygons++];
    poly->NumVertices = 0;
    poly->FrameId = frameId;

    poly->Attr = CurPolygonAttr;
    poly->TexParam = TexParam;
    poly->TexPalette = TexPalette;

    poly->Degenerate = false;
    poly->Type = 0;

    poly->FacingView = facingview;

    u32 texfmt = (TexParam >> 26) & 0x7;
    u32 polyalpha = (CurPolygonAttr >> 16) & 0x1F;
    poly->Translucent = (texfmt == 1 || texfmt == 6) || (polyalpha > 0 && polyalpha < 31);

    poly->IsShadowMask = ((CurPolygonAttr & 0x3F000030) == 0x00000030);
    poly->IsShadow = ((CurPolygonAttr & 0x30) == 0x30) && !poly->IsShadowMask;

    if (!poly->Translucent && !extra) NumOpaquePolygons++;

    poly->Type = polytype;

    if (LastStripPolygon && clipstart > 0)
    {
        if (nverts == lastpolyverts)
        {
            poly->Vertices[0] = reusedvertices[0];
            poly->Vertices[1] = reusedvertices[1];
        }
        else
        {
            Vertex v0 = *reusedvertices[0];
            Vertex v1 = *reusedvertices[1];

            if (extra)
            {
                poly->Vertices[0] = NewSubVertex();
                *poly->Vertices[0] = v0;
                poly->Vertices[1] = NewSubVertex();
                *poly->Vertices[1] = v1;
            }
            else
            {
                CurVertexRAM[NumVertices] = v0;
                poly->Vertices[0] = &CurVertexRAM[NumVertices];
                CurVertexRAM[NumVertices+1] = v1;
                poly->Vertices[1] = &CurVertexRAM[NumVertices+1];
                NumVertices += 2;
            }
        }

        poly->NumVertices += 2;
    }

    for (int i = clipstart; i < nverts; i++)
    {
        Vertex* vtx = extra ? NewSubVertex() : &CurVertexRAM[NumVertices++];
        *vtx = clippedvertices[i];
        poly->Vertices[i] = vtx;

        poly->NumVertices++;

        ComputeFinalColor(vtx, HighColor);
    }

    FinalizePolygon(poly, nverts);

    if (PolygonMode >= 2)
        LastStripPolygon = poly;
    else
        LastStripPolygon = NULL;

    if (extra)
        ExtraPolygons[CurRAMBank].push_back(poly);

    poly->SubPolygonCount = 0;
    if (PolygonMultiplierLevel > 1)
        MultiplyPolygon(poly, srcverts);
}

void GPU3D::SetUnlimitedPolygons(bool enable) noexcept
{
    UnlimitedPolygons = enable;
}

void GPU3D::SetPolygonMultiplier(int level) noexcept
{
    if (level < 1) level = 1;
    if (level > PolygonMultiplier::MaxLevel) level = PolygonMultiplier::MaxLevel;

    PolygonMultiplierLevel = level;
}

void GPU3D::ClearSubPolygons() noexcept
{
    NumSubVertices = 0;
    NumSubPolygons = 0;
    ExtraPolygons[0].clear();
    ExtraPolygons[1].clear();
    for (Polygon& poly : PolygonRAM)
        poly.SubPolygonCount = 0;
}

Vertex* GPU3D::NewSubVertex() noexcept
{
    if (NumSubVertices >= MaxSubVertices)
        return nullptr;
    auto& blocks = SubVertexBlocks[CurRAMBank];
    u32 block = NumSubVertices / SubBlockSize;
    if (block >= blocks.size())
        blocks.push_back(std::make_unique<Vertex[]>(SubBlockSize));
    return &blocks[block][NumSubVertices++ % SubBlockSize];
}

Polygon* GPU3D::NewSubPolygon() noexcept
{
    if (NumSubPolygons >= MaxSubPolygons)
        return nullptr;
    auto& blocks = SubPolygonBlocks[CurRAMBank];
    u32 block = NumSubPolygons / SubBlockSize;
    if (block >= blocks.size())
        blocks.push_back(std::make_unique<Polygon[]>(SubBlockSize));
    return &blocks[block][NumSubPolygons++ % SubBlockSize];
}

void GPU3D::MultiplyPolygon(Polygon* parent, int nverts) noexcept
{
    // lines and shadow volumes keep their exact hardware shape
    if (parent->Type != 0 || parent->IsShadowMask || parent->IsShadow)
        return;

    // only polygons whose every corner has a normal (lit geometry); UI and
    // other unlit polygons have none and are left alone
    MultiplierVertex corners[4];
    // new points are lit with the DS lighting (same lights and material) when
    // every corner's colour came from it; otherwise colours are interpolated
    bool relight = (parent->Attr & 0xF) != 0;
    double normallength = 0;
    for (int i = 0; i < nverts; i++)
    {
        const Vertex& src = TempVertexBuffer[i];
        if (!src.HasViewNormal)
            return;
        relight = relight && src.LitColor;
        normallength += std::sqrt((double)src.ViewNormal[0]*src.ViewNormal[0] + (double)src.ViewNormal[1]*src.ViewNormal[1] +
                                  (double)src.ViewNormal[2]*src.ViewNormal[2]);
        // a projective position matrix (W varying) isn't handled
        if (src.ViewPosition[3] != TempVertexBuffer[0].ViewPosition[3])
            return;

        MultiplierVertex& c = corners[i];
        for (int k = 0; k < 3; k++)
        {
            c.Position[k] = src.ViewPosition[k];
            c.Normal[k] = src.ViewNormal[k];
            c.Color[k] = src.Color[k];
        }
        c.TexCoords[0] = src.TexCoords[0];
        c.TexCoords[1] = src.TexCoords[1];
        if (!PolygonMultiplier::NormalizeNormal(c))
            return;
    }

    // geometry fidelity from neighbours: share of curvature kept along each
    // edge, from earlier frames (see RegisterEdgeFaces)
    double edgeKeep[4];
    for (int i = 0; i < nverts; i++)
    {
        auto known = EdgeKeep.find(EdgeKey(i, nverts));
        edgeKeep[i] = known != EdgeKeep.end() ? known->second : 1.0;
    }

    if (!PolygonMultiplier::IsCurved(corners, nverts))
        return;

    // normals must all point to the same side of the polygon, otherwise
    // (inverted or hand-tweaked normals) the surface would fold
    {
        const double* p0 = corners[0].Position;
        const double* p1 = corners[1].Position;
        const double* p2 = corners[2].Position;
        const double* p3 = corners[nverts - 1].Position;
        // for quads, the cross product of the diagonals
        double e1[3], e2[3];
        for (int k = 0; k < 3; k++)
        {
            e1[k] = (nverts == 4 ? p2[k] - p0[k] : p1[k] - p0[k]);
            e2[k] = (nverts == 4 ? p3[k] - p1[k] : p2[k] - p0[k]);
        }
        double face[3] = {e1[1]*e2[2] - e1[2]*e2[1], e1[2]*e2[0] - e1[0]*e2[2], e1[0]*e2[1] - e1[1]*e2[0]};
        int positive = 0;
        for (int i = 0; i < nverts; i++)
        {
            const double* n = corners[i].Normal;
            double dot = n[0]*face[0] + n[1]*face[1] + n[2]*face[2];
            if (dot > 0) positive++;
            else if (dot == 0) return;
        }
        if (positive != 0 && positive != nverts)
            return;
    }

    const int level = PolygonMultiplierLevel;
    const int numtris = nverts - 2;
    if (NumSubPolygons + numtris*level*level > MaxSubPolygons)
        return;

    const float vieww = TempVertexBuffer[0].ViewPosition[3];
    // scale of the lighting's transformed normal: (normal * vector matrix) >> 12
    normallength = normallength / nverts / 4096.0;
    const u32 firstpoly = NumSubPolygons;
    const u32 firstvert = NumSubVertices;

    MultiplierVertex grid[PolygonMultiplier::MaxGridPoints];
    // each point of the subdivision as a vertex, computed once: inner points are
    // shared by six sub-triangles, edge points by up to three
    Vertex points[PolygonMultiplier::MaxGridPoints];
    bool inside[PolygonMultiplier::MaxGridPoints];
    // a point's finished vertex (screen position, final colour), shared by the
    // sub-triangles that need no clipping; created when first used
    Vertex* finished[PolygonMultiplier::MaxGridPoints];
    const int numpoints = (level + 1) * (level + 2) / 2;
    for (int t = 0; t < numtris; t++)
    {
        // quads are split along the 0-2 diagonal
        int a = 0, b = t + 1, c = t + 2;
        // the polygon's edges are 0-1, 1-2 ... (n-1)-0; a quad's diagonal is
        // inside it, curved only as much as its least curved edge
        double inside_keep = std::min({edgeKeep[0], edgeKeep[1], edgeKeep[2], edgeKeep[nverts - 1]});
        const double keep[3] = {b == 1 ? edgeKeep[0] : inside_keep,
                                edgeKeep[b],
                                c == nverts - 1 ? edgeKeep[nverts - 1] : inside_keep};
        PolygonMultiplier::SubdivideGrid(corners[a], corners[b], corners[c], level, grid,
                                         CurveMethod::PNhong, keep);

        for (int g = 0; g < numpoints; g++)
        {
            const MultiplierVertex& mv = grid[g];
            Vertex& out = points[g];
            out = {};

            // corners keep the hardware's exact clip coordinates, so edges shared
            // with polygons that aren't multiplied line up
            int corner = -1;
            const int ids[3] = {a, b, c};
            for (int id : ids)
            {
                if (mv.Position[0] == corners[id].Position[0] &&
                    mv.Position[1] == corners[id].Position[1] &&
                    mv.Position[2] == corners[id].Position[2])
                    corner = id;
            }

            if (corner >= 0)
            {
                for (int i = 0; i < 4; i++)
                    out.Position[i] = TempVertexBuffer[corner].Position[i];
            }
            else
            {
                const double view[4] = {mv.Position[0], mv.Position[1], mv.Position[2], vieww};
                for (int i = 0; i < 4; i++)
                {
                    double clip = view[0]*ProjMatrix[i] + view[1]*ProjMatrix[4+i] + view[2]*ProjMatrix[8+i] + view[3]*ProjMatrix[12+i];
                    out.Position[i] = (s32)std::floor(clip / 4096.0);
                }
            }

            if (corner >= 0)
            {
                for (int i = 0; i < 3; i++)
                    out.Color[i] = TempVertexBuffer[corner].Color[i];
            }
            else if (relight)
            {
                s32 normaltrans[3];
                for (int i = 0; i < 3; i++)
                    normaltrans[i] = (s32)std::lround(mv.Normal[i] * normallength);
                u8 color[3];
                s32 precise[3];
                LightVertex(normaltrans, color, precise);
                for (int i = 0; i < 3; i++)
                    out.Color[i] = HighColor ? precise[i] : (color[i] << 12) + 0xFFF;
            }
            else
            {
                for (int i = 0; i < 3; i++)
                    out.Color[i] = (s32)std::lround(mv.Color[i]);
            }
            out.TexCoords[0] = (s16)std::lround(mv.TexCoords[0]);
            out.TexCoords[1] = (s16)std::lround(mv.TexCoords[1]);
            out.Clipped = false;
            // view-space data for the renderer's effects
            if (corner >= 0)
            {
                const Vertex& src = TempVertexBuffer[corner];
                for (int i = 0; i < 4; i++) out.ViewPosition[i] = src.ViewPosition[i];
            }
            else
            {
                for (int i = 0; i < 3; i++) out.ViewPosition[i] = (float)mv.Position[i];
                out.ViewPosition[3] = vieww;
            }
            for (int i = 0; i < 3; i++) out.ViewNormal[i] = (float)mv.Normal[i];
            out.HasViewNormal = true;
            out.Orthographic = TempVertexBuffer[0].Orthographic;
            out.Specular = TempVertexBuffer[0].Specular;

            const s32 w = out.Position[3];
            inside[g] = true;
            for (int i = 0; i < 3; i++)
                inside[g] = inside[g] && out.Position[i] <= w && out.Position[i] >= -w;
            finished[g] = nullptr;
        }

        int s = 0;
        bool full = false;
        PolygonMultiplier::ForEachGridTriangle(level, [&](int p0, int p1, int p2) {
            // its place in the subdivision, the same whether or not earlier
            // sub-triangles were clipped away (at most 2 x 8 x 8 = 128)
            const u32 place = (t * level * level + s++) & 0xFF;
            if (full)
                return;
            const int ids[3] = {p0, p1, p2};
            Vertex* vertices[10];
            int nv;

            if (inside[p0] && inside[p1] && inside[p2])
            {
                // inside the view volume: ClipPolygon would return the three
                // vertices as they are, apart from its colour rounding (below)
                int needed = 0;
                for (int id : ids)
                    needed += finished[id] ? 0 : 1;
                if (NumSubPolygons >= MaxSubPolygons || NumSubVertices + needed > MaxSubVertices)
                {
                    full = true;
                    return;
                }
                for (int k = 0; k < 3; k++)
                {
                    Vertex*& vtx = finished[ids[k]];
                    if (!vtx)
                    {
                        vtx = NewSubVertex();
                        *vtx = points[ids[k]];
                        for (int i = 0; i < 3; i++)
                        {
                            vtx->Color[i] &= ~0xFFF;
                            vtx->Color[i] += 0xFFF;
                        }
                        ComputeScreenPosition(vtx);
                        ComputeFinalColor(vtx, HighColor);
                    }
                    vertices[k] = vtx;
                }
                nv = 3;
            }
            else
            {
                Vertex clipped[10];
                for (int k = 0; k < 3; k++)
                    clipped[k] = points[ids[k]];
                nv = ClipPolygon<true>(*this, clipped, 3, 0);
                if (nv == 0)
                    return;
                if (NumSubPolygons >= MaxSubPolygons || NumSubVertices + nv > MaxSubVertices)
                {
                    full = true;
                    return;
                }
                for (int i = 0; i < nv; i++)
                {
                    Vertex* vtx = NewSubVertex();
                    *vtx = clipped[i];
                    ComputeScreenPosition(vtx);
                    ComputeFinalColor(vtx, HighColor);
                    vertices[i] = vtx;
                }
            }

            Polygon* poly = NewSubPolygon();
            for (int i = 0; i < nv; i++)
                poly->Vertices[i] = vertices[i];
            poly->NumVertices = nv;

            poly->Attr = parent->Attr;
            poly->TexParam = parent->TexParam;
            poly->TexPalette = parent->TexPalette;
            poly->Degenerate = false;
            poly->Type = 0;
            poly->FacingView = parent->FacingView;
            poly->Translucent = parent->Translucent;
            poly->IsShadowMask = false;
            poly->IsShadow = false;
            poly->FrameId = (parent->FrameId & ~0xFFu) | place;

            FinalizePolygon(poly, nv);
        });

        if (full)
        {
            // out of room: draw the parent polygon as is
            NumSubPolygons = firstpoly;
            NumSubVertices = firstvert;
            return;
        }
    }

    parent->SubPolygonStart = firstpoly;
    parent->SubPolygonCount = NumSubPolygons - firstpoly;
}

u64 GPU3D::EdgeKey(int corner, int nverts) const noexcept
{
    // the edge from this corner to the next, by its corners' model positions
    // (48 bits each, in either order) and the polygon's texture, mixed into
    // one key. Model coordinates alone are shared by unrelated meshes (bones
    // and objects in their own local space); the texture separates most.
    const s16* ma = TempVertexBuffer[corner].ModelPosition;
    const s16* mb = TempVertexBuffer[(corner + 1) % nverts].ModelPosition;
    u64 ka = ((u64)(u16)ma[0] << 32) | ((u64)(u16)ma[1] << 16) | (u16)ma[2];
    u64 kb = ((u64)(u16)mb[0] << 32) | ((u64)(u16)mb[1] << 16) | (u16)mb[2];
    if (ka > kb) std::swap(ka, kb);
    u64 texture = ((u64)TexPalette << 32) | TexParam;
    u64 key = ka * 0x9E3779B97F4A7C15ull ^ (kb + 0x632BE59BD9B4E019ull + (ka << 6) + (ka >> 2));
    return key ^ (texture * 0xC2B2AE3D27D4EB4Full + (key << 6) + (key >> 2));
}

// Shadows come from the scene, not from what the camera sees: each polygon
// is recorded as the game submits it, before the DS discards the faces turned
// away from the camera (half of each character, which half depending on the
// camera's angle) and clips what lies outside the view (objects off screen
// still shadow what is on screen). Only what the shadow map may draw: opaque
// polygons the DS lights, in perspective (see GLRenderer::RenderShadowMap).
// Geometry the game never submits (culled by the game itself) casts nothing.
void GPU3D::RecordShadowCaster(int nverts)
{
    u32 attr = CurPolygonAttr;
    u32 texfmt = (TexParam >> 26) & 0x7;
    if (!(attr & 0xF) || ((attr >> 4) & 0x3) == 3 || ((attr >> 16) & 0x1F) != 31 || texfmt == 1 || texfmt == 6)
        return;
    float normal[3] = {};
    for (int i = 0; i < nverts; i++)
    {
        const Vertex& v = TempVertexBuffer[i];
        if (v.Orthographic || !v.HasViewNormal)
            return;
        for (int c = 0; c < 3; c++) normal[c] += v.ViewNormal[c];
    }
    // quads as two triangles (0 1 2, 0 2 3)
    static constexpr int order[2][3] = {{0, 1, 2}, {0, 2, 3}};
    for (int t = 0; t < nverts - 2; t++)
    {
        for (int k : order[t])
            for (int c = 0; c < 3; c++)
                ShadowCasters.push_back(TempVertexBuffer[k].ViewPosition[c]);
        for (int c = 0; c < 3; c++)
            ShadowCasters.push_back(normal[c]);
    }
}

void GPU3D::RegisterEdgeFaces(int nverts) noexcept
{
    // only lit geometry with view data (what the multiplier curves)
    double pos[4][3], avg[3] = {};
    for (int i = 0; i < nverts; i++)
    {
        const Vertex& v = TempVertexBuffer[i];
        if (!v.HasViewNormal)
            return;
        for (int k = 0; k < 3; k++)
        {
            pos[i][k] = v.ViewPosition[k];
            avg[k] += v.ViewNormal[k];
        }
    }

    // the face's normal (a quad's: across its diagonals), on the side its
    // normals point to, whatever the winding
    double e1[3], e2[3];
    for (int k = 0; k < 3; k++)
    {
        e1[k] = (nverts == 4 ? pos[2][k] - pos[0][k] : pos[1][k] - pos[0][k]);
        e2[k] = (nverts == 4 ? pos[3][k] - pos[1][k] : pos[2][k] - pos[0][k]);
    }
    double face[3] = {e1[1]*e2[2] - e1[2]*e2[1], e1[2]*e2[0] - e1[0]*e2[2], e1[0]*e2[1] - e1[1]*e2[0]};
    double len = std::sqrt(face[0]*face[0] + face[1]*face[1] + face[2]*face[2]);
    if (len == 0)
        return;
    if (face[0]*avg[0] + face[1]*avg[1] + face[2]*avg[2] < 0) len = -len;
    for (int k = 0; k < 3; k++) face[k] /= len;

    for (int i = 0; i < nverts; i++)
    {
        u64 key = EdgeKey(i, nverts);
        // faces pair up two by two: one mesh gives one pair per edge, the
        // same mesh drawn n times n pairs that agree (see UpdateEdgeKeep)
        EdgeFace& f = EdgeFaces[key];
        if (f.Faces % 2 == 0)
        {
            for (int k = 0; k < 3; k++) f.Normal[k] = (float)face[k];
        }
        else
        {
            double cosAngle = f.Normal[0]*face[0] + f.Normal[1]*face[1] + f.Normal[2]*face[2];
            float keep = (float)PolygonMultiplier::DihedralKeep(cosAngle);
            if (f.Faces == 1) f.Keep = keep;
            else if (std::fabs(keep - f.Keep) > 0.1f) f.Conflict = true;
        }
        if (f.Faces < 255) f.Faces++;
    }
}

void GPU3D::UpdateEdgeKeep() noexcept
{
    if (EdgeFaces.empty())
        return;
    // bounded: models come and go
    if (EdgeKeep.size() > (1u << 20))
        EdgeKeep.clear();
    for (const auto& [key, f] : EdgeFaces)
    {
        if (f.Faces < 2) continue; // its other face wasn't sent this frame
        // pairs that disagree, or a face left over: faces of different meshes
        // share these model coordinates. Left curved, as without this information.
        EdgeKeep[key] = (f.Conflict || f.Faces % 2 != 0) ? 1.0f : f.Keep;
    }
    EdgeFaces.clear();
}

bool YSort(Polygon* a, Polygon* b); // hardware polygon sorting, see VBlank

void GPU3D::BuildMultipliedRenderList() noexcept
{
    RenderMultiplied = false;
    if (NumSubPolygons == 0)
        return;

    auto& blocks = SubPolygonBlocks[CurRAMBank];
    auto& extras = ExtraPolygons[CurRAMBank];
    if (MultipliedRenderPolygons.size() < RenderNumPolygons + NumSubPolygons)
        MultipliedRenderPolygons.resize(RenderNumPolygons + NumSubPolygons);

    // each multiplied polygon is replaced by its sub-polygons
    u32 n = 0;
    auto append = [&](Polygon* poly) {
        if (poly->SubPolygonCount == 0)
        {
            MultipliedRenderPolygons[n++] = poly;
            return;
        }
        for (u32 j = poly->SubPolygonStart; j < poly->SubPolygonStart + poly->SubPolygonCount; j++)
            MultipliedRenderPolygons[n++] = &blocks[j / SubBlockSize][j % SubBlockSize];
    };

    if (extras.empty())
    {
        // same order as the hardware list (opaque first, Y-sorted)
        for (u32 i = 0; i < RenderNumPolygons; i++)
            append(RenderPolygonRAM[i]);
    }
    else
    {
        // polygons past the hardware limit: sort them with the others, by the
        // hardware's rules (see VBlank): submission order, then Y-sorting of the
        // opaque polygons, and of the translucent ones unless sorted manually
        std::vector<Polygon*> all;
        all.reserve(NumPolygons + extras.size());
        for (u32 i = 0; i < NumPolygons; i++)
            all.push_back(&CurPolygonRAM[i]);
        all.insert(all.end(), extras.begin(), extras.end());

        auto firsttrans = std::stable_partition(all.begin(), all.end(), [](Polygon* p) { return !p->Translucent; });
        std::stable_sort(all.begin(), (FlushAttributes & 0x1) ? firsttrans : all.end(), YSort);

        for (Polygon* poly : all)
            append(poly);
    }

    MultipliedRenderNumPolygons = n;
    RenderMultiplied = true;
}

void GPU3D::SubmitVertex() noexcept
{
    s64 vertex[4] = {(s64)CurVertex[0], (s64)CurVertex[1], (s64)CurVertex[2], 0x1000};
    Vertex* vertextrans = &TempVertexBuffer[VertexNumInPoly];

    UpdateClipMatrix();
    vertextrans->Position[0] = (vertex[0]*ClipMatrix[0] + vertex[1]*ClipMatrix[4] + vertex[2]*ClipMatrix[8] + vertex[3]*ClipMatrix[12]) >> 12;
    vertextrans->Position[1] = (vertex[0]*ClipMatrix[1] + vertex[1]*ClipMatrix[5] + vertex[2]*ClipMatrix[9] + vertex[3]*ClipMatrix[13]) >> 12;
    vertextrans->Position[2] = (vertex[0]*ClipMatrix[2] + vertex[1]*ClipMatrix[6] + vertex[2]*ClipMatrix[10] + vertex[3]*ClipMatrix[14]) >> 12;
    vertextrans->Position[3] = (vertex[0]*ClipMatrix[3] + vertex[1]*ClipMatrix[7] + vertex[2]*ClipMatrix[11] + vertex[3]*ClipMatrix[15]) >> 12;

    // this probably shouldn't be.
    // the way color is handled during clipping needs investigation. TODO
    vertextrans->Color[0] = (VertexColor[0] << 12) + 0xFFF;
    vertextrans->Color[1] = (VertexColor[1] << 12) + 0xFFF;
    vertextrans->Color[2] = (VertexColor[2] << 12) + 0xFFF;
    if (HighColor)
    {
        for (int i = 0; i < 3; i++)
            vertextrans->Color[i] = VertexColorPrecise[i];
    }

    if ((TexParam >> 30) == 3)
    {
        vertextrans->TexCoords[0] = ((vertex[0]*TexMatrix[0] + vertex[1]*TexMatrix[4] + vertex[2]*TexMatrix[8]) >> 24) + RawTexCoords[0];
        vertextrans->TexCoords[1] = ((vertex[0]*TexMatrix[1] + vertex[1]*TexMatrix[5] + vertex[2]*TexMatrix[9]) >> 24) + RawTexCoords[1];
    }
    else
    {
        vertextrans->TexCoords[0] = TexCoords[0];
        vertextrans->TexCoords[1] = TexCoords[1];
    }

    vertextrans->Clipped = false;

    // polygon multiplier: view-space data for this vertex
    if (CaptureViewData())
    {
        for (int i = 0; i < 4; i++)
            vertextrans->ViewPosition[i] = (float)((vertex[0]*PosMatrix[i] + vertex[1]*PosMatrix[4+i] + vertex[2]*PosMatrix[8+i] + vertex[3]*PosMatrix[12+i]) >> 12);
        for (int i = 0; i < 3; i++)
            vertextrans->ViewNormal[i] = CurViewNormal[i];
        vertextrans->HasViewNormal = CurViewNormalValid;
        vertextrans->LitColor = CurColorFromLighting;
        // the projection's W column (row-vector matrices: elements 3, 7, 11)
        // is zero when W doesn't depend on depth
        vertextrans->Orthographic = ProjMatrix[3] == 0 && ProjMatrix[7] == 0 && ProjMatrix[11] == 0;
        vertextrans->Specular = CurColorFromLighting ? (MatSpecular[0] + MatSpecular[1] + MatSpecular[2]) / 93.0f : 0.0f;
        for (int i = 0; i < 3; i++) vertextrans->ModelPosition[i] = CurVertex[i];
        // the perspective projection and viewport used by the most vertices
        // this frame (the scene's, not a small 3D icon's), compared only when
        // a matrix or the viewport changed
        if (!vertextrans->Orthographic)
        {
            if ((FrameProjectionCheck || FrameProjVertices == 0) &&
                (memcmp(FrameProjMatrix, ProjMatrix, sizeof(ProjMatrix)) || memcmp(FrameViewport, Viewport, sizeof(FrameViewport))))
            {
                if (FrameProjVertices > BestProjVertices)
                {
                    memcpy(BestProjMatrix, FrameProjMatrix, sizeof(FrameProjMatrix));
                    memcpy(BestViewport, FrameViewport, sizeof(FrameViewport));
                    BestProjVertices = FrameProjVertices;
                }
                memcpy(FrameProjMatrix, ProjMatrix, sizeof(ProjMatrix));
                memcpy(FrameViewport, Viewport, sizeof(FrameViewport));
                FrameProjVertices = 0;
            }
            FrameProjectionCheck = false;
            FrameProjVertices++;
        }
    }
    else
    {
        vertextrans->HasViewNormal = false;
        vertextrans->LitColor = false;
        vertextrans->Orthographic = false;
        vertextrans->Specular = 0.0f;
    }

    VertexNum++;
    VertexNumInPoly++;

    switch (PolygonMode)
    {
    case 0: // triangle
        if (VertexNumInPoly == 3)
        {
            VertexNumInPoly = 0;
            SubmitPolygon();
            NumConsecutivePolygons++;
        }
        break;

    case 1: // quad
        if (VertexNumInPoly == 4)
        {
            VertexNumInPoly = 0;
            SubmitPolygon();
            NumConsecutivePolygons++;
        }
        break;

    case 2: // triangle strip
        if (NumConsecutivePolygons & 1)
        {
            Vertex tmp = TempVertexBuffer[1];
            TempVertexBuffer[1] = TempVertexBuffer[0];
            TempVertexBuffer[0] = tmp;

            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        else if (VertexNumInPoly == 3)
        {
            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[0] = TempVertexBuffer[1];
            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        break;

    case 3: // quad strip
        if (VertexNumInPoly == 4)
        {
            Vertex tmp = TempVertexBuffer[3];
            TempVertexBuffer[3] = TempVertexBuffer[2];
            TempVertexBuffer[2] = tmp;

            VertexNumInPoly = 2;
            SubmitPolygon();
            NumConsecutivePolygons++;

            TempVertexBuffer[0] = TempVertexBuffer[3];
            TempVertexBuffer[1] = TempVertexBuffer[2];
        }
        break;
    }

    VertexPipeline = 7;
    AddCycles(3);
}

// DS lighting for one vertex: colour from the transformed normal and the
// current lights and material. Returns the number of lights applied.
s32 GPU3D::LightVertex(const s32* normaltrans, u8* color, s32* precisecolor) const noexcept
{
    s32 c = 0;
    u32 vtxbuff[3] =
    {
        (u32)MatEmission[0] << 14,
        (u32)MatEmission[1] << 14,
        (u32)MatEmission[2] << 14
    };
    for (int i = 0; i < 4; i++)
    {
        if (!(CurPolygonAttr & (1<<i)))
            continue;

        // (credit to azusa for working out most of the details of the diff. algorithm, and essentially the entire spec. algorithm)
        
        // calculate dot product
        // bottom 9 bits are discarded after multiplying and before adding
        s32 dot = ((LightDirection[i][0]*normaltrans[0]) >> 9) +
                  ((LightDirection[i][1]*normaltrans[1]) >> 9) +
                  ((LightDirection[i][2]*normaltrans[2]) >> 9);

        s32 shinelevel;
        if (dot > 0) 
        {
            // -- diffuse lighting --
            
            // convert dot to signed 11 bit int
            // then we truncate the result of the multiplications to an unsigned 20 bits before adding to the vtx color
            s32 diffdot = (dot << 21) >> 21;
            vtxbuff[0] += (MatDiffuse[0] * LightColor[i][0] * diffdot) & 0xFFFFF;
            vtxbuff[1] += (MatDiffuse[1] * LightColor[i][1] * diffdot) & 0xFFFFF;
            vtxbuff[2] += (MatDiffuse[2] * LightColor[i][2] * diffdot) & 0xFFFFF;

            // -- specular lighting --
        
            // reuse the dot product from diffuse lighting
            dot += normaltrans[2];

            // convert to s11, then square it, and truncate to 10 bits
            dot = (dot << 21) >> 21;
            dot = ((dot * dot) >> 10) & 0x3FF;

            // multiply dot and reciprocal, the subtract '1'
            shinelevel = ((dot * SpecRecip[i]) >> 8) - (1<<9);

            if (shinelevel < 0) shinelevel = 0;
            else
            {
                // sign extend to convert to signed 14 bit integer
                shinelevel = (shinelevel << 18) >> 18;
                if (shinelevel < 0) shinelevel = 0; // for some reason there seems to be a redundant check for <0?
                else if (shinelevel > 0x1FF) shinelevel = 0x1FF;
            }
        }
        else shinelevel = 0;

        // convert shinelevel to use for lookup in the shininess table if enabled.
        if (UseShininessTable)
        {
            shinelevel >>= 2;
            shinelevel = ShininessTable[shinelevel];
            shinelevel <<= 1;
        }

        // Note: ambient seems to be a plain bitshift
        vtxbuff[0] += ((MatSpecular[0] * shinelevel) + (MatAmbient[0] << 9)) * LightColor[i][0];
        vtxbuff[1] += ((MatSpecular[1] * shinelevel) + (MatAmbient[1] << 9)) * LightColor[i][1];
        vtxbuff[2] += ((MatSpecular[2] * shinelevel) + (MatAmbient[2] << 9)) * LightColor[i][2];

        c++;
    }

    color[0] = (vtxbuff[0] >> 14 > 31) ? 31 : (vtxbuff[0] >> 14);
    color[1] = (vtxbuff[1] >> 14 > 31) ? 31 : (vtxbuff[1] >> 14);
    color[2] = (vtxbuff[2] >> 14 > 31) ? 31 : (vtxbuff[2] >> 14);

    // high colour: the same result before rounding, 5.12 fixed point
    if (precisecolor)
    {
        for (int i = 0; i < 3; i++)
            precisecolor[i] = (s32)std::min<u32>(vtxbuff[i] >> 2, 31u << 12);
    }

    return c;
}

void GPU3D::CalculateLighting() noexcept
{
    if ((TexParam >> 30) == 2)
    {
        TexCoords[0] = RawTexCoords[0] + (((s64)Normal[0]*TexMatrix[0] + (s64)Normal[1]*TexMatrix[4] + (s64)Normal[2]*TexMatrix[8]) >> 21);
        TexCoords[1] = RawTexCoords[1] + (((s64)Normal[0]*TexMatrix[1] + (s64)Normal[1]*TexMatrix[5] + (s64)Normal[2]*TexMatrix[9]) >> 21);
    }

    s32 normaltrans[3]; // should be 1 bit sign 10 bits frac
    normaltrans[0] = ((Normal[0]*VecMatrix[0] + Normal[1]*VecMatrix[4] + Normal[2]*VecMatrix[8]) << 9) >> 21;
    normaltrans[1] = ((Normal[0]*VecMatrix[1] + Normal[1]*VecMatrix[5] + Normal[2]*VecMatrix[9]) << 9) >> 21;
    normaltrans[2] = ((Normal[0]*VecMatrix[2] + Normal[1]*VecMatrix[6] + Normal[2]*VecMatrix[10]) << 9) >> 21;

    s32 c = LightVertex(normaltrans, VertexColor, VertexColorPrecise);

    if (c < 1) c = 1;
    NormalPipeline = 7;
    AddCycles(c);
}


void GPU3D::BoxTest(const u32* params) noexcept
{
    Vertex cube[8];
    Vertex face[10];
    int res;

    AddCycles(254);
    GXStat &= ~(1<<1);

    s16 x0 = (s16)(params[0] & 0xFFFF);
    s16 y0 = ((s32)params[0]) >> 16;
    s16 z0 = (s16)(params[1] & 0xFFFF);
    s16 x1 = ((s32)params[1]) >> 16;
    s16 y1 = (s16)(params[2] & 0xFFFF);
    s16 z1 = ((s32)params[2]) >> 16;

    x1 += x0;
    y1 += y0;
    z1 += z0;

    cube[0].Position[0] = x0; cube[0].Position[1] = y0; cube[0].Position[2] = z0;
    cube[1].Position[0] = x1; cube[1].Position[1] = y0; cube[1].Position[2] = z0;
    cube[2].Position[0] = x1; cube[2].Position[1] = y1; cube[2].Position[2] = z0;
    cube[3].Position[0] = x0; cube[3].Position[1] = y1; cube[3].Position[2] = z0;
    cube[4].Position[0] = x0; cube[4].Position[1] = y1; cube[4].Position[2] = z1;
    cube[5].Position[0] = x0; cube[5].Position[1] = y0; cube[5].Position[2] = z1;
    cube[6].Position[0] = x1; cube[6].Position[1] = y0; cube[6].Position[2] = z1;
    cube[7].Position[0] = x1; cube[7].Position[1] = y1; cube[7].Position[2] = z1;

    UpdateClipMatrix();
    for (int i = 0; i < 8; i++)
    {
        s32 x = cube[i].Position[0];
        s32 y = cube[i].Position[1];
        s32 z = cube[i].Position[2];

        cube[i].Position[0] = ((s64)x*ClipMatrix[0] + (s64)y*ClipMatrix[4] + (s64)z*ClipMatrix[8] + (s64)0x1000*ClipMatrix[12]) >> 12;
        cube[i].Position[1] = ((s64)x*ClipMatrix[1] + (s64)y*ClipMatrix[5] + (s64)z*ClipMatrix[9] + (s64)0x1000*ClipMatrix[13]) >> 12;
        cube[i].Position[2] = ((s64)x*ClipMatrix[2] + (s64)y*ClipMatrix[6] + (s64)z*ClipMatrix[10] + (s64)0x1000*ClipMatrix[14]) >> 12;
        cube[i].Position[3] = ((s64)x*ClipMatrix[3] + (s64)y*ClipMatrix[7] + (s64)z*ClipMatrix[11] + (s64)0x1000*ClipMatrix[15]) >> 12;
    }

    // front face (-Z)
    face[0] = cube[0]; face[1] = cube[1]; face[2] = cube[2]; face[3] = cube[3];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // back face (+Z)
    face[0] = cube[4]; face[1] = cube[5]; face[2] = cube[6]; face[3] = cube[7];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // left face (-X)
    face[0] = cube[0]; face[1] = cube[3]; face[2] = cube[4]; face[3] = cube[5];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // right face (+X)
    face[0] = cube[1]; face[1] = cube[2]; face[2] = cube[7]; face[3] = cube[6];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // bottom face (-Y)
    face[0] = cube[0]; face[1] = cube[1]; face[2] = cube[6]; face[3] = cube[5];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }

    // top face (+Y)
    face[0] = cube[2]; face[1] = cube[3]; face[2] = cube[4]; face[3] = cube[7];
    res = ClipPolygon<false>(*this, face, 4, 0);
    if (res > 0)
    {
        GXStat |= (1<<1);
        return;
    }
}

void GPU3D::PosTest() noexcept
{
    s64 vertex[4] = {(s64)CurVertex[0], (s64)CurVertex[1], (s64)CurVertex[2], 0x1000};

    UpdateClipMatrix();
    PosTestResult[0] = (vertex[0]*ClipMatrix[0] + vertex[1]*ClipMatrix[4] + vertex[2]*ClipMatrix[8] + vertex[3]*ClipMatrix[12]) >> 12;
    PosTestResult[1] = (vertex[0]*ClipMatrix[1] + vertex[1]*ClipMatrix[5] + vertex[2]*ClipMatrix[9] + vertex[3]*ClipMatrix[13]) >> 12;
    PosTestResult[2] = (vertex[0]*ClipMatrix[2] + vertex[1]*ClipMatrix[6] + vertex[2]*ClipMatrix[10] + vertex[3]*ClipMatrix[14]) >> 12;
    PosTestResult[3] = (vertex[0]*ClipMatrix[3] + vertex[1]*ClipMatrix[7] + vertex[2]*ClipMatrix[11] + vertex[3]*ClipMatrix[15]) >> 12;

    AddCycles(5);
}

void GPU3D::VecTest(u32 param) noexcept
{
    // TODO: maybe it overwrites the normal registers, too

    s16 normal[3];

    normal[0] = (s16)((param & 0x000003FF) << 6) >> 6;
    normal[1] = (s16)((param & 0x000FFC00) >> 4) >> 6;
    normal[2] = (s16)((param & 0x3FF00000) >> 14) >> 6;

    VecTestResult[0] = (normal[0]*VecMatrix[0] + normal[1]*VecMatrix[4] + normal[2]*VecMatrix[8]) >> 9;
    VecTestResult[1] = (normal[0]*VecMatrix[1] + normal[1]*VecMatrix[5] + normal[2]*VecMatrix[9]) >> 9;
    VecTestResult[2] = (normal[0]*VecMatrix[2] + normal[1]*VecMatrix[6] + normal[2]*VecMatrix[10]) >> 9;

    if (VecTestResult[0] & 0x1000) VecTestResult[0] |= 0xF000;
    if (VecTestResult[1] & 0x1000) VecTestResult[1] |= 0xF000;
    if (VecTestResult[2] & 0x1000) VecTestResult[2] |= 0xF000;

    AddCycles(4);
}



void GPU3D::CmdFIFOWrite(const CmdFIFOEntry& entry) noexcept
{
    if (CmdFIFO.IsEmpty() && !CmdPIPE.IsFull())
    {
        CmdPIPE.Write(entry);
    }
    else
    {
        if (CmdFIFO.IsFull())
        {
            // store it to the stall queue. stall the system.
            // worst case is if a STMxx opcode causes this, which is why our stall queue
            // has 64 entries. this is less complicated than trying to make STMxx stall-able.

            CmdStallQueue.Write(entry);
            NDS.GXFIFOStall();
            return;
        }

        CmdFIFO.Write(entry);
    }

    GXStat |= (1<<27);

    if (entry.Command == 0x11 || entry.Command == 0x12)
    {
        GXStat |= (1<<14); // push/pop matrix
        NumPushPopCommands++;
    }
    else if (entry.Command == 0x70 || entry.Command == 0x71 || entry.Command == 0x72)
    {
        GXStat |= (1<<0); // box/pos/vec test
        NumTestCommands++;
    }
}

GPU3D::CmdFIFOEntry GPU3D::CmdFIFORead() noexcept
{
    CmdFIFOEntry ret = CmdPIPE.Read();

    if (CmdPIPE.Level() <= 2)
    {
        if (!CmdFIFO.IsEmpty())
            CmdPIPE.Write(CmdFIFO.Read());
        if (!CmdFIFO.IsEmpty())
            CmdPIPE.Write(CmdFIFO.Read());

        // empty stall queue if needed
        // CmdFIFO should not be full at this point.
        if (!CmdStallQueue.IsEmpty())
        {
            while (!CmdStallQueue.IsEmpty())
            {
                if (CmdFIFO.IsFull()) break;
                CmdFIFOEntry entry = CmdStallQueue.Read();
                CmdFIFOWrite(entry);
            }

            if (CmdStallQueue.IsEmpty())
                NDS.GXFIFOUnstall();
        }

        CheckFIFODMA();
        CheckFIFOIRQ();
    }

    return ret;
}

void GPU3D::ExecuteCommand() noexcept
{
    CmdFIFOEntry entry = CmdFIFORead();

    //printf("FIFO: processing %02X %08X. Levels: FIFO=%d, PIPE=%d\n", entry.Command, entry.Param, CmdFIFO->Level(), CmdPIPE->Level());

    // each FIFO entry takes 1 cycle to be processed
    // commands (presumably) run when all the needed parameters have been read
    // which is where we add the remaining cycles if any

    u32 paramsRequiredCount = CmdNumParams[entry.Command];
    if (paramsRequiredCount <= 1)
    {
        // fast path for command which only have a single parameter

        /*printf("[GXS:%08X] 0x%02X,  0x%08X", GXStat, entry.Command, entry.Param);*/

        switch (entry.Command)
        {
        case 0x10: // matrix mode
            VertexPipelineCmdDelayed4();
            MatrixMode = entry.Param & 0x3;
            break;

        case 0x11: // push matrix
            VertexPipelineCmdDelayed4();
            NumPushPopCommands--;
            if (MatrixMode == 0)
            {
                if (ProjMatrixStackPointer > 0) GXStat |= (1<<15);

                memcpy(ProjMatrixStack, ProjMatrix, 16*4);
                ProjMatrixStackPointer++;
                ProjMatrixStackPointer &= 0x1;
            }
            else if (MatrixMode == 3)
            {
                if (TexMatrixStackPointer > 0) GXStat |= (1<<15);

                memcpy(TexMatrixStack, TexMatrix, 16*4);
                TexMatrixStackPointer++;
                TexMatrixStackPointer &= 0x1;
            }
            else
            {
                if (PosMatrixStackPointer > 30) GXStat |= (1<<15);

                memcpy(PosMatrixStack[PosMatrixStackPointer & 0x1F], PosMatrix, 16*4);
                memcpy(VecMatrixStack[PosMatrixStackPointer & 0x1F], VecMatrix, 16*4);
                PosMatrixStackPointer++;
                PosMatrixStackPointer &= 0x3F;
            }
            AddCycles(16);
            break;

        case 0x12: // pop matrix
            VertexPipelineCmdDelayed4();
            NumPushPopCommands--;
            if (MatrixMode == 0)
            {
                if (ProjMatrixStackPointer == 0) GXStat |= (1<<15);

                ProjMatrixStackPointer--;
                ProjMatrixStackPointer &= 0x1;
                memcpy(ProjMatrix, ProjMatrixStack, 16*4);
                ClipMatrixDirty = true;
                AddCycles(35);
            }
            else if (MatrixMode == 3)
            {
                if (TexMatrixStackPointer == 0) GXStat |= (1<<15);

                TexMatrixStackPointer--;
                TexMatrixStackPointer &= 0x1;
                memcpy(TexMatrix, TexMatrixStack, 16*4);
                AddCycles(17);
            }
            else
            {
                s32 offset = (s32)(entry.Param << 26) >> 26;
                PosMatrixStackPointer -= offset;
                PosMatrixStackPointer &= 0x3F;

                if (PosMatrixStackPointer > 30) GXStat |= (1<<15);

                memcpy(PosMatrix, PosMatrixStack[PosMatrixStackPointer & 0x1F], 16*4);
                memcpy(VecMatrix, VecMatrixStack[PosMatrixStackPointer & 0x1F], 16*4);
                ClipMatrixDirty = true;
                AddCycles(35);
            }
            break;

        case 0x13: // store matrix
            VertexPipelineCmdDelayed4();
            if (MatrixMode == 0)
            {
                memcpy(ProjMatrixStack, ProjMatrix, 16*4);
            }
            else if (MatrixMode == 3)
            {
                memcpy(TexMatrixStack, TexMatrix, 16*4);
            }
            else
            {
                u32 addr = entry.Param & 0x1F;
                if (addr > 30) GXStat |= (1<<15);

                memcpy(PosMatrixStack[addr], PosMatrix, 16*4);
                memcpy(VecMatrixStack[addr], VecMatrix, 16*4);
            }
            AddCycles(16);
            break;

        case 0x14: // restore matrix
            VertexPipelineCmdDelayed4();
            if (MatrixMode == 0)
            {
                memcpy(ProjMatrix, ProjMatrixStack, 16*4);
                ClipMatrixDirty = true;
                AddCycles(35);
            }
            else if (MatrixMode == 3)
            {
                memcpy(TexMatrix, TexMatrixStack, 16*4);
                AddCycles(17);
            }
            else
            {
                u32 addr = entry.Param & 0x1F;
                if (addr > 30) GXStat |= (1<<15);

                memcpy(PosMatrix, PosMatrixStack[addr], 16*4);
                memcpy(VecMatrix, VecMatrixStack[addr], 16*4);
                ClipMatrixDirty = true;
                AddCycles(35);
            }
            break;

        case 0x15: // identity
            VertexPipelineCmdDelayed4();
            if (MatrixMode == 0)
            {
                MatrixLoadIdentity(ProjMatrix);
                ClipMatrixDirty = true;
                AddCycles(18);
            }
            else if (MatrixMode == 3)
                MatrixLoadIdentity(TexMatrix);
            else
            {
                MatrixLoadIdentity(PosMatrix);
                if (MatrixMode == 2)
                    MatrixLoadIdentity(VecMatrix);
                ClipMatrixDirty = true;
                AddCycles(18);
            }
            break;

        case 0x20: // vertex color
            VertexPipelineCmdDelayed6();
            {
                u32 c = entry.Param;
                u32 r = c & 0x1F;
                u32 g = (c >> 5) & 0x1F;
                u32 b = (c >> 10) & 0x1F;
                VertexColor[0] = r;
                VertexColor[1] = g;
                VertexColor[2] = b;
                CurColorFromLighting = false;
                for (int i = 0; i < 3; i++)
                    VertexColorPrecise[i] = VertexColor[i] << 12;
            }
            break;

        case 0x21: // normal
            VertexPipelineCmdDelayed4();
            Normal[0] = (s16)((entry.Param & 0x000003FF) << 6) >> 6;
            Normal[1] = (s16)((entry.Param & 0x000FFC00) >> 4) >> 6;
            Normal[2] = (s16)((entry.Param & 0x3FF00000) >> 14) >> 6;
            CalculateLighting();
            // polygon multiplier: the normal in view space, as used for lighting
            for (int i = 0; i < 3; i++)
                CurViewNormal[i] = (float)Normal[0]*VecMatrix[i] + (float)Normal[1]*VecMatrix[4+i] + (float)Normal[2]*VecMatrix[8+i];
            CurViewNormalValid = true;
            CurColorFromLighting = true;
            break;

        case 0x22: // texcoord
            VertexPipelineCmdDelayed4();
            RawTexCoords[0] = entry.Param & 0xFFFF;
            RawTexCoords[1] = entry.Param >> 16;
            if ((TexParam >> 30) == 1)
            {
                TexCoords[0] = (RawTexCoords[0]*TexMatrix[0] + RawTexCoords[1]*TexMatrix[4] + TexMatrix[8] + TexMatrix[12]) >> 12;
                TexCoords[1] = (RawTexCoords[0]*TexMatrix[1] + RawTexCoords[1]*TexMatrix[5] + TexMatrix[9] + TexMatrix[13]) >> 12;
            }
            else
            {
                TexCoords[0] = RawTexCoords[0];
                TexCoords[1] = RawTexCoords[1];
            }
            break;

        case 0x24: // 10-bit vertex
            VertexPipelineSubmitCmd();
            CurVertex[0] = (entry.Param & 0x000003FF) << 6;
            CurVertex[1] = (entry.Param & 0x000FFC00) >> 4;
            CurVertex[2] = (entry.Param & 0x3FF00000) >> 14;
            SubmitVertex();
            break;

        case 0x25: // vertex XY
            VertexPipelineSubmitCmd();
            CurVertex[0] = entry.Param & 0xFFFF;
            CurVertex[1] = entry.Param >> 16;
            SubmitVertex();
            break;

        case 0x26: // vertex XZ
            VertexPipelineSubmitCmd();
            CurVertex[0] = entry.Param & 0xFFFF;
            CurVertex[2] = entry.Param >> 16;
            SubmitVertex();
            break;

        case 0x27: // vertex YZ
            VertexPipelineSubmitCmd();
            CurVertex[1] = entry.Param & 0xFFFF;
            CurVertex[2] = entry.Param >> 16;
            SubmitVertex();
            break;

        case 0x28: // 10-bit delta vertex
            VertexPipelineSubmitCmd();
            CurVertex[0] += (s16)((entry.Param & 0x000003FF) << 6) >> 6;
            CurVertex[1] += (s16)((entry.Param & 0x000FFC00) >> 4) >> 6;
            CurVertex[2] += (s16)((entry.Param & 0x3FF00000) >> 14) >> 6;
            SubmitVertex();
            break;

        case 0x29: // polygon attributes
            VertexPipelineCmdDelayed8();
            PolygonAttr = entry.Param;
            break;

        case 0x2A: // texture param
            VertexPipelineCmdDelayed8();
            TexParam = entry.Param;
            break;

        case 0x2B: // texture palette
            VertexPipelineCmdDelayed8();
            TexPalette = entry.Param & 0x1FFF;
            break;

        case 0x30: // diffuse/ambient material
            VertexPipelineCmdDelayed6();
            MatDiffuse[0] = entry.Param & 0x1F;
            MatDiffuse[1] = (entry.Param >> 5) & 0x1F;
            MatDiffuse[2] = (entry.Param >> 10) & 0x1F;
            MatAmbient[0] = (entry.Param >> 16) & 0x1F;
            MatAmbient[1] = (entry.Param >> 21) & 0x1F;
            MatAmbient[2] = (entry.Param >> 26) & 0x1F;
            if (entry.Param & 0x8000)
            {
                VertexColor[0] = MatDiffuse[0];
                VertexColor[1] = MatDiffuse[1];
                VertexColor[2] = MatDiffuse[2];
                CurColorFromLighting = false;
                for (int i = 0; i < 3; i++)
                    VertexColorPrecise[i] = VertexColor[i] << 12;
            }
            AddCycles(3);
            break;

        case 0x31: // specular/emission material
            VertexPipelineCmdDelayed6();
            MatSpecular[0] = entry.Param & 0x1F;
            MatSpecular[1] = (entry.Param >> 5) & 0x1F;
            MatSpecular[2] = (entry.Param >> 10) & 0x1F;
            MatEmission[0] = (entry.Param >> 16) & 0x1F;
            MatEmission[1] = (entry.Param >> 21) & 0x1F;
            MatEmission[2] = (entry.Param >> 26) & 0x1F;
            UseShininessTable = (entry.Param & 0x8000) != 0;
            AddCycles(3);
            break;

        case 0x32: // light direction
            StallPolygonPipeline(8 + 1,  2); // 0x32 can run 6 cycles after a vertex
            {
                u32 l = entry.Param >> 30;
                s16 dir[3];
                dir[0] = (s16)((entry.Param & 0x000003FF) << 6) >> 6;
                dir[1] = (s16)((entry.Param & 0x000FFC00) >> 4) >> 6;
                dir[2] = (s16)((entry.Param & 0x3FF00000) >> 14) >> 6;
                // the order of operations here is very specific: discard bottom 12 bits -> negate -> then sign extend to convert to 11 bit signed int
                // except for when used to calculate the specular reciprocal; then it's: sign extend -> discard lsb -> negate.
                LightDirection[l][0] = (-((dir[0]*VecMatrix[0] + dir[1]*VecMatrix[4] + dir[2]*VecMatrix[8] ) >> 12) << 21) >> 21;
                LightDirection[l][1] = (-((dir[0]*VecMatrix[1] + dir[1]*VecMatrix[5] + dir[2]*VecMatrix[9] ) >> 12) << 21) >> 21;
                LightDirection[l][2] = (-((dir[0]*VecMatrix[2] + dir[1]*VecMatrix[6] + dir[2]*VecMatrix[10]) >> 12) << 21) >> 21;
                s32 den =              -(((dir[0]*VecMatrix[2] + dir[1]*VecMatrix[6] + dir[2]*VecMatrix[10]) << 9) >> 21) + (1<<9);

                if (den == 0) SpecRecip[l] = 0;
                else SpecRecip[l] = (1<<18) / den;
            }
            AddCycles(5);
            break;

        case 0x33: // light color
            VertexPipelineCmdDelayed8();
            {
                u32 l = entry.Param >> 30;
                LightColor[l][0] = entry.Param & 0x1F;
                LightColor[l][1] = (entry.Param >> 5) & 0x1F;
                LightColor[l][2] = (entry.Param >> 10) & 0x1F;
            }
            AddCycles(1);
            break;

        case 0x40: // begin polygons
            StallPolygonPipeline(1, 0);
            // TODO: check if there was a polygon being defined but incomplete
            // such cases seem to freeze the GPU
            PolygonMode = entry.Param & 0x3;
            VertexNum = 0;
            VertexNumInPoly = 0;
            NumConsecutivePolygons = 0;
            LastStripPolygon = NULL;
            CurPolygonAttr = PolygonAttr;
            break;

        case 0x41: // end polygons
            VertexPipelineCmdDelayed8();
            // TODO: research this?
            // it doesn't seem to have any effect whatsoever, but
            // its timing characteristics are different from those of other
            // no-op commands
            break;

        case 0x50: // flush
            VertexPipelineCmdDelayed4();
            FlushRequest = 1;
            FlushAttributes = entry.Param & 0x3;
            CycleCount = 325;
            // probably safe to just reset all pipelines
            // but needs checked
            VertexPipeline = 0;
            NormalPipeline = 0;
            PolygonPipeline = 0;
            VertexSlotCounter = 0;
            VertexSlotsFree = 1;
            break;

        case 0x60: // viewport x1,y1,x2,y2
            VertexPipelineCmdDelayed8();
            // note: viewport Y coordinates are upside-down
            Viewport[0] = entry.Param & 0xFF;                             // x0
            Viewport[1] = (191 - ((entry.Param >> 8) & 0xFF)) & 0xFF;     // y0
            Viewport[2] = (entry.Param >> 16) & 0xFF;                     // x1
            Viewport[3] = (191 - (entry.Param >> 24)) & 0xFF;             // y1
            Viewport[4] = (Viewport[2] - Viewport[0] + 1) & 0x1FF;          // width
            Viewport[5] = (Viewport[1] - Viewport[3] + 1) & 0xFF;           // height
            FrameProjectionCheck = true; // Pomegrade: see SubmitVertex
            break;

        case 0x72: // vec test
            VertexPipelineCmdDelayed6();
            NumTestCommands--;
            VecTest(entry.Param);
            break;

        default:
            VertexPipelineCmdDelayed4();
            //printf("!! UNKNOWN GX COMMAND %02X %08X\n", entry.Command, entry.Param);
            break;
        }
    }
    else
    {
        ExecParams[ExecParamCount] = entry.Param;
        ExecParamCount++;

        if (ExecParamCount == 1)
        {
            // delay the first command entry as needed
            switch (entry.Command)
            {
            // commands that stall the polygon pipeline
            case 0x23: VertexPipelineSubmitCmd(); break;
            case 0x34:
            case 0x71:
                VertexPipelineCmdDelayed8();
                break;
            case 0x70: StallPolygonPipeline(10 + 1, 0); break;
            default: VertexPipelineCmdDelayed4(); break;
            }
        }
        else
        {
            AddCycles(1);

            if (ExecParamCount >= paramsRequiredCount)
            {
                /*printf("[GXS:%08X] 0x%02X,  ", GXStat, entry.Command);
                for (int k = 0; k < ExecParamCount; k++) printf("0x%08X, ", ExecParams[k]);
                printf("\n");*/

                ExecParamCount = 0;

                switch (entry.Command)
                {
                case 0x16: // load 4x4
                    if (MatrixMode == 0)
                    {
                        MatrixLoad4x4(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(18);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixLoad4x4(TexMatrix, (s32*)ExecParams);
                        AddCycles(10);
                    }
                    else
                    {
                        MatrixLoad4x4(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                            MatrixLoad4x4(VecMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(18);
                    }
                    break;

                case 0x17: // load 4x3
                    if (MatrixMode == 0)
                    {
                        MatrixLoad4x3(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(18);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixLoad4x3(TexMatrix, (s32*)ExecParams);
                        AddCycles(7);
                    }
                    else
                    {
                        MatrixLoad4x3(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                            MatrixLoad4x3(VecMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(18);
                    }
                    break;

                case 0x18: // mult 4x4
                    if (MatrixMode == 0)
                    {
                        MatrixMult4x4(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 16);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixMult4x4(TexMatrix, (s32*)ExecParams);
                        AddCycles(33 - 16);
                    }
                    else
                    {
                        MatrixMult4x4(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                        {
                            MatrixMult4x4(VecMatrix, (s32*)ExecParams);
                            AddCycles(35 + 30 - 16);
                        }
                        else AddCycles(35 - 16);
                        ClipMatrixDirty = true;
                    }
                    break;

                case 0x19: // mult 4x3
                    if (MatrixMode == 0)
                    {
                        MatrixMult4x3(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 12);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixMult4x3(TexMatrix, (s32*)ExecParams);
                        AddCycles(33 - 12);
                    }
                    else
                    {
                        MatrixMult4x3(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                        {
                            MatrixMult4x3(VecMatrix, (s32*)ExecParams);
                            AddCycles(35 + 30 - 12);
                        }
                        else AddCycles(35 - 12);
                        ClipMatrixDirty = true;
                    }
                    break;

                case 0x1A: // mult 3x3
                    if (MatrixMode == 0)
                    {
                        MatrixMult3x3(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 9);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixMult3x3(TexMatrix, (s32*)ExecParams);
                        AddCycles(33 - 9);
                    }
                    else
                    {
                        MatrixMult3x3(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                        {
                            MatrixMult3x3(VecMatrix, (s32*)ExecParams);
                            AddCycles(35 + 30 - 9);
                        }
                        else AddCycles(35 - 9);
                        ClipMatrixDirty = true;
                    }
                    break;

                case 0x1B: // scale
                    if (MatrixMode == 0)
                    {
                        MatrixScale(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 3);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixScale(TexMatrix, (s32*)ExecParams);
                        AddCycles(33 - 3);
                    }
                    else
                    {
                        MatrixScale(PosMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 3);
                    }
                    break;

                case 0x1C: // translate
                    if (MatrixMode == 0)
                    {
                        MatrixTranslate(ProjMatrix, (s32*)ExecParams);
                        ClipMatrixDirty = true;
                        AddCycles(35 - 3);
                    }
                    else if (MatrixMode == 3)
                    {
                        MatrixTranslate(TexMatrix, (s32*)ExecParams);
                        AddCycles(33 - 3);
                    }
                    else
                    {
                        MatrixTranslate(PosMatrix, (s32*)ExecParams);
                        if (MatrixMode == 2)
                        {
                            MatrixTranslate(VecMatrix, (s32*)ExecParams);
                            AddCycles(35 + 30 - 3);
                        }
                        else AddCycles(35 - 3);
                        ClipMatrixDirty = true;
                    }
                    break;

                case 0x23: // full vertex
                    CurVertex[0] = ExecParams[0] & 0xFFFF;
                    CurVertex[1] = ExecParams[0] >> 16;
                    CurVertex[2] = ExecParams[1] & 0xFFFF;
                    SubmitVertex();
                    break;

                case 0x34: // shininess table
                    {
                        for (int i = 0; i < 128; i += 4)
                        {
                            u32 val = ExecParams[i >> 2];
                            ShininessTable[i + 0] = val & 0xFF;
                            ShininessTable[i + 1] = (val >> 8) & 0xFF;
                            ShininessTable[i + 2] = (val >> 16) & 0xFF;
                            ShininessTable[i + 3] = val >> 24;
                        }
                    }
                    break;

                case 0x71: // pos test
                    NumTestCommands -= 2;
                    CurVertex[0] = ExecParams[0] & 0xFFFF;
                    CurVertex[1] = ExecParams[0] >> 16;
                    CurVertex[2] = ExecParams[1] & 0xFFFF;
                    PosTest();
                    break;

                case 0x70: // box test
                    NumTestCommands -= 3;
                    BoxTest(ExecParams);
                    break;

                default:
                    __builtin_unreachable();
                }
            }
        }
    }
}

s32 GPU3D::CyclesToRunFor() const noexcept
{
    if (CycleCount < 0) return 0;
    return CycleCount;
}

void GPU3D::FinishWork(s32 cycles) noexcept
{
    AddCycles(cycles);
    if (NormalPipeline)
        NormalPipeline -= std::min(NormalPipeline, cycles);

    CycleCount = 0;

    if (VertexPipeline || NormalPipeline || PolygonPipeline)
        return;

    GXStat &= ~(1<<27);
}

void GPU3D::Run() noexcept
{
    if (!GeometryEnabled || FlushRequest ||
        (CmdPIPE.IsEmpty() && !(GXStat & (1<<27))))
    {
        Timestamp = NDS.ARM9Timestamp >> NDS.ARM9ClockShift;
        return;
    }

    s32 cycles = (NDS.ARM9Timestamp >> NDS.ARM9ClockShift) - Timestamp;
    CycleCount -= cycles;
    Timestamp = NDS.ARM9Timestamp >> NDS.ARM9ClockShift;

    if (CycleCount <= 0)
    {
        while (CycleCount <= 0 && !CmdPIPE.IsEmpty())
        {
            if (NumPushPopCommands == 0) GXStat &= ~(1<<14);
            if (NumTestCommands == 0)    GXStat &= ~(1<<0);

            ExecuteCommand();
        }
    }

    if (CycleCount <= 0 && CmdPIPE.IsEmpty())
    {
        if (GXStat & (1<<27)) FinishWork(-CycleCount);
        else                  CycleCount = 0;

        if (NumPushPopCommands == 0) GXStat &= ~(1<<14);
        if (NumTestCommands == 0)    GXStat &= ~(1<<0);
    }
}


void GPU3D::CheckFIFOIRQ() noexcept
{
    bool irq = false;
    switch (GXStat >> 30)
    {
    case 1: irq = (CmdFIFO.Level() < 128); break;
    case 2: irq = CmdFIFO.IsEmpty(); break;
    }

    if (irq) NDS.SetIRQ(0, IRQ_GXFIFO);
    else     NDS.ClearIRQ(0, IRQ_GXFIFO);
}

void GPU3D::CheckFIFODMA() noexcept
{
    if (CmdFIFO.Level() < 128)
        NDS.CheckDMAs(0, 0x07);
}

void GPU3D::VCount144(GPU& gpu) noexcept
{
    CurrentRenderer->VCount144(gpu);
}

void GPU3D::RestartFrame(GPU& gpu) noexcept
{
    CurrentRenderer->RestartFrame(gpu);
}

void GPU3D::Stop(const GPU& gpu) noexcept
{
    if (CurrentRenderer)
        CurrentRenderer->Stop(gpu);
}


bool YSort(Polygon* a, Polygon* b)
{
    // polygon sorting rules:
    // * opaque polygons come first
    // * polygons with lower bottom Y come first
    // * upon equal bottom Y, polygons with lower top Y come first
    // * upon equal bottom AND top Y, original ordering is used
    // the SortKey is calculated as to implement these rules

    return a->SortKey < b->SortKey;
}

void GPU3D::VBlank() noexcept
{
    if (GeometryEnabled)
    {
        if (RenderingEnabled)
        {
            if (FlushRequest)
            {
                if (NumPolygons)
                {
                    // separate translucent polygons from opaque ones

                    u32 io = 0, it = NumOpaquePolygons;
                    for (u32 i = 0; i < NumPolygons; i++)
                    {
                        Polygon* poly = &CurPolygonRAM[i];
                        if (poly->Translucent)
                            RenderPolygonRAM[it++] = poly;
                        else
                            RenderPolygonRAM[io++] = poly;
                    }

                    // apply Y-sorting

                    std::stable_sort(RenderPolygonRAM.begin(),
                        RenderPolygonRAM.begin() + ((FlushAttributes & 0x1) ? NumOpaquePolygons : NumPolygons),
                        YSort);
                }

                RenderNumPolygons = NumPolygons;
                RenderFrameIdentical = false;

                // for the renderer's effects (shadows, reflections)
                memcpy(RenderLightDirection, LightDirection, sizeof(LightDirection));
                std::swap(RenderShadowCasters, ShadowCasters);
                if (FrameProjVertices >= BestProjVertices)
                {
                    memcpy(RenderProjMatrix, FrameProjMatrix, sizeof(FrameProjMatrix));
                    memcpy(RenderViewport, FrameViewport, sizeof(FrameViewport));
                }
                else if (BestProjVertices)
                {
                    memcpy(RenderProjMatrix, BestProjMatrix, sizeof(BestProjMatrix));
                    memcpy(RenderViewport, BestViewport, sizeof(BestViewport));
                }
                FrameProjVertices = BestProjVertices = 0;
                UpdateEdgeKeep();

                BuildMultipliedRenderList();
            }
            else
            {
                RenderFrameIdentical = RenderDispCnt == DispCnt
                    && RenderAlphaRef == AlphaRef
                    && RenderClearAttr1 == ClearAttr1
                    && RenderClearAttr2 == ClearAttr2
                    && RenderFogColor == FogColor
                    && RenderFogOffset == FogOffset * 0x200
                    && memcmp(RenderEdgeTable, EdgeTable, 8*2) == 0
                    && memcmp(RenderFogDensityTable + 1, FogDensityTable, 32) == 0
                    && memcmp(RenderToonTable, ToonTable, 32*2) == 0;
            }

            RenderDispCnt = DispCnt;
            RenderAlphaRef = AlphaRef;

            memcpy(RenderEdgeTable, EdgeTable, 8*2);
            memcpy(RenderToonTable, ToonTable, 32*2);

            RenderFogColor = FogColor;
            RenderFogOffset = FogOffset * 0x200;
            RenderFogShift = (RenderDispCnt >> 8) & 0xF;
            RenderFogDensityTable[0] = FogDensityTable[0];
            memcpy(&RenderFogDensityTable[1], FogDensityTable, 32);
            RenderFogDensityTable[33] = FogDensityTable[31];

            RenderClearAttr1 = ClearAttr1;
            RenderClearAttr2 = ClearAttr2;
        }

        if (FlushRequest)
        {
            CurRAMBank = CurRAMBank?0:1;
            CurVertexRAM = &VertexRAM[CurRAMBank ? 6144 : 0];
            CurPolygonRAM = &PolygonRAM[CurRAMBank ? 2048 : 0];

            NumVertices = 0;
            NumPolygons = 0;
            NumOpaquePolygons = 0;
            NumSubVertices = 0;
            NumSubPolygons = 0;
            ExtraPolygons[CurRAMBank].clear();
            ShadowCasters.clear();
            PolygonSubmitCount = 0;

            FlushRequest = 0;
        }
    }
}

void GPU3D::VCount215(GPU& gpu) noexcept
{
    CurrentRenderer->RenderFrame(gpu);
}

void GPU3D::SetRenderXPos(u16 xpos) noexcept
{
    if (!RenderingEnabled) return;

    RenderXPos = xpos & 0x01FF;
}


u32* GPU3D::GetLine(int line) noexcept
{
    if (!AbortFrame)
    {
        u32* rawline = CurrentRenderer->GetLine(line);

        if (RenderXPos == 0) return rawline;

        // apply X scroll

        if (RenderXPos & 0x100)
        {
            int i = 0, j = RenderXPos;
            for (; j < 512; i++, j++)
                ScrolledLine[i] = 0;
            for (j = 0; i < 256; i++, j++)
                ScrolledLine[i] = rawline[j];
        }
        else
        {
            int i = 0, j = RenderXPos;
            for (; j < 256; i++, j++)
                ScrolledLine[i] = rawline[j];
            for (; i < 256; i++)
                ScrolledLine[i] = 0;
        }
    }
    else
    {
        memset(ScrolledLine, 0, 256*4);
    }

    return ScrolledLine;
}

bool GPU3D::IsRendererAccelerated() const noexcept
{
    return CurrentRenderer && CurrentRenderer->Accelerated;
}

void GPU3D::WriteToGXFIFO(u32 val) noexcept
{
    if (NumCommands == 0)
    {
        NumCommands = 4;
        CurCommand = val;
        ParamCount = 0;
        TotalParams = CmdNumParams[CurCommand & 0xFF];

        if (TotalParams > 0) return;
    }
    else
        ParamCount++;

    for (;;)
    {
        if ((CurCommand & 0xFF) || (NumCommands == 4 && CurCommand == 0))
        {
            CmdFIFOEntry entry;
            entry.Command = CurCommand & 0xFF;
            entry.Param = val;
            CmdFIFOWrite(entry);
        }

        if (ParamCount >= TotalParams)
        {
            CurCommand >>= 8;
            NumCommands--;
            if (NumCommands == 0) break;

            ParamCount = 0;
            TotalParams = CmdNumParams[CurCommand & 0xFF];
        }
        if (ParamCount < TotalParams)
            break;
    }
}


u8 GPU3D::Read8(u32 addr) noexcept
{
    switch (addr)
    {
    case 0x04000600:
        Run();
        return GXStat & 0xFF;
    case 0x04000601:
        {
            Run();
            return ((GXStat >> 8) & 0xFF) |
                   (PosMatrixStackPointer & 0x1F) |
                   ((ProjMatrixStackPointer & 0x1) << 5);
        }
    case 0x04000602:
        {
            Run();

            u32 fifolevel = CmdFIFO.Level();

            return fifolevel & 0xFF;
        }
    case 0x04000603:
        {
            Run();

            u32 fifolevel = CmdFIFO.Level();

            return ((GXStat >> 24) & 0xFF) |
                   (fifolevel >> 8) |
                   (fifolevel < 128 ? (1<<1) : 0) |
                   (fifolevel == 0  ? (1<<2) : 0);
        }
    }

    Log(LogLevel::Debug, "unknown GPU3D read8 %08X\n", addr);
    return 0;
}

u16 GPU3D::Read16(u32 addr) noexcept
{
    switch (addr)
    {
    case 0x04000060:
        return DispCnt;

    case 0x04000320:
        return 46; // TODO, eventually

    case 0x04000600:
        {
            Run();

            return (GXStat & 0xFFFF) |
                   ((PosMatrixStackPointer & 0x1F) << 8) |
                   ((ProjMatrixStackPointer & 0x1) << 13);
        }
    case 0x04000602:
        {
            Run();

            u32 fifolevel = CmdFIFO.Level();

            return (GXStat >> 16) |
                   fifolevel |
                   (fifolevel < 128 ? (1<<9) : 0) |
                   (fifolevel == 0  ? (1<<10) : 0);
        }

    case 0x04000604:
        return NumPolygons;
    case 0x04000606:
        return NumVertices;

    case 0x04000630: return VecTestResult[0];
    case 0x04000632: return VecTestResult[1];
    case 0x04000634: return VecTestResult[2];
    }

    Log(LogLevel::Debug, "unknown GPU3D read16 %08X\n", addr);
    return 0;
}

u32 GPU3D::Read32(u32 addr) noexcept
{
    switch (addr)
    {
    case 0x04000060:
        return DispCnt;

    case 0x04000320:
        return 46; // TODO, eventually

    case 0x04000600:
        {
            Run();

            u32 fifolevel = CmdFIFO.Level();

            return GXStat |
                   ((PosMatrixStackPointer & 0x1F) << 8) |
                   ((ProjMatrixStackPointer & 0x1) << 13) |
                   (fifolevel << 16) |
                   (fifolevel < 128 ? (1<<25) : 0) |
                   (fifolevel == 0  ? (1<<26) : 0);
        }

    case 0x04000604:
        return NumPolygons | (NumVertices << 16);

    case 0x04000620: return PosTestResult[0];
    case 0x04000624: return PosTestResult[1];
    case 0x04000628: return PosTestResult[2];
    case 0x0400062C: return PosTestResult[3];

    case 0x04000680: return VecMatrix[0];
    case 0x04000684: return VecMatrix[1];
    case 0x04000688: return VecMatrix[2];
    case 0x0400068C: return VecMatrix[4];
    case 0x04000690: return VecMatrix[5];
    case 0x04000694: return VecMatrix[6];
    case 0x04000698: return VecMatrix[8];
    case 0x0400069C: return VecMatrix[9];
    case 0x040006A0: return VecMatrix[10];
    }

    if (addr >= 0x04000640 && addr < 0x04000680)
    {
        UpdateClipMatrix();
        return ClipMatrix[(addr & 0x3C) >> 2];
    }

    //printf("unknown GPU3D read32 %08X\n", addr);
    return 0;
}

void GPU3D::Write8(u32 addr, u8 val) noexcept
{
    if (!RenderingEnabled && addr >= 0x04000320 && addr < 0x04000400) return;
    if (!GeometryEnabled  && addr >= 0x04000400 && addr < 0x04000700) return;

    switch (addr)
    {
    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000601:
        if (val & 0x80)
        {
            GXStat &= ~0x8000;
            ProjMatrixStackPointer = 0;
            //PosMatrixStackPointer = 0;
            TexMatrixStackPointer = 0; // CHECKME
        }
        return;
    case 0x04000603:
        val &= 0xC0;
        GXStat &= 0x3FFFFFFF;
        GXStat |= (val << 24);
        CheckFIFOIRQ();
        return;
    }

    if (addr >= 0x04000330 && addr < 0x04000340)
    {
        ((u8*)EdgeTable)[addr - 0x04000330] = val;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        FogDensityTable[addr - 0x04000360] = val & 0x7F;
        return;
    }

    if (addr >= 0x04000380 && addr < 0x040003C0)
    {
        ((u8*)ToonTable)[addr - 0x04000380] = val;
        return;
    }

    Log(LogLevel::Debug, "unknown GPU3D write8 %08X %02X\n", addr, val);
}

void GPU3D::Write16(u32 addr, u16 val) noexcept
{
    if (!RenderingEnabled && addr >= 0x04000320 && addr < 0x04000400) return;
    if (!GeometryEnabled  && addr >= 0x04000400 && addr < 0x04000700) return;

    switch (addr)
    {
    case 0x04000060:
        DispCnt = (val & 0x4FFF) | (DispCnt & 0x3000);
        if (val & (1<<12)) DispCnt &= ~(1<<12);
        if (val & (1<<13)) DispCnt &= ~(1<<13);
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000350:
        ClearAttr1 = (ClearAttr1 & 0xFFFF0000) | val;
        return;
    case 0x04000352:
        ClearAttr1 = (ClearAttr1 & 0xFFFF) | (val << 16);
        return;
    case 0x04000354:
        ClearAttr2 = (ClearAttr2 & 0xFFFF0000) | val;
        return;
    case 0x04000356:
        ClearAttr2 = (ClearAttr2 & 0xFFFF) | (val << 16);
        return;

    case 0x04000358:
        FogColor = (FogColor & 0xFFFF0000) | val;
        return;
    case 0x0400035A:
        FogColor = (FogColor & 0xFFFF) | (val << 16);
        return;
    case 0x0400035C:
        FogOffset = val & 0x7FFF;
        return;

    case 0x04000600:
        if (val & 0x8000)
        {
            GXStat &= ~0x8000;
            ProjMatrixStackPointer = 0;
            //PosMatrixStackPointer = 0;
            TexMatrixStackPointer = 0; // CHECKME
        }
        return;
    case 0x04000602:
        val &= 0xC000;
        GXStat &= 0x3FFFFFFF;
        GXStat |= (val << 16);
        CheckFIFOIRQ();
        return;

    case 0x04000610:
        val &= 0x7FFF;
        ZeroDotWLimit = (val * 0x200) + 0x1FF;
        return;
    }

    if (addr >= 0x04000330 && addr < 0x04000340)
    {
        EdgeTable[(addr - 0x04000330) >> 1] = val;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        addr -= 0x04000360;
        FogDensityTable[addr] = val & 0x7F;
        FogDensityTable[addr+1] = (val >> 8) & 0x7F;
        return;
    }

    if (addr >= 0x04000380 && addr < 0x040003C0)
    {
        ToonTable[(addr - 0x04000380) >> 1] = val;
        return;
    }

    Log(LogLevel::Debug, "unknown GPU3D write16 %08X %04X\n", addr, val);
}

void GPU3D::Write32(u32 addr, u32 val) noexcept
{
    if (!RenderingEnabled && addr >= 0x04000320 && addr < 0x04000400) return;
    if (!GeometryEnabled  && addr >= 0x04000400 && addr < 0x04000700) return;

    switch (addr)
    {
    case 0x04000060:
        DispCnt = (val & 0x4FFF) | (DispCnt & 0x3000);
        if (val & (1<<12)) DispCnt &= ~(1<<12);
        if (val & (1<<13)) DispCnt &= ~(1<<13);
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000340:
        AlphaRefVal = val & 0x1F;
        AlphaRef = (DispCnt & (1<<2)) ? AlphaRefVal : 0;
        return;

    case 0x04000350:
        ClearAttr1 = val;
        return;
    case 0x04000354:
        ClearAttr2 = val;
        return;

    case 0x04000358:
        FogColor = val;
        return;
    case 0x0400035C:
        FogOffset = val & 0x7FFF;
        return;

    case 0x04000600:
        if (val & 0x8000)
        {
            GXStat &= ~0x8000;
            ProjMatrixStackPointer = 0;
            //PosMatrixStackPointer = 0;
            TexMatrixStackPointer = 0; // CHECKME
        }
        val &= 0xC0000000;
        GXStat &= 0x3FFFFFFF;
        GXStat |= val;
        CheckFIFOIRQ();
        return;

    case 0x04000610:
        val &= 0x7FFF;
        ZeroDotWLimit = (val * 0x200) + 0x1FF;
        return;
    }

    if (addr >= 0x04000400 && addr < 0x04000440)
    {
        WriteToGXFIFO(val);
        return;
    }

    if (addr >= 0x04000440 && addr < 0x040005CC)
    {
        CmdFIFOEntry entry;
        entry.Command = (addr & 0x1FC) >> 2;
        entry.Param = val;
        CmdFIFOWrite(entry);
        return;
    }

    if (addr >= 0x04000330 && addr < 0x04000340)
    {
        addr = (addr - 0x04000330) >> 1;
        EdgeTable[addr] = val & 0xFFFF;
        EdgeTable[addr+1] = val >> 16;
        return;
    }

    if (addr >= 0x04000360 && addr < 0x04000380)
    {
        addr -= 0x04000360;
        FogDensityTable[addr] = val & 0x7F;
        FogDensityTable[addr+1] = (val >> 8) & 0x7F;
        FogDensityTable[addr+2] = (val >> 16) & 0x7F;
        FogDensityTable[addr+3] = (val >> 24) & 0x7F;
        return;
    }

    if (addr >= 0x04000380 && addr < 0x040003C0)
    {
        addr = (addr - 0x04000380) >> 1;
        ToonTable[addr] = val & 0xFFFF;
        ToonTable[addr+1] = val >> 16;
        return;
    }

    Log(LogLevel::Debug, "unknown GPU3D write32 %08X %08X\n", addr, val);
}

void GPU3D::Blit(const GPU& gpu) noexcept
{
    if (CurrentRenderer)
        CurrentRenderer->Blit(gpu);
}

Renderer3D::Renderer3D(bool Accelerated)
: Accelerated(Accelerated)
{ }

}

