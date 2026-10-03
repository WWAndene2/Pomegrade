// Frame generation in the OpenGL renderer: drives the real DS 3D engine with
// geometry commands (as a game would), renders two frames of a moving sphere
// and checks the intermediate frame against a real render halfway.
// Writes the images next to the binary.
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

static const double F = 1.0 / std::tan(25.0 * M_PI / 180.0), Aspect = 256.0 / 192.0, Zn = 0.5, Zf = 20;

static void DrawSphere(double cx, double cy, double cz, double radius, double turn)
{
    const int seg = 12, rings = 8;
    double c = std::cos(turn), s = std::sin(turn);
    auto point = [&](int r, int sg, double* p) {
        double th = M_PI * r / rings, ph = 2 * M_PI * sg / seg;
        double x = std::sin(th) * std::cos(ph), y = std::cos(th), z = std::sin(th) * std::sin(ph);
        p[0] = c * x + s * z; p[1] = y; p[2] = -s * x + c * z;
    };
    Cmd(0x40, {1});
    for (int r = 0; r < rings; r++)
        for (int sg = 0; sg < seg; sg++)
        {
            int corners[4][2] = {{r, sg}, {r, sg + 1}, {r + 1, sg + 1}, {r + 1, sg}};
            for (auto& k : corners)
            {
                double p[3]; point(k[0], k[1], p);
                Normal(p[0], p[1], p[2]);
                // a checker of two colours, so the rotation shows
                Cmd(0x20, {((r + sg) & 1) ? 0x7FFFu : 0x03FFu});
                Vertex16(cx + p[0] * radius, cy + p[1] * radius, cz + p[2] * radius);
            }
        }
    Cmd(0x41);
}

// sphere at x, turned by turn; other = draw something else entirely (a cut);
// particles: small triangles sent before the sphere (in one frame only, they
// shift the sphere's submission order)
static void SubmitScene(double x, double turn, bool other = false, int particles = 0, double z = -3.5)
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000 | (8 << 10) | (6 << 5) | 4);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    double proj[16] = {F / Aspect, 0, 0, 0,  0, F, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80}); // no lights: vertex colours
    if (particles)
    {
        Cmd(0x29, {(31 << 16) | (5 << 24) | 0xC0}); // another polygon ID, both sides
        Cmd(0x40, {0});
        for (int i = 0; i < particles; i++)
        {
            double px = -2.2 + 0.15 * i;
            Cmd(0x20, {0x7C1F}); Vertex16(px, 1.2, -4);
            Cmd(0x20, {0x7C1F}); Vertex16(px + 0.1, 1.2, -4);
            Cmd(0x20, {0x7C1F}); Vertex16(px, 1.3, -4);
        }
        Cmd(0x41);
        Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80});
    }
    if (!other)
        DrawSphere(x, 0, z, 0.9, turn);
    else
    {
        Cmd(0x29, {(31 << 16) | (2 << 24) | 0xC0});
        Cmd(0x40, {0});
        for (int i = 0; i < 40; i++)
        {
            Cmd(0x20, {0x7C00}); Vertex16(-2 + i * 0.1, -1, -4);
            Cmd(0x20, {0x7C00}); Vertex16(-2 + i * 0.1 + 0.08, -1, -4);
            Cmd(0x20, {0x7C00}); Vertex16(-2 + i * 0.1, 1, -4);
        }
        Cmd(0x41);
    }
    Cmd(0x50, {0});
    g.VBlank();
}

// --- output ---

// The compositor's output (what the screen shows), averaged back to 256x192
// when the renderer runs at a higher resolution. 8 bits per channel, blue in
// the low byte and red in the third (a red sphere reads d30000 here and 000035
// in the display capture).
static std::vector<u32> Composite(GLRenderer& r, GPU& gpu, int scale)
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
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // the renderer leaves its capture buffer bound
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    // the top screen is rows 194-385, in screen order (checked against the
    // display capture, which has the DS orientation)
    std::vector<u32> top(256 * 192);
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
        {
            u32 sum[3] = {};
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                {
                    u32 c = all[(size_t)((194 + y) * scale + sy) * w + x * scale + sx];
                    for (int k = 0; k < 3; k++) sum[k] += (c >> (8 * k)) & 0xFF;
                }
            int n = scale * scale;
            top[y * 256 + x] = ((sum[0] + n / 2) / n) | (((sum[1] + n / 2) / n) << 8) | (((sum[2] + n / 2) / n) << 16);
        }
    return top;
}

// What the game can capture (display capture), RGB6.
static std::vector<u32> Capture(GLRenderer& r)
{
    r.PrepareCaptureFrame();
    std::vector<u32> img(256 * 192);
    for (int y = 0; y < 192; y++) memcpy(&img[y * 256], r.GetLine(y), 256 * 4);
    return img;
}

// An intermediate frame, read like Composite (with the 2D layers the last
// Composite uploaded, as the frame's own composite does in the app).
static std::vector<u32> Intermediate(GLRenderer& r, GPU& gpu, bool& rendered)
{

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 386, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    rendered = r.RenderIntermediateFrame(gpu, tex);
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    std::vector<u32> all(256 * 386);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glReadPixels(0, 0, 256, 386, GL_RGBA, GL_UNSIGNED_BYTE, all.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    std::vector<u32> top(256 * 192);
    for (int y = 0; y < 192; y++)
        memcpy(&top[y * 256], &all[(194 + y) * 256], 256 * 4);
    return top;
}

static void SavePng(const std::string& name, const std::vector<u32>& img)
{
    const int k = 2;
    std::vector<u8> out(256 * k * 192 * k * 3);
    for (int y = 0; y < 192 * k; y++)
        for (int x = 0; x < 256 * k; x++)
        {
            u32 c = img[(y / k) * 256 + x / k];
            u8* o = &out[(y * 256 * k + x) * 3];
            o[0] = (c >> 16) & 0xFF; o[1] = (c >> 8) & 0xFF; o[2] = c & 0xFF;
        }
    stbi_write_png(name.c_str(), 256 * k, 192 * k, 3, out.data(), 256 * k * 3);
}

// pixels that differ by more than a 6-bit step on some channel
static int Differences(const std::vector<u32>& a, const std::vector<u32>& b)
{
    int n = 0;
    for (size_t i = 0; i < a.size(); i++)
        for (int k = 0; k < 3; k++)
            if (std::abs((int)((a[i] >> (8 * k)) & 0xFF) - (int)((b[i] >> (8 * k)) & 0xFF)) > 4) { n++; break; }
    return n;
}

// horizontal centre of the sphere (pixels unlike the background)
static double CenterX(const std::vector<u32>& img)
{
    u32 bg = img[0] & 0xFFFFFF;
    double sum = 0; int n = 0;
    for (int y = 0; y < 192; y++)
        for (int x = 0; x < 256; x++)
            if ((img[y * 256 + x] & 0xFFFFFF) != bg) { sum += x; n++; }
    return n ? sum / n : -1;
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    printf("OpenGL: %s\n", (const char*)glGetString(GL_VERSION));

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
    auto frame = [&](double x, double turn, bool other = false) {
        SubmitScene(x, turn, other);
        r->RenderFrame(gpu);
    };

    r->SetFrameGeneration(true);
    bool rendered;

    // the sphere moves 0.4 to the right (and in the second case turns 12
    // degrees) between two frames; the intermediate frame is compared with the
    // two frames and with a real render halfway
    for (double turn : {0.0, 12 * M_PI / 180})
    {
        const char* name = turn == 0 ? "moving" : "moving and turning";
        frame(-0.2, 0);
        auto before = Composite(*r, gpu, 1);
        frame(0.2, turn);
        auto capture = Capture(*r);
        auto mid = Intermediate(*r, gpu, rendered);
        check(rendered, "intermediate frame rendered");
        // the renderer's image is restored: the game's display capture and the
        // next composite are what they would have been
        check(Capture(*r) == capture, "display capture unchanged by the intermediate frame");
        auto after = Composite(*r, gpu, 1);
        frame(0.0, turn / 2);
        auto halfway = Composite(*r, gpu, 1);
        if (turn != 0)
        {
            SavePng("frame_a.png", before);
            SavePng("frame_mid.png", mid);
            SavePng("frame_b.png", after);
            SavePng("frame_real_halfway.png", halfway);
        }

        double ca = CenterX(before), cm = CenterX(mid), cb = CenterX(after), ch = CenterX(halfway);
        int diffMid = Differences(mid, halfway), diffA = Differences(before, halfway), diffB = Differences(after, halfway);
        printf("%s: sphere centre %.1f -> intermediate %.1f -> %.1f (halfway %.2f, real render %.1f)\n",
               name, ca, cm, cb, (ca + cb) / 2, ch);
        printf("%s: pixels differing from the real halfway render: intermediate %d, previous frame %d, next frame %d\n",
               name, diffMid, diffA, diffB);
        // moving: within half a pixel of halfway. Turning as well: faces that
        // turn towards or away from the camera between the two frames exist in
        // one of them only, and are drawn where the newer frame has them (or
        // not at all), which pulls the silhouette by about a pixel (1.1 px
        // here, measured; 0.35 px without the turn)
        double tolerance = turn == 0 ? 0.5 : 1.5;
        check(std::fabs(cm - (ca + cb) / 2) < tolerance, "intermediate sphere halfway");
        check(diffMid * 5 < std::min(diffA, diffB), "intermediate close to the real halfway render (5x fewer differences than either frame)");
    }

    // particles appear before the sphere in the second frame only: the sphere
    // is still paired with itself (its polygons are sent later than before)
    {
        auto frameP = [&](double x, int particles) { SubmitScene(x, 0, false, particles); r->RenderFrame(gpu); };
        frameP(-0.2, 0);
        auto before = Composite(*r, gpu, 1);
        frameP(0.2, 12);
        auto mid = Intermediate(*r, gpu, rendered);
        auto after = Composite(*r, gpu, 1);
        // the sphere's centre, ignoring the particles' rows (y < 60)
        auto sphereX = [](const std::vector<u32>& img) {
            u32 bg = img[191 * 256] & 0xFFFFFF;
            double sum = 0; int n = 0;
            for (int y = 60; y < 192; y++)
                for (int x = 0; x < 256; x++)
                    if ((img[y * 256 + x] & 0xFFFFFF) != bg) { sum += x; n++; }
            return n ? sum / n : -1.0;
        };
        double ca = sphereX(before), cm = sphereX(mid), cb = sphereX(after);
        printf("particles added before the sphere: sphere centre %.1f -> intermediate %.1f -> %.1f (halfway %.2f)\n",
               ca, cm, cb, (ca + cb) / 2);
        check(rendered && std::fabs(cm - (ca + cb) / 2) < 0.5, "polygons sent in one frame only: the rest still paired");
    }

    // with the polygon multiplier: sub-polygons are paired by their place in
    // their polygon's subdivision
    {
        gpu.GPU3D.SetPolygonMultiplier(4);
        frame(-0.2, 0);
        auto before = Composite(*r, gpu, 1);
        frame(0.2, 0);
        auto mid = Intermediate(*r, gpu, rendered);
        auto after = Composite(*r, gpu, 1);
        frame(0.0, 0);
        auto halfway = Composite(*r, gpu, 1);
        double ca = CenterX(before), cm = CenterX(mid), cb = CenterX(after);
        int diffMid = Differences(mid, halfway), diffA = Differences(before, halfway), diffB = Differences(after, halfway);
        printf("multiplier x16: sphere centre %.1f -> intermediate %.1f -> %.1f (halfway %.2f); differing pixels %d vs %d/%d\n",
               ca, cm, cb, (ca + cb) / 2, diffMid, diffA, diffB);
        check(rendered && std::fabs(cm - (ca + cb) / 2) < 0.5 && diffMid * 5 < std::min(diffA, diffB),
              "multiplier: sub-polygons paired, intermediate halfway");
        gpu.GPU3D.SetPolygonMultiplier(1);
    }

    // adaptive multiplier level: the sphere comes closer between the two
    // frames, so its polygons are subdivided more finely in the second. Their
    // pieces have no counterpart of the same subdivision in the first frame:
    // each corner's place in its triangle is found in the first frame's
    // subdivision instead. The intermediate frame must be as good as with
    // every polygon at the full level (the same motion, each piece paired
    // with itself), with no gaps between pieces
    {
        gpu.GPU3D.SetPolygonMultiplier(8);
        auto frameZ = [&](double x, double z) { SubmitScene(x, 0, false, 0, z); r->RenderFrame(gpu); };
        double centre[2];
        int differing[2], gaps[2];
        u32 pieces[2][2];
        for (int adaptive = 0; adaptive < 2; adaptive++)
        {
            gpu.GPU3D.SetPolygonMultiplierScale(adaptive ? 2 : 0); // as if rendered at twice the DS resolution
            frameZ(-0.2, -3.5); // the levels settle from one frame to the next (see SteadyEdgeLevel)
            frameZ(-0.2, -3.5);
            pieces[adaptive][0] = gpu.GPU3D.GetRenderNumPolygons();
            frameZ(0.2, -2.95);
            pieces[adaptive][1] = gpu.GPU3D.GetRenderNumPolygons();
            auto mid = Intermediate(*r, gpu, rendered);
            check(rendered, "intermediate frame rendered");
            frameZ(0.0, -3.2);
            auto halfway = Composite(*r, gpu, 1);
            centre[adaptive] = CenterX(mid);
            differing[adaptive] = Differences(mid, halfway);
            // gaps: background pixels inside the sphere's outline, row by row
            u32 bg = mid[191 * 256] & 0xFFFFFF;
            gaps[adaptive] = 0;
            for (int y = 0; y < 192; y++)
            {
                int x0 = -1, x1 = -1;
                for (int x = 0; x < 256; x++)
                    if ((mid[y * 256 + x] & 0xFFFFFF) != bg) { if (x0 < 0) x0 = x; x1 = x; }
                for (int x = x0 + 1; x0 >= 0 && x < x1; x++)
                    if ((mid[y * 256 + x] & 0xFFFFFF) == bg) gaps[adaptive]++;
            }
            if (adaptive) SavePng("frame_adaptive_mid.png", mid);
            printf("%s level: %u -> %u pieces; intermediate sphere centre %.2f, %d pixels differing from the real halfway render, gaps %d\n",
                   adaptive ? "adaptive" : "full", pieces[adaptive][0], pieces[adaptive][1], centre[adaptive], differing[adaptive], gaps[adaptive]);
        }
        check(pieces[1][1] > pieces[1][0] && pieces[1][1] < pieces[0][1], "adaptive level: fewer pieces, more for the closer sphere");
        check(std::fabs(centre[1] - centre[0]) < 0.25 && differing[1] <= differing[0] * 1.2 + 20 && gaps[1] == 0,
              "adaptive level: pieces resampled across subdivisions, intermediate as with the full level, no gaps");
        gpu.GPU3D.SetPolygonMultiplierScale(0);
        gpu.GPU3D.SetPolygonMultiplier(1);
    }

    // a still scene: the intermediate frame is the frame itself
    frame(0.0, 0);
    frame(0.0, 0);
    auto still = Composite(*r, gpu, 1);
    auto stillMid = Intermediate(*r, gpu, rendered);
    // colours only: the display draws frames with its own alpha
    bool sameColours = true;
    for (size_t i = 0; i < still.size(); i++)
        sameColours = sameColours && (still[i] & 0xFFFFFF) == (stillMid[i] & 0xFFFFFF);
    check(rendered && sameColours, "still scene: intermediate identical to the frame");

    // a cut to something else: nothing in between
    frame(0.0, 0, true);
    Intermediate(*r, gpu, rendered);
    check(!rendered, "scene cut: no intermediate frame");

    // off: nothing rendered
    r->SetFrameGeneration(false);
    frame(0.0, 0);
    Intermediate(*r, gpu, rendered);
    check(!rendered, "frame generation off: no intermediate frame");

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
