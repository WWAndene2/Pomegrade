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

#include "GPU_OpenGL.h"

#include <assert.h>
#include <cstdio>
#include <cstring>

#include "NDS.h"
#include "GPU.h"
#include "GPU3D_OpenGL.h"
#include "OpenGLSupport.h"
#include "GPU_OpenGL_shaders.h"

namespace melonDS
{

using namespace OpenGL;

std::optional<GLCompositor> GLCompositor::New() noexcept
{
    assert(glBindAttribLocation != nullptr);
    GLuint CompShader {};

    if (!OpenGL::CompileVertexFragmentProgram(CompShader,
            kCompositorVS, kCompositorFS_Nearest,
            "CompositorShader",
            {{"vPosition", 0}, {"vTexcoord", 1}},
            {{"oColor", 0}}))
        return std::nullopt;

    return { GLCompositor(CompShader) };
}

GLCompositor::GLCompositor(GLuint compShader) noexcept : CompShader(compShader)
{
    CompScaleLoc = glGetUniformLocation(CompShader, "u3DScale");
    CompHighColorLoc = glGetUniformLocation(CompShader, "uHighColor");
    SceneColourState.AdjustLoc = glGetUniformLocation(CompShader, "uAdjust");
    SceneColourState.LevelsLoc = glGetUniformLocation(CompShader, "uLevels");
    SceneColourState.SaturationLoc = glGetUniformLocation(CompShader, "uSaturation");
    SceneColourState.OledLoc = glGetUniformLocation(CompShader, "uOledThreshold");

    glUseProgram(CompShader);
    GLuint screenTextureUniform = glGetUniformLocation(CompShader, "ScreenTex");
    glUniform1i(screenTextureUniform, 0);
    GLuint _3dTextureUniform = glGetUniformLocation(CompShader, "_3DTex");
    glUniform1i(_3dTextureUniform, 1);

    // all this mess is to prevent bleeding
#define SETVERTEX(i, x, y, offset) \
    CompVertices[i].Position[0] = x; \
    CompVertices[i].Position[1] = y + offset; \
    CompVertices[i].Texcoord[0] = (x + 1.f) * (256.f / 2.f); \
    CompVertices[i].Texcoord[1] = (y + 1.f) * (384.f / 2.f)

    const float padOffset = 1.f/(192*2.f+2.f)*2.f;
    // top screen
    SETVERTEX(0, -1, 1, 0);
    SETVERTEX(1, 1, 0, padOffset);
    SETVERTEX(2, 1, 1, 0);
    SETVERTEX(3, -1, 1, 0);
    SETVERTEX(4, -1, 0, padOffset);
    SETVERTEX(5, 1, 0, padOffset);

    // bottom screen
    SETVERTEX(6, -1, 0, -padOffset);
    SETVERTEX(7, 1, -1, 0);
    SETVERTEX(8, 1, 0, -padOffset);
    SETVERTEX(9, -1, 0, -padOffset);
    SETVERTEX(10, -1, -1, 0);
    SETVERTEX(11, 1, -1, 0);

#undef SETVERTEX

    glGenBuffers(1, &CompVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CompVertices), &CompVertices[0], GL_STATIC_DRAW);

    glGenVertexArrays(1, &CompVertexArrayID);
    glBindVertexArray(CompVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CompVertex), (void*)(offsetof(CompVertex, Position)));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CompVertex), (void*)(offsetof(CompVertex, Texcoord)));

    glGenFramebuffers(CompScreenOutputFB.size(), &CompScreenOutputFB[0]);

    glGenTextures(1, &CompScreenInputTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CompScreenInputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256*3 + 1, 192*2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);

    glGenTextures(CompScreenOutputTex.size(), &CompScreenOutputTex[0]);
    for (GLuint i : CompScreenOutputTex)
    {
        glBindTexture(GL_TEXTURE_2D, i);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLCompositor::~GLCompositor()
{
    assert(glDeleteFramebuffers != nullptr);

    glDeleteFramebuffers(CompScreenOutputFB.size(), &CompScreenOutputFB[0]);
    glDeleteFramebuffers(1, &IntermediateFB);
    glDeleteTextures(1, &CompScreenInputTex);
    glDeleteTextures(CompScreenOutputTex.size(), &CompScreenOutputTex[0]);

    glDeleteVertexArrays(1, &CompVertexArrayID);
    glDeleteBuffers(1, &CompVertexBufferID);

    glDeleteProgram(CompShader);
    DeleteSceneColourTargets();
}

void GLCompositor::DeleteSceneColourTargets() noexcept
{
    SceneColourPass& s = SceneColourState;
    if (s.FB) glDeleteFramebuffers(1, &s.FB);
    if (s.Tex) glDeleteTextures(1, &s.Tex);
    if (s.PBO) glDeleteBuffers(1, &s.PBO);
    s.FB = s.Tex = s.PBO = 0;
    s.Pending = false;
}

void GLCompositor::RunSceneColourPass() noexcept
{
    // expects the compositor's program, textures and vertex array bound
    SceneColourPass& s = SceneColourState;
    if (!s.FB)
    {
        // the active unit holds the 3D layer: put its binding back afterwards
        GLint prevTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        glGenTextures(1, &s.Tex);
        glBindTexture(GL_TEXTURE_2D, s.Tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SceneColourPass::Width, SceneColourPass::Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &s.FB);
        glBindFramebuffer(GL_FRAMEBUFFER, s.FB);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.Tex, 0);
        glGenBuffers(1, &s.PBO);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, s.PBO);
        glBufferData(GL_PIXEL_PACK_BUFFER, SceneColourPass::Width * SceneColourPass::Height * 4, nullptr, GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, prevTexture);
    }

    // last frame's samples, read back in the background meanwhile
    if (s.Pending)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, s.PBO);
        void* samples = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, SceneColourPass::Width * SceneColourPass::Height * 4, GL_MAP_READ_BIT);
        if (samples)
        {
            SceneStats stats = SceneColour::Measure((const u32*)samples, SceneColourPass::Width * SceneColourPass::Height);
            s.Target = SceneColour::Target(stats, s.Adaptive, s.Oled);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        s.Pending = false;
    }
    SceneColour::Step(s.Current, s.Target);

    // this frame's unadjusted image, small
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.FB);
    glViewport(0, 0, SceneColourPass::Width, SceneColourPass::Height);
    glUniform1ui(s.AdjustLoc, 0);
    glDrawArrays(GL_TRIANGLES, 0, 4*3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s.FB);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, s.PBO);
    glReadPixels(0, 0, SceneColourPass::Width, SceneColourPass::Height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    s.Pending = true;
}


GLCompositor::GLCompositor(GLCompositor&& other) noexcept :
    Scale(other.Scale),
    ScreenH(other.ScreenH),
    ScreenW(other.ScreenW),
    CompScaleLoc(other.CompScaleLoc),
    CompHighColorLoc(other.CompHighColorLoc),
    HighColor(other.HighColor),
    SceneColourState(other.SceneColourState),
    CompVertices(other.CompVertices),
    CompShader(other.CompShader),
    CompVertexBufferID(other.CompVertexBufferID),
    CompVertexArrayID(other.CompVertexArrayID),
    CompScreenInputTex(other.CompScreenInputTex),
    CompScreenOutputTex(other.CompScreenOutputTex),
    CompScreenOutputFB(other.CompScreenOutputFB),
    IntermediateFB(other.IntermediateFB)
{
    other.IntermediateFB = 0;
    other.SceneColourState.FB = other.SceneColourState.Tex = other.SceneColourState.PBO = 0;
    other.CompScreenOutputFB = {};
    other.CompScreenInputTex = {};
    other.CompScreenOutputTex = {};
    other.CompVertexArrayID = {};
    other.CompVertexBufferID = {};
    other.CompShader = {};
}

GLCompositor& GLCompositor::operator=(GLCompositor&& other) noexcept
{
    if (this != &other)
    {
        Scale = other.Scale;
        ScreenH = other.ScreenH;
        ScreenW = other.ScreenW;
        CompScaleLoc = other.CompScaleLoc;
        CompHighColorLoc = other.CompHighColorLoc;
        HighColor = other.HighColor;
        DeleteSceneColourTargets();
        SceneColourState = other.SceneColourState;
        other.SceneColourState.FB = other.SceneColourState.Tex = other.SceneColourState.PBO = 0;
        CompVertices = other.CompVertices;

        // Clean up these resources before overwriting them
        glDeleteProgram(CompShader);
        CompShader = other.CompShader;

        glDeleteBuffers(1, &CompVertexBufferID);
        CompVertexBufferID = other.CompVertexBufferID;

        glDeleteVertexArrays(1, &CompVertexArrayID);
        CompVertexArrayID = other.CompVertexArrayID;

        glDeleteTextures(1, &CompScreenInputTex);
        CompScreenInputTex = other.CompScreenInputTex;

        glDeleteTextures(CompScreenOutputTex.size(), &CompScreenOutputTex[0]);
        CompScreenOutputTex = other.CompScreenOutputTex;

        glDeleteFramebuffers(CompScreenOutputFB.size(), &CompScreenOutputFB[0]);
        CompScreenOutputFB = other.CompScreenOutputFB;

        glDeleteFramebuffers(1, &IntermediateFB);
        IntermediateFB = other.IntermediateFB;
        other.IntermediateFB = 0;

        other.CompScreenOutputFB = {};
        other.CompScreenInputTex = {};
        other.CompScreenOutputTex = {};
        other.CompVertexArrayID = {};
        other.CompVertexBufferID = {};
        other.CompShader = {};
    }

    return *this;
}


void GLCompositor::SetScaleFactor(int scale) noexcept
{
    if (scale == Scale)
        return;

    Scale = scale;
    ScreenW = 256 * scale;
    ScreenH = (384+2) * scale;

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        // fill the padding
        u8* zeroPixels = (u8*) calloc(1, ScreenW*2*scale*4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192*scale, ScreenW, 2*scale, GL_RGBA, GL_UNSIGNED_BYTE, zeroPixels);

        GLenum fbassign[] = {GL_COLOR_ATTACHMENT0};
        glBindFramebuffer(GL_FRAMEBUFFER, CompScreenOutputFB[i]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CompScreenOutputTex[i], 0);
        glDrawBuffers(1, fbassign);
        free(zeroPixels);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLCompositor::Stop(const GPU& gpu) noexcept
{
    for (int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CompScreenOutputFB[gpu.FrontBuffer]);

        glClear(GL_COLOR_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLCompositor::RenderFrame(const GPU& gpu, Renderer3D& renderer) noexcept
{
    int backbuf = gpu.FrontBuffer ^ 1;
    Composite(gpu, renderer, CompScreenOutputFB[backbuf], backbuf, true);
}

void GLCompositor::RenderIntermediateFrame(const GPU& gpu, Renderer3D& renderer, GLuint texture) noexcept
{
    if (!IntermediateFB) glGenFramebuffers(1, &IntermediateFB);
    glBindFramebuffer(GL_FRAMEBUFFER, IntermediateFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    // the frame's 2D layers are already in the input texture: its composite
    // uploaded them at VBlank, from the buffer that became the front buffer
    // when the frame ended
    Composite(gpu, renderer, IntermediateFB, -1, false);
}

void GLCompositor::Composite(const GPU& gpu, Renderer3D& renderer, GLuint framebuffer, int backbuf, bool advanceSceneColour) noexcept
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, ScreenW, ScreenH);

    // TODO: select more shaders (filtering, etc)
    glUseProgram(CompShader);
    glUniform1ui(CompScaleLoc, Scale);
    glUniform1ui(CompHighColorLoc, HighColor ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CompScreenInputTex);

    if (backbuf >= 0 && gpu.Framebuffer[backbuf][0] && gpu.Framebuffer[backbuf][1])
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256*3 + 1, 192, GL_RGBA_INTEGER,
                        GL_UNSIGNED_BYTE, gpu.Framebuffer[backbuf][0].get());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256*3 + 1, 192, GL_RGBA_INTEGER,
                        GL_UNSIGNED_BYTE, gpu.Framebuffer[backbuf][1].get());
    }

    glActiveTexture(GL_TEXTURE1);
    renderer.SetupAccelFrame();

    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBindVertexArray(CompVertexArrayID);

    SceneColourPass& s = SceneColourState;
    bool adjust = s.Adaptive || s.Oled;
    if (adjust)
    {
        if (advanceSceneColour)
        {
            RunSceneColourPass();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            glViewport(0, 0, ScreenW, ScreenH);
        }
        glUniform2f(s.LevelsLoc, s.Current.LevelBlack, s.Current.LevelWhite);
        glUniform1f(s.SaturationLoc, s.Current.Saturation);
        glUniform1f(s.OledLoc, s.Current.OledThreshold);
    }
    else if (advanceSceneColour)
    {
        // back to neutral, so turning it on again starts from the plain image
        s.Current = s.Target = SceneColourParams();
    }
    glUniform1ui(s.AdjustLoc, adjust ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 4*3);
}

void GLCompositor::SetOutputTexture(int buf, GLuint texture)
{
    GLenum fbassign[] = {GL_COLOR_ATTACHMENT0};
    glBindFramebuffer(GL_FRAMEBUFFER, CompScreenOutputFB[buf]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
    glDrawBuffers(1, fbassign);
}

void GLCompositor::BindOutputTexture(int buf)
{
    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex[buf]);
}

}
