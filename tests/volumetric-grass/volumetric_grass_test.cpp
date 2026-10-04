// Relief by material and volumetric grass (OpenGL renderer): a floor drawn
// through the real DS 3D engine with a grass texture, then a sky texture,
// relief off and strong. The grass texture is classified as foliage and
// drawn as a slab of blades (more detail than the flat texture, still
// green); the sky texture gets no relief at all. Writes the images next to
// the binary.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "stb/stb_image_write.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace melonDS;

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

static void TexCoord(double s, double t) { Cmd(0x22, {((u32)(s32)std::lround(s * 16) & 0xFFFF) | ((u32)(s32)std::lround(t * 16) << 16)}); }

static const double F = 1.0 / std::tan(25.0 * M_PI / 180.0), Aspect = 256.0 / 192.0, Zn = 0.5, Zf = 20, FloorY = -1;

// a lit floor seen from above, textured with the 64x64 direct-colour texture
// at VRAM address 0, one texel for 1/8 of a unit
static void SubmitScene()
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0x1); // DISP3DCNT: textures
    g.Write32(0x04000350, 0x1F0000 | (8 << 10) | (6 << 5) | 4);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    double proj[16] = {F / Aspect, 0, 0, 0,  0, F, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x32, {N10(-0.3) | (N10(-0.8) << 10) | (N10(-0.5) << 20)}); // light 0 from above
    Cmd(0x33, {0x7FFF});
    Cmd(0x30, {(0x5AD6) | (0x2108u << 16)});
    Cmd(0x31, {0});
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01});
    // texture: direct colour (7), 64x64 (3, 3), repeat in s and t, address 0
    Cmd(0x2A, {(7u << 26) | (3u << 23) | (3u << 20) | (3u << 16)});
    Cmd(0x40, {1});
    for (int i = 0; i < 8; i++)
    {
        double z0 = -1.6 - i * 0.8, z1 = z0 - 0.8;
        Cmd(0x21, {N10(0) | (N10(1) << 10)}); TexCoord(0, -z0 * 8);  Vertex16(-4, FloorY, z0);
        Cmd(0x21, {N10(0) | (N10(1) << 10)}); TexCoord(64, -z0 * 8); Vertex16(4, FloorY, z0);
        Cmd(0x21, {N10(0) | (N10(1) << 10)}); TexCoord(64, -z1 * 8); Vertex16(4, FloorY, z1);
        Cmd(0x21, {N10(0) | (N10(1) << 10)}); TexCoord(0, -z1 * 8);  Vertex16(-4, FloorY, z1);
    }
    Cmd(0x41);
    Cmd(0x50, {0});
    Nds->GPU.GPU3D.VBlank();
    Nds->GPU.GPU3D.VCount215(Nds->GPU);
}

// the top screen as the renderer composites it, at its scale, 0xBBGGRR
static std::vector<u32> Composite(GLRenderer& r, GPU& gpu, int scale)
{
    int backbuf = gpu.FrontBuffer ^ 1;
    const int stride = 256 * 3 + 1;
    for (int s = 0; s < 2; s++)
    {
        gpu.Framebuffer[backbuf][s] = std::make_unique<u32[]>(stride * 192);
        u32* fb = gpu.Framebuffer[backbuf][s].get();
        memset(fb, 0, stride * 192 * 4);
        for (int y = 0; y < 192; y++) fb[y * stride + 768] = 1 << 16;
    }
    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    const int w = 256 * scale, h = 386 * scale;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    r.SetOutputTexture(backbuf, tex);
    r.Blit(gpu);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    std::vector<u32> all((size_t)w * h);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    std::vector<u32> top((size_t)w * 192 * scale);
    for (int y = 0; y < 192 * scale; y++) memcpy(&top[(size_t)y * w], &all[(size_t)(194 * scale + y) * w], w * 4);
    return top;
}

static void SavePng(const std::string& name, const std::vector<u32>& img, int w)
{
    std::vector<u32> out(img);
    for (u32& c : out) c |= 0xFF000000;
    stbi_write_png(name.c_str(), w, (int)(img.size() / w), 4, out.data(), w * 4);
}

static std::vector<u32> Frame(GLRenderer& r, GPU& gpu)
{
    // twice: view data is captured from the frame after relief is asked for
    SubmitScene(); r.RenderFrame(gpu);
    SubmitScene(); r.RenderFrame(gpu);
    return Composite(r, gpu, r.GetScaleFactor());
}

// mean per-channel difference, and mean difference between horizontal
// neighbours (fine detail), over rows y0-y1
static double MeanDiff(const std::vector<u32>& a, const std::vector<u32>& b, int w, int y0, int y1)
{
    double s = 0; long n = 0;
    for (int y = y0; y < y1; y++) for (int x = 0; x < w; x++)
        for (int k = 0; k < 3; k++) { s += std::abs((int)((a[y * w + x] >> (8 * k)) & 0xFF) - (int)((b[y * w + x] >> (8 * k)) & 0xFF)); n++; }
    return s / n;
}
// share of pixels whose largest channel difference exceeds 16 levels
static double Changed(const std::vector<u32>& a, const std::vector<u32>& b, int w, int y0, int y1)
{
    long n = 0, m = 0;
    for (int y = y0; y < y1; y++) for (int x = 0; x < w; x++)
    {
        int d = 0;
        for (int k = 0; k < 3; k++) d = std::max(d, std::abs((int)((a[y * w + x] >> (8 * k)) & 0xFF) - (int)((b[y * w + x] >> (8 * k)) & 0xFF)));
        m += d > 16; n++;
    }
    return (double)m / n;
}
static double Detail(const std::vector<u32>& a, int w, int y0, int y1)
{
    double s = 0; long n = 0;
    for (int y = y0; y < y1; y++) for (int x = 0; x + 1 < w; x++)
        for (int k = 0; k < 3; k++) { s += std::abs((int)((a[y * w + x] >> (8 * k)) & 0xFF) - (int)((a[y * w + x + 1] >> (8 * k)) & 0xFF)); n++; }
    return s / n;
}
static double Channel(const std::vector<u32>& a, int w, int y0, int y1, int k)
{
    double s = 0; long n = 0;
    for (int y = y0; y < y1; y++) for (int x = 0; x < w; x++) { s += (a[y * w + x] >> (8 * k)) & 0xFF; n++; }
    return s / n;
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    Nds = nds.get();
    GPU& gpu = nds->GPU;
    gpu.GPU3D.Reset();
    gpu.GPU3D.SetEnabled(true, true);

    const int scale = 4;
    auto r = GLRenderer::New();
    if (!r) { puts("GLRenderer::New failed"); return 1; }
    r->SetRenderSettings(false, scale);
    while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }
    // textures are written as a game does, through bank A mapped to the CPU
    // (LCDC), so the renderer sees VRAM change; then mapped as texture slot 0
    std::vector<u16> texels(64 * 64);
    auto upload = [&]() {
        gpu.MapVRAM_AB(0, 0x80);
        for (int i = 0; i < 64 * 64; i++) nds->ARM9Write16(0x06800000 + i * 2, texels[i]);
        gpu.MapVRAM_AB(0, 0x83);
    };
    u32 seed = 12345;
    auto rnd = [&]() { seed = seed * 1103515245 + 12345; return (seed >> 16) & 0x7FFF; };

    bool ok = true;
    auto check = [&](bool cond, const std::string& what) { printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO"); ok = ok && cond; };
    const int w = 256 * scale, nearY0 = 150 * scale, nearY1 = 190 * scale;

    // grass: green of varied shade, noisy (MaterialClassifier: foliage)
    for (int i = 0; i < 64 * 64; i++)
    {
        u32 v = rnd() % 12;
        texels[i] = 0x8000 | (2 + v / 3) | ((14 + v) << 5) | ((1 + v / 4) << 10);
    }
    upload();
    r->SetRelief(0);
    auto grassOff = Frame(*r, gpu);
    r->SetRelief(2);
    auto grassOn = Frame(*r, gpu);
    SavePng("grass_relief_off.png", grassOff, w);
    SavePng("grass_relief_on.png", grassOn, w);
    const double change = MeanDiff(grassOn, grassOff, w, nearY0, nearY1);
    const double detailOff = Detail(grassOff, w, nearY0, nearY1), detailOn = Detail(grassOn, w, nearY0, nearY1);
    printf("grass, near floor: mean change %.2f, fine detail %.2f -> %.2f, green %.1f -> %.1f\n", change, detailOff, detailOn,
           Channel(grassOff, w, nearY0, nearY1, 1), Channel(grassOn, w, nearY0, nearY1, 1));
    check(change > 8, "grass: drawn as blades (the near floor changes)");
    // plain relief on the same texture: x1.4; blades: x4.6 (measured)
    check(detailOn > detailOff * 3, "grass: blades add fine detail (edges between blades), far more than parallax");
    check(Channel(grassOn, w, nearY0, nearY1, 1) > 2 * Channel(grassOn, w, nearY0, nearY1, 0), "grass: still green (blades coloured by the texture under them)");

    // wind: the blades sway with time (2 radians a second): the field moves,
    // a little from one frame to the next, more over a quarter second
    SubmitScene(); r->RenderFrame(gpu);
    auto next = Composite(*r, gpu, scale);
    for (int i = 0; i < 14; i++) { SubmitScene(); r->RenderFrame(gpu); }
    auto later = Composite(*r, gpu, scale);
    SavePng("grass_wind_later.png", later, w);
    const double step = Changed(next, grassOn, w, nearY0, nearY1), sway = Changed(later, grassOn, w, nearY0, nearY1);
    printf("wind: pixels changed in one frame %.1f%%, in 15 frames %.1f%%\n", step * 100, sway * 100);
    check(sway > 0.05 && step < sway / 3, "grass: sways in the wind, smoothly (one frame moves it far less than a quarter second)");

    // sky: bright, smooth blue (MaterialClassifier: sky): no relief
    for (int i = 0; i < 64 * 64; i++) texels[i] = 0x8000 | (14 + rnd() % 2) | (24 << 5) | (31 << 10);
    upload();
    r->SetRelief(0);
    auto skyOff = Frame(*r, gpu);
    r->SetRelief(2);
    auto skyOn = Frame(*r, gpu);
    check(MeanDiff(skyOn, skyOff, w, 0, 192 * scale) == 0, "sky texture: relief leaves it untouched");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
