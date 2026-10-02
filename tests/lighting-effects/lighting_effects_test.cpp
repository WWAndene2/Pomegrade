// Pseudo ray tracing in the OpenGL renderer: drives the real DS 3D engine
// with geometry commands (as a game would) - a lit floor, a sphere resting on
// it, a back wall and a 2D HUD panel - and renders it with the lighting
// effects off and on. Writes the images next to the binary.
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

// projection: perspective, 50 degrees, 4:3 (row vectors, as the DS loads them)
static const double F = 1.0 / std::tan(25.0 * M_PI / 180.0), Aspect = 256.0 / 192.0, Zn = 0.5, Zf = 20;

// screen position of a view-space point
static void Project(double x, double y, double z, int& sx, int& sy)
{
    double w = -z;
    sx = (int)std::lround((x * F / Aspect / w + 1) * 128);
    sy = (int)std::lround((1 - y * F / w) * 96);
}

static const double SphereRadius = 0.8, SphereCenter[3] = {0, -0.2, -3.5}, FloorY = -1;
static bool RedSphere = false; // light bounce test: a red sphere tints the grey floor
static bool FrontLight = false; // light from the camera's side: the sphere's front is lit
static bool SideLight = false;  // shadow test: light from the upper left, the sphere's shadow falls to its right
static bool ShinyFloor = false; // reflection test: the floor's material has a white specular colour
static bool PerspectiveIcon = false; // a small 3D icon drawn last, with its own projection and viewport

static void DrawSphere()
{
    const int seg = 12, rings = 8;
    auto point = [&](int r, int s, double* p) {
        double th = M_PI * r / rings, ph = 2 * M_PI * s / seg;
        p[0] = std::sin(th) * std::cos(ph); p[1] = std::cos(th); p[2] = std::sin(th) * std::sin(ph);
    };
    Cmd(0x40, {1}); // quads
    for (int r = 0; r < rings; r++)
        for (int s = 0; s < seg; s++)
        {
            // counter-clockwise seen from outside
            int corners[4][2] = {{r, s}, {r, s + 1}, {r + 1, s + 1}, {r + 1, s}};
            for (auto& c : corners)
            {
                double p[3]; point(c[0], c[1], p);
                Normal(p[0], p[1], p[2]);
                Vertex16(SphereCenter[0] + p[0] * SphereRadius, SphereCenter[1] + p[1] * SphereRadius,
                         SphereCenter[2] + p[2] * SphereRadius);
            }
        }
    Cmd(0x41);
}

static void SubmitScene()
{
    GPU3D& g = Nds->GPU.GPU3D;
    g.Write32(0x04000060, 0);          // DISP3DCNT
    g.Write32(0x04000350, 0x1F0000 | (8 << 10) | (6 << 5) | 4); // clear colour, opaque
    g.Write32(0x04000354, 0x7FFF);     // clear depth
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)}); // viewport

    double proj[16] = {F / Aspect, 0, 0, 0,  0, F, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);

    Cmd(0x10, {2}); Cmd(0x15); // position & vector: identity (view space = model space)
    if (FrontLight)
        Cmd(0x32, {N10(-0.2) | (N10(-0.45) << 10) | (N10(-0.87) << 20)}); // light 0, from the front
    else if (SideLight)
        Cmd(0x32, {N10(0.69) | (N10(-0.69) << 10) | (N10(-0.23) << 20)}); // light 0 travelling right and down
    else
        Cmd(0x32, {N10(-0.3) | (N10(-0.8) << 10) | (N10(-0.5) << 20)}); // light 0, from above
    Cmd(0x33, {0x7FFF});                                          // light 0 white
    Cmd(0x30, {(0x5AD6) | (0x2108u << 16)});                        // diffuse light grey, ambient dark grey
    Cmd(0x31, {ShinyFloor ? 0x7FFFu : 0u}); // specular
    Cmd(0x29, {(31 << 16) | (1 << 24) | 0x80 | 0x01}); // light 0, front faces, opaque

    // floor, facing up, in strips so that its depth stays precise
    Cmd(0x40, {1});
    for (int i = 0; i < 8; i++)
    {
        double z0 = -1.6 - i * 0.8, z1 = z0 - 0.8;
        Normal(0, 1, 0); Vertex16(-4, FloorY, z0);
        Normal(0, 1, 0); Vertex16(4, FloorY, z0);
        Normal(0, 1, 0); Vertex16(4, FloorY, z1);
        Normal(0, 1, 0); Vertex16(-4, FloorY, z1);
    }
    // back wall, facing the camera, standing on the floor
    Normal(0, 0, 1); Vertex16(-4, FloorY, -6);
    Normal(0, 0, 1); Vertex16(4, FloorY, -6);
    Normal(0, 0, 1); Vertex16(4, 3, -6);
    Normal(0, 0, 1); Vertex16(-4, 3, -6);
    Cmd(0x41);

    if (RedSphere) Cmd(0x30, {(0x001F) | (0x0008u << 16)}); // diffuse red, ambient dark red
    Cmd(0x31, {0}); // the sphere isn't shiny
    DrawSphere();

    if (PerspectiveIcon)
    {
        // top-left corner (viewport y counts from the bottom), narrow field of view
        Cmd(0x60, {4 | (150u << 8) | (44u << 16) | (185u << 24)});
        double f2 = 1.0 / std::tan(15.0 * M_PI / 180.0);
        double icon[16] = {f2, 0, 0, 0,  0, f2, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
        Cmd(0x10, {0}); LoadMatrix(icon);
        Cmd(0x10, {1}); Cmd(0x15);
        Cmd(0x29, {(31 << 16) | (3 << 24) | 0xC0});
        Cmd(0x40, {0});
        Cmd(0x20, {0x03E0}); Vertex16(-0.3, -0.3, -2);
        Cmd(0x20, {0x03E0}); Vertex16(0.3, -0.3, -2);
        Cmd(0x20, {0x03E0}); Vertex16(0, 0.3, -2);
        Cmd(0x41);
        Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)});
    }

    // 2D HUD panel: orthographic projection, no normals
    double ortho[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, -1, 0,  0, 0, 0, 1};
    Cmd(0x10, {0}); LoadMatrix(ortho);
    Cmd(0x10, {1}); Cmd(0x15);
    Cmd(0x29, {(31 << 16) | (2 << 24) | 0xC0}); // no lights, both sides
    Cmd(0x40, {1});
    Cmd(0x20, {0x001F}); Vertex16(0.55, -0.95, 0);
    Cmd(0x20, {0x03E0}); Vertex16(0.95, -0.95, 0);
    Cmd(0x20, {0x7C00}); Vertex16(0.95, -0.55, 0);
    Cmd(0x20, {0x7FFF}); Vertex16(0.55, -0.55, 0);
    Cmd(0x41);

    Cmd(0x50, {0}); // swap buffers
    g.VBlank();     // builds the render list, as at the end of a frame
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

static double Luma(u32 c) { return 0.2126 * ((c >> 16) & 0xFF) + 0.7152 * ((c >> 8) & 0xFF) + 0.0722 * (c & 0xFF); }

// average brightness over a box centred on (x, y)
static double Brightness(const std::vector<u32>& img, int x, int y, int r)
{
    double sum = 0; int n = 0;
    for (int yy = y - r; yy <= y + r; yy++)
        for (int xx = x - r; xx <= x + r; xx++)
            if (xx >= 0 && xx < 256 && yy >= 0 && yy < 192) { sum += Luma(img[yy * 256 + xx]); n++; }
    return sum / n;
}

static int Diff(const std::vector<u32>& a, const std::vector<u32>& b, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++)
        if ((a[y*256+x] & 0xFFFFFF) != (b[y*256+x] & 0xFFFFFF)) n++;
    return n;
}

static std::vector<u32> Frame(GLRenderer& r, GPU& gpu, std::vector<u32>* capture = nullptr)
{
    SubmitScene();
    r.RenderFrame(gpu);
    if (capture) *capture = Capture(r);
    return Composite(r, gpu, r.GetScaleFactor());
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    printf("OpenGL: %s\n", (const char*)glGetString(GL_VERSION));

    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    Nds = nds.get();
    GPU& gpu = nds->GPU;
    gpu.GPU3D.Reset(); // as at emulator start: the geometry pipeline needs its reset state
    gpu.GPU3D.SetEnabled(true, true);

    auto r = GLRenderer::New();
    if (!r) { puts("GLRenderer::New failed"); return 1; }
    r->SetRenderSettings(false, 1);
    while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }

    bool ok = true;
    auto check = [&](bool cond, const char* what) { printf("%s: %s\n", what, cond ? "yes" : "NO"); ok = ok && cond; };

    std::vector<u32> captureOff, captureOn;
    auto off = Frame(*r, gpu, &captureOff);
    SavePng("lighting_off.png", off);

    // the effect starts with the first frame the game submits after it is enabled
    r->SetAmbientOcclusion(true);
    auto first = Frame(*r, gpu);
    check(first == off, "first frame after enabling (no view data yet) unchanged");
    auto ao = Frame(*r, gpu, &captureOn);
    SavePng("lighting_ao.png", ao);

    // regions, from the scene's geometry
    int cx, cy, fx, fy, tx, ty, wx, wy, crx, cry;
    // floor just in front of the sphere's contact point, under its overhang
    Project(SphereCenter[0], FloorY, SphereCenter[2] + 0.3, cx, cy);
    // open floor far from everything
    Project(-1.2, FloorY, -3.0, fx, fy);
    // top of the sphere, facing the open sky
    Project(SphereCenter[0], SphereCenter[1] + SphereRadius * 0.9, SphereCenter[2] + 0.3, tx, ty);
    // middle of the back wall, high above the floor
    Project(-2.0, 1.6, -6, wx, wy);
    // the crease between floor and back wall
    Project(-2.0, FloorY + 0.05, -6, crx, cry);

    struct Region { const char* name; int x, y; double off, on; };
    std::vector<Region> regions = {
        {"floor at the sphere's contact", cx, cy}, {"open floor", fx, fy}, {"top of the sphere", tx, ty},
        {"middle of the back wall", wx, wy}, {"floor-wall crease", crx, cry}};
    for (Region& reg : regions)
    {
        reg.off = Brightness(off, reg.x, reg.y, 2);
        reg.on = Brightness(ao, reg.x, reg.y, 2);
        printf("%-30s (%3d,%3d): %6.1f -> %6.1f (x%.2f)\n", reg.name, reg.x, reg.y, reg.off, reg.on, reg.on / reg.off);
    }
    check(regions[0].on < regions[0].off * 0.9, "contact shadow under the sphere (at least 10% darker)");
    check(regions[4].on < regions[4].off * 0.9, "floor-wall crease (at least 10% darker)");
    check(regions[1].on > regions[1].off * 0.97, "open floor unchanged (within 3%)");
    check(regions[2].on > regions[2].off * 0.97, "top of the sphere unchanged (within 3%)");
    check(regions[3].on > regions[3].off * 0.97, "open wall unchanged (within 3%)");

    // the HUD panel (orthographic: 2D-like) is left alone
    int hx0 = (int)((0.55 + 1) * 128) + 2, hx1 = (int)((0.95 + 1) * 128) - 2;
    int hy0 = (int)((1 - -0.55) * 96) + 2, hy1 = (int)((1 - -0.95) * 96) - 2;
    check(Diff(off, ao, hx0, hy0, hx1, hy1) == 0, "2D HUD panel untouched");
    // the game's display capture stays the DS render
    check(captureOn == captureOff, "display capture (what the game can read back) unchanged");

    r->SetAmbientOcclusion(false);
    check(Frame(*r, gpu) == off, "off again == original");

    // same shading with the polygon multiplier (sub-polygons carry their own
    // view data) and at a higher internal resolution (radius and filter scale)
    struct Config { const char* name; int scale, multiplier; };
    for (Config c : {Config{"multiplier x16", 1, 4}, Config{"resolution x2", 2, 1}, Config{"resolution x4", 4, 1}})
    {
        r->SetRenderSettings(false, c.scale);
        gpu.GPU3D.SetPolygonMultiplier(c.multiplier);
        r->SetAmbientOcclusion(false);
        auto cOff = Frame(*r, gpu);
        // the effect was on before the resolution changed: the image must still fill the screen
        check(Brightness(cOff, fx, fy, 2) > 150, (std::string(c.name) + ": effect off, full image").c_str());
        r->SetAmbientOcclusion(true);
        Frame(*r, gpu);
        auto cOn = Frame(*r, gpu);
        SavePng(std::string("lighting_ao_") + (c.scale > 1 ? "res" + std::to_string(c.scale) : "mult") + ".png", cOn);
        // compared over 13x13 native pixels: at higher resolution the
        // darkening along the crease is drawn more finely (its peak, right at
        // the line, goes from x0.81 to x0.56) for the same amount of shadow
        // across it (summed over the rows: 1.44 native, 1.68 at x2, 1.61 at x4)
        auto ratio = [&](const std::vector<u32>& on, const std::vector<u32>& off, int x, int y) {
            return Brightness(on, x, y, 6) / Brightness(off, x, y, 6);
        };
        double contact = ratio(cOn, cOff, cx, cy), crease = ratio(cOn, cOff, crx, cry);
        double open = ratio(cOn, cOff, fx, fy);
        double nativeContact = ratio(ao, off, cx, cy), nativeCrease = ratio(ao, off, crx, cry);
        printf("%-15s contact x%.2f (native x%.2f), crease x%.2f (native x%.2f), open floor x%.2f\n",
               c.name, contact, nativeContact, crease, nativeCrease, open);
        check(std::fabs(contact - nativeContact) < 0.04 && std::fabs(crease - nativeCrease) < 0.04 && open > 0.97,
              (std::string(c.name) + ": same shading").c_str());
    }

    // light bounce: the floor beside a red sphere turns reddish, the far floor doesn't
    r->SetRenderSettings(false, 1);
    gpu.GPU3D.SetPolygonMultiplier(1);
    r->SetAmbientOcclusion(false);
    RedSphere = true;
    FrontLight = true;
    auto redOff = Frame(*r, gpu);
    SavePng("bounce_off.png", redOff);
    r->SetLightBounce(true);
    Frame(*r, gpu);
    auto redOn = Frame(*r, gpu);
    SavePng("bounce_on.png", redOn);
    auto redness = [](const std::vector<u32>& img, int x, int y) {
        double rsum = 0, gsum = 0;
        for (int yy = y - 2; yy <= y + 2; yy++)
            for (int xx = x - 2; xx <= x + 2; xx++)
            {
                u32 c = img[yy * 256 + xx];
                rsum += (c >> 16) & 0xFF;
                gsum += (c >> 8) & 0xFF;
            }
        return rsum / gsum;
    };
    int bx, by;
    // floor in front of the sphere, which its lower front faces (screen-space:
    // only surfaces the camera sees can send light)
    Project(SphereCenter[0], FloorY, SphereCenter[2] + SphereRadius * 0.8, bx, by);
    double nearOff = redness(redOff, bx, by), nearOn = redness(redOn, bx, by);
    double creaseBounce = Brightness(redOn, crx, cry, 2) / Brightness(redOff, crx, cry, 2);
    printf("floor-wall crease brightness x%.3f\n", creaseBounce);
    check(creaseBounce > 1.0 && creaseBounce < 1.06, "floor-wall crease a little brighter (light between them), under 6%");    double farOff = redness(redOff, fx, fy), farOn = redness(redOn, fx, fy);
    printf("floor in front of the red sphere (%d,%d): red/green %.3f -> %.3f\n", bx, by, nearOff, nearOn);
    printf("open floor: red/green %.3f -> %.3f, brightness x%.3f\n", farOff, farOn,
           Brightness(redOn, fx, fy, 2) / Brightness(redOff, fx, fy, 2));
    check(nearOn > nearOff * 1.03, "floor in front of the red sphere tinted red (3% or more)");
    check(std::fabs(farOn - farOff) < 0.01, "open floor keeps its colour");
    check(Diff(redOff, redOn, hx0, hy0, hx1, hy1) == 0, "light bounce: 2D HUD panel untouched");
    r->SetLightBounce(false);
    check(Frame(*r, gpu) == redOff, "light bounce off again == original");
    RedSphere = false;
    FrontLight = false;

    // shadows: the sphere casts its shadow on the floor, away from the light
    SideLight = true;
    auto shadowOff = Frame(*r, gpu);
    SavePng("shadows_off.png", shadowOff);
    r->SetShadows(true);
    Frame(*r, gpu);
    auto shadowOn = Frame(*r, gpu);
    SavePng("shadows_on.png", shadowOn);
    {
        // the sphere's centre, projected along the light onto the floor: (0.8, -1, -3.77)
        int sx, sy, lx, ly;
        Project(SphereCenter[0] + 1.1, FloorY, SphereCenter[2] - 0.25, sx, sy);
        // the sphere's lit upper left
        Project(SphereCenter[0] - 0.45, SphereCenter[1] + 0.45, SphereCenter[2] + 0.5, lx, ly);
        struct Region { const char* name; int x, y; } regions[] = {
            {"floor in the sphere's shadow", sx, sy}, {"open floor", fx, fy},
            {"lit side of the sphere", lx, ly}, {"back wall", wx, wy}};
        double ratio[4];
        for (int i = 0; i < 4; i++)
        {
            ratio[i] = Brightness(shadowOn, regions[i].x, regions[i].y, 2) / Brightness(shadowOff, regions[i].x, regions[i].y, 2);
            printf("%-30s (%3d,%3d): x%.2f\n", regions[i].name, regions[i].x, regions[i].y, ratio[i]);
        }
        check(ratio[0] < 0.8, "floor in the sphere's shadow at least 20% darker");
        check(ratio[1] > 0.99 && ratio[2] > 0.99 && ratio[3] > 0.99, "lit surfaces unchanged");
        check(Diff(shadowOff, shadowOn, hx0, hy0, hx1, hy1) == 0, "shadows: 2D HUD panel untouched");
    }
    r->SetShadows(false);
    check(Frame(*r, gpu) == shadowOff, "shadows off again == original");
    SideLight = false;

    // reflections: the red sphere mirrored in a shiny floor, below its contact point
    RedSphere = true;
    FrontLight = true;
    ShinyFloor = true;
    auto reflOff = Frame(*r, gpu);
    SavePng("reflections_off.png", reflOff);
    r->SetReflections(true);
    Frame(*r, gpu);
    auto reflOn = Frame(*r, gpu);
    SavePng("reflections_on.png", reflOn);
    {
        // floor point seeing the mirror image of the sphere's lower front, (0, -0.9, -3.1)
        int mx, my;
        Project(0, FloorY, -2.82, mx, my);
        double before = redness(reflOff, mx, my), after = redness(reflOn, mx, my);
        printf("floor mirroring the red sphere (%d,%d): red/green %.3f -> %.3f\n", mx, my, before, after);
        printf("open floor: red/green %.3f -> %.3f\n", redness(reflOff, fx, fy), redness(reflOn, fx, fy));
        check(after > before * 1.1, "red sphere reflected in the shiny floor (10% redder or more)");

        // a small perspective 3D icon drawn last: the reflections keep the scene's projection
        PerspectiveIcon = true;
        Frame(*r, gpu);
        auto withIcon = Frame(*r, gpu);
        double iconAfter = redness(withIcon, mx, my);
        printf("with a 3D icon drawn last: mirror point red/green %.3f\n", iconAfter);
        check(std::fabs(iconAfter - after) < 0.02, "reflections use the scene's projection, not the icon's");
        PerspectiveIcon = false;
        check(std::fabs(redness(reflOn, fx, fy) - redness(reflOff, fx, fy)) < 0.02, "open floor keeps its colour (reflects the grey wall)");
        check(Diff(reflOff, reflOn, hx0, hy0, hx1, hy1) == 0, "reflections: 2D HUD panel untouched");
    }
    // a matte floor (no specular colour) only reflects a little, at grazing angles
    ShinyFloor = false;
    r->SetReflections(false);
    auto matteOff = Frame(*r, gpu);
    r->SetReflections(true);
    Frame(*r, gpu);
    auto matteOn = Frame(*r, gpu);
    {
        int mx, my;
        Project(0, FloorY, -2.82, mx, my);
        double change = redness(matteOn, mx, my) / redness(matteOff, mx, my);
        double open = Brightness(matteOn, fx, fy, 2) / Brightness(matteOff, fx, fy, 2);
        printf("matte floor: mirror point red/green x%.3f, open floor brightness x%.3f\n", change, open);
        check(change < 1.1 && std::fabs(open - 1) < 0.05, "matte floor: faint reflections only (under 10% / 5%)");
    }
    ShinyFloor = true;
    r->SetReflections(false);
    check(Frame(*r, gpu) == reflOff, "reflections off again == original");
    RedSphere = FrontLight = ShinyFloor = false;

    // all four together, with the multiplier, at resolution x2
    r->SetRenderSettings(false, 2);
    gpu.GPU3D.SetPolygonMultiplier(4);
    std::vector<u32> allCaptureOff, allCaptureOn;
    auto allOff = Frame(*r, gpu, &allCaptureOff);
    r->SetAmbientOcclusion(true);
    r->SetLightBounce(true);
    r->SetShadows(true);
    r->SetReflections(true);
    Frame(*r, gpu);
    auto allOn = Frame(*r, gpu, &allCaptureOn);
    SavePng("lighting_all.png", allOn);
    check(Brightness(allOn, crx, cry, 6) < Brightness(allOff, crx, cry, 6), "all effects: crease darker");
    check(Diff(allOff, allOn, hx0, hy0, hx1, hy1) == 0, "all effects: 2D HUD panel untouched");
    check(allCaptureOn == allCaptureOff, "all effects: display capture unchanged");
    r->SetAmbientOcclusion(false);
    r->SetLightBounce(false);
    r->SetShadows(false);
    r->SetReflections(false);
    check(Frame(*r, gpu) == allOff, "all effects off again == original");

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
