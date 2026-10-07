// Performance counters: drives the real DS 3D engine with geometry commands
// (a lit floor, wall and sphere, as in tests/lighting-effects), renders frames
// with the OpenGL renderer's effects and frame generation, and checks that
// - off, nothing is timed: no query object, the same image;
// - on, each section the frame used is measured, and only those;
// - turned off again, the published values are cleared.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "GPU_OpenGL_Timer.h"
#include "PerformanceCounters.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace melonDS;
using PerformanceCounters::Section;

static bool InitEGL()
{
    auto getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (!eglInitialize(dpy, nullptr, nullptr)) return false;
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint cfgAttr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n;
    if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n == 0) return false;
    EGLint ctxAttr[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
}

// --- geometry commands, through the GX command ports ---

static NDS* Nds;

static void Pump()
{
    GPU3D& g = Nds->GPU.GPU3D;
    for (int i = 0; i < 1000 && (g.GXStat & (1 << 27)); i++)
    {
        Nds->ARM9Timestamp += (u64)64 << Nds->ARM9ClockShift;
        g.Run();
    }
}

static void Cmd(u8 cmd, std::initializer_list<u32> params = {0})
{
    for (u32 p : params)
    {
        Nds->GPU.GPU3D.Write32(0x04000400 + cmd * 4, p);
        Pump();
    }
}

static u32 Fx(double v) { return (u32)(s32)std::lround(v * 4096.0); } // 20.12
static u32 N10(double v) { s32 i = (s32)std::lround(v * 511.0); return (u32)i & 0x3FF; } // s0.9

static void LoadMatrix(const double m[16])
{
    std::initializer_list<u32> p = {Fx(m[0]),Fx(m[1]),Fx(m[2]),Fx(m[3]),Fx(m[4]),Fx(m[5]),Fx(m[6]),Fx(m[7]),
                                    Fx(m[8]),Fx(m[9]),Fx(m[10]),Fx(m[11]),Fx(m[12]),Fx(m[13]),Fx(m[14]),Fx(m[15])};
    Cmd(0x16, p);
}

static void Vertex16(double x, double y, double z)
{
    u32 ix = Fx(x) & 0xFFFF, iy = Fx(y) & 0xFFFF, iz = Fx(z) & 0xFFFF;
    Cmd(0x23, {ix | (iy << 16), iz});
}

static void Normal(double x, double y, double z)
{
    Cmd(0x21, {N10(x) | (N10(y) << 10) | (N10(z) << 20)});
}

static const double F = 1.0 / std::tan(25.0 * M_PI / 180.0), Aspect = 256.0 / 192.0, Zn = 0.5, Zf = 20;
static const double FloorY = -1.0;

// lit sphere above the floor, moved by x from frame to frame
static void DrawSphere(double x)
{
    const int seg = 12, rings = 8;
    const double cz = -3.5, r = 0.8;
    auto point = [&](int ring, int s, double* p) {
        double th = M_PI * ring / rings, ph = 2 * M_PI * s / seg;
        p[0] = std::sin(th) * std::cos(ph); p[1] = std::cos(th); p[2] = std::sin(th) * std::sin(ph);
    };
    Cmd(0x40, {1});
    for (int ring = 0; ring < rings; ring++)
        for (int s = 0; s < seg; s++)
        {
            int corners[4][2] = {{ring, s}, {ring, s + 1}, {ring + 1, s + 1}, {ring + 1, s}};
            for (auto& c : corners)
            {
                double p[3]; point(c[0], c[1], p);
                Normal(p[0], p[1], p[2]);
                Vertex16(x + p[0] * r, FloorY + r + 0.1 + p[1] * r, cz + p[2] * r);
            }
        }
    Cmd(0x41);
}

static void SubmitScene(double x)
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000 | (8 << 10) | (6 << 5) | 4);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    double proj[16] = {F / Aspect, 0, 0, 0,  0, F, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x32, {N10(-0.3) | (N10(-0.8) << 10) | (N10(-0.5) << 20)}); // light 0, from above
    Cmd(0x33, {0x7FFF});
    Cmd(0x30, {(0x5AD6) | (0x2108u << 16)});
    Cmd(0x31, {0x7FFF}); // shiny: reflections
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01}); // light 0, front faces, opaque

    Cmd(0x40, {1});
    for (int i = 0; i < 8; i++)
    {
        double z0 = -1.6 - i * 0.8, z1 = z0 - 0.8;
        Normal(0, 1, 0); Vertex16(-4, FloorY, z0);
        Normal(0, 1, 0); Vertex16(4, FloorY, z0);
        Normal(0, 1, 0); Vertex16(4, FloorY, z1);
        Normal(0, 1, 0); Vertex16(-4, FloorY, z1);
    }
    Normal(0, 0, 1); Vertex16(-4, FloorY, -6);
    Normal(0, 0, 1); Vertex16(4, FloorY, -6);
    Normal(0, 0, 1); Vertex16(4, 3, -6);
    Normal(0, 0, 1); Vertex16(-4, 3, -6);
    Cmd(0x41);

    Cmd(0x31, {0});
    DrawSphere(x);
    Cmd(0x50, {0});
    g.VBlank();
}

// the compositor's output, top screen
static std::vector<u32> Composite(GLRenderer& r, GPU& gpu)
{
    int backbuf = gpu.FrontBuffer ^ 1;
    const int stride = 256 * 3 + 1;
    for (int s = 0; s < 2; s++)
    {
        gpu.Framebuffer[backbuf][s] = std::make_unique<u32[]>(stride * 192);
        u32* fb = gpu.Framebuffer[backbuf][s].get();
        memset(fb, 0, stride * 192 * 4);
        for (int y = 0; y < 192; y++)
            fb[y * stride + 768] = 1 << 16; // master brightness entry: display mode 1, 3D shown
    }
    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 386, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    r.SetOutputTexture(backbuf, tex);
    r.Blit(gpu);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    std::vector<u32> all(256 * 386);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glReadPixels(0, 0, 256, 386, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return std::vector<u32>(all.begin() + 194 * 256, all.end());
}

static bool RenderIntermediate(GLRenderer& r, GPU& gpu)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 386, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    bool rendered = r.RenderIntermediateFrame(gpu, tex);
    glFinish();
    glDeleteTextures(1, &tex);
    return rendered;
}

// one emulated frame as the app's loop runs it (MelonInstance): the frame, its
// in-between image, then the counters
static std::vector<u32> Frame(GLRenderer& r, GPU& gpu, double x, bool intermediate)
{
    std::vector<u32> img;
    {
        PerformanceCounters::CpuScope cpu(Section::EmulationCpu);
        SubmitScene(x);
        r.RenderFrame(gpu);
        img = Composite(r, gpu);
        if (intermediate) RenderIntermediate(r, gpu);
    }
    GLTimer::Collect();
    PerformanceCounters::EndFrame();
    return img;
}

// frames until a second of them has been published (llvmpipe: a few seconds)
static PerformanceCounters::Snapshot Measure(GLRenderer& r, GPU& gpu, bool intermediate)
{
    // the first published window may hold sections timed before a change of
    // settings: two windows
    int published = 0;
    PerformanceCounters::Snapshot last = PerformanceCounters::Get();
    for (int i = 0; i < 2000 && published < 2; i++)
    {
        Frame(r, gpu, (i % 2) ? 0.3 : -0.3, intermediate);
        PerformanceCounters::Snapshot s = PerformanceCounters::Get();
        if (memcmp(&s, &last, sizeof(s)) && s.Frames > 0) { published++; last = s; }
    }
    return last;
}

static void Print(const PerformanceCounters::Snapshot& s)
{
    static const char* names[] = {"emulation CPU", "polygon multiplier CPU", "textures CPU", "GPU scene", "GPU shadows",
                                  "GPU lighting", "GPU compositor", "GPU frame generation"};
    printf("  %d frames:", s.Frames);
    for (int i = 0; i < PerformanceCounters::SectionCount; i++) printf(" %s %.3f ms;", names[i], s.Ms[i]);
    printf("\n");
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    printf("OpenGL: %s / %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    Nds = nds.get();
    GPU& gpu = nds->GPU;
    gpu.GPU3D.Reset();
    gpu.GPU3D.SetEnabled(true, true);

    auto r = GLRenderer::New();
    if (!r) { puts("GLRenderer::New failed"); return 1; }
    r->SetRenderSettings(false, 1);
    while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }

    bool ok = true;
    auto check = [&](bool cond, const char* what) { printf("%s: %s\n", what, cond ? "yes" : "NO"); ok = ok && cond; };

    r->SetAmbientOcclusion(true);
    r->SetLightBounce(true);
    r->SetShadows(true);
    r->SetReflections(true);
    r->SetFrameGeneration(true);
    gpu.GPU3D.SetPolygonMultiplier(4);

    // --- off ---
    for (int i = 0; i < 4; i++) Frame(*r, gpu, 0, true);
    auto offImage = Frame(*r, gpu, 0, true);
    bool anyQuery = false;
    for (GLuint id = 1; id < 256; id++) anyQuery = anyQuery || glIsQuery(id);
    check(!anyQuery, "off: no query object created");
    check(PerformanceCounters::Get().Frames == 0, "off: nothing published");

    // --- on: every effect ---
    PerformanceCounters::SetEnabled(true);
    auto onImage = Frame(*r, gpu, 0, true);
    check(onImage == offImage, "on: the image is the same as off");
    auto all = Measure(*r, gpu, true);
    Print(all);
    check(all.Frames > 0, "on: values published");
    check(all.Ms[(int)Section::EmulationCpu] > 0, "emulation CPU measured");
    check(all.Ms[(int)Section::PolygonMultiplierCpu] > 0, "polygon multiplier CPU measured");
    check(all.Ms[(int)Section::PolygonMultiplierCpu] < all.Ms[(int)Section::EmulationCpu],
          "polygon multiplier part of the emulation CPU");
    check(all.Ms[(int)Section::TexturesCpu] >= 0, "textures CPU measured (untextured scene: about 0)");
    check(all.Ms[(int)Section::GpuScene] > 0, "GPU scene measured");
    check(all.Ms[(int)Section::GpuShadows] > 0, "GPU shadows measured");
    check(all.Ms[(int)Section::GpuLighting] > 0, "GPU lighting measured");
    check(all.Ms[(int)Section::GpuCompositor] > 0, "GPU compositor measured");
    check(all.Ms[(int)Section::GpuFrameGeneration] > 0, "GPU frame generation measured");
    double gpuTotal = 0;
    for (int s = (int)Section::GpuScene; s < PerformanceCounters::SectionCount; s++) gpuTotal += all.Ms[s];
    // llvmpipe runs on the CPU, inside the GL calls the emulation thread makes
    check(gpuTotal < all.Ms[(int)Section::EmulationCpu] * 1.5, "GPU total plausible against the frame's CPU time");

    // --- on: effects off, no frame generation, no multiplier ---
    r->SetAmbientOcclusion(false);
    r->SetLightBounce(false);
    r->SetShadows(false);
    r->SetReflections(false);
    r->SetFrameGeneration(false);
    gpu.GPU3D.SetPolygonMultiplier(1);
    auto plain = Measure(*r, gpu, false);
    Print(plain);
    check(plain.Ms[(int)Section::GpuScene] > 0, "plain: GPU scene measured");
    check(plain.Ms[(int)Section::GpuShadows] == 0, "plain: no shadow time");
    check(plain.Ms[(int)Section::GpuLighting] == 0, "plain: no lighting time");
    check(plain.Ms[(int)Section::GpuFrameGeneration] == 0, "plain: no frame generation time");
    check(plain.Ms[(int)Section::PolygonMultiplierCpu] == 0, "plain: no multiplier time");
    check(plain.Ms[(int)Section::GpuScene] < all.Ms[(int)Section::GpuScene] + all.Ms[(int)Section::GpuLighting]
                                                 + all.Ms[(int)Section::GpuShadows],
          "plain: the scene costs less than with the effects");

    // --- on: nothing drawn with OpenGL (as with the software renderer) ---
    PerformanceCounters::Snapshot noGpu = PerformanceCounters::Get();
    for (int i = 0; i < 100000 && (noGpu.Frames == 0 || noGpu.Ms[(int)Section::GpuScene] >= 0); i++)
    {
        {
            PerformanceCounters::CpuScope cpu(Section::EmulationCpu);
            SubmitScene(0);
        }
        GLTimer::Collect();
        PerformanceCounters::EndFrame();
        noGpu = PerformanceCounters::Get();
    }
    Print(noGpu);
    check(noGpu.Ms[(int)Section::EmulationCpu] > 0, "no GPU work: CPU still measured");
    check(noGpu.Ms[(int)Section::GpuScene] < 0 && noGpu.Ms[(int)Section::GpuCompositor] < 0,
          "no GPU work: GPU reported as not timed");

    // --- off again ---
    PerformanceCounters::SetEnabled(false);
    Frame(*r, gpu, 0, false);
    check(PerformanceCounters::Get().Frames == 0, "off again: published values cleared");

    r.reset();
    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
