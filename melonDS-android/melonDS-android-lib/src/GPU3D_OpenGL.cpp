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

#include "GPU3D_OpenGL.h"

#include <algorithm>
#include <cmath>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D_OpenGL_shaders.h"
#include "GPU_OpenGL_Timer.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

bool GLRenderer::BuildRenderShader(u32 flags, const std::string& vs, const std::string& fs)
{
    char shadername[32];
    snprintf(shadername, sizeof(shadername), "RenderShader%02X", flags);

    int headerlen = strlen(kShaderHeader);

    std::string vsbuf;
    vsbuf += kShaderHeader;
    vsbuf += kRenderVSCommon;
    vsbuf += vs;

    std::string fsbuf;
    fsbuf += kShaderHeader;
    fsbuf += kRenderFSCommon;
    fsbuf += fs;

    GLuint prog;
    bool ret = OpenGL::CompileVertexFragmentProgram(prog,
        vsbuf, fsbuf,
        shadername,
        {{"vPosition", 0}, {"vColor", 1}, {"vTexcoord", 2}, {"vPolygonAttr", 3}, {"vHDTexture", 4},
         {"vViewPosition", 5}, {"vViewNormal", 6}},
        {{"oColor", 0}, {"oAttr", 1}, {"oViewPosition", 2}, {"oViewNormal", 3}});

    if (!ret) return false;

    GLint uni_id = glGetUniformBlockIndex(prog, "uConfig");
    glUniformBlockBinding(prog, uni_id, 0);

    glUseProgram(prog);

    uni_id = glGetUniformLocation(prog, "TexMem");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(prog, "TexPalMem");
    glUniform1i(uni_id, 1);
    uni_id = glGetUniformLocation(prog, "HDAtlas");
    glUniform1i(uni_id, 2);

    RenderShader[flags] = prog;

    return true;
}

void GLRenderer::UseRenderShader(u32 flags)
{
    if (CurShaderID == flags) return;
    glUseProgram(RenderShader[flags]);
    CurShaderID = flags;
}

void SetupDefaultTexParams(GLuint tex)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

GLRenderer::GLRenderer(GLCompositor&& compositor) noexcept :
    Renderer3D(true),
    CurGLCompositor(std::move(compositor))
{
    // GLRenderer::New() will be used to actually initialize the renderer;
    // The various glDelete* functions silently ignore invalid IDs,
    // so we can just let the destructor clean up a half-initialized renderer.
}

std::unique_ptr<GLRenderer> GLRenderer::New() noexcept
{
    assert(glEnable != nullptr);

    std::optional<GLCompositor> compositor =  GLCompositor::New();
    if (!compositor)
        return nullptr;

    // Will be returned if the initialization succeeds,
    // or cleaned up via RAII if it fails.
    std::unique_ptr<GLRenderer> result = std::unique_ptr<GLRenderer>(new GLRenderer(std::move(*compositor)));
    compositor = std::nullopt;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);

    glDepthRangef(0.0f, 1.0f);
    glClearDepthf(1.0f);

    if (!OpenGL::CompileVertexFragmentProgram(result->ClearShaderPlain,
            kClearVS, kClearFS,
            "ClearShader",
            {{"vPosition", 0}},
            {{"oColor", 0}, {"oAttr", 1}}))
        return nullptr;

    result->ClearUniformLoc[0] = glGetUniformLocation(result->ClearShaderPlain, "uColor");
    result->ClearUniformLoc[1] = glGetUniformLocation(result->ClearShaderPlain, "uDepth");
    result->ClearUniformLoc[2] = glGetUniformLocation(result->ClearShaderPlain, "uOpaquePolyID");
    result->ClearUniformLoc[3] = glGetUniformLocation(result->ClearShaderPlain, "uFogFlag");

    memset(result->RenderShader, 0, sizeof(RenderShader));

    if (!result->BuildRenderShader(0, kRenderVS_Z, kRenderFS_ZO))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_WBuffer, kRenderVS_W, kRenderFS_WO))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_Edge, kRenderVS_Z, kRenderFS_ZE))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_Edge | RenderFlag_WBuffer, kRenderVS_W, kRenderFS_WE))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_Trans, kRenderVS_Z, kRenderFS_ZT))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_Trans | RenderFlag_WBuffer, kRenderVS_W, kRenderFS_WT))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_ShadowMask, kRenderVS_Z, kRenderFS_ZSM))
        return nullptr;

    if (!result->BuildRenderShader(RenderFlag_ShadowMask | RenderFlag_WBuffer, kRenderVS_W, kRenderFS_WSM))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(result->FinalPassEdgeShader,
            kFinalPassVS, kFinalPassEdgeFS,
            "FinalPassEdgeShader",
            {{"vPosition", 0}},
            {{"oColor", 0}}))
        return nullptr;
    if (!OpenGL::CompileVertexFragmentProgram(result->FinalPassFogShader,
            kFinalPassVS, kFinalPassFogFS,
            "FinalPassFogShader",
            {{"vPosition", 0}},
            {{"oColor", 0}}))
        return nullptr;

    GLuint uni_id = glGetUniformBlockIndex(result->FinalPassEdgeShader, "uConfig");
    glUniformBlockBinding(result->FinalPassEdgeShader, uni_id, 0);

    glUseProgram(result->FinalPassEdgeShader);
    uni_id = glGetUniformLocation(result->FinalPassEdgeShader, "DepthBuffer");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(result->FinalPassEdgeShader, "AttrBuffer");
    glUniform1i(uni_id, 1);

    uni_id = glGetUniformBlockIndex(result->FinalPassFogShader, "uConfig");
    glUniformBlockBinding(result->FinalPassFogShader, uni_id, 0);

    glUseProgram(result->FinalPassFogShader);
    uni_id = glGetUniformLocation(result->FinalPassFogShader, "DepthBuffer");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(result->FinalPassFogShader, "AttrBuffer");
    glUniform1i(uni_id, 1);


    // lighting effects (Pomegrade)
    if (!OpenGL::CompileVertexFragmentProgram(result->LightingAOShader,
            kFinalPassVS, kLightingAOFS,
            "LightingAOShader",
            {{"vPosition", 0}},
            {{"oAO", 0}, {"oBounce", 1}}))
        return nullptr;
    glUseProgram(result->LightingAOShader);
    glUniform1i(glGetUniformLocation(result->LightingAOShader, "GPosition"), 0);
    glUniform1i(glGetUniformLocation(result->LightingAOShader, "GNormal"), 1);
    glUniform1i(glGetUniformLocation(result->LightingAOShader, "Color"), 2);
    result->LightingAORadiusLoc = glGetUniformLocation(result->LightingAOShader, "uRadius");
    result->LightingAOPixelInWLoc = glGetUniformLocation(result->LightingAOShader, "uPixelInW");
    result->LightingBounceRadiusLoc = glGetUniformLocation(result->LightingAOShader, "uBounceRadius");

    if (!OpenGL::CompileVertexFragmentProgram(result->LightingComposeShader,
            kFinalPassVS, kLightingComposeFS,
            "LightingComposeShader",
            {{"vPosition", 0}},
            {{"oColor", 0}}))
        return nullptr;
    glUseProgram(result->LightingComposeShader);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "Color"), 0);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "GPosition"), 1);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "GNormal"), 2);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "AO"), 3);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "Bounce"), 4);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "ShadowMap"), 5);
    glUniform1i(glGetUniformLocation(result->LightingComposeShader, "ShadowDepthMap"), 6);
    {
        const char* names[7] = {"uShadowStrength", "uLightRight", "uLightUp", "uLightDir", "uShadowBounds", "uShadowDepth", "uShadowTexel"};
        for (int i = 0; i < 7; i++)
            result->ComposeShadowLoc[i] = glGetUniformLocation(result->LightingComposeShader, names[i]);
    }

    {
        const char* names[4] = {"uReflectionStrength", "uProj", "uViewport", "uScale"};
        for (int i = 0; i < 4; i++)
            result->ComposeReflectionLoc[i] = glGetUniformLocation(result->LightingComposeShader, names[i]);
    }

    if (!OpenGL::CompileVertexFragmentProgram(result->LightingShadowShader,
            kLightingShadowVS, kLightingShadowFS,
            "LightingShadowShader",
            {{"vViewPosition", 5}},
            {}))
        return nullptr;
    {
        const char* names[5] = {"uLightRight", "uLightUp", "uLightDir", "uShadowBounds", "uShadowDepth"};
        for (int i = 0; i < 5; i++)
            result->ShadowLoc[i] = glGetUniformLocation(result->LightingShadowShader, names[i]);
    }
    result->LightingComposeAOLoc = glGetUniformLocation(result->LightingComposeShader, "uAmbientOcclusion");
    result->LightingComposeBounceLoc = glGetUniformLocation(result->LightingComposeShader, "uBounceIntensity");
    result->ComposeAttrLoc[0] = glGetUniformLocation(result->LightingComposeShader, "AttrBuf");
    result->ComposeAttrLoc[1] = glGetUniformLocation(result->LightingComposeShader, "uSceneryId");

    // lighting terms at high resolutions (see MaxLightingScale)
    if (!OpenGL::CompileVertexFragmentProgram(result->LightingDownsampleShader,
            kFinalPassVS, kLightingDownsampleFS,
            "LightingDownsampleShader",
            {{"vPosition", 0}},
            {{"oPosition", 0}, {"oNormal", 1}, {"oColor", 2}}))
        return nullptr;
    glUseProgram(result->LightingDownsampleShader);
    glUniform1i(glGetUniformLocation(result->LightingDownsampleShader, "GPosition"), 0);
    glUniform1i(glGetUniformLocation(result->LightingDownsampleShader, "GNormal"), 1);
    glUniform1i(glGetUniformLocation(result->LightingDownsampleShader, "Color"), 2);
    result->DownsampleFactorLoc = glGetUniformLocation(result->LightingDownsampleShader, "uFactor");

    if (!OpenGL::CompileVertexFragmentProgram(result->LightingTermsShader,
            kFinalPassVS, kLightingTermsFS,
            "LightingTermsShader",
            {{"vPosition", 0}},
            {{"oTerms", 0}, {"oBounce", 1}, {"oReflected", 2}}))
        return nullptr;
    glUseProgram(result->LightingTermsShader);
    // the compose shader's texture units
    {
        const char* samplers[7] = {"Color", "GPosition", "GNormal", "AO", "Bounce", "ShadowMap", "ShadowDepthMap"};
        for (int i = 0; i < 7; i++)
            glUniform1i(glGetUniformLocation(result->LightingTermsShader, samplers[i]), i);
        const char* shadow[7] = {"uShadowStrength", "uLightRight", "uLightUp", "uLightDir", "uShadowBounds", "uShadowDepth", "uShadowTexel"};
        for (int i = 0; i < 7; i++)
            result->TermsShadowLoc[i] = glGetUniformLocation(result->LightingTermsShader, shadow[i]);
        const char* reflection[4] = {"uReflectionStrength", "uProj", "uViewport", "uScale"};
        for (int i = 0; i < 4; i++)
            result->TermsReflectionLoc[i] = glGetUniformLocation(result->LightingTermsShader, reflection[i]);
    }

    if (!OpenGL::CompileVertexFragmentProgram(result->LightingUpsampleShader,
            kFinalPassVS, kLightingUpsampleFS,
            "LightingUpsampleShader",
            {{"vPosition", 0}},
            {{"oColor", 0}}))
        return nullptr;
    glUseProgram(result->LightingUpsampleShader);
    {
        const char* samplers[9] = {"Color", "GPosition", "GNormal", "LowPosition", "LowNormal", "Terms", "TermsBounce", "TermsReflected", "LowAO"};
        for (int i = 0; i < 9; i++)
            glUniform1i(glGetUniformLocation(result->LightingUpsampleShader, samplers[i]), i);
        const char* names[6] = {"uFactor", "uAmbientOcclusion", "uBounceIntensity", "uShadowStrength", "uLightDir", "uReflectionStrength"};
        for (int i = 0; i < 6; i++)
            result->UpsampleLoc[i] = glGetUniformLocation(result->LightingUpsampleShader, names[i]);
    }
    result->UpsampleAttrLoc[0] = glGetUniformLocation(result->LightingUpsampleShader, "AttrBuf");
    result->UpsampleAttrLoc[1] = glGetUniformLocation(result->LightingUpsampleShader, "uSceneryId");


    memset(&result->ShaderConfig, 0, sizeof(ShaderConfig));

    glGenBuffers(1, &result->ShaderConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, result->ShaderConfigUBO);
    static_assert((sizeof(ShaderConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ShaderConfig), &result->ShaderConfig, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, result->ShaderConfigUBO);


    float clearvtx[6*2] =
    {
        -1.0, -1.0,
        1.0, 1.0,
        -1.0, 1.0,

        -1.0, -1.0,
        1.0, -1.0,
        1.0, 1.0
    };

    glGenBuffers(1, &result->ClearVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, result->ClearVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(clearvtx), clearvtx, GL_STATIC_DRAW);

    glGenVertexArrays(1, &result->ClearVertexArrayID);
    glBindVertexArray(result->ClearVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(0));


    glGenBuffers(1, &result->VertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, result->VertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, result->VertexBuffer.size() * sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &result->VertexArrayID);
    glBindVertexArray(result->VertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribIPointer(0, 4, GL_UNSIGNED_SHORT, VertexSize*4, (void*)(0));
    glEnableVertexAttribArray(1); // color
    glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, VertexSize*4, (void*)(2*4));
    glEnableVertexAttribArray(2); // texcoords
    glVertexAttribIPointer(2, 2, GL_SHORT, VertexSize*4, (void*)(3*4));
    glEnableVertexAttribArray(3); // attrib
    glVertexAttribIPointer(3, 3, GL_UNSIGNED_INT, VertexSize*4, (void*)(4*4));
    glEnableVertexAttribArray(4); // HD texture atlas location
    glVertexAttribIPointer(4, 1, GL_INT, VertexSize*4, (void*)(7*4));

    // view-space data for the lighting effects, enabled per frame (RenderFrame)
    glGenBuffers(1, &result->ViewVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, result->ViewVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, result->ViewVertexBuffer.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, ViewVertexSize*4, (void*)(0));
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, ViewVertexSize*4, (void*)(4*4));

    glGenBuffers(1, &result->IndexBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result->IndexBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, result->IndexBuffer.size() * sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

    glGenFramebuffers(1, &result->MainFramebuffer);
    glGenFramebuffers(1, &result->DownscaleFramebuffer);
    glGenFramebuffers(1, &result->AOFramebuffer);
    glGenFramebuffers(1, &result->LightingFramebuffer);
    glGenFramebuffers(1, &result->LowGBufferFramebuffer);
    glGenFramebuffers(1, &result->TermsFramebuffer);
    glGenFramebuffers(1, &result->ShadowFramebuffer);

    // color buffers
    glGenTextures(1, &result->ColorBufferTex);
    SetupDefaultTexParams(result->ColorBufferTex);

    // depth/stencil buffer
    glGenTextures(1, &result->DepthBufferTex);
    SetupDefaultTexParams(result->DepthBufferTex);

    // attribute buffer
    // R: opaque polyID (for edgemarking)
    // G: edge flag
    // B: fog flag
    glGenTextures(1, &result->AttrBufferTex);
    SetupDefaultTexParams(result->AttrBufferTex);

    // downscale framebuffer for display capture (always 256x192)
    glGenTextures(1, &result->DownScaleBufferTex);
    SetupDefaultTexParams(result->DownScaleBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);

    glGenBuffers(1, &result->PixelbufferID);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &result->TexMemID);
    glBindTexture(GL_TEXTURE_2D, result->TexMemID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, 512, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);

    glActiveTexture(GL_TEXTURE1);
    glGenTextures(1, &result->TexPalMemID);
    glBindTexture(GL_TEXTURE_2D, result->TexPalMemID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024, 48, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, NULL);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return result;
}

GLRenderer::~GLRenderer()
{
    assert(glDeleteTextures != nullptr);

    GLTimer::Release();

    glDeleteTextures(1, &TexMemID);
    glDeleteTextures(1, &TexPalMemID);

    glDeleteFramebuffers(1, &MainFramebuffer);
    glDeleteFramebuffers(1, &DownscaleFramebuffer);
    glDeleteTextures(1, &ColorBufferTex);
    glDeleteTextures(1, &DepthBufferTex);
    glDeleteTextures(1, &AttrBufferTex);
    glDeleteTextures(1, &DownScaleBufferTex);

    glDeleteFramebuffers(1, &AOFramebuffer);
    glDeleteFramebuffers(1, &LightingFramebuffer);
    glDeleteFramebuffers(1, &LowGBufferFramebuffer);
    glDeleteFramebuffers(1, &TermsFramebuffer);
    for (GLuint* tex : {&LowPositionTex, &LowNormalTex, &LowColorTex, &TermsTex, &TermsBounceTex, &TermsReflectedTex})
        glDeleteTextures(1, tex);
    glDeleteProgram(LightingDownsampleShader);
    glDeleteProgram(LightingTermsShader);
    glDeleteProgram(LightingUpsampleShader);
    glDeleteTextures(1, &ViewPositionTex);
    glDeleteTextures(1, &ViewNormalTex);
    glDeleteTextures(1, &AOTex);
    glDeleteTextures(1, &BounceTex);
    glDeleteTextures(1, &LightingTex);
    glDeleteTextures(1, &LitDepthTex);
    glDeleteTextures(1, &LitAttrTex);
    glDeleteBuffers(1, &ViewVertexBufferID);
    glDeleteProgram(LightingAOShader);
    glDeleteProgram(LightingComposeShader);
    glDeleteProgram(LightingShadowShader);
    glDeleteTextures(1, &BackupColorTex);
    glDeleteTextures(1, &BackupLightingTex);
    glDeleteFramebuffers(2, CopyFramebuffers);
    glDeleteFramebuffers(1, &ShadowFramebuffer);
    glDeleteTextures(1, &ShadowMapTex);
    glDeleteSamplers(1, &ShadowDepthSampler);
    glDeleteBuffers(1, &ShadowCasterBufferID);
    glDeleteVertexArrays(1, &ShadowCasterArrayID);

    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteBuffers(1, &VertexBufferID);
    glDeleteVertexArrays(1, &ClearVertexArrayID);
    glDeleteBuffers(1, &ClearVertexBufferID);

    glDeleteBuffers(1, &ShaderConfigUBO);

    for (int i = 0; i < 16; i++)
    {
        if (!RenderShader[i]) continue;
        glDeleteProgram(RenderShader[i]);
    }
}

void GLRenderer::Reset(GPU& gpu)
{
    // This is where the compositor's Reset() method would be called,
    // except there's no such method right now.

    HDTextures.Reset();
}

void GLRenderer::SetBetterPolygons(bool betterpolygons) noexcept
{
    SetRenderSettings(betterpolygons, ScaleFactor);
}

void GLRenderer::SetScaleFactor(int scale) noexcept
{
    SetRenderSettings(BetterPolygons, scale);
}


void GLRenderer::SetRenderSettings(bool betterpolygons, int scale) noexcept
{
    if (betterpolygons == BetterPolygons && scale == ScaleFactor)
        return;

    CurGLCompositor.SetScaleFactor(scale);
    ScaleFactor = scale;
    BetterPolygons = betterpolygons;

    ScreenW = 256 * scale;
    ScreenH = 192 * scale;

    glBindTexture(GL_TEXTURE_2D, ColorBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, DepthBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ScreenW, ScreenH, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glBindTexture(GL_TEXTURE_2D, AttrBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ScreenW, ScreenH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindFramebuffer(GL_FRAMEBUFFER, DownscaleFramebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, DownScaleBufferTex, 0);

    GLenum fbassign[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ColorBufferTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, DepthBufferTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, AttrBufferTex, 0);
    // lighting targets still at the old size would limit rendering to their
    // area: detached here, made again at the new size when next used
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, 0, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, 0, 0);
    LightingTargetsW = LightingTargetsH = 0;
    glDrawBuffers(2, fbassign);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*192*4, NULL, GL_DYNAMIC_READ);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //glLineWidth(scale);
    //glLineWidth(1.5);
}


void GLRenderer::SetupPolygon(GLRenderer::RendererPolygon* rp, Polygon* polygon) const
{
    rp->PolyData = polygon;

    // render key: depending on what we're drawing
    // opaque polygons:
    // - depthfunc
    // -- alpha=0
    // regular translucent polygons:
    // - depthfunc
    // -- depthwrite
    // --- polyID
    // ---- need opaque
    // shadow mask polygons:
    // - depthfunc?????
    // shadow polygons:
    // - depthfunc
    // -- depthwrite
    // --- polyID

    rp->RenderKey = (polygon->Attr >> 14) & 0x1; // bit14 - depth func
    if (!polygon->IsShadowMask)
    {
        if (polygon->Translucent)
        {
            if (polygon->IsShadow) rp->RenderKey |= 0x20000;
            else                   rp->RenderKey |= 0x10000;
            rp->RenderKey |= (polygon->Attr >> 10) & 0x2; // bit11 - depth write
            rp->RenderKey |= (polygon->Attr >> 13) & 0x4; // bit15 - fog
            rp->RenderKey |= (polygon->Attr & 0x3F000000) >> 16; // polygon ID
            if ((polygon->Attr & 0x001F0000) == 0x001F0000) // need opaque
                rp->RenderKey |= 0x4000;
        }
        else
        {
            if ((polygon->Attr & 0x001F0000) == 0)
                rp->RenderKey |= 0x2;
            rp->RenderKey |= (polygon->Attr & 0x3F000000) >> 16; // polygon ID
        }
    }
    else
    {
        rp->RenderKey |= 0x30000;
    }
}

bool GLRenderer::DepthPlane(const Polygon* poly, double plane[3]) const
{
    // Pomegrade: rounding a vertex to a whole output pixel moves it on screen
    // but not in depth, so two coplanar polygons (a floor, a decal on it) get
    // different screen-space depth planes, differing by far more than the DS's
    // "depth equal" margin where the depth changes fast (towards the horizon).
    // In Z-buffer mode, depth is affine on screen: fitted here from the exact
    // screen positions (output units), it gives each rounded vertex the depth
    // its polygon really has there.
    if (HighPrecision || poly->WBuffer || poly->NumVertices < 3)
        return false;
    const double scale = ScaleFactor;
    auto pos = [&](u32 j, double& x, double& y) {
        x = poly->Vertices[j]->PreciseScreen[0] * scale;
        y = poly->Vertices[j]->PreciseScreen[1] * scale;
    };
    double x0, y0;
    pos(0, x0, y0);
    double best = 0, a = 0, b = 0;
    for (u32 j = 1; j + 1 < poly->NumVertices; j++)
        for (u32 k = j + 1; k < poly->NumVertices; k++)
        {
            double x1, y1, x2, y2;
            pos(j, x1, y1);
            pos(k, x2, y2);
            double det = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
            if (std::abs(det) <= best) continue;
            double dz1 = (double)poly->FinalZ[j] - poly->FinalZ[0], dz2 = (double)poly->FinalZ[k] - poly->FinalZ[0];
            best = std::abs(det);
            a = (dz1 * (y2 - y0) - dz2 * (y1 - y0)) / det;
            b = (dz2 * (x1 - x0) - dz1 * (x2 - x0)) / det;
        }
    // degenerate on screen (or a vertex the DS couldn't place): keep its depths
    if (best < 0.25 * scale * scale)
        return false;
    plane[0] = a;
    plane[1] = b;
    plane[2] = poly->FinalZ[0] - a * x0 - b * y0;
    return true;
}

u32 GLRenderer::PlaneDepth(const double plane[3], u32 x, u32 y) noexcept
{
    double z = plane[0] * x + plane[1] * y + plane[2];
    return (u32)std::clamp(std::lround(z), 0L, 0xFFFFFFL);
}

u32* GLRenderer::SetupVertex(const Polygon* poly, int vid, const Vertex* vtx, u32 vtxattr, u32 hdTexture, u32* vptr, u32 procedural) const
{
    u32 z = poly->FinalZ[vid];
    u32 w = poly->FinalW[vid];

    u32 alpha = (poly->Attr >> 16) & 0x1F;

    // Z should always fit within 16 bits, so it's okay to do this
    u32 zshift = 0;
    while (z > 0xFFFF) { z >>= 1; zshift++; }

    u32 x, y;
    if (HighPrecision)
    {
        // positions in 1/SubpixelScale() of an output pixel; uScreenSize is scaled to match
        float scale = (float)(ScaleFactor * SubpixelScale());
        x = (u32)std::clamp(std::lround(vtx->PreciseScreen[0] * scale), 0L, 0xFFFFL);
        y = (u32)std::clamp(std::lround(vtx->PreciseScreen[1] * scale), 0L, 0xFFFFL);
    }
    else if (ScaleFactor > 1)
    {
        x = (vtx->HiresPosition[0] * ScaleFactor) >> 4;
        y = (vtx->HiresPosition[1] * ScaleFactor) >> 4;
    }
    else
    {
        x = vtx->FinalPosition[0];
        y = vtx->FinalPosition[1];
    }

    // correct nearly-vertical edges that would look vertical on the DS
    /*{
        int vtopid = vid - 1;
        if (vtopid < 0) vtopid = poly->NumVertices-1;
        Vertex* vtop = poly->Vertices[vtopid];
        if (vtop->FinalPosition[1] >= vtx->FinalPosition[1])
        {
            vtopid = vid + 1;
            if (vtopid >= poly->NumVertices) vtopid = 0;
            vtop = poly->Vertices[vtopid];
        }
        if ((vtop->FinalPosition[1] < vtx->FinalPosition[1]) &&
            (vtx->FinalPosition[0] == vtop->FinalPosition[0]-1))
        {
            if (ScaleFactor > 1)
                x = (vtop->HiresPosition[0] * ScaleFactor) >> 4;
            else
                x = vtop->FinalPosition[0];
        }
    }*/

    double plane[3];
    if (DepthPlane(poly, plane))
    {
        z = PlaneDepth(plane, x, y);
        zshift = 0;
        while (z > 0xFFFF) { z >>= 1; zshift++; }
    }

    *vptr++ = x | (y << 16);
    *vptr++ = z | (w << 16);

    *vptr++ =  (vtx->FinalColor[0] >> 1) |
              ((vtx->FinalColor[1] >> 1) << 8) |
              ((vtx->FinalColor[2] >> 1) << 16) |
              (alpha << 24);

    *vptr++ = (u16)vtx->TexCoords[0] | ((u16)vtx->TexCoords[1] << 16);

    // Split TexParam into 2 because some GPUs don't have 32 bit ints. TexPalette only uses 13 bits
    *vptr++ = vtxattr | (zshift << 16);
    *vptr++ = (poly->TexParam & 0xFFFF) | (procedural << 16);
    *vptr++ = (poly->TexParam >> 16 ) | (poly->TexPalette << 16);
    *vptr++ = hdTexture;

    return vptr;
}

float* GLRenderer::SetupViewVertex(const Vertex* vtx, float* gptr) const
{
    for (int i = 0; i < 3; i++) *gptr++ = vtx->ViewPosition[i];
    *gptr++ = vtx->Orthographic ? 0.f : 1.f;
    for (int i = 0; i < 3; i++) *gptr++ = vtx->ViewNormal[i];
    *gptr++ = vtx->Specular;
    return gptr;
}

float* GLRenderer::SetupViewCenterVertex(const Polygon* poly, float* gptr) const
{
    // the centre vertex sits at the average screen position: its view-space
    // data is the perspective-correct average (weights 1/W), as for its colour
    float pos[3] = {}, normal[3] = {}, specular = 0, weight = 0;
    bool perspective = true;
    for (u32 j = 0; j < poly->NumVertices; j++)
    {
        const Vertex* vtx = poly->Vertices[j];
        float w = 1.0f / (float)std::max(poly->FinalW[j], 1);
        for (int i = 0; i < 3; i++)
        {
            pos[i] += vtx->ViewPosition[i] * w;
            normal[i] += vtx->ViewNormal[i] * w;
        }
        specular += vtx->Specular * w;
        weight += w;
        perspective = perspective && !vtx->Orthographic;
    }
    for (int i = 0; i < 3; i++) *gptr++ = pos[i] / weight;
    *gptr++ = perspective ? 1.f : 0.f;
    for (int i = 0; i < 3; i++) *gptr++ = normal[i] / weight;
    *gptr++ = specular / weight;
    return gptr;
}

void GLRenderer::LookupHDTextures(GPU& gpu, int npolys)
{
    PerformanceCounters::CpuScope cpuTime(PerformanceCounters::Section::TexturesCpu);
    bool textured = gpu.GPU3D.RenderDispCnt & (1<<0);
    // relief by material reads texels from the flat texture VRAM
    HDTextures.SetKeepCoherent(ViewDataActive && Relief > 0);
    if (!HDTextures.BeginFrame(gpu) || !textured)
    {
        for (int i = 0; i < npolys; i++)
            PolygonList[i].HDTexture = 0;
        return;
    }

    // the atlas lives on texture unit 2, don't disturb the rest of the renderer
    GLint prevActiveTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);

    // looked up again if the atlas had to be rebuilt along the way
    do
    {
        u32 prevParam = 0, prevPal = 0, prevInfo = 0;
        bool havePrev = false;
        for (int i = 0; i < npolys; i++)
        {
            Polygon* poly = PolygonList[i].PolyData;
            u32 info = 0;
            if (((poly->TexParam >> 26) & 0x7) != 0)
            {
                // consecutive polygons very often share a texture
                if (havePrev && poly->TexParam == prevParam && poly->TexPalette == prevPal)
                    info = prevInfo;
                else
                {
                    info = HDTextures.Lookup(gpu, poly->TexParam, poly->TexPalette);
                    prevParam = poly->TexParam;
                    prevPal = poly->TexPalette;
                    prevInfo = info;
                    havePrev = true;
                }
            }
            PolygonList[i].HDTexture = info;
        }
    }
    while (HDTextures.ConsumeAtlasRebuilt());

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, HDTextures.AtlasTexture());
    glActiveTexture(prevActiveTexture);
}

void GLRenderer::LookupReliefScales(GPU& gpu, int npolys)
{
    const bool active = ViewDataActive && Relief > 0 && (gpu.GPU3D.RenderDispCnt & (1<<0));
    // the frame's polygon IDs per texture: evidence for the character class
    // (relief) and characters' softer shade (lighting effects)
    std::unordered_map<u64, u64> textureIds;
    if (active || LightingActive)
        for (int i = 0; i < npolys; i++)
        {
            const Polygon* poly = PolygonList[i].PolyData;
            if ((poly->TexParam >> 26) & 0x7)
                textureIds[TexcacheKey(poly->TexParam, poly->TexPalette)] |= 1ull << ((poly->Attr >> 24) & 0x3F);
        }
    int counts[64] = {};
    for (auto& [key, ids] : textureIds)
        for (int n = 0; n < 64; n++) counts[n] += (ids >> n) & 1;
    FrameSceneryId = MaterialClassifier::SceneryId(counts);
    MaterialRelief.BeginFrame(HDTextures.TexturesChanged(), FrameSceneryId);
    u32 prevParam = 0, prevPal = 0, prevScale = 0;
    bool havePrev = false;
    for (int i = 0; i < npolys; i++)
    {
        Polygon* poly = PolygonList[i].PolyData;
        u32 scale = 0;
        if (active && ((poly->TexParam >> 26) & 0x7) != 0)
        {
            if (havePrev && poly->TexParam == prevParam && poly->TexPalette == prevPal)
                scale = prevScale;
            else
            {
                scale = MaterialRelief.Scale(gpu, poly->TexParam, poly->TexPalette, textureIds[TexcacheKey(poly->TexParam, poly->TexPalette)]);
                prevParam = poly->TexParam; prevPal = poly->TexPalette; prevScale = scale;
                havePrev = true;
            }
        }
        PolygonList[i].ReliefScale = scale;
    }
}

void GLRenderer::EnsureCapacity(Polygon** polygons, u32 npolys)
{
    // upper bounds per polygon of n vertices, as BuildPolygons fills them:
    // n vertices + 1 centre vertex, at most 3n triangle indices, 2n edge indices
    u32 vertices = 0, triIndices = 0, edgeIndices = 0;
    for (u32 i = 0; i < npolys; i++)
    {
        u32 n = polygons[i]->NumVertices;
        vertices += n + 1;
        triIndices += 3 * n;
        edgeIndices += 2 * n;
    }

    if (npolys > PolygonList.size())
        PolygonList.resize(npolys);

    if (vertices * VertexSize > VertexBuffer.size())
    {
        VertexBuffer.resize(vertices * VertexSize);
        glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, VertexBuffer.size() * sizeof(u32), nullptr, GL_DYNAMIC_DRAW);
    }

    if (ViewDataActive && vertices * ViewVertexSize > ViewVertexBuffer.size())
    {
        ViewVertexBuffer.resize(vertices * ViewVertexSize);
        glBindBuffer(GL_ARRAY_BUFFER, ViewVertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, ViewVertexBuffer.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    }

    if (triIndices > EdgeIndicesOffset || EdgeIndicesOffset + edgeIndices > IndexBuffer.size())
    {
        EdgeIndicesOffset = std::max(EdgeIndicesOffset, triIndices);
        IndexBuffer.resize(std::max<size_t>(IndexBuffer.size(), EdgeIndicesOffset + edgeIndices));
        // the element buffer binding belongs to the vertex array
        glBindVertexArray(VertexArrayID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer.size() * sizeof(u32), nullptr, GL_DYNAMIC_DRAW);
    }
}

void GLRenderer::BuildPolygons(GLRenderer::RendererPolygon* polygons, int npolys)
{
    u32* vptr = &VertexBuffer[0];
    u32 vidx = 0;
    // view-space data, one entry per vertex of VertexBuffer (lighting effects)
    float* gptr = &ViewVertexBuffer[0];
    const bool viewdata = ViewDataActive;
    memset(LightUse, 0, sizeof(LightUse));

    u32 iidx = 0;
    u32 eidx = EdgeIndicesOffset;

    for (int i = 0; i < npolys; i++)
    {
        RendererPolygon* rp = &polygons[i];
        Polygon* poly = rp->PolyData;

        if (viewdata && !poly->Translucent)
            for (int l = 0; l < 4; l++)
                if (poly->Attr & (1 << l)) LightUse[l]++;

        rp->IndicesOffset = iidx;
        rp->NumIndices = 0;

        u32 vidx_first = vidx;

        u32 polyattr = poly->Attr;

        u32 alpha = (polyattr >> 16) & 0x1F;

        u32 vtxattr = polyattr & 0x1F00C8F0;
        vtxattr |= rp->ReliefScale & (0xF | GLMaterialRelief::VolumetricGrass | GLMaterialRelief::Fabric); // bits 0-3, 12, 13: free in the DS attributes kept here
        if (poly->FacingView) vtxattr |= (1<<8);
        if (poly->WBuffer)    vtxattr |= (1<<9);

        // assemble vertices
        if (poly->Type == 1) // line
        {
            rp->PrimType = GL_LINES;

            u32 lastx, lasty;
            int nout = 0;
            for (u32 j = 0; j < poly->NumVertices; j++)
            {
                Vertex* vtx = poly->Vertices[j];

                if (j > 0)
                {
                    if (lastx == vtx->FinalPosition[0] &&
                        lasty == vtx->FinalPosition[1]) continue;
                }

                lastx = vtx->FinalPosition[0];
                lasty = vtx->FinalPosition[1];

                vptr = SetupVertex(poly, j, vtx, vtxattr, rp->HDTexture, vptr, (rp->ReliefScale >> GLMaterialRelief::ProceduralShift) & 0xF);
                if (viewdata) gptr = SetupViewVertex(vtx, gptr);

                IndexBuffer[iidx++] = vidx;
                rp->NumIndices++;

                vidx++;
                nout++;
                if (nout >= 2) break;
            }
        }
        else if (poly->NumVertices == 3) // regular triangle
        {
            rp->PrimType = GL_TRIANGLES;

            for (int j = 0; j < 3; j++)
            {
                Vertex* vtx = poly->Vertices[j];

                vptr = SetupVertex(poly, j, vtx, vtxattr, rp->HDTexture, vptr, (rp->ReliefScale >> GLMaterialRelief::ProceduralShift) & 0xF);
                if (viewdata) gptr = SetupViewVertex(vtx, gptr);
                vidx++;
            }

            // build a triangle
            IndexBuffer[iidx++] = vidx_first;
            IndexBuffer[iidx++] = vidx - 2;
            IndexBuffer[iidx++] = vidx - 1;
            rp->NumIndices += 3;
        }
        else // quad, pentagon, etc
        {
            rp->PrimType = GL_TRIANGLES;

            if (!BetterPolygons)
            {
                // regular triangle-splitting

                for (u32 j = 0; j < poly->NumVertices; j++)
                {
                    Vertex* vtx = poly->Vertices[j];

                    vptr = SetupVertex(poly, j, vtx, vtxattr, rp->HDTexture, vptr, (rp->ReliefScale >> GLMaterialRelief::ProceduralShift) & 0xF);
                    if (viewdata) gptr = SetupViewVertex(vtx, gptr);

                    if (j >= 2)
                    {
                        // build a triangle
                        IndexBuffer[iidx++] = vidx_first;
                        IndexBuffer[iidx++] = vidx - 1;
                        IndexBuffer[iidx++] = vidx;
                        rp->NumIndices += 3;
                    }

                    vidx++;
                }
            }
            else
            {
                // attempt at 'better' splitting
                // this doesn't get rid of the error while splitting a bigger polygon into triangles
                // but we can attempt to reduce it

                u32 cX = 0, cY = 0;
                float cXf = 0, cYf = 0;
                float cZ = 0;
                float cW = 0;

                float cR = 0, cG = 0, cB = 0;
                float cS = 0, cT = 0;

                for (u32 j = 0; j < poly->NumVertices; j++)
                {
                    Vertex* vtx = poly->Vertices[j];

                    cX += vtx->HiresPosition[0];
                    cY += vtx->HiresPosition[1];
                    cXf += vtx->PreciseScreen[0];
                    cYf += vtx->PreciseScreen[1];

                    float fw = (float)poly->FinalW[j] * poly->NumVertices;
                    cW += 1.0f / fw;

                    if (poly->WBuffer) cZ += poly->FinalZ[j] / fw;
                    else               cZ += poly->FinalZ[j];

                    cR += (vtx->FinalColor[0] >> 1) / fw;
                    cG += (vtx->FinalColor[1] >> 1) / fw;
                    cB += (vtx->FinalColor[2] >> 1) / fw;

                    cS += vtx->TexCoords[0] / fw;
                    cT += vtx->TexCoords[1] / fw;
                }

                cX /= poly->NumVertices;
                cY /= poly->NumVertices;

                cW = 1.0f / cW;

                if (poly->WBuffer) cZ *= cW;
                else               cZ /= poly->NumVertices;

                cR *= cW;
                cG *= cW;
                cB *= cW;

                cS *= cW;
                cT *= cW;

                if (HighPrecision)
                {
                    float scale = (float)(ScaleFactor * SubpixelScale()) / poly->NumVertices;
                    cX = (u32)std::clamp(std::lround(cXf * scale), 0L, 0xFFFFL);
                    cY = (u32)std::clamp(std::lround(cYf * scale), 0L, 0xFFFFL);
                }
                else
                {
                    cX = (cX * ScaleFactor) >> 4;
                    cY = (cY * ScaleFactor) >> 4;
                }

                u32 w = (u32)cW;

                // Pomegrade: the centre's depth where it is drawn (see DepthPlane)
                double plane[3];
                if (DepthPlane(poly, plane))
                    cZ = (float)PlaneDepth(plane, cX, cY);

                u32 z = (u32)cZ;
                u32 zshift = 0;
                while (z > 0xFFFF) { z >>= 1; zshift++; }

                // build center vertex
                *vptr++ = cX | (cY << 16);
                *vptr++ = z | (w << 16);

                *vptr++ =  (u32)cR |
                          ((u32)cG << 8) |
                          ((u32)cB << 16) |
                          (alpha << 24);

                // Pomegrade: through a signed integer. A negative float converted
                // straight to an unsigned type is undefined: x86 happens to wrap
                // it, ARM64 clamps it to 0, so on phones the centre of every
                // quad with negative texture coordinates (repeated textures)
                // sampled the wrong texel and the texture swirled into it
                *vptr++ = (u16)(s32)std::lround(cS) | ((u32)(u16)(s32)std::lround(cT) << 16);

                // Split TexParam into 2 because some GPUs don't have 32 bit ints. TexPalette only uses 13 bits
                *vptr++ = vtxattr | (zshift << 16);
                *vptr++ = (poly->TexParam & 0xFFFF) | (((rp->ReliefScale >> GLMaterialRelief::ProceduralShift) & 0xF) << 16);
                *vptr++ = (poly->TexParam >> 16 ) | (poly->TexPalette << 16);
                *vptr++ = rp->HDTexture;
                if (viewdata) gptr = SetupViewCenterVertex(poly, gptr);

                vidx++;

                // build the final polygon
                for (u32 j = 0; j < poly->NumVertices; j++)
                {
                    Vertex* vtx = poly->Vertices[j];

                    vptr = SetupVertex(poly, j, vtx, vtxattr, rp->HDTexture, vptr, (rp->ReliefScale >> GLMaterialRelief::ProceduralShift) & 0xF);
                    if (viewdata) gptr = SetupViewVertex(vtx, gptr);

                    if (j >= 1)
                    {
                        // build a triangle
                        IndexBuffer[iidx++] = vidx_first;
                        IndexBuffer[iidx++] = vidx - 1;
                        IndexBuffer[iidx++] = vidx;
                        rp->NumIndices += 3;
                    }

                    vidx++;
                }

                IndexBuffer[iidx++] = vidx_first;
                IndexBuffer[iidx++] = vidx - 1;
                IndexBuffer[iidx++] = vidx_first + 1;
                rp->NumIndices += 3;
            }
        }

        if (ViewInspector)
        {
            // flat, untextured, in the polygon's inspector colour (alpha kept)
            // 5 bits per channel to the vertex colour's 8 (FinalColor >> 1: white is 255)
            u32 c = ViewInspector->ViewColour(*poly);
            auto to8 = [](u32 v) { return (v << 3) | (v >> 2); };
            u32 rgb = to8(c & 0x1F) | (to8((c >> 5) & 0x1F) << 8) | (to8((c >> 10) & 0x1F) << 16);
            for (u32 v = vidx_first; v < vidx; v++)
            {
                u32* vtx = &VertexBuffer[v * VertexSize];
                vtx[2] = (vtx[2] & 0xFF000000) | rgb;
                vtx[5] = 0;
                vtx[6] &= 0xFFFF0000; // texture format 0: none (the palette stays)
                vtx[7] = 0;
            }
        }

        rp->EdgeIndicesOffset = eidx;
        rp->NumEdgeIndices = 0;

        u32 vidx_cur = vidx_first;
        for (u32 j = 1; j < poly->NumVertices; j++)
        {
            IndexBuffer[eidx++] = vidx_cur;
            IndexBuffer[eidx++] = vidx_cur + 1;
            vidx_cur++;
            rp->NumEdgeIndices += 2;
        }
        IndexBuffer[eidx++] = vidx_cur;
        IndexBuffer[eidx++] = vidx_first;
        rp->NumEdgeIndices += 2;
    }

    NumVertices = vidx;
    NumIndices = iidx;
    NumEdgeIndices = eidx - EdgeIndicesOffset;
}

int GLRenderer::RenderSinglePolygon(int i) const
{
    const RendererPolygon* rp = &PolygonList[i];

    glDrawElements(rp->PrimType, rp->NumIndices, GL_UNSIGNED_INT, (void*)(uintptr_t)(rp->IndicesOffset * 4));

    return 1;
}

int GLRenderer::RenderPolygonBatch(int i) const
{
    const RendererPolygon* rp = &PolygonList[i];
    GLuint primtype = rp->PrimType;
    u32 key = rp->RenderKey;
    int numpolys = 0;
    u32 numindices = 0;

    for (int iend = i; iend < NumFinalPolys; iend++)
    {
        const RendererPolygon* cur_rp = &PolygonList[iend];
        if (cur_rp->PrimType != primtype) break;
        if (cur_rp->RenderKey != key) break;

        numpolys++;
        numindices += cur_rp->NumIndices;
    }

    glDrawElements(primtype, numindices, GL_UNSIGNED_INT, (void*)(uintptr_t)(rp->IndicesOffset * 4));
    return numpolys;
}

int GLRenderer::RenderPolygonEdgeBatch(int i) const
{
    const RendererPolygon* rp = &PolygonList[i];
    u32 key = rp->RenderKey;
    int numpolys = 0;
    u32 numindices = 0;

    for (int iend = i; iend < NumFinalPolys; iend++)
    {
        const RendererPolygon* cur_rp = &PolygonList[iend];
        if (cur_rp->RenderKey != key) break;

        numpolys++;
        numindices += cur_rp->NumEdgeIndices;
    }

    glDrawElements(GL_LINES, numindices, GL_UNSIGNED_INT, (void*)(uintptr_t)(rp->EdgeIndicesOffset * 4));
    return numpolys;
}

void GLRenderer::RenderSceneChunk(const GPU3D& gpu3d, int y, int h)
{
    u32 flags = 0;
    if (gpu3d.RenderPolygonRAM[0]->WBuffer) flags |= RenderFlag_WBuffer;

    if (h != 192) glScissor(0, y<<ScaleFactor, 256<<ScaleFactor, h<<ScaleFactor);

    GLboolean fogenable = (gpu3d.RenderDispCnt & (1<<7)) ? GL_TRUE : GL_FALSE;

    // 'equal' depth test: the DS passes within a margin of +-0x200 in Z-buffer
    // mode, +-0xFF in W-buffer mode. Pomegrade: the vertex shaders move these
    // polygons that margin towards the camera and GL_LEQUAL is used, so the
    // near half of the margin is honoured; a polygon more than the margin in
    // front of what is there still passes (the far bound would need the depth
    // already drawn, which one depth test can't check from both sides)

    // pass 1: opaque pixels

    UseRenderShader(flags);
    glLineWidth(1.0);

    glColorMaski(1, GL_TRUE, GL_TRUE, fogenable, GL_FALSE);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glBindVertexArray(VertexArrayID);

    // lighting effects: opaque polygons also write their view-space position and normal
    const GLenum drawbuffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    if (LightingActive && !TranslucentPassOnly)
        glDrawBuffers(4, drawbuffers);

    for (int i = TranslucentPassOnly ? NumFinalPolys : 0; i < NumFinalPolys; )
    {
        RendererPolygon* rp = &PolygonList[i];

        if (rp->PolyData->IsShadowMask) { i++; continue; }
        if (rp->PolyData->Translucent) { i++; continue; }

        if (rp->PolyData->Attr & (1<<14))
            glDepthFunc(GL_LEQUAL);
        else
            glDepthFunc(GL_LESS);

        u32 polyattr = rp->PolyData->Attr;
        u32 polyid = (polyattr >> 24) & 0x3F;

        glStencilFunc(GL_ALWAYS, polyid, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);

        i += RenderPolygonBatch(i);
    }

    if (LightingActive)
        glDrawBuffers(2, drawbuffers);
    // the lit image of the opaque layer (LightingTex), before translucent polygons
    if (LightingActive && !TranslucentPassOnly)
    {
        RenderLighting(gpu3d);
        UseRenderShader(flags);
    }

    // if edge marking is enabled, mark all opaque edges
    // TODO BETTER EDGE MARKING!!! THIS SUCKS
    /*if (RenderDispCnt & (1<<5))
    {
        UseRenderShader(flags | RenderFlag_Edge);
        glLineWidth(1.5);

        glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE);

        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0);

        for (int i = 0; i < NumFinalPolys; )
        {
            RendererPolygon* rp = &PolygonList[i];

            if (rp->PolyData->IsShadowMask) { i++; continue; }

            i += RenderPolygonEdgeBatch(i);
        }

        glDepthMask(GL_TRUE);
    }*/

    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);

    if (gpu3d.RenderDispCnt & (1<<3))
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    else
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ONE);

    glLineWidth(1.0);

    if (NumOpaqueFinalPolys > -1)
    {
        // pass 2: if needed, render translucent pixels that are against background pixels
        // when background alpha is zero, those need to be rendered with blending disabled

        if ((gpu3d.RenderClearAttr1 & 0x001F0000) == 0)
        {
            glDisable(GL_BLEND);

            for (int i = 0; i < NumFinalPolys; )
            {
                RendererPolygon* rp = &PolygonList[i];

                if (DSShadowsReplaced && rp->PolyData->IsShadow && ShadowReplaced[i]) { i++; continue; }

                if (rp->PolyData->IsShadowMask)
                {
                    // draw actual shadow mask

                    UseRenderShader(flags | RenderFlag_ShadowMask);

                    glDisable(GL_BLEND);
                    glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glDepthMask(GL_FALSE);

                    glDepthFunc(GL_LESS);
                    glStencilFunc(GL_EQUAL, 0xFF, 0xFF);
                    glStencilOp(GL_KEEP, GL_INVERT, GL_KEEP);
                    glStencilMask(0x01);

                    i += RenderPolygonBatch(i);
                }
                else if (rp->PolyData->Translucent)
                {
                    bool needopaque = ((rp->PolyData->Attr & 0x001F0000) == 0x001F0000);

                    u32 polyattr = rp->PolyData->Attr;
                    u32 polyid = (polyattr >> 24) & 0x3F;

                    if (polyattr & (1<<14))
                        glDepthFunc(GL_LEQUAL);
                    else
                        glDepthFunc(GL_LESS);

                    if (needopaque)
                    {
                        UseRenderShader(flags);

                        glDisable(GL_BLEND);
                        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glColorMaski(1, GL_TRUE, GL_TRUE, fogenable, GL_FALSE);

                        glStencilFunc(GL_ALWAYS, polyid, 0xFF);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        glStencilMask(0xFF);

                        glDepthMask(GL_TRUE);

                        RenderSinglePolygon(i);
                    }

                    UseRenderShader(flags | RenderFlag_Trans);

                    GLboolean transfog;
                    if (!(polyattr & (1<<15))) transfog = fogenable;
                    else                       transfog = GL_FALSE;

                    if (rp->PolyData->IsShadow)
                    {
                        // shadow against clear-plane will only pass if its polyID matches that of the clear plane
                        u32 clrpolyid = (gpu3d.RenderClearAttr1 >> 24) & 0x3F;
                        if (polyid != clrpolyid) { i++; continue; }

                        glEnable(GL_BLEND);
                        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glColorMaski(1, GL_FALSE, GL_FALSE, transfog, GL_FALSE);

                        glStencilFunc(GL_EQUAL, 0xFE, 0xFF);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
                        glStencilMask(~(0x40|polyid)); // heheh

                        if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                        else                    glDepthMask(GL_FALSE);

                        i += needopaque ? RenderSinglePolygon(i) : RenderPolygonBatch(i);
                    }
                    else
                    {
                        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glColorMaski(1, GL_FALSE, GL_FALSE, transfog, GL_FALSE);

                        glStencilFunc(GL_EQUAL, 0xFF, 0xFE);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
                        glStencilMask(~(0x40|polyid)); // heheh

                        if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                        else                    glDepthMask(GL_FALSE);

                        i += needopaque ? RenderSinglePolygon(i) : RenderPolygonBatch(i);
                    }
                }
                else
                    i++;
            }

            glEnable(GL_BLEND);
            glStencilMask(0xFF);
        }

        // pass 3: translucent pixels

        for (int i = 0; i < NumFinalPolys; )
        {
            RendererPolygon* rp = &PolygonList[i];

            if (DSShadowsReplaced && rp->PolyData->IsShadow && ShadowReplaced[i]) { i++; continue; }

            if (rp->PolyData->IsShadowMask)
            {
                // clear shadow bits in stencil buffer

                glStencilMask(0x80);
                glClear(GL_STENCIL_BUFFER_BIT);

                // draw actual shadow mask

                UseRenderShader(flags | RenderFlag_ShadowMask);

                glDisable(GL_BLEND);
                glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glDepthMask(GL_FALSE);

                glDepthFunc(GL_LESS);
                glStencilFunc(GL_ALWAYS, 0x80, 0x80);
                glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);

                i += RenderPolygonBatch(i);
            }
            else if (rp->PolyData->Translucent)
            {
                bool needopaque = ((rp->PolyData->Attr & 0x001F0000) == 0x001F0000);

                u32 polyattr = rp->PolyData->Attr;
                u32 polyid = (polyattr >> 24) & 0x3F;

                if (polyattr & (1<<14))
                    glDepthFunc(GL_LEQUAL);
                else
                    glDepthFunc(GL_LESS);

                if (needopaque)
                {
                    UseRenderShader(flags);

                    glDisable(GL_BLEND);
                    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glColorMaski(1, GL_TRUE, GL_TRUE, fogenable, GL_FALSE);

                    glStencilFunc(GL_ALWAYS, polyid, 0xFF);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilMask(0xFF);

                    glDepthMask(GL_TRUE);

                    RenderSinglePolygon(i);
                }

                UseRenderShader(flags | RenderFlag_Trans);

                GLboolean transfog;
                if (!(polyattr & (1<<15))) transfog = fogenable;
                else                       transfog = GL_FALSE;

                if (rp->PolyData->IsShadow)
                {
                    glDisable(GL_BLEND);
                    glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glDepthMask(GL_FALSE);
                    glStencilFunc(GL_EQUAL, polyid, 0x3F);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
                    glStencilMask(0x80);

                    RenderSinglePolygon(i);

                    glEnable(GL_BLEND);
                    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glColorMaski(1, GL_FALSE, GL_FALSE, transfog, GL_FALSE);

                    glStencilFunc(GL_EQUAL, 0xC0|polyid, 0x80);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilMask(0x7F);

                    if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                    else                    glDepthMask(GL_FALSE);

                    i += RenderSinglePolygon(i);
                }
                else
                {
                    glEnable(GL_BLEND);
                    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glColorMaski(1, GL_FALSE, GL_FALSE, transfog, GL_FALSE);

                    glStencilFunc(GL_NOTEQUAL, 0x40|polyid, 0x7F);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glStencilMask(0x7F);

                    if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                    else                    glDepthMask(GL_FALSE);

                    i += needopaque ? RenderSinglePolygon(i) : RenderPolygonBatch(i);
                }
            }
            else
                i++;
        }
    }

    if (gpu3d.RenderDispCnt & 0x00A0) // fog/edge enabled
    {
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_FALSE);
        glStencilFunc(GL_ALWAYS, 0, 0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0);
        // While depth and stencil writing operations are disabled by the commands above, the fact that the same texture is used as both input and output results in undefined
        // behaviour, which manifests as visual artifacts on some devices. Depth/stencil texture is attached again after fog/edge rendering
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TranslucentPassOnly ? LitDepthTex : DepthBufferTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, TranslucentPassOnly ? LitAttrTex : AttrBufferTex);

        glBindBuffer(GL_ARRAY_BUFFER, ClearVertexBufferID);
        glBindVertexArray(ClearVertexArrayID);

        if (gpu3d.RenderDispCnt & (1<<5))
        {
            // edge marking
            // TODO: depth/polyid values at screen edges

            glUseProgram(FinalPassEdgeShader);

            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);

            glDrawArrays(GL_TRIANGLES, 0, 2*3);
        }

        if (gpu3d.RenderDispCnt & (1<<7))
        {
            // fog

            glUseProgram(FinalPassFogShader);

            if (gpu3d.RenderDispCnt & (1<<6))
                glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            else
                glBlendFuncSeparate(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_ALPHA);

            {
                u32 c = gpu3d.RenderFogColor;
                u32 r = c & 0x1F;
                u32 g = (c >> 5) & 0x1F;
                u32 b = (c >> 10) & 0x1F;
                u32 a = (c >> 16) & 0x1F;

                glBlendColor((float)b/31.0, (float)g/31.0, (float)r/31.0, (float)a/31.0);
            }

            glDrawArrays(GL_TRIANGLES, 0, 2*3);
        }

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, TranslucentPassOnly ? LitDepthTex : DepthBufferTex, 0);
    }
}


void GLRenderer::SetupLightingTargets()
{
    if (LightingTargetsW == ScreenW && LightingTargetsH == ScreenH)
        return;
    LightingTargetsW = ScreenW;
    LightingTargetsH = ScreenH;
    // the lighting terms' resolution (see MaxLightingScale); 256 * scale is even
    LightingFactor = (ScaleFactor + MaxLightingScale - 1) / MaxLightingScale;
    const int lowW = ScreenW / LightingFactor, lowH = ScreenH / LightingFactor;

    auto makeTarget = [&](GLuint& tex, GLenum internalFormat, GLenum format, GLenum type, bool low = false)
    {
        if (!tex) glGenTextures(1, &tex);
        SetupDefaultTexParams(tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, low ? lowW : ScreenW, low ? lowH : ScreenH, 0, format, type, nullptr);
    };
    glActiveTexture(GL_TEXTURE0);
    // view positions need float32: view-space units are 1/4096, with depths in the thousands
    makeTarget(ViewPositionTex, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    makeTarget(ViewNormalTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    makeTarget(AOTex, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    makeTarget(BounceTex, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    makeTarget(LightingTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    // same formats as DepthBufferTex and AttrBufferTex (copied with blits)
    makeTarget(LitDepthTex, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
    makeTarget(LitAttrTex, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);

    // attachments 2 and 3 of the main framebuffer, drawn to by opaque polygons only
    glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, ViewPositionTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, ViewNormalTex, 0);

    const GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, buffers);
    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glDrawBuffers(2, buffers);

    glBindFramebuffer(GL_FRAMEBUFFER, AOFramebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, AOTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, BounceTex, 0);
    glDrawBuffers(2, buffers);
    complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, LightingFramebuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, LightingTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, LitAttrTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, LitDepthTex, 0);
    glDrawBuffers(2, buffers);
    complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    if (LightingFactor > 1)
    {
        // the smaller copy of the frame's data, and the terms computed on it
        makeTarget(LowPositionTex, GL_RGBA32F, GL_RGBA, GL_FLOAT, true);
        makeTarget(LowNormalTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        makeTarget(LowColorTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        makeTarget(TermsTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        makeTarget(TermsBounceTex, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
        makeTarget(TermsReflectedTex, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        glBindFramebuffer(GL_FRAMEBUFFER, LowGBufferFramebuffer);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, LowPositionTex, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, LowNormalTex, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, LowColorTex, 0);
        glDrawBuffers(3, buffers);
        complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, TermsFramebuffer);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, TermsTex, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, TermsBounceTex, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, TermsReflectedTex, 0);
        glDrawBuffers(3, buffers);
        complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    if (!complete)
    {
        // float render targets missing: the effects stay off, and the main
        // framebuffer gets back to its own attachments (with the unusable ones
        // attached it would be incomplete, and nothing would render)
        Log(LogLevel::Warn, "Lighting effects: render targets not supported, effects disabled\n");
        glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, 0, 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, 0, 0);
        LightingSupported = false;
        LightingActive = false;
    }
}

bool GLRenderer::RenderShadowMap(const GPU3D& gpu3d)
{
    // the main light: the one the most opaque polygons use
    int light = 0;
    for (int l = 1; l < 4; l++)
        if (LightUse[l] > LightUse[light]) light = l;
    if (!LightUse[light])
        return false;

    float dir[3];
    for (int i = 0; i < 3; i++) dir[i] = (float)gpu3d.RenderLightDirection[light][i];
    float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
    if (len <= 0)
        return false;
    auto& sp = ShadowParams;
    for (int i = 0; i < 3; i++) sp.Dir[i] = dir[i] / len;

    // light space: any two axes across the light's direction
    float hint[3] = {0, 1, 0};
    if (std::fabs(sp.Dir[1]) > 0.9f) { hint[0] = 1; hint[1] = 0; }
    auto cross = [](const float* a, const float* b, float* out) {
        out[0] = a[1]*b[2] - a[2]*b[1]; out[1] = a[2]*b[0] - a[0]*b[2]; out[2] = a[0]*b[1] - a[1]*b[0];
    };
    cross(hint, sp.Dir, sp.Right);
    float rlen = std::sqrt(sp.Right[0]*sp.Right[0] + sp.Right[1]*sp.Right[1] + sp.Right[2]*sp.Right[2]);
    for (int i = 0; i < 3; i++) sp.Right[i] /= rlen;
    cross(sp.Dir, sp.Right, sp.Up);

    // fitted to the 3D geometry of the frame
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (u32 v = 0; v < NumVertices; v++)
    {
        const float* g = &ViewVertexBuffer[v * ViewVertexSize];
        if (g[3] < 0.5f) continue;
        const float* axes[3] = {sp.Right, sp.Up, sp.Dir};
        for (int a = 0; a < 3; a++)
        {
            float d = g[0]*axes[a][0] + g[1]*axes[a][1] + g[2]*axes[a][2];
            lo[a] = std::min(lo[a], d);
            hi[a] = std::max(hi[a], d);
        }
    }
    // what casts may lie outside the view (see GPU3D::RecordShadowCaster):
    // the depth range covers it, across the light only what is on screen
    // matters (a directional light's shadows fall straight along it)
    // Only faces turned towards the light cast: a closed object's lit half
    // has its whole outline, and the inside of what encloses the scene (a
    // room's ceiling and walls, lit by the DS too) faces away from a light
    // coming from outside, which it would otherwise block entirely
    // Far background (a sky dome, distant mountains) casts nothing: what lies
    // over 8 times further than the median distance of what the frame draws.
    // On Joker's harbour a piece of the sky dome, a million units out (the
    // scene within about 60 thousand), joined the casters on some frames and
    // put the whole scene in shadow
    ShadowDistances.clear();
    for (u32 v = 0; v < NumVertices; v++)
    {
        const float* g = &ViewVertexBuffer[v * ViewVertexSize];
        if (g[3] >= 0.5f) ShadowDistances.push_back(g[0]*g[0] + g[1]*g[1] + g[2]*g[2]);
    }
    float farthest = 1e30f;
    if (!ShadowDistances.empty())
    {
        auto mid = ShadowDistances.begin() + ShadowDistances.size() / 2;
        std::nth_element(ShadowDistances.begin(), mid, ShadowDistances.end());
        farthest = *mid * 64.0f; // squared: 8 times the distance
    }
    const std::vector<float>& recorded = gpu3d.RenderShadowCasters;
    ShadowCasterVertices.clear();
    for (size_t t = 0; t + 11 < recorded.size(); t += 12)
    {
        const float* n = &recorded[t + 9];
        if (n[0]*sp.Dir[0] + n[1]*sp.Dir[1] + n[2]*sp.Dir[2] <= 0)
            continue;
        bool background = false;
        for (int v = 0; v < 3; v++)
        {
            const float* q = &recorded[t + v * 3];
            background = background || q[0]*q[0] + q[1]*q[1] + q[2]*q[2] > farthest;
        }
        if (background)
            continue;
        ShadowCasterVertices.insert(ShadowCasterVertices.end(), &recorded[t], &recorded[t + 9]);
    }
    const std::vector<float>& casters = ShadowCasterVertices;
    for (size_t v = 0; v + 2 < casters.size(); v += 3)
    {
        float d = casters[v]*sp.Dir[0] + casters[v+1]*sp.Dir[1] + casters[v+2]*sp.Dir[2];
        lo[2] = std::min(lo[2], d);
        hi[2] = std::max(hi[2], d);
    }
    float extent = std::max(hi[0] - lo[0], hi[1] - lo[1]);
    if (!(extent > 0) || !(hi[2] > lo[2]) || casters.empty())
        return false;
    // stable from frame to frame: the map's width in steps of 19% (with 4
    // texels of margin), its corner on whole texels, so that a moving view
    // doesn't make shadow edges crawl
    float width = std::exp2(std::ceil(std::log2(extent * (1.0f + 4.0f / ShadowMapSize)) * 4.0f) / 4.0f);
    float texel = width / ShadowMapSize;
    sp.Bounds[0] = (std::floor(lo[0] / texel) - 1.0f) * texel;
    sp.Bounds[1] = (std::floor(lo[1] / texel) - 1.0f) * texel;
    sp.Bounds[2] = 1.0f / width;
    sp.Bounds[3] = 1.0f / width;
    sp.Depth[0] = hi[2] + texel;
    sp.Depth[1] = 1.0f / (hi[2] - lo[2] + 2 * texel);
    sp.Texel = texel;

    if (!ShadowMapTex)
    {
        glGenTextures(1, &ShadowMapTex);
        glBindTexture(GL_TEXTURE_2D, ShadowMapTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // hardware depth comparison, bilinear: each lookup is a 2x2 filtered test
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, ShadowMapSize, ShadowMapSize, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, ShadowFramebuffer);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ShadowMapTex, 0);
        const GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Log(LogLevel::Warn, "Lighting effects: shadow map not supported, shadows disabled\n");
            glDeleteTextures(1, &ShadowMapTex);
            ShadowMapTex = 0;
            Shadows = false;
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, ShadowFramebuffer);
    glViewport(0, 0, ShadowMapSize, ShadowMapSize);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(LightingShadowShader);
    glUniform3fv(ShadowLoc[0], 1, sp.Right);
    glUniform3fv(ShadowLoc[1], 1, sp.Up);
    glUniform3fv(ShadowLoc[2], 1, sp.Dir);
    glUniform4fv(ShadowLoc[3], 1, sp.Bounds);
    glUniform2fv(ShadowLoc[4], 1, sp.Depth);

    // the casters, on screen or not, whichever side the camera sees. Only
    // geometry the DS lights casts: unlit polygons carry lighting the game
    // painted in, occlusion included, or are backdrops (a sky behind a
    // window would block the sun)
    if (!ShadowCasterBufferID)
    {
        glGenBuffers(1, &ShadowCasterBufferID);
        glGenVertexArrays(1, &ShadowCasterArrayID);
        glBindVertexArray(ShadowCasterArrayID);
        glBindBuffer(GL_ARRAY_BUFFER, ShadowCasterBufferID);
        glEnableVertexAttribArray(5); // vViewPosition (w = 1: perspective)
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    }
    glBindVertexArray(ShadowCasterArrayID);
    glBindBuffer(GL_ARRAY_BUFFER, ShadowCasterBufferID);
    glBufferData(GL_ARRAY_BUFFER, casters.size() * sizeof(float), casters.data(), GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(casters.size() / 3));
    glBindVertexArray(VertexArrayID);
    return true;
}

// The game's own shadows (DS shadow polygons) that real-time shadows replace:
// each run of consecutive shadow polygons with one polygon id (one volume,
// usually a disc under a character) where something that casts lies within
// its bounds, grown by its own size. Elsewhere (a character the DS doesn't
// light, a shadow polygon used for another effect) the game's shadow stays.
// Their masks are drawn either way (stencil only).
bool GLRenderer::FindReplacedShadows(const GPU3D& gpu3d)
{
    ShadowReplaced.assign(NumFinalPolys, false);
    const std::vector<float>& casters = gpu3d.RenderShadowCasters;
    bool any = false;
    for (int i = 0; i < NumFinalPolys; )
    {
        const Polygon* first = PolygonList[i].PolyData;
        if (!first->IsShadow) { i++; continue; }
        u32 id = (first->Attr >> 24) & 0x3F;
        int end = i;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        while (end < NumFinalPolys && PolygonList[end].PolyData->IsShadow && ((PolygonList[end].PolyData->Attr >> 24) & 0x3F) == id)
        {
            const Polygon* p = PolygonList[end].PolyData;
            for (u32 k = 0; k < p->NumVertices; k++)
                for (int c = 0; c < 3; c++)
                {
                    lo[c] = std::min(lo[c], p->Vertices[k]->ViewPosition[c]);
                    hi[c] = std::max(hi[c], p->Vertices[k]->ViewPosition[c]);
                }
            end++;
        }
        float grow = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
        // the game's shadow gives way only where the real one covers it: its
        // middle, seen from the main light, inside the outline of the casters
        // near it, and those casters between it and the light. A caster near
        // the volume is not enough: with a grazing light (Joker's harbour) the
        // real shadow falls away from the feet and characters lost theirs
        const auto& sp = ShadowParams;
        auto across = [&](const float* q, const float* axis) { return q[0]*axis[0] + q[1]*axis[1] + q[2]*axis[2]; };
        const float mid[3] = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
        float cLo[2] = {1e30f, 1e30f}, cHi[2] = {-1e30f, -1e30f}, casterNearest = -1e30f;
        bool nearby = false;
        for (size_t t = 0; t + 11 < casters.size(); t += 12)
            for (int v = 0; v < 3; v++)
            {
                const float* p = &casters[t + v * 3];
                if (!(p[0] >= lo[0] - grow && p[0] <= hi[0] + grow && p[1] >= lo[1] - grow && p[1] <= hi[1] + grow &&
                      p[2] >= lo[2] - grow && p[2] <= hi[2] + grow))
                    continue;
                nearby = true;
                const float r = across(p, sp.Right), u = across(p, sp.Up);
                cLo[0] = std::min(cLo[0], r); cHi[0] = std::max(cHi[0], r);
                cLo[1] = std::min(cLo[1], u); cHi[1] = std::max(cHi[1], u);
                // nearer the light: larger along Dir (as the shadow map's depth)
                casterNearest = std::max(casterNearest, across(p, sp.Dir));
            }
        const float mr = across(mid, sp.Right), mu = across(mid, sp.Up);
        const bool covered = nearby && mr >= cLo[0] && mr <= cHi[0] && mu >= cLo[1] && mu <= cHi[1] && casterNearest > across(mid, sp.Dir);
        for (int k = i; k < end; k++) ShadowReplaced[k] = covered;
        any = any || covered;
        i = end;
    }
    return any;
}

void GLRenderer::SetLightingTermUniforms(const GLint* shadowLoc, const GLint* reflectionLoc, bool shadows,
                                         const GPU3D& gpu3d, float scale) const
{
    glUniform1f(shadowLoc[0], shadows ? ShadowStrength : 0.0f);
    if (shadows)
    {
        glUniform3fv(shadowLoc[1], 1, ShadowParams.Right);
        glUniform3fv(shadowLoc[2], 1, ShadowParams.Up);
        glUniform3fv(shadowLoc[3], 1, ShadowParams.Dir);
        glUniform4fv(shadowLoc[4], 1, ShadowParams.Bounds);
        glUniform2fv(shadowLoc[5], 1, ShadowParams.Depth);
        glUniform1f(shadowLoc[6], ShadowParams.Texel);
    }
    glUniform1f(reflectionLoc[0], Reflections ? ReflectionStrength : 0.0f);
    if (Reflections)
    {
        float proj[16], viewport[4];
        for (int i = 0; i < 16; i++) proj[i] = (float)gpu3d.RenderProjMatrix[i];
        // x0, top row, width, height (see GPU3D::ComputeScreenPosition)
        viewport[0] = (float)gpu3d.RenderViewport[0];
        viewport[1] = (float)gpu3d.RenderViewport[3];
        viewport[2] = (float)gpu3d.RenderViewport[4];
        viewport[3] = (float)gpu3d.RenderViewport[5];
        glUniformMatrix4fv(reflectionLoc[1], 1, GL_FALSE, proj);
        glUniform4fv(reflectionLoc[2], 1, viewport);
        // output pixels per DS pixel, of the image the terms are computed on
        glUniform1f(reflectionLoc[3], scale);
    }
}

void GLRenderer::RenderLighting(const GPU3D& gpu3d)
{
    GLTimer::Scope gpuTime(PerformanceCounters::Section::GpuLighting);
    // depth/stencil and attributes as the opaque pass left them, for drawing
    // the translucent layer again over the lit image
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MainFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, LightingFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, ScreenW, ScreenH, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    const GLenum attrOnly[2] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glDrawBuffers(2, attrOnly);
    glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, ScreenW, ScreenH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum colourOnly = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &colourOnly);

    bool shadows;
    {
        GLTimer::Scope shadowTime(PerformanceCounters::Section::GpuShadows);
        shadows = Shadows && RenderShadowMap(gpu3d);
    }
    ShadowsDrawn = shadows;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    // write masks are per draw buffer index, not per framebuffer: the scene
    // render leaves index 1 (its attribute buffer) partly masked
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); // the passes at high resolutions write 3
    glViewport(0, 0, ScreenW, ScreenH);
    glBindBuffer(GL_ARRAY_BUFFER, ClearVertexBufferID);
    glBindVertexArray(ClearVertexArrayID);

    // at high resolutions, the smaller copy of the frame's data the terms are computed on
    const int lowW = ScreenW / LightingFactor, lowH = ScreenH / LightingFactor;
    if (LightingFactor > 1)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, LowGBufferFramebuffer);
        glViewport(0, 0, lowW, lowH);
        glUseProgram(LightingDownsampleShader);
        glUniform1i(DownsampleFactorLoc, LightingFactor);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ViewPositionTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ViewNormalTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ColorBufferTex);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
    }

    // pass 1: ambient occlusion up to 24 native pixels around each pixel, light bounce up to 64
    glBindFramebuffer(GL_FRAMEBUFFER, AOFramebuffer);
    glViewport(0, 0, lowW, lowH);
    glUseProgram(LightingAOShader);
    glUniform1f(LightingAORadiusLoc, 24.0f * ScaleFactor / LightingFactor);
    glUniform1f(LightingBounceRadiusLoc, 64.0f * ScaleFactor / LightingFactor);
    glUniform1i(LightingAOPixelInWLoc, LightingFactor > 1 ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, LightingFactor > 1 ? LowPositionTex : ViewPositionTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, LightingFactor > 1 ? LowNormalTex : ViewNormalTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, LightingFactor > 1 ? LowColorTex : ColorBufferTex);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
    glViewport(0, 0, ScreenW, ScreenH);

    // pass 2: the lit image
    glBindFramebuffer(GL_FRAMEBUFFER, LightingFramebuffer);
    glUseProgram(LightingComposeShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ColorBufferTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ViewPositionTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ViewNormalTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, AOTex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, BounceTex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, shadows ? ShadowMapTex : 0);
    // the shadow map again, read as depths: the sampler overrides its comparison mode
    if (!ShadowDepthSampler)
    {
        glGenSamplers(1, &ShadowDepthSampler);
        glSamplerParameteri(ShadowDepthSampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glSamplerParameteri(ShadowDepthSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(ShadowDepthSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(ShadowDepthSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(ShadowDepthSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, shadows ? ShadowMapTex : 0);
    glBindSampler(6, ShadowDepthSampler);
    // characters' softer shade: polygon IDs (the opaque pass's attributes)
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, AttrBufferTex);
    glUseProgram(LightingComposeShader);
    glUniform1i(ComposeAttrLoc[0], 9);
    glUniform1i(ComposeAttrLoc[1], FrameSceneryId);
    glUseProgram(LightingUpsampleShader);
    glUniform1i(UpsampleAttrLoc[0], 9);
    glUniform1i(UpsampleAttrLoc[1], FrameSceneryId);
    glUseProgram(LightingComposeShader);
    if (LightingFactor == 1)
    {
        SetLightingTermUniforms(ComposeShadowLoc, ComposeReflectionLoc, shadows, gpu3d, (float)ScaleFactor);
        glUniform1i(LightingComposeAOLoc, AmbientOcclusion ? 1 : 0);
        glUniform1f(LightingComposeBounceLoc, LightBounce ? BounceIntensity : 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
        glBindSampler(6, 0);
    }
    else
    {
        // pass 2: the terms, on the smaller copy (the same texture units as the
        // compose shader, its inputs replaced by the copy's)
        glBindFramebuffer(GL_FRAMEBUFFER, TermsFramebuffer);
        glViewport(0, 0, lowW, lowH);
        glUseProgram(LightingTermsShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, LowColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, LowPositionTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, LowNormalTex);
        SetLightingTermUniforms(TermsShadowLoc, TermsReflectionLoc, shadows, gpu3d, (float)ScaleFactor / LightingFactor);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
        glBindSampler(6, 0);

        // pass 3: the lit image at full resolution
        glBindFramebuffer(GL_FRAMEBUFFER, LightingFramebuffer);
        glViewport(0, 0, ScreenW, ScreenH);
        glUseProgram(LightingUpsampleShader);
        const GLuint inputs[9] = {ColorBufferTex, ViewPositionTex, ViewNormalTex, LowPositionTex, LowNormalTex,
                                  TermsTex, TermsBounceTex, TermsReflectedTex, AOTex};
        for (int i = 0; i < 9; i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, inputs[i]);
        }
        glUniform1i(UpsampleLoc[0], LightingFactor);
        glUniform1i(UpsampleLoc[1], AmbientOcclusion ? 1 : 0);
        glUniform1f(UpsampleLoc[2], LightBounce ? BounceIntensity : 0.0f);
        glUniform1f(UpsampleLoc[3], shadows ? ShadowStrength : 0.0f);
        glUniform3fv(UpsampleLoc[4], 1, ShadowParams.Dir);
        glUniform1f(UpsampleLoc[5], Reflections ? ReflectionStrength : 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
    }

    // state as the opaque pass leaves it (the translucent pass follows)
    const GLenum colourAndAttr[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, colourAndAttr);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TexPalMemID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexMemID);
    glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
    glBindVertexArray(VertexArrayID);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    CurShaderID = -1;
    LightingDone = true;
}

void GLRenderer::RenderFrame(GPU& gpu)
{
    Polygon** renderpolys = gpu.GPU3D.GetRenderPolygons();
    u32 numrenderpolys = gpu.GPU3D.GetRenderNumPolygons();
    if (FrameGeneration)
    {
        std::swap(Snapshots[0], Snapshots[1]);
        Snapshots[1].Take(renderpolys, numrenderpolys);
    }
    GLTimer::Scope gpuTime(PerformanceCounters::Section::GpuScene);
    RenderScene(gpu, renderpolys, numrenderpolys, false);
}

void GLRenderer::RenderScene(GPU& gpu, Polygon** renderpolys, u32 numrenderpolys, bool intermediate)
{
    CurShaderID = -1;

    // lighting effects: GPU3D captures view-space data while the game submits a
    // frame, so they start with the first frame submitted after being enabled
    // (an intermediate frame keeps the decision of the frame it comes from)
    if (!intermediate)
    {
        CaptureStreak = CapturedSinceRender ? CaptureStreak + 1 : 0;
        CapturedSinceRender = false;
        if (CaptureStreak >= 2)
            CapturePauseFrames = CapturePauseLength;
        else if (CapturePauseFrames > 0)
            CapturePauseFrames--;
        LightingActive = LightingEnabled() && ViewDataCaptured && LightingSupported && !CapturePauseFrames;
        ViewDataActive = ViewDataCaptured && LightingSupported && (LightingActive || Relief > 0);
        gpu.GPU3D.SetViewDataCapture(ViewDataWanted() && LightingSupported);
        ViewDataCaptured = gpu.GPU3D.CaptureViewData();
    }
    LightingDone = false;
    if (LightingActive)
        SetupLightingTargets();
    glBindVertexArray(VertexArrayID);
    if (ViewDataActive) // lighting effects or relief textures
    {
        glEnableVertexAttribArray(5);
        glEnableVertexAttribArray(6);
    }
    else
    {
        glDisableVertexAttribArray(5);
        glDisableVertexAttribArray(6);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, MainFramebuffer);

    // vertex positions come in 1/SubpixelScale() pixel units (high-precision geometry)
    ShaderConfig.uScreenSize[0] = ScreenW * SubpixelScale();
    ShaderConfig.uScreenSize[1] = ScreenH * SubpixelScale();
    ShaderConfig.uDispCnt = gpu.GPU3D.RenderDispCnt;

    for (int i = 0; i < 32; i++)
    {
        u16 c = gpu.GPU3D.RenderToonTable[i];
        u32 r = c & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 b = (c >> 10) & 0x1F;

        ShaderConfig.uToonColors[i][0] = (float)r / 31.0;
        ShaderConfig.uToonColors[i][1] = (float)g / 31.0;
        ShaderConfig.uToonColors[i][2] = (float)b / 31.0;
    }

    for (int i = 0; i < 8; i++)
    {
        u16 c = gpu.GPU3D.RenderEdgeTable[i];
        u32 r = c & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 b = (c >> 10) & 0x1F;

        ShaderConfig.uEdgeColors[i][0] = (float)r / 31.0;
        ShaderConfig.uEdgeColors[i][1] = (float)g / 31.0;
        ShaderConfig.uEdgeColors[i][2] = (float)b / 31.0;
    }

    {
        u32 c = gpu.GPU3D.RenderFogColor;
        u32 r = c & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 b = (c >> 10) & 0x1F;
        u32 a = (c >> 16) & 0x1F;

        ShaderConfig.uFogColor[0] = (float)r / 31.0;
        ShaderConfig.uFogColor[1] = (float)g / 31.0;
        ShaderConfig.uFogColor[2] = (float)b / 31.0;
        ShaderConfig.uFogColor[3] = (float)a / 31.0;
    }

    for (int i = 0; i < 34; i++)
    {
        u8 d = gpu.GPU3D.RenderFogDensityTable[i];
        ShaderConfig.uFogDensity[i][0] = (float)d / 127.0;
    }

    // relief textures: depth in texels, and the main light (most used by
    // opaque polygons last frame, in view space, towards the light)
    ShaderConfig.uRelief = ViewDataActive ? (Relief >= 2 ? 2.0f : Relief == 1 ? 1.0f : 0.0f) : 0.0f;
    // wind for volumetric grass: 2 radians a second at the DS's 60 frames a
    // second, reduced here in doubles so the float never loses precision
    if (ShaderConfig.uRelief > 0) WindFrames++;
    ShaderConfig.uWindPhase = (float)std::fmod(WindFrames * (2.0 / 60.0), 2.0 * M_PI);
    ShaderConfig.uStyle = ShaderConfig.uRelief > 0 && Relief >= 3 ? 1.0f : 0.0f;
    {
        int light = 0;
        for (int l = 1; l < 4; l++)
            if (LightUse[l] > LightUse[light]) light = l;
        float dir[3], len = 0;
        for (int i = 0; i < 3; i++) { dir[i] = (float)gpu.GPU3D.RenderLightDirection[light][i]; len += dir[i] * dir[i]; }
        len = std::sqrt(len);
        for (int i = 0; i < 3; i++) ShaderConfig.uReliefLight[i] = len > 0 ? dir[i] / len : 0.0f;
        ShaderConfig.uReliefLight[3] = (len > 0 && LightUse[light]) ? 1.0f : 0.0f;
    }

    ShaderConfig.uFogOffset = gpu.GPU3D.RenderFogOffset;
    ShaderConfig.uFogShift = gpu.GPU3D.RenderFogShift;

    glBindBuffer(GL_UNIFORM_BUFFER, ShaderConfigUBO);
    void* unibuf = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(ShaderConfig), GL_MAP_WRITE_BIT);
    if (unibuf) memcpy(unibuf, &ShaderConfig, sizeof(ShaderConfig));
    glUnmapBuffer(GL_UNIFORM_BUFFER);

    // SUCKY!!!!!!!!!!!!!!!!!!
    // TODO: detect when VRAM blocks are modified!
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexMemID);
    for (int i = 0; i < 4; i++)
    {
        u32 mask = gpu.VRAMMap_Texture[i];
        u8* vram;
        if (!mask) continue;
        else if (mask & (1<<0)) vram = gpu.VRAM_A;
        else if (mask & (1<<1)) vram = gpu.VRAM_B;
        else if (mask & (1<<2)) vram = gpu.VRAM_C;
        else if (mask & (1<<3)) vram = gpu.VRAM_D;

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i*128, 1024, 128, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vram);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TexPalMemID);

    u16* tempBuffer = (u16*) malloc(1024 * 8 * 2);
    for (int i = 0; i < 6; i++)
    {
        // 6 x 16K chunks
        u32 mask = gpu.VRAMMap_TexPal[i];
        u8* vram;
        if (!mask) continue;
        else if (mask & (1<<4)) vram = &gpu.VRAM_E[(i&3)*0x4000];
        else if (mask & (1<<5)) vram = gpu.VRAM_F;
        else if (mask & (1<<6)) vram = gpu.VRAM_G;

        memcpy(tempBuffer, vram, 1024 * 8 * 2);
        for (int j = 0; j < 1024 * 8; j++)
        {
            u16 value = tempBuffer[j];

            u8 a = (value >> 15) & 0x1;
            u8 b = (value >> 10) & 0x1F;
            u8 g = (value >> 5) & 0x1F;
            u8 r = (value >> 0) & 0x1F;

            tempBuffer[j] = (r << 11) | (g << 6) | (b << 1) | a;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i*8, 1024, 8, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, tempBuffer);
    }
    free(tempBuffer);

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);

    glViewport(0, 0, ScreenW, ScreenH);

    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);

    // clear buffers
    // TODO: clear bitmap
    // TODO: check whether 'clear polygon ID' affects translucent polyID
    // (for example when alpha is 1..30)
    {
        glUseProgram(ClearShaderPlain);
        glDepthFunc(GL_ALWAYS);

        u32 r = gpu.GPU3D.RenderClearAttr1 & 0x1F;
        u32 g = (gpu.GPU3D.RenderClearAttr1 >> 5) & 0x1F;
        u32 b = (gpu.GPU3D.RenderClearAttr1 >> 10) & 0x1F;
        u32 fog = (gpu.GPU3D.RenderClearAttr1 >> 15) & 0x1;
        u32 a = (gpu.GPU3D.RenderClearAttr1 >> 16) & 0x1F;
        u32 polyid = (gpu.GPU3D.RenderClearAttr1 >> 24) & 0x3F;
        u32 z = ((gpu.GPU3D.RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;

        glStencilFunc(GL_ALWAYS, 0xFF, 0xFF);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

        /*if (r) r = r*2 + 1;
        if (g) g = g*2 + 1;
        if (b) b = b*2 + 1;*/

        glUniform4ui(ClearUniformLoc[0], r, g, b, a);
        glUniform1ui(ClearUniformLoc[1], z);
        glUniform1ui(ClearUniformLoc[2], polyid);
        glUniform1ui(ClearUniformLoc[3], fog);

        glBindBuffer(GL_ARRAY_BUFFER, ClearVertexBufferID);
        glBindVertexArray(ClearVertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
    }

    if (LightingActive)
    {
        // no view data = background, 2D-like geometry, translucent polygons
        const GLenum buffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        const float zero[4] = {};
        glDrawBuffers(4, buffers);
        glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glColorMaski(3, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearBufferfv(GL_COLOR, 2, zero);
        glClearBufferfv(GL_COLOR, 3, zero);
        glDrawBuffers(2, buffers);
    }

    if (numrenderpolys)
    {
        // render shit here
        u32 flags = 0;
        if (renderpolys[0]->WBuffer) flags |= RenderFlag_WBuffer;

        EnsureCapacity(renderpolys, numrenderpolys);

        int npolys = 0;
        int firsttrans = -1;
        for (u32 i = 0; i < numrenderpolys; i++)
        {
            if (renderpolys[i]->Degenerate) continue;

            SetupPolygon(&PolygonList[npolys], renderpolys[i]);
            if (firsttrans < 0 && renderpolys[i]->Translucent)
                firsttrans = npolys;

            npolys++;
        }
        NumFinalPolys = npolys;
        NumOpaqueFinalPolys = firsttrans;

        LookupHDTextures(gpu, npolys);
        LookupReliefScales(gpu, npolys);

        // inspector (Pomegrade): polygons coloured by what drew them
        ViewInspector = gpu.NDS.Inspector.GetView() != Inspector::View::Off ? &gpu.NDS.Inspector : nullptr;
        BuildPolygons(&PolygonList[0], npolys);
        glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, NumVertices*VertexSize*4, VertexBuffer.data());
        if (ViewDataActive)
        {
            glBindBuffer(GL_ARRAY_BUFFER, ViewVertexBufferID);
            glBufferSubData(GL_ARRAY_BUFFER, 0, NumVertices*ViewVertexSize*4, ViewVertexBuffer.data());
        }

        // bind to access the index buffer
        glBindVertexArray(VertexArrayID);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, NumIndices * 4, IndexBuffer.data());
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, EdgeIndicesOffset * 4, NumEdgeIndices * 4, IndexBuffer.data() + EdgeIndicesOffset);

        RenderSceneChunk(gpu.GPU3D, 0, 192);

        if (LightingDone)
        {
            // the translucent layer, fog and edge marking again, over the lit
            // opaque layer: the effects don't touch what is drawn in front of
            // the opaque geometry (2D panels, water, smoke, fog)
            glBindFramebuffer(GL_FRAMEBUFFER, LightingFramebuffer);
            // the fog/edge pass left the depth and attribute buffers on these units
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, TexPalMemID);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, TexMemID);
            TranslucentPassOnly = true;
            DSShadowsReplaced = ShadowsDrawn && FindReplacedShadows(gpu.GPU3D);
            RenderSceneChunk(gpu.GPU3D, 0, 192);
            DSShadowsReplaced = false;
            TranslucentPassOnly = false;
            glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
        }
    }
}

void GLRenderer::FrameSnapshot::Take(Polygon** polys, u32 count)
{
    Polygons.resize(count);
    u32 nverts = 0;
    for (u32 i = 0; i < count; i++) nverts += polys[i]->NumVertices;
    Vertices.resize(nverts);
    u32 v = 0;
    for (u32 i = 0; i < count; i++)
    {
        Polygon& copy = Polygons[i];
        copy = *polys[i];
        for (u32 j = 0; j < copy.NumVertices; j++)
        {
            Vertices[v] = *polys[i]->Vertices[j];
            copy.Vertices[j] = &Vertices[v++];
        }
    }
    // the game's submission order (the render list is sorted for drawing)
    Order.resize(count);
    for (u32 i = 0; i < count; i++) Order[i] = i;
    std::stable_sort(Order.begin(), Order.end(), [&](u32 a, u32 b) { return Polygons[a].FrameId < Polygons[b].FrameId; });
}

void GLRenderer::SetFrameGeneration(bool enable) noexcept
{
    if (enable == FrameGeneration) return;
    FrameGeneration = enable;
    for (FrameSnapshot& snapshot : Snapshots)
    {
        snapshot.Polygons.clear();
        snapshot.Vertices.clear();
        snapshot.Order.clear();
    }
}

bool GLRenderer::ResamplePrevious(const Polygon& cur, const Polygon* const* prevPieces, u32 count, Polygon& out)
{
    // a sub-polygon not cut by clipping: every corner knows its place
    if (cur.NumVertices != 3)
        return false;
    for (u32 j = 0; j < 3; j++)
        if (!cur.Vertices[j]->GridPoint) return false;
    auto placeOf = [](u16 g, double& v, double& w) {
        const double n = (g >> 8) & 0xF;
        v = (g & 0xF) / n;
        w = ((g >> 4) & 0xF) / n;
    };

    out = cur;
    for (u32 j = 0; j < 3; j++)
    {
        double v, w;
        placeOf(cur.Vertices[j]->GridPoint, v, w);
        // the previous frame's piece containing that place (pieces with a
        // corner cut by clipping can't tell their places)
        const Polygon* piece = nullptr;
        double weight[3];
        for (u32 q = 0; q < count && !piece; q++)
        {
            const Polygon& p = *prevPieces[q];
            if (p.NumVertices != 3 || !p.Vertices[0]->GridPoint || !p.Vertices[1]->GridPoint || !p.Vertices[2]->GridPoint)
                continue;
            double pv[3], pw[3];
            for (int k = 0; k < 3; k++) placeOf(p.Vertices[k]->GridPoint, pv[k], pw[k]);
            const double det = (pv[1] - pv[0]) * (pw[2] - pw[0]) - (pv[2] - pv[0]) * (pw[1] - pw[0]);
            if (det == 0)
                continue;
            weight[1] = ((v - pv[0]) * (pw[2] - pw[0]) - (pv[2] - pv[0]) * (w - pw[0])) / det;
            weight[2] = ((pv[1] - pv[0]) * (w - pw[0]) - (v - pv[0]) * (pw[1] - pw[0])) / det;
            weight[0] = 1 - weight[1] - weight[2];
            const double eps = 1e-9;
            if (weight[0] >= -eps && weight[1] >= -eps && weight[2] >= -eps)
                piece = &p;
        }
        if (!piece)
            return false;

        Resampled.Vertices.push_back(*cur.Vertices[j]);
        Vertex& vtx = Resampled.Vertices.back();
        out.Vertices[j] = &vtx;
        auto lerp = [&](auto get) {
            double sum = 0;
            for (int k = 0; k < 3; k++) sum += weight[k] * (double)get(*piece->Vertices[k]);
            return sum;
        };
        for (int k = 0; k < 2; k++)
        {
            vtx.PreciseScreen[k] = (float)lerp([k](const Vertex& x) { return x.PreciseScreen[k]; });
            vtx.FinalPosition[k] = (s32)std::lround(lerp([k](const Vertex& x) { return x.FinalPosition[k]; }));
            vtx.HiresPosition[k] = (s32)std::lround(lerp([k](const Vertex& x) { return x.HiresPosition[k]; }));
            vtx.TexCoords[k] = (s16)std::lround(lerp([k](const Vertex& x) { return x.TexCoords[k]; }));
        }
        for (int k = 0; k < 3; k++)
        {
            vtx.FinalColor[k] = (s32)std::lround(lerp([k](const Vertex& x) { return x.FinalColor[k]; }));
            vtx.ViewNormal[k] = (float)lerp([k](const Vertex& x) { return x.ViewNormal[k]; });
        }
        for (int k = 0; k < 4; k++)
            vtx.ViewPosition[k] = (float)lerp([k](const Vertex& x) { return x.ViewPosition[k]; });
        vtx.Specular = (float)lerp([](const Vertex& x) { return x.Specular; });
        double z = 0, w4 = 0;
        for (int k = 0; k < 3; k++)
        {
            z += weight[k] * piece->FinalZ[k];
            w4 += weight[k] * piece->Vertices[k]->Position[3];
        }
        out.FinalZ[j] = (s32)std::lround(z);
        // W as this polygon normalizes it (see GPU3D::FinalizePolygon)
        const s32 curW = cur.Vertices[j]->Position[3];
        out.FinalW[j] = curW > 0 ? (s32)std::lround(w4 * cur.FinalW[j] / curW) : cur.FinalW[j];
    }
    return true;
}

bool GLRenderer::RenderIntermediateFrame(GPU& gpu, u32 outputTexture)
{
    GLTimer::Scope gpuTime(PerformanceCounters::Section::GpuFrameGeneration);
    const FrameSnapshot& prev = Snapshots[0];
    const FrameSnapshot& cur = Snapshots[1];
    if (!FrameGeneration || cur.Polygons.empty() || prev.Polygons.empty())
        return false;

    // pair each polygon with itself in the previous frame. Both frames are
    // walked in the game's submission order, slot by slot (Polygon::FrameId:
    // every polygon the game sent, drawn or not), and aligned like a diff on
    // each slot's signature (texture, palette, attributes): a polygon sent in
    // one frame only (a particle, a line of text) doesn't shift the pairing of
    // the rest. A slot with nothing drawn (culled or clipped away) is
    // compatible with anything, so faces turning away don't shift it either.
    struct Slots
    {
        std::vector<int> First; // first polygon (index into Order) of each slot, -1 if none drawn
        u32 Count = 0;
    };
    auto slotsOf = [](const FrameSnapshot& f) {
        Slots sl;
        sl.Count = f.Order.empty() ? 0 : (f.Polygons[f.Order.back()].FrameId >> 8) + 1;
        sl.First.assign(sl.Count, -1);
        for (size_t k = f.Order.size(); k-- > 0;)
            sl.First[f.Polygons[f.Order[k]].FrameId >> 8] = (int)k;
        return sl;
    };
    const Slots cs = slotsOf(cur), ps = slotsOf(prev);
    auto same = [](const Polygon& a, const Polygon& b) {
        return a.TexParam == b.TexParam && a.TexPalette == b.TexPalette && a.Attr == b.Attr &&
               a.WBuffer == b.WBuffer && a.Type == b.Type;
    };
    // 1 = both drawn and alike, 0 = one or both not drawn, -1 = different
    auto compare = [&](u32 ci, u32 pi) {
        int a = cs.First[ci], b = ps.First[pi];
        if (a < 0 || b < 0) return 0;
        return same(cur.Polygons[cur.Order[a]], prev.Polygons[prev.Order[b]]) ? 1 : -1;
    };
    // a resync point: both drawn and alike, and the next slots don't disagree
    auto resync = [&](u32 ci, u32 pi) {
        return compare(ci, pi) == 1 && (ci + 1 >= cs.Count || pi + 1 >= ps.Count || compare(ci + 1, pi + 1) >= 0);
    };
    std::vector<const Polygon*> match(cur.Polygons.size(), nullptr);
    u32 matched = 0;
    // room for every polygon's resampled counterpart (pointers into it stay valid)
    Resampled.Polygons.clear();
    Resampled.Vertices.clear();
    Resampled.Polygons.reserve(cur.Polygons.size());
    Resampled.Vertices.reserve(cur.Polygons.size() * 3);
    std::vector<const Polygon*> prevPieces;
    u32 ci = 0, pi = 0;
    while (ci < cs.Count && pi < ps.Count)
    {
        if (compare(ci, pi) < 0)
        {
            // the nearest point where both continue together, up to 64 slots ahead on either side
            bool found = false;
            for (u32 d = 1; d <= 64 && !found; d++)
            {
                if (pi + d < ps.Count && resync(ci, pi + d)) { pi += d; found = true; }
                else if (ci + d < cs.Count && resync(ci + d, pi)) { ci += d; found = true; }
            }
            if (!found) { ci++; pi++; continue; } // replaced by something else
        }
        // pair the slots' polygons: a polygon, or the sub-polygons of one by their place
        int a = cs.First[ci], b = ps.First[pi];
        if (a >= 0 && b >= 0)
        {
            size_t ka = a, kb = b;
            while (ka < cur.Order.size() && (cur.Polygons[cur.Order[ka]].FrameId >> 8) == ci &&
                   kb < prev.Order.size() && (prev.Polygons[prev.Order[kb]].FrameId >> 8) == pi)
            {
                const Polygon& c = cur.Polygons[cur.Order[ka]];
                const Polygon& p = prev.Polygons[prev.Order[kb]];
                u32 subC = c.FrameId & 0xFF, subP = p.FrameId & 0xFF;
                if (subC == subP)
                {
                    // the same piece of the parent only if it was subdivided alike
                    // (adaptive multiplier level, see GPU3D::SetPolygonMultiplierScale)
                    if (c.NumVertices == p.NumVertices && c.Subdivision == p.Subdivision)
                    {
                        match[cur.Order[ka]] = &p;
                        matched++;
                    }
                    ka++;
                    kb++;
                }
                else if (subC < subP) ka++;
                else kb++;
            }

            // sub-polygons left without a counterpart, their triangle subdivided
            // differently in the previous frame (its level changed): each
            // corner's place in the triangle is found in the previous frame's
            // subdivision of the same triangle, and interpolated there
            for (size_t k = a; k < cur.Order.size() && (cur.Polygons[cur.Order[k]].FrameId >> 8) == ci; k++)
            {
                const u32 i = cur.Order[k];
                const Polygon& c = cur.Polygons[i];
                if (match[i] || (c.FrameId & 0xFF) == 0xFF)
                    continue;
                prevPieces.clear();
                for (size_t q = b; q < prev.Order.size() && (prev.Polygons[prev.Order[q]].FrameId >> 8) == pi; q++)
                {
                    const Polygon& p = prev.Polygons[prev.Order[q]];
                    if ((p.FrameId & 0xFF) != 0xFF && (p.Subdivision >> 16) == (c.Subdivision >> 16))
                        prevPieces.push_back(&p);
                }
                Resampled.Polygons.emplace_back();
                if (ResamplePrevious(c, prevPieces.data(), (u32)prevPieces.size(), Resampled.Polygons.back()))
                {
                    match[i] = &Resampled.Polygons.back();
                    matched++;
                }
                else
                    Resampled.Polygons.pop_back();
            }
        }
        ci++;
        pi++;
    }
    // most of the scene changed (a cut, a new screen): nothing in between to
    // show, the frame already shown stays
    if (matched * 2 < cur.Polygons.size())
        return false;

    Intermediate.Polygons.resize(cur.Polygons.size());
    Intermediate.Vertices.resize(cur.Vertices.size());
    IntermediateList.resize(cur.Polygons.size());
    u32 v = 0;
    for (u32 i = 0; i < cur.Polygons.size(); i++)
    {
        Polygon& out = Intermediate.Polygons[i];
        const Polygon& c = cur.Polygons[i];
        out = c;
        const Polygon* p = match[i];
        // a polygon that jumped across the screen (teleport) isn't interpolated
        if (p)
        {
            float jump = 0;
            for (u32 j = 0; j < c.NumVertices; j++)
                jump = std::max(jump, std::fabs(c.Vertices[j]->PreciseScreen[0] - p->Vertices[j]->PreciseScreen[0]) +
                                      std::fabs(c.Vertices[j]->PreciseScreen[1] - p->Vertices[j]->PreciseScreen[1]));
            if (jump > 64) p = nullptr;
        }
        for (u32 j = 0; j < c.NumVertices; j++)
        {
            Vertex& vtx = Intermediate.Vertices[v++];
            vtx = *c.Vertices[j];
            out.Vertices[j] = &vtx;
            if (!p) continue;
            const Vertex& a = *p->Vertices[j];
            const Vertex& b = *c.Vertices[j];
            auto mid = [](s32 x, s32 y) { return (s32)(((s64)x + y) >> 1); };
            for (int k = 0; k < 2; k++)
            {
                vtx.FinalPosition[k] = mid(a.FinalPosition[k], b.FinalPosition[k]);
                vtx.HiresPosition[k] = mid(a.HiresPosition[k], b.HiresPosition[k]);
                vtx.PreciseScreen[k] = (a.PreciseScreen[k] + b.PreciseScreen[k]) * 0.5f;
                vtx.TexCoords[k] = (s16)mid(a.TexCoords[k], b.TexCoords[k]);
            }
            for (int k = 0; k < 3; k++)
            {
                vtx.FinalColor[k] = mid(a.FinalColor[k], b.FinalColor[k]);
                vtx.ViewNormal[k] = (a.ViewNormal[k] + b.ViewNormal[k]) * 0.5f;
            }
            for (int k = 0; k < 4; k++)
                vtx.ViewPosition[k] = (a.ViewPosition[k] + b.ViewPosition[k]) * 0.5f;
            vtx.Specular = (a.Specular + b.Specular) * 0.5f;
            out.FinalZ[j] = mid(p->FinalZ[j], c.FinalZ[j]);
            out.FinalW[j] = mid(p->FinalW[j], c.FinalW[j]);
        }
        IntermediateList[i] = &out;
    }

    // keep the renderer's image (the next frame shows it, the game may capture it)
    if (BackupW != ScreenW || BackupH != ScreenH)
    {
        for (GLuint* tex : {&BackupColorTex, &BackupLightingTex})
        {
            if (!*tex) glGenTextures(1, tex);
            SetupDefaultTexParams(*tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        BackupW = ScreenW;
        BackupH = ScreenH;
    }
    // copied by blits: glCopyImageSubData refuses the colour buffer, whose
    // format is unsized GL_RGBA (GL_INVALID_OPERATION, measured on Mesa)
    if (!CopyFramebuffers[0]) glGenFramebuffers(2, CopyFramebuffers);
    auto copy = [&](GLuint from, GLuint to) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, CopyFramebuffers[0]);
        glFramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, from, 0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CopyFramebuffers[1]);
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, to, 0);
        const GLenum attachment = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &attachment);
        glDisable(GL_SCISSOR_TEST);
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, ScreenW, ScreenH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    };
    const bool lightingDone = LightingDone;
    copy(ColorBufferTex, BackupColorTex);
    if (lightingDone)
        copy(LightingTex, BackupLightingTex);

    RenderScene(gpu, IntermediateList.data(), (u32)IntermediateList.size(), true);
    CurGLCompositor.RenderIntermediateFrame(gpu, *this, outputTexture);

    copy(BackupColorTex, ColorBufferTex);
    if (lightingDone)
        copy(BackupLightingTex, LightingTex);
    LightingDone = lightingDone;
    glBindFramebuffer(GL_FRAMEBUFFER, MainFramebuffer);
    return true;
}

void GLRenderer::Stop(const GPU& gpu)
{
    CurGLCompositor.Stop(gpu);
}

void GLRenderer::PrepareCaptureFrame()
{
    CapturedSinceRender = true;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MainFramebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DownscaleFramebuffer);
    GLenum bufferAttachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &bufferAttachment);
    glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, DownscaleFramebuffer);
    glReadPixels(0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}

void GLRenderer::Blit(const GPU& gpu)
{
    CurGLCompositor.RenderFrame(gpu, *this);
}

void GLRenderer::SetOutputTexture(int buffer, u32 texture)
{
    CurGLCompositor.SetOutputTexture(buffer, (GLuint) texture);
}

void GLRenderer::BindOutputTexture(int buffer)
{
    CurGLCompositor.BindOutputTexture(buffer);
}

u32* GLRenderer::GetLine(int line)
{
    int stride = 256;

    if (line == 0)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
        u8* data = (u8*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 4 * stride * 192, GL_MAP_READ_BIT);
        if (data) memcpy(&Framebuffer[stride*0], data, 4*stride*192);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    u64* ptr = (u64*)&Framebuffer[stride * line];
    for (int i = 0; i < stride; i+=2)
    {
        // Data is in BGRA format but we need to convert it to RGBA
        u64 redBits = (*ptr & 0x000000FC000000FC) << 16;
        u64 blueBits = (*ptr & 0x00FC000000FC0000) >> 16;

        u64 rgb = (*ptr & 0x0000FC000000FC00) | redBits | blueBits;
        u64 a = *ptr & 0xF8000000F8000000;

        *ptr++ = (rgb >> 2) | (a >> 3);
    }

    return &Framebuffer[stride * line];
}

void GLRenderer::SetupAccelFrame()
{
    // the lit image is for display only: display capture (PrepareCaptureFrame)
    // keeps reading the DS render, like the downscale of upscaled frames
    glBindTexture(GL_TEXTURE_2D, LightingDone ? LightingTex : ColorBufferTex);
}

}
