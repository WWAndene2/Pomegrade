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

// Geometry fidelity from neighbours: a box whose normals the game smoothed
// (each corner's normal points diagonally out, as a lighting artist would to
// soften it) and a sphere, side by side.
static const double BoxCenter[3] = {-1.0, 0.0, -4.0}, BoxHalf = 0.7, BoxTurn[2] = {0.5, 0.35};
static const double BallCenter[3] = {1.6, 0.0, -4.2}, BallRadius = 1.0;

// box model -> view: rotation about y, then x, then the translation (row vectors)
static void BoxRotation(double r[3][3])
{
    double cy = std::cos(BoxTurn[0]), sy = std::sin(BoxTurn[0]), cx = std::cos(BoxTurn[1]), sx = std::sin(BoxTurn[1]);
    double ry[3][3] = {{cy, 0, -sy}, {0, 1, 0}, {sy, 0, cy}};
    double rx[3][3] = {{1, 0, 0}, {0, cx, sx}, {0, -sx, cx}};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            r[i][j] = 0;
            for (int k = 0; k < 3; k++) r[i][j] += ry[i][k] * rx[k][j];
        }
}

// secondBox: the same box mesh drawn again elsewhere, turned differently
static void SubmitBoxScene(bool secondBox = false)
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000 | (4 << 10) | (3 << 5) | 2);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    double f = 1.0 / std::tan(25.0 * M_PI / 180.0), a = 256.0 / 192.0, zn = 0.5, zf = 20;
    double proj[16] = {f / a, 0, 0, 0,  0, f, 0, 0,  0, 0, (zf + zn) / (zn - zf), -1,  0, 0, 2 * zf * zn / (zn - zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x32, {N10(-0.5) | (N10(-0.6) << 10) | (N10(-0.62) << 20)});
    Cmd(0x33, {0x7FFF});
    Cmd(0x30, {(0x7FFF) | (0x2108u << 16)});
    Cmd(0x31, {0});
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01});

    double r[3][3];
    BoxRotation(r);
    double box[16] = {r[0][0], r[0][1], r[0][2], 0,  r[1][0], r[1][1], r[1][2], 0,  r[2][0], r[2][1], r[2][2], 0,
                      BoxCenter[0], BoxCenter[1], BoxCenter[2], 1};
    LoadMatrix(box);
    // six faces, counter-clockwise from outside; corner normals = the corner's direction
    static const int faces[6][4][3] = {
        {{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}}, {{ 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}},
        {{ 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}}, {{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}},
        {{-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1}}, {{-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1}}};
    auto drawBox = [&]() {
        Cmd(0x40, {1});
        for (auto& face : faces)
            for (auto& c : face)
            {
                Normal(c[0] / std::sqrt(3.0), c[1] / std::sqrt(3.0), c[2] / std::sqrt(3.0));
                Vertex16(c[0] * BoxHalf, c[1] * BoxHalf, c[2] * BoxHalf);
            }
        Cmd(0x41);
    };
    drawBox();
    if (secondBox)
    {
        // above, turned the other way
        double c2 = std::cos(-0.8), s2 = std::sin(-0.8);
        double other[16] = {c2, 0, -s2, 0,  0, 1, 0, 0,  s2, 0, c2, 0,  -1.0, 1.9, -6.0, 1};
        LoadMatrix(other);
        drawBox();
    }

    double ball[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  BallCenter[0], BallCenter[1], BallCenter[2], 1};
    LoadMatrix(ball);
    DrawSphere(BallRadius);

    Cmd(0x50, {0});
    g.VBlank();
}

// Of the render list's vertices: the box's furthest point outside the box
// (relative to its half size), and the sphere's average distance to the
// true sphere (relative to its radius).
static void BoxAndBallShape(GPU3D& g, double& boxBulge, double& ballError)
{
    double r[3][3];
    BoxRotation(r);
    boxBulge = 0;
    double ballSum = 0;
    int ballCount = 0;
    Polygon** polys = g.GetRenderPolygons();
    for (u32 i = 0; i < g.GetRenderNumPolygons(); i++)
        for (u32 j = 0; j < polys[i]->NumVertices; j++)
        {
            const Vertex* v = polys[i]->Vertices[j];
            if (v->Clipped) continue; // clipping moves points along the edges, not off the surface
            double p[3], d[3], dist = 0;
            for (int k = 0; k < 3; k++) p[k] = v->ViewPosition[k] / 4096.0;
            for (int k = 0; k < 3; k++) { d[k] = p[k] - BoxCenter[k]; dist += d[k] * d[k]; }
            if (std::sqrt(dist) < 1.4)
            {
                // back to the box's own axes: local = d . R^T
                double out = 0;
                for (int axis = 0; axis < 3; axis++)
                {
                    double local = d[0] * r[axis][0] + d[1] * r[axis][1] + d[2] * r[axis][2];
                    out = std::fmax(out, std::fabs(local) - BoxHalf);
                }
                boxBulge = std::fmax(boxBulge, out / BoxHalf);
                continue;
            }
            double rr = 0;
            for (int k = 0; k < 3; k++) rr += (p[k] - BallCenter[k]) * (p[k] - BallCenter[k]);
            if (std::sqrt(rr) > BallRadius * 1.5) continue; // another object
            ballSum += std::fabs(std::sqrt(rr) - BallRadius) / BallRadius;
            ballCount++;
        }
    ballError = ballCount ? ballSum / ballCount : 1e9;
}

// Many spheres at once: sub-polygons span several storage blocks and the
// renderers' buffers have to grow well past the hardware's sizes.
static void SubmitStressScene(int spheres)
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    double f = 1.0 / std::tan(25.0 * M_PI / 180.0), a = 256.0 / 192.0, zn = 0.5, zf = 20;
    double proj[16] = {f / a, 0, 0, 0,  0, f, 0, 0,  0, 0, (zf + zn) / (zn - zf), -1,  0, 0, 2 * zf * zn / (zn - zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x32, {N10(-0.5) | (N10(-0.6) << 10) | (N10(-0.62) << 20)});
    Cmd(0x33, {0x7FFF});
    Cmd(0x30, {(0x7FFF) | (0x2108u << 16)});
    for (int i = 0; i < spheres; i++)
    {
        double model[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  -2.4 + 1.6 * (i % 4), 1.2 - 1.2 * (i / 4), -6, 1};
        LoadMatrix(model);
        Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01});
        DrawSphere(0.55);
    }
    Cmd(0x50, {0});
    g.VBlank();
}

// More polygons than the DS can hold: a 60x50 grid of small flat quads
// (3000 polygons, 12000 vertices), drawn with identity matrices.
static void SubmitOverflowScene()
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    Cmd(0x10, {0}); Cmd(0x15);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x29, {(31 << 16) | (3 << 24) | 0xC0});
    Cmd(0x20, {0x7FFF});
    Cmd(0x40, {1});
    const int cols = 60, rows = 50;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
        {
            double x0 = -0.95 + 1.9 * c / cols, x1 = x0 + 1.9 / cols * 0.8;
            double y0 = 0.95 - 1.9 * r / rows, y1 = y0 - 1.9 / rows * 0.8;
            Vertex16(x0, y0, -0.5); Vertex16(x0, y1, -0.5); Vertex16(x1, y1, -0.5); Vertex16(x1, y0, -0.5);
        }
    Cmd(0x41);
    Cmd(0x50, {0});
    g.VBlank();
}

// A triangle rotated by a small angle: its true area never changes, so how
// much its drawn area varies from frame to frame measures vertex jitter.
static void SubmitRotatingTriangle(double angle)
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);
    g.Write32(0x04000350, 0x1F0000);
    g.Write32(0x04000354, 0x7FFF);
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    Cmd(0x10, {0}); Cmd(0x15);
    Cmd(0x10, {2}); Cmd(0x15);
    Cmd(0x29, {(31 << 16) | (4 << 24) | 0xC0});
    Cmd(0x20, {0x7FFF});
    Cmd(0x40, {0});
    for (int i = 0; i < 3; i++)
    {
        double a = angle + i * 2 * M_PI / 3;
        // 60 native pixels around the centre (clip x: 128 px, clip y: 96 px)
        Vertex16(60.0 / 128 * std::cos(a), 60.0 / 96 * std::sin(a), -0.5);
    }
    Cmd(0x41);
    Cmd(0x50, {0});
    g.VBlank();
}

// Final image through the OpenGL compositor, with a 2D layer that only shows
// the 3D one ("3D on top", no blending). Returns RGBA8 pixels of the top screen.
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
        // both screens show the 3D layer (which one is on top depends on the screen swap)
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
    for (int level : {1, 2, 3, 4, 6, 8})
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

    // stress: 12 spheres at x64, tens of thousands of polygons
    {
        gpu.GPU3D.SetPolygonMultiplier(8);
        SubmitStressScene(12);
        u32 hw = gpu.GPU3D.RenderNumPolygons, rendered = gpu.GPU3D.GetRenderNumPolygons();
        soft.RenderFrame(gpu);
        auto img = Capture(soft);
        SavePng("stress_soft.png", img);
        if (glr)
        {
            glr->RenderFrame(gpu);
            glr->PrepareCaptureFrame();
            SavePng("stress_gl.png", Capture(*glr));
        }
        int lit = 0; // pixels that aren't the black clear colour
        for (u32 c : img) if (c & 0x3F3F3F) lit++;
        bool drawn = lit > 3000;
        printf("stress: %u hardware polygons -> %u rendered polygons, %d pixels drawn\n", hw, rendered, lit);
        ok = ok && rendered > 4 * SubBlockSize && drawn;
    }

    // polygon limit: off drops what doesn't fit, on draws everything; the game
    // sees the hardware overflow and counters either way
    {
        gpu.GPU3D.SetPolygonMultiplier(1);
        int lit[2]; u32 counted[2], rendered[2]; bool overflow[2];
        for (int unlimited = 0; unlimited < 2; unlimited++)
        {
            gpu.GPU3D.SetUnlimitedPolygons(unlimited);
            gpu.GPU3D.DispCnt &= ~(1 << 13);
            SubmitOverflowScene();
            overflow[unlimited] = gpu.GPU3D.DispCnt & (1 << 13);
            counted[unlimited] = gpu.GPU3D.RenderNumPolygons;
            rendered[unlimited] = gpu.GPU3D.GetRenderNumPolygons();
            soft.RenderFrame(gpu);
            auto img = Capture(soft);
            SavePng(unlimited ? "limit_removed.png" : "limit_hardware.png", img);
            lit[unlimited] = 0;
            for (u32 c : img) if (c & 0x3F3F3F) lit[unlimited]++;
            if (glr)
            {
                glr->RenderFrame(gpu);
                glr->PrepareCaptureFrame();
                SavePng(unlimited ? "limit_removed_gl.png" : "limit_hardware_gl.png", Capture(*glr));
            }
            printf("polygon limit %s: hardware polygons %u (overflow flag %s), drawn polygons %u, pixels %d\n",
                   unlimited ? "removed" : "kept   ", counted[unlimited], overflow[unlimited] ? "set" : "clear",
                   rendered[unlimited], lit[unlimited]);
        }
        ok = ok && overflow[0] && overflow[1] && counted[0] == counted[1] && counted[0] <= 2048;
        ok = ok && rendered[0] < 3000 && rendered[1] == 3000 && lit[1] > lit[0];
        gpu.GPU3D.SetUnlimitedPolygons(false);
    }

    // high-precision geometry: less frame-to-frame area variation of a rotating triangle
    if (glr)
    {
        gpu.GPU3D.SetPolygonMultiplier(1);
        double spread[2];
        for (int precise = 0; precise < 2; precise++)
        {
            glr->SetHighPrecision(precise);
            double sum = 0, sum2 = 0, jump = 0; int prev = -1;
            const int frames = 90;
            for (int f = 0; f < frames; f++)
            {
                SubmitRotatingTriangle(f * 0.0044); // ~0.25 degree per frame
                glr->RenderFrame(gpu);
                glr->PrepareCaptureFrame();
                auto img = Capture(*glr);
                int lit = 0;
                for (u32 c : img) if (c & 0x3F3F3F) lit++;
                sum += lit; sum2 += (double)lit * lit;
                if (prev >= 0) jump += std::abs(lit - prev);
                prev = lit;
            }
            double mean = sum / frames;
            spread[precise] = std::sqrt(sum2 / frames - mean * mean);
            printf("high precision %s: drawn area %.0f px, variation %.1f px (std dev), %.1f px average change per frame\n",
                   precise ? "on " : "off", mean, spread[precise], jump / (frames - 1));
        }
        glr->SetHighPrecision(false);
        ok = ok && spread[1] < spread[0];
    }

    // high colour: the sphere's shading keeps more levels through the compositor
    if (glr)
    {
        gpu.GPU3D.SetPolygonMultiplier(4);
        size_t levels[2];
        bool off6bit = true;
        for (int high = 0; high < 2; high++)
        {
            gpu.GPU3D.SetHighColor(high);
            glr->SetHighColor(high);
            SubmitScene();
            glr->RenderFrame(gpu);
            auto img = Composite(*glr, gpu);

            std::vector<u8> png(256 * 192 * 3);
            std::vector<bool> seen(1 << 24);
            levels[high] = 0;
            for (int y = 0; y < 192; y++)
                for (int x = 0; x < 256; x++)
                {
                    u32 c = img[y * 256 + x] & 0xFFFFFF;
                    for (int k = 0; k < 3; k++)
                    {
                        u8 v = (c >> (8 * k)) & 0xFF;
                        png[(y * 256 + x) * 3 + k] = v;
                        // without high colour every channel is a 6-bit value expanded to 8
                        if (!high && v != (u8)(((v >> 2) << 2) | (v >> 6))) off6bit = false;
                    }
                    if (x >= 60 && !seen[c]) { seen[c] = true; levels[high]++; } // sphere side of the screen
                }
            stbi_write_png(high ? "colour_high.png" : "colour_ds.png", 256, 192, 3, png.data(), 256 * 3);
            printf("high colour %s: %zu distinct colours in the sphere's shading\n", high ? "on " : "off", levels[high]);
        }
        printf("without high colour, output is 6-bit per channel: %s\n", off6bit ? "yes" : "NO");
        gpu.GPU3D.SetHighColor(false);
        glr->SetHighColor(false);
        ok = ok && off6bit && levels[1] > levels[0] * 2;
    }

    // geometry fidelity from neighbours: on the first frame the angles between
    // faces aren't known yet (the box bulges as before); from the second, the
    // box's 90-degree edges stay sharp while the sphere (36-45 degrees) keeps
    // its curve. No cracks either way: both polygons of an edge use the same
    // value in a frame.
    {
        gpu.GPU3D.SetPolygonMultiplier(4);
        double bulge[3], ballErr[3];
        int cracks[3];
        for (int frame = 0; frame < 3; frame++)
        {
            SubmitBoxScene();
            BoxAndBallShape(gpu.GPU3D, bulge[frame], ballErr[frame]);
            soft.RenderFrame(gpu);
            auto img = Capture(soft);
            if (frame == 2) SavePng("fidelity_box_sphere.png", img);
            // background showing through inside the box's silhouette = a crack
            int bx = (int)std::lround((BoxCenter[0] / -BoxCenter[2] * (1.0 / std::tan(25.0 * M_PI / 180.0)) / (256.0 / 192.0) + 1) * 128);
            int by = 96;
            u32 bg = img[2 * 256 + 2] & 0x3F3F3F;
            cracks[frame] = 0;
            for (int y = by - 15; y <= by + 15; y++)
                for (int x = bx - 15; x <= bx + 15; x++)
                    if ((img[y * 256 + x] & 0x3F3F3F) == bg) cracks[frame]++;
            printf("box and sphere, frame %d: box bulge %.1f%% of its half size, sphere off by %.2f%%, background pixels inside the box: %d\n",
                   frame + 1, bulge[frame] * 100, ballErr[frame] * 100, cracks[frame]);
        }
        bool sharpened = bulge[0] > 0.1 && bulge[1] < 0.01 && bulge[2] < 0.01;
        // the same box drawn twice (each edge then has two pairs of faces that agree)
        double twice = 1, ballTwice = 1;
        for (int frame = 0; frame < 2; frame++)
        {
            SubmitBoxScene(true);
            BoxAndBallShape(gpu.GPU3D, twice, ballTwice);
        }
        printf("same box drawn twice: bulge %.1f%% from the second frame\n", twice * 100);
        sharpened = sharpened && twice < 0.01;
        bool ballKept = std::fabs(ballErr[1] - ballErr[0]) < 1e-9 && std::fabs(ballErr[2] - ballErr[0]) < 1e-9;
        bool noCracks = cracks[0] == 0 && cracks[1] == 0 && cracks[2] == 0;
        printf("box edges kept sharp from the second frame: %s, sphere unchanged: %s, no cracks: %s\n",
               sharpened ? "yes" : "NO", ballKept ? "yes" : "NO", noCracks ? "yes" : "NO");
        ok = ok && sharpened && ballKept && noCracks;
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
