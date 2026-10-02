#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_TextureReplacement.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
using namespace melonDS;
namespace fs = std::filesystem;

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

static void SetupScene(GPU& gpu)
{
    // VRAM A -> texture slot 0, texture: 8x8 direct colour, texel (x,y) = (r=x*4, g=y*4, b=0)
    gpu.MapVRAM_AB(0, 0x83);
    u16* tex = (u16*)gpu.VRAM_A;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            tex[y*8+x] = (x*4) | ((y*4) << 5) | 0x8000;

    int pos[4][2] = {{0,0},{256,0},{256,192},{0,192}};
    int tc[4][2] = {{0,0},{8*16,0},{8*16,8*16},{0,8*16}};
    memset(&poly, 0, sizeof(poly));
    for (int i = 0; i < 4; i++)
    {
        Vertex& v = verts[i];
        memset(&v, 0, sizeof(v));
        v.FinalPosition[0] = pos[i][0]; v.FinalPosition[1] = pos[i][1];
        v.HiresPosition[0] = pos[i][0] << 4; v.HiresPosition[1] = pos[i][1] << 4;
        v.FinalColor[0] = v.FinalColor[1] = v.FinalColor[2] = 510;
        v.TexCoords[0] = tc[i][0]; v.TexCoords[1] = tc[i][1];
        poly.Vertices[i] = &v;
        poly.FinalZ[i] = 0x1000;
        poly.FinalW[i] = 0x1000;
    }
    poly.NumVertices = 4;
    poly.Attr = (31 << 16) | (1 << 24);
    poly.TexParam = (7u << 26); // 8x8, addr 0, clamp
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

static void Px(const char* tag, const std::vector<u32>& img, int x, int y)
{
    u32 c = img[y*256+x];
    printf("%s (%3d,%3d): r=%2u g=%2u b=%2u\n", tag, x, y, c & 0x3F, (c >> 8) & 0x3F, (c >> 16) & 0x3F);
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    printf("GL: %s\n", glGetString(GL_VERSION));

    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    GPU& gpu = nds->GPU;
    SetupScene(gpu);

    auto r = GLRenderer::New();
    if (!r) { puts("GLRenderer::New failed"); return 1; }
    r->SetRenderSettings(false, 1);
    while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }

    // 1. no replacement
    auto native = Render(*r, gpu);
    Px("native", native, 4, 3); Px("native", native, 12, 3); Px("native", native, 40, 30);

    // 2. dump
    fs::remove_all("tex");
    TextureReplacement::SetConfig({"tex", true, true});
    TextureReplacement::SetGameCode("TEST");
    auto dumped = Render(*r, gpu);
    std::string dumpFile;
    for (auto& e : fs::directory_iterator("tex/TEST/dump")) dumpFile = e.path().string();
    printf("dumped: %s, same image as native: %s\n", dumpFile.c_str(), dumped == native ? "yes" : "NO");
    if (dumpFile.empty()) return 1;

    // 3. 4x replacement: HD texel = native colour on even (X+Y), pure blue on odd
    int w, h, c;
    u8* p = stbi_load(dumpFile.c_str(), &w, &h, &c, 4);
    std::vector<u8> hd(32*32*4);
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++)
    {
        u8* d = &hd[(y*32+x)*4];
        if (((x+y) & 1) == 0) memcpy(d, &p[((y/4)*8 + x/4)*4], 4);
        else { d[0] = 0; d[1] = 0; d[2] = 255; d[3] = 255; }
    }
    std::string name = fs::path(dumpFile).filename().string();
    stbi_write_png(("tex/TEST/" + name).c_str(), 32, 32, 4, hd.data(), 32*4);
    TextureReplacement::SetConfig({"tex", true, false}); // rescan
    auto hdimg = Render(*r, gpu);
    // each HD texel is 8x6 output pixels
    Px("hd", hdimg, 4, 3); Px("hd", hdimg, 12, 3); Px("hd", hdimg, 12, 9); Px("hd", hdimg, 44, 33);

    int bad = 0;
    for (int y = 0; y < 192; y++) for (int x = 0; x < 256; x++)
    {
        int X = x / 8, Y = y / 6;
        u32 col = hdimg[y*256+x];
        u32 b = (col >> 16) & 0x3F;
        bool expectBlue = ((X+Y) & 1) != 0;
        u32 expectR = (X/4) * 8, expectG = (Y/4) * 8; // native texel colour (5-bit *2)
        auto near = [](u32 a, u32 b) { return a + 1 >= b && a <= b + 1; }; // 8-bit HD vs 5-bit DS colours
        bool ok = expectBlue ? (b >= 62 && (col & 0x3F) == 0) : (b == 0 && near(col & 0x3F, expectR) && near((col >> 8) & 0x3F, expectG));
        if (!ok && bad++ < 5) Px("MISMATCH", hdimg, x, y);
    }
    printf("HD mismatches: %d / %d\n", bad, 256*192);

    // 3b. repeat + mirror on S, repeat on T: texcoords cover 2x2 texture periods
    {
        poly.TexParam = (7u << 26) | (1 << 16) | (1 << 18) | (1 << 17);
        int tc2[4][2] = {{0,0},{16*16,0},{16*16,16*16},{0,16*16}};
        for (int i = 0; i < 4; i++) { verts[i].TexCoords[0] = tc2[i][0]; verts[i].TexCoords[1] = tc2[i][1]; }
        auto img = Render(*r, gpu);
        int badw = 0;
        for (int y = 0; y < 192; y++) for (int x = 0; x < 256; x++)
        {
            // HD texel coordinates over 2 periods of 32: 256px / 64 -> 4px per texel, 192/64 -> 3px
            int X = x / 4, Y = y / 3;
            if (X >= 32) X = 63 - X; // mirrored second period
            Y &= 31;                 // repeated
            u32 col = img[y*256+x];
            u32 b = (col >> 16) & 0x3F;
            bool expectBlue = ((X+Y) & 1) != 0;
            auto near = [](u32 a, u32 b) { return a + 1 >= b && a <= b + 1; };
            bool ok = expectBlue ? (b >= 62) : (b == 0 && near(col & 0x3F, (X/4)*8) && near((col >> 8) & 0x3F, (Y/4)*8));
            if (!ok && badw++ < 5) Px("WRAP MISMATCH", img, x, y);
        }
        printf("wrap/mirror mismatches: %d / %d\n", badw, 256*192);
        bad += badw;
        poly.TexParam = (7u << 26);
        for (int i = 0; i < 4; i++) { verts[i].TexCoords[0] = (i==1||i==2) ? 128 : 0; verts[i].TexCoords[1] = (i>=2) ? 128 : 0; }
    }

    // 4. turning replacement off goes back to native
    TextureReplacement::SetConfig({"tex", false, false});
    auto off = Render(*r, gpu);
    printf("off == native: %s\n", off == native ? "yes" : "NO");
    return bad != 0 || off != native;
}
