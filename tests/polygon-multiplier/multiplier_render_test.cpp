// Drives the real DS 3D engine with geometry commands (as a game would) and
// renders a low-poly lit sphere with the polygon multiplier off and on, with
// the software and OpenGL renderers. Writes the images next to the binary.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"
#include "stb/stb_image_write.h"
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

static void Normal(double x, double y, double z)
{
    Cmd(0x21, {N10(x) | (N10(y) << 10) | (N10(z) << 20)});
}

// Low-poly UV sphere: 8 segments, 5 rings, as quads (the poles as triangles).
static void DrawSphere(double radius)
{
    const int seg = 8, rings = 5;
    auto point = [&](int r, int s, double* p) {
        double th = M_PI * r / rings, ph = 2 * M_PI * s / seg;
        p[0] = std::sin(th) * std::cos(ph); p[1] = std::cos(th); p[2] = std::sin(th) * std::sin(ph);
    };
    Cmd(0x40, {1}); // quads
    for (int r = 0; r < rings; r++)
        for (int s = 0; s < seg; s++)
        {
            int corners[4][2] = {{r, s}, {r + 1, s}, {r + 1, s + 1}, {r, s + 1}};
            for (auto& c : corners)
            {
                double p[3]; point(c[0], c[1], p);
                Normal(p[0], p[1], p[2]);
                Vertex16(p[0] * radius, p[1] * radius, p[2] * radius);
            }
        }
    Cmd(0x41);
}

// A flat textured-looking panel without normals (like a 2D UI element): must stay untouched.
static void DrawFlatPanel()
{
    Cmd(0x29, {(31 << 16) | (2 << 24) | 0xC0}); // no lights, both sides
    Cmd(0x20, {0x7FE0 | 0x1F}); // white-ish
    Cmd(0x40, {1});
    // bottom-left of the screen, about x 8..49, y 151..185
    Cmd(0x20, {0x001F}); Vertex16(-1.75, -1.3, -3);
    Cmd(0x20, {0x03E0}); Vertex16(-1.15, -1.3, -3);
    Cmd(0x20, {0x7C00}); Vertex16(-1.15, -0.8, -3);
    Cmd(0x20, {0x7FFF}); Vertex16(-1.75, -0.8, -3);
    Cmd(0x41);
}

static void SubmitScene()
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);          // DISP3DCNT
    g.Write32(0x04000350, 0x1F0000 | (4 << 10) | (3 << 5) | 2); // clear colour, opaque
    g.Write32(0x04000354, 0x7FFF);     // clear depth
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)}); // viewport

    // projection: perspective, 50 degrees, 4:3
    double f = 1.0 / std::tan(25.0 * M_PI / 180.0), a = 256.0 / 192.0, zn = 0.5, zf = 20;
    double proj[16] = {f / a, 0, 0, 0,  0, f, 0, 0,  0, 0, (zf + zn) / (zn - zf), -1,  0, 0, 2 * zf * zn / (zn - zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);

    Cmd(0x10, {2}); Cmd(0x15); // position & vector: identity
    Cmd(0x32, {N10(-0.5) | (N10(-0.6) << 10) | (N10(-0.62) << 20)}); // light 0 direction
    Cmd(0x33, {0x7FFF});                                            // light 0 white
    Cmd(0x30, {(0x7FFF) | (0x2108u << 16)});                          // diffuse white, ambient dark grey
    Cmd(0x31, {0x4210});                                            // some specular

    DrawFlatPanel();

    // sphere in front of the camera, slightly rotated
    double c = std::cos(0.4), s = std::sin(0.4);
    double model[16] = {c, 0, -s, 0,  0, 1, 0, 0,  s, 0, c, 0,  0.3, 0.05, -3.2, 1};
    LoadMatrix(model);
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01}); // light 0, front faces, opaque
    DrawSphere(1.25);

    Cmd(0x50, {0}); // swap buffers
    g.VBlank();     // builds the render list, as at the end of a frame
}

static std::vector<u32> Capture(Renderer3D& r)
{
    std::vector<u32> img(256 * 192);
    for (int y = 0; y < 192; y++) memcpy(&img[y * 256], r.GetLine(y), 256 * 4);
    return img;
}

static void SavePng(const std::string& name, const std::vector<u32>& img)
{
    // RGB6 -> RGB8, 3x nearest-neighbour so the edges are visible
    const int k = 3;
    std::vector<u8> out(256 * k * 192 * k * 3);
    for (int y = 0; y < 192 * k; y++)
        for (int x = 0; x < 256 * k; x++)
        {
            u32 c = img[(y / k) * 256 + x / k];
            u8* o = &out[(y * 256 * k + x) * 3];
            o[0] = ((c & 0x3F) * 255) / 63; o[1] = (((c >> 8) & 0x3F) * 255) / 63; o[2] = (((c >> 16) & 0x3F) * 255) / 63;
        }
    stbi_write_png(name.c_str(), 256 * k, 192 * k, 3, out.data(), 256 * k * 3);
}

static int Diff(const std::vector<u32>& a, const std::vector<u32>& b, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++)
        if ((a[y*256+x] & 0x3F3F3F) != (b[y*256+x] & 0x3F3F3F)) n++;
    return n;
}

int main()
{
    bool gl = InitEGL();
    printf("OpenGL: %s\n", gl ? (const char*)glGetString(GL_VERSION) : "unavailable");

    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    Nds = nds.get();
    GPU& gpu = nds->GPU;
    gpu.GPU3D.Reset(); // as at emulator start: the geometry pipeline needs its reset state
    gpu.GPU3D.SetEnabled(true, true);

    auto glr = gl ? GLRenderer::New() : nullptr;
    if (glr)
    {
        glr->SetRenderSettings(false, 1);
        while (glr->NeedsShaderCompile()) { int cur, cnt; glr->ShaderCompileStep(cur, cnt); }
    }
    SoftRenderer soft;

    std::vector<u32> softOff, glOff;
    bool ok = true;
    // the flat panel's area, with a margin; the sphere stays right of x=60
    const int px0 = 2, py0 = 146, px1 = 56, py1 = 190;
    for (int level = 1; level <= 4; level++)
    {
        gpu.GPU3D.SetPolygonMultiplier(level);
        SubmitScene();
        printf("level %d: %u hardware polygons -> %u rendered polygons\n", level,
               gpu.GPU3D.RenderNumPolygons, gpu.GPU3D.GetRenderNumPolygons());

        soft.RenderFrame(gpu);
        auto img = Capture(soft);
        SavePng("soft_level" + std::to_string(level) + ".png", img);
        if (level == 1)
        {
            softOff = img;
            // the panel must really be on screen (not the clear colour)
            bool drawn = (img[168*256 + 28] & 0x3F3F3F) != (img[5*256 + 5] & 0x3F3F3F);
            printf("flat panel drawn: %s\n", drawn ? "yes" : "NO");
            ok = ok && drawn;
        }

        if (glr)
        {
            glr->RenderFrame(gpu);
            glr->PrepareCaptureFrame();
            auto gimg = Capture(*glr);
            SavePng("gl_level" + std::to_string(level) + ".png", gimg);
            if (level == 1) glOff = gimg;
            else
            {
                int panel = Diff(glOff, gimg, px0, py0, px1, py1);
                printf("  GL: pixels changed %d, of which in the flat panel area %d\n", Diff(glOff, gimg, 0, 0, 256, 192), panel);
                ok = ok && panel == 0;
            }
        }

        if (level > 1)
        {
            int panel = Diff(softOff, img, px0, py0, px1, py1);
            int total = Diff(softOff, img, 0, 0, 256, 192);
            printf("  soft: pixels changed %d, of which in the flat panel area %d\n", total, panel);
            ok = ok && total > 0 && panel == 0;
            ok = ok && gpu.GPU3D.GetRenderNumPolygons() > gpu.GPU3D.RenderNumPolygons;
        }
    }

    // off again: back to exactly the original image
    gpu.GPU3D.SetPolygonMultiplier(1);
    SubmitScene();
    soft.RenderFrame(gpu);
    bool same = Capture(soft) == softOff;
    printf("multiplier off again: identical to the original: %s\n", same ? "yes" : "NO");
    ok = ok && same && gpu.GPU3D.GetRenderNumPolygons() == gpu.GPU3D.RenderNumPolygons;

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
