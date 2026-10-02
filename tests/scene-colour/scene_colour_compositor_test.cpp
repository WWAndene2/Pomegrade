// Scene-adaptive colour through the real OpenGL compositor: a dark grey
// background with a washed-out gradient quad on top, over 90 frames.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include <cmath>
#include <cstdio>
#include <cstring>
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

// background: clear colour, dark grey 2/31; quad over the right half: greys 10/31 to 22/31 (washed out)
static void SetupScene(GPU& gpu)
{
    int pos[4][2] = {{128,16},{240,16},{240,176},{128,176}};
    int grey[4] = {10, 22, 22, 10};
    memset(&poly, 0, sizeof(poly));
    for (int i = 0; i < 4; i++)
    {
        Vertex& v = verts[i];
        memset(&v, 0, sizeof(v));
        v.FinalPosition[0] = pos[i][0]; v.FinalPosition[1] = pos[i][1];
        v.HiresPosition[0] = pos[i][0] << 4; v.HiresPosition[1] = pos[i][1] << 4;
        v.FinalColor[0] = v.FinalColor[1] = v.FinalColor[2] = (grey[i] << 4) + 0xF;
        poly.Vertices[i] = &v;
        poly.FinalZ[i] = 0x1000;
        poly.FinalW[i] = 0x1000;
    }
    poly.NumVertices = 4;
    poly.Attr = (31 << 16) | (1 << 24);
    poly.FacingView = true;
    gpu.GPU3D.RenderPolygonRAM[0] = &poly;
    gpu.GPU3D.RenderNumPolygons = 1;
    gpu.GPU3D.RenderDispCnt = 0;
    gpu.GPU3D.RenderClearAttr1 = 2 | (2 << 5) | (2 << 10) | (31 << 16);
    gpu.GPU3D.RenderClearAttr2 = 0x7FFF;
}

// one frame through the compositor, with a 2D layer that only shows the 3D one;
// returns the top screen, RGBA8
static std::vector<u32> Composite(GLRenderer& r, GPU& gpu)
{
    int backbuf = gpu.FrontBuffer ^ 1;
    const int stride = 256 * 3 + 1;
    for (int s = 0; s < 2; s++)
    {
        // the GPU allocated them for the software renderer (256 wide): replace with the accelerated layout
        gpu.Framebuffer[backbuf][s] = std::make_unique<u32[]>(stride * 192);
        u32* fb = gpu.Framebuffer[backbuf][s].get();
        memset(fb, 0, stride * 192 * 4);
        for (int y = 0; y < 192; y++)
            fb[y * stride + 768] = 1 << 16; // master brightness entry: display mode 1, 3D shown
    }

    r.RenderFrame(gpu);
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
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // the renderer leaves its capture buffer bound
    glReadPixels(0, 0, 256, 386, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    // the top screen is rows 194-385, in screen order (checked against the
    // display capture in tests/lighting-effects)
    std::vector<u32> top(256 * 192);
    for (int y = 0; y < 192; y++)
        memcpy(&top[y * 256], &all[(194 + y) * 256], 256 * 4);
    return top;
}

static double Luma(u32 c) { return (0.2126 * (c & 0xFF) + 0.7152 * ((c >> 8) & 0xFF) + 0.0722 * ((c >> 16) & 0xFF)) / 255; }

// spread (standard deviation) of luma over the quad
static double QuadContrast(const std::vector<u32>& img)
{
    double sum = 0, sum2 = 0; int n = 0;
    for (int y = 30; y < 160; y++)
        for (int x = 140; x < 230; x++) { double l = Luma(img[y*256+x]); sum += l; sum2 += l*l; n++; }
    double mean = sum / n;
    return std::sqrt(sum2 / n - mean * mean);
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
    auto plain = Composite(*r, gpu);
    u32 bgPlain = plain[96*256 + 40] & 0xFFFFFF;
    double contrastPlain = QuadContrast(plain);
    printf("off: background %06x, quad contrast %.4f\n", bgPlain, contrastPlain);

    // OLED deep blacks
    r->SetOledBlacks(true);
    std::vector<u32> img;
    for (int f = 0; f < 90; f++) img = Composite(*r, gpu);
    u32 bg = img[96*256 + 40] & 0xFFFFFF;
    double quadMid = Luma(img[96*256 + 184]), quadMidPlain = Luma(plain[96*256 + 184]);
    printf("OLED: background %06x (was %06x), quad middle luma %.3f (was %.3f)\n", bg, bgPlain, quadMid, quadMidPlain);
    ok = ok && bgPlain != 0 && bg == 0 && std::fabs(quadMid - quadMidPlain) < 0.02;
    r->SetOledBlacks(false);

    // adaptive colours
    r->SetAdaptiveColours(true);
    for (int f = 0; f < 90; f++) img = Composite(*r, gpu);
    double contrast = QuadContrast(img);
    printf("adaptive: quad contrast %.4f (was %.4f)\n", contrast, contrastPlain);
    // expected stretch: the scene's darkest 2% is the background (luma ~0.047), so the
    // black level only moves to ~0.03; the white level (quad top, ~0.71) moves to ~0.83:
    // contrast x 1 / (0.83 - 0.03) = ~1.25
    ok = ok && contrast > contrastPlain * 1.2;
    r->SetAdaptiveColours(false);

    // both off again: the same image as before
    bool same = Composite(*r, gpu) == plain;
    printf("off again: identical: %s\n", same ? "yes" : "NO");
    ok = ok && same;

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
