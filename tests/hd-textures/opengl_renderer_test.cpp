#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_TextureReplacement.h"
#include "OpenGLSupport.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>
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

    // 4b. background loading (Pomegrade, as on Android): the frame that first
    // meets the texture draws it at native resolution while the file is read,
    // a later frame draws the same picture as the synchronous load
    TextureReplacement::SetConfig({"tex", true, false, true});
    auto firstFrame = Render(*r, gpu);
    int frames = 1;
    auto later = firstFrame;
    while (later != hdimg && frames < 1000)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        later = Render(*r, gpu);
        frames++;
    }
    bool backgroundOk = firstFrame == native && later == hdimg;
    printf("background: first frame == native: %s, HD after %d frames == synchronous HD: %s\n",
           firstFrame == native ? "yes" : "NO", frames, later == hdimg ? "yes" : "NO");
    TextureReplacement::SetConfig({"tex", false, false});

    // 5. texture filtering (Pomegrade): 7.5 texels across the screen, each
    // 34.13 pixels wide, so texel edges fall inside pixels. Texel middles keep
    // their colour, each texel edge is blended over a pixel or two (none with
    // nearest sampling), transparent texels cover the same pixels
    bool filterOk = true;
    {
        auto check = [&](bool cond, const char* what) { printf("texture filter: %s: %s\n", what, cond ? "yes" : "NO"); filterOk = filterOk && cond; };
        for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 120 : 0;
        const double w = 256.0 / 7.5;
        u16* tex = (u16*)gpu.VRAM_A;
        tex[3*8+3] = 0; // one transparent texel (alpha bit clear)
        auto nearest = Render(*r, gpu);
        r->SetTextureFilter(true);
        auto filtered = Render(*r, gpu);
        r->SetTextureFilter(false);
        auto again = Render(*r, gpu);
        tex[3*8+3] = (3*4) | ((3*4) << 5) | 0x8000;
        for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 128 : 0;

        int middles = 0, middlesSame = 0;
        for (int Y = 0; Y < 8; Y++) for (int X = 0; X < 7; X++)
            for (int dy = -4; dy <= 4; dy++) for (int dx = -8; dx <= 8; dx++)
            {
                int x = (int)((X + 0.5) * w) + dx, y = Y*24 + 12 + dy;
                middles++;
                if ((filtered[y*256+x] & 0xFFFFFF) == (nearest[y*256+x] & 0xFFFFFF)) middlesSame++;
            }
        check(middlesSame == middles, "texel middles unchanged (no blur)");

        // along a row through texel middles: pixels unlike both texels they sit between
        int blendedNearest = 0, blendedFiltered = 0, badEdges = 0;
        int y = 12 + 24;
        for (int edge = 1; edge < 7; edge++)
        {
            int ex = (int)(edge * w), inEdge = 0;
            u32 left = nearest[y*256 + ex - 8] & 0xFFFFFF, right = nearest[y*256 + ex + 8] & 0xFFFFFF;
            for (int x = ex - 4; x <= ex + 4; x++)
            {
                u32 n = nearest[y*256+x] & 0xFFFFFF, f = filtered[y*256+x] & 0xFFFFFF;
                if (n != left && n != right) blendedNearest++;
                if (f != left && f != right) { blendedFiltered++; inEdge++; }
            }
            if (inEdge < 1 || inEdge > 2) badEdges++;
        }
        printf("texture filter: blended pixels on 6 texel edges: nearest %d, filtered %d\n", blendedNearest, blendedFiltered);
        check(blendedNearest == 0 && badEdges == 0, "each texel edge blended over 1-2 pixels (nearest: none)");

        int coverageDiff = 0, transparent = 0;
        for (int i = 0; i < 256*192; i++)
        {
            if ((nearest[i] & 0xFFFFFF) == 0) transparent++;
            if (((nearest[i] & 0xFFFFFF) == 0) != ((filtered[i] & 0xFFFFFF) == 0)) coverageDiff++;
        }
        printf("texture filter: transparent pixels %d, covered differently %d\n", transparent, coverageDiff);
        check(transparent > 500 && coverageDiff == 0, "transparent texel covers the same pixels");
        check(again == nearest, "off again == nearest");

        // negative texture coordinates (repeat): -4 to 3.5 across the screen.
        // Texel middles keep the colour the unfiltered lookup gives them
        {
            poly.TexParam = (7u << 26) | (1 << 16);
            for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 56 : -64;
            auto nearNeg = Render(*r, gpu);
            r->SetTextureFilter(true);
            auto filtNeg = Render(*r, gpu);
            r->SetTextureFilter(false);
            poly.TexParam = (7u << 26);
            for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 128 : 0;
            int differ = 0, total = 0;
            for (int X = 0; X < 7; X++)
                for (int dy = -4; dy <= 4; dy++) for (int dx = -6; dx <= 6; dx++)
                {
                    int x = (int)((X + 0.5) * w) + dx, y = 3*24 + 12 + dy;
                    total++;
                    if ((filtNeg[y*256+x] & 0xFFFFFF) != (nearNeg[y*256+x] & 0xFFFFFF)) differ++;
                }
            printf("texture filter: negative coordinates, texel middles differing from unfiltered: %d / %d\n", differ, total);
            check(differ == 0, "negative coordinates: texel middles unchanged");
        }

        // minified: the texture repeated across the screen at 4 texels per
        // pixel. Nearest sampling picks one texel per pixel (aliasing); the
        // filter's samples along the footprint come close to the average of
        // the texels under each pixel (the texture is red = x * 4)
        poly.TexParam = (7u << 26) | (1 << 16);
        for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 1024*16 : 0;
        auto nearMin = Render(*r, gpu);
        r->SetTextureFilter(true);
        auto filtMin = Render(*r, gpu);
        r->SetTextureFilter(false);
        poly.TexParam = (7u << 26);
        for (int i = 0; i < 4; i++) verts[i].TexCoords[0] = (i==1||i==2) ? 128 : 0;
        double errNearest = 0, errFiltered = 0;
        for (int x = 0; x < 256; x++)
        {
            // the true colour: the average of the 4 texels under the pixel
            double truth = 0;
            for (int k = 4 * x; k < 4 * x + 4; k++) truth += (k & 7) * 4 / 4.0;
            // capture is RGB6: the 5-bit texel red, doubled
            errNearest += std::fabs((nearMin[96*256+x] & 0x3F) / 2.0 - truth);
            errFiltered += std::fabs((filtMin[96*256+x] & 0x3F) / 2.0 - truth);
        }
        printf("texture filter: minified x4, mean red error from each pixel's texels: nearest %.2f, filtered %.2f\n", errNearest / 256, errFiltered / 256);
        check(errFiltered < errNearest * 0.6, "minified: much closer to the average (less aliasing)");
    }
    // a shader the driver refuses: its log is kept for the app to show (a
    // renderer that can't start falls back to software, see MelonInstance)
    bool errorOk;
    {
        OpenGL::TakeLastError();
        GLuint prog = 0;
        const bool built = OpenGL::CompileVertexFragmentProgram(prog,
            "#version 320 es\nvoid main() { gl_Position = vec4(0.0); }\n",
            "#version 320 es\nprecision highp float;\nout vec4 c;\nvoid main() { c = undefinedName; }\n",
            "BrokenShader", {}, {});
        const std::string error = OpenGL::TakeLastError();
        printf("refused shader: %s\n", error.c_str());
        errorOk = !built && error.find("BrokenShader") != std::string::npos && OpenGL::TakeLastError().empty();
        printf("refused shader: error kept once, naming the shader: %s\n", errorOk ? "yes" : "NO");
    }
    return bad != 0 || off != native || !backgroundOk || !filterOk || !errorOk;
}
