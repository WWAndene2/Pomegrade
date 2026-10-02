// Native texture upscaling through the OpenGL renderer: a 16x16 pixel-art
// texture (a diagonal line) on a full-screen quad, upscaling off, x2 and x4.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "stb/stb_image_write.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
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

static Vertex verts[4];
static Polygon poly;
static const u16 Red = 31 | 0x8000, Black = 0x8000;

// 16x16 direct-colour texture in VRAM A, texel (x, y)
static void SetTexture(GPU& gpu, bool line)
{
    gpu.MapVRAM_AB(0, 0x83);
    u16* tex = (u16*)gpu.VRAM_A;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            tex[y*16+x] = line ? ((x == y || x == y + 1) ? Red : Black) : Red;
    // what the emulator's VRAM writes do: flag the bank so the texture caches reload it
    for (u32 i = 0; i < 128*1024 / VRAMDirtyGranularity; i++)
        gpu.VRAMDirty[0][i] = true;
}

static void SetupScene(GPU& gpu)
{
    int pos[4][2] = {{0,0},{256,0},{256,192},{0,192}};
    int tc[4][2] = {{0,0},{16*16,0},{16*16,16*16},{0,16*16}};
    memset(&poly, 0, sizeof(poly));
    for (int i = 0; i < 4; i++)
    {
        Vertex& v = verts[i];
        memset(&v, 0, sizeof(v));
        v.FinalPosition[0] = pos[i][0]; v.FinalPosition[1] = pos[i][1];
        v.HiresPosition[0] = pos[i][0] << 4; v.HiresPosition[1] = pos[i][1] << 4;
        v.FinalColor[0] = v.FinalColor[1] = v.FinalColor[2] = 511;
        v.TexCoords[0] = tc[i][0]; v.TexCoords[1] = tc[i][1];
        poly.Vertices[i] = &v;
        poly.FinalZ[i] = 0x1000;
        poly.FinalW[i] = 0x1000;
    }
    poly.NumVertices = 4;
    poly.Attr = (31 << 16) | (1 << 24);
    poly.TexParam = (7u << 26) | (1u << 23) | (1u << 20); // direct colour, 16x16
    poly.FacingView = true;

    gpu.GPU3D.RenderPolygonRAM[0] = &poly;
    gpu.GPU3D.RenderNumPolygons = 1;
    gpu.GPU3D.RenderDispCnt = 1; // textures on
    gpu.GPU3D.RenderClearAttr1 = 0;
    gpu.GPU3D.RenderClearAttr2 = 0x7FFF;
}

static std::vector<u32> Render(GLRenderer& r, GPU& gpu)
{
    r.RenderFrame(gpu);
    r.PrepareCaptureFrame();
    std::vector<u32> out(256*192);
    for (int y = 0; y < 192; y++) memcpy(&out[y*256], r.GetLine(y), 256*4);
    return out;
}

static void SavePng(const char* name, const std::vector<u32>& img)
{
    std::vector<u8> rgb(256*192*3);
    for (int i = 0; i < 256*192; i++)
        for (int k = 0; k < 3; k++)
            rgb[i*3+k] = ((img[i] >> (8*k)) & 0x3F) * 255 / 63;
    stbi_write_png(name, 256, 192, 3, rgb.data(), 256*3);
}

static bool IsRed(u32 c) { return (c & 0x3F) > 32; }

// average distance (pixels) between the line's left edge and the ideal straight
// edge, over the rows where the line is drawn
static double EdgeError(const std::vector<u32>& img)
{
    double sum = 0; int rows = 0;
    for (int y = 0; y < 192; y++)
    {
        int first = -1;
        for (int x = 0; x < 256 && first < 0; x++) if (IsRed(img[y*256+x])) first = x;
        if (first < 0) continue;
        // texel x == y: left edge at x = y_texel * 16 px, y_texel = y / 12
        double ideal = (y + 0.5) * 16.0 / 12.0 - 0.5;
        sum += std::fabs(first - ideal);
        rows++;
    }
    return rows ? sum / rows : 1e9;
}

static std::set<u32> Colours(const std::vector<u32>& img)
{
    std::set<u32> s;
    for (u32 c : img) s.insert(c & 0x3F3F3F);
    return s;
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    GPU& gpu = nds->GPU;
    SetupScene(gpu);

    auto r = GLRenderer::New();
    if (!r) { puts("GLRenderer::New failed"); return 1; }
    r->SetRenderSettings(false, 1);
    while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }

    bool ok = true;
    SetTexture(gpu, true);
    auto native = Render(*r, gpu);
    SavePng("upscale_off.png", native);
    double nativeError = EdgeError(native);
    printf("off: edge %.2f px from the ideal line, %zu colours\n", nativeError, Colours(native).size());

    for (int factor : {2, 4})
    {
        r->SetTextureUpscale(factor);
        auto img = Render(*r, gpu);
        SavePng(factor == 2 ? "upscale_x2.png" : "upscale_x4.png", img);
        double error = EdgeError(img);
        // MMPX only copies texel colours: nothing new may appear
        bool sameColours = Colours(img) == Colours(native);
        printf("x%d: edge %.2f px from the ideal line, same colours as the texture: %s\n", factor, error, sameColours ? "yes" : "NO");
        ok = ok && error < nativeError * 0.75 && sameColours && img != native;
    }

    // a single-colour texture has nothing to smooth
    SetTexture(gpu, false);
    r->SetTextureUpscale(1);
    auto flatNative = Render(*r, gpu);
    r->SetTextureUpscale(4);
    bool flatSame = Render(*r, gpu) == flatNative;
    printf("single-colour texture unchanged: %s\n", flatSame ? "yes" : "NO");
    ok = ok && flatSame;

    // off again: back to native
    SetTexture(gpu, true);
    r->SetTextureUpscale(1);
    bool offSame = Render(*r, gpu) == native;
    printf("off again == native: %s\n", offSame ? "yes" : "NO");
    ok = ok && offSame;

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
