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
#include <map>
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
static bool UnlitCanopy = false; // shadow test: an unlit canopy over the back of the room, its colours painted in
// layering tests: what is drawn over the opaque geometry keeps its own colours
static bool TranslucentPanel = false; // a translucent 2D dialog box over the lower screen
static bool CutoutSprite = false;     // a sprite with a cut-out texture (A5I3: the DS draws it in the translucent pass)
static bool FullFog = false;          // fog at full density everywhere
// contact-hardening shadows: two lit bars running away from the camera (z -4
// to -6), one just above the floor and one high, lit from the left (light 0
// travelling (0.6, -0.8, 0)): each casts a band on the floor whose right
// edge runs away from the camera too, measured across the screen
static bool Bars = false;
// a DS shadow volume (the dark disc games draw under characters) crossing the floor, left of the sphere
static bool DSShadowVolume = false;
static double DSShadowVolumeX = -1.3, DSShadowVolumeZ = -4.0; // its middle on the floor
// shadows come from the scene, not the view: a lit box outside the camera's
// view, a one-sided panel turned away from the camera (the DS culls it), a
// lit ceiling over everything, facing down (as a room's)
static bool OffscreenBox = false, CulledPanel = false, Ceiling = false;
static int CulledPanelWinding = 0;
static const double LowBarX = -2.0, LowBarHeight = 0.1, HighBarX = 0.6, HighBarHeight = 2.4, BarHalf = 0.1;

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
    // DISP3DCNT: textures and alpha blending for the layering tests, fog
    g.Write32(0x04000060, (TranslucentPanel || CutoutSprite || DSShadowVolume ? 0x9 : 0) | (FullFog ? 0x80 : 0));
    if (FullFog)
    {
        g.Write32(0x04000358, (31u << 16) | (24 << 10) | (24 << 5) | 24); // light grey, opaque
        g.Write16(0x0400035C, 0);
        for (int i = 0; i < 32; i += 4) g.Write32(0x04000360 + i, 0x7F7F7F7F);
    }
    g.Write32(0x04000350, 0x1F0000 | (8 << 10) | (6 << 5) | 4); // clear colour, opaque
    g.Write32(0x04000354, 0x7FFF);     // clear depth
    Cmd(0x60, {0 | (0 << 8) | (255u << 16) | (191u << 24)}); // viewport

    double proj[16] = {F / Aspect, 0, 0, 0,  0, F, 0, 0,  0, 0, (Zf + Zn) / (Zn - Zf), -1,  0, 0, 2 * Zf * Zn / (Zn - Zf), 0};
    Cmd(0x10, {0}); LoadMatrix(proj);

    Cmd(0x10, {2}); Cmd(0x15); // position & vector: identity (view space = model space)
    if (Bars)
        Cmd(0x32, {N10(0.6) | (N10(-0.8) << 10) | (N10(0) << 20)});
    else if (FrontLight)
        Cmd(0x32, {N10(-0.2) | (N10(-0.45) << 10) | (N10(-0.87) << 20)}); // light 0, from the front
    else if (SideLight)
        Cmd(0x32, {N10(0.69) | (N10(-0.69) << 10) | (N10(-0.23) << 20)}); // light 0 travelling right and down
    else
        Cmd(0x32, {N10(-0.3) | (N10(-0.8) << 10) | (N10(-0.5) << 20)}); // light 0, from above
    Cmd(0x33, {0x7FFF});                                          // light 0 white
    Cmd(0x30, {(0x5AD6) | (0x2108u << 16)});                        // diffuse light grey, ambient dark grey
    Cmd(0x31, {ShinyFloor ? 0x7FFFu : 0u}); // specular
    Cmd(0x29, {(FullFog ? 1u << 15 : 0) | (31 << 16) | (1 << 24) | 0x80 | 0x01}); // light 0, front faces, opaque, fog

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

    if (UnlitCanopy)
    {
        // between the light and the back of the floor, as a sky or a painted
        // ceiling would be: it carries its own lighting, it casts no shadow
        Cmd(0x29, {(31 << 16) | (4 << 24) | 0xC0}); // no lights, both sides
        Cmd(0x40, {1});
        Cmd(0x20, {0x4210}); Vertex16(-4, 1.6, -3.5);
        Cmd(0x20, {0x4210}); Vertex16(4, 1.6, -3.5);
        Cmd(0x20, {0x4210}); Vertex16(4, 1.6, -6);
        Cmd(0x20, {0x4210}); Vertex16(-4, 1.6, -6);
        Cmd(0x41);
    }

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

    if (Bars)
    {
        Cmd(0x29, {(31 << 16) | (7 << 24) | 0xC0 | 0x01}); // light 0, both sides
        Cmd(0x40, {1});
        for (auto [x, h] : {std::pair<double, double>(LowBarX, LowBarHeight), {HighBarX, HighBarHeight}})
        {
            double x0 = x - BarHalf, x1 = x + BarHalf, y = FloorY + h;
            Normal(0, 1, 0); Vertex16(x0, y, -4); Vertex16(x1, y, -4); Vertex16(x1, y, -6); Vertex16(x0, y, -6);
        }
        Cmd(0x41);
    }

    if (OffscreenBox || CulledPanel || Ceiling)
    {
        Cmd(0x30, {(0x5AD6) | (0x2108u << 16)});
        Cmd(0x31, {0});
        auto quad = [](const double c[4][3], const double n[3]) {
            for (int i = 0; i < 4; i++) { Normal(n[0], n[1], n[2]); Vertex16(c[i][0], c[i][1], c[i][2]); }
        };
        if (OffscreenBox)
        {
            // left of the view, 2 above the floor: its shadow falls on the visible floor around (-2, -5.17)
            Cmd(0x29, {(31 << 16) | (8 << 24) | 0xC0 | 0x01});
            Cmd(0x40, {1});
            double x0 = -4.3, x1 = -3.7, y0 = FloorY + 1.7, y1 = FloorY + 2.3, z0 = -4.2, z1 = -4.8;
            const double top[4][3] = {{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, up[3] = {0, 1, 0};
            const double bot[4][3] = {{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}}, down[3] = {0, -1, 0};
            const double lef[4][3] = {{x0, y0, z1}, {x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}}, left[3] = {-1, 0, 0};
            const double rig[4][3] = {{x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0}}, right[3] = {1, 0, 0};
            quad(top, up); quad(bot, down); quad(lef, left); quad(rig, right);
            Cmd(0x41);
        }
        if (CulledPanel)
        {
            // one-sided, facing up and to the left (towards the light), its
            // front turned away from the camera: its shadow falls at (-1.6, -4.8)
            Cmd(0x29, {(31 << 16) | (9 << 24) | 0x80 | 0x01});
            Cmd(0x40, {1});
            const double n[3] = {-0.7071, 0.7071, 0};
            double c[3] = {-2.6, 0.0, -4.5}, u[3] = {0, 0, 0.3}, v[3] = {0.3, 0.3, 0};
            double q[4][3];
            int signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            for (int i = 0; i < 4; i++)
            {
                int j = CulledPanelWinding ? 3 - i : i;
                for (int k = 0; k < 3; k++) q[i][k] = c[k] + signs[j][0] * u[k] + signs[j][1] * v[k];
            }
            quad(q, n);
            Cmd(0x41);
        }
        if (Ceiling)
        {
            // over the whole scene, lit, facing down into it
            Cmd(0x29, {(31 << 16) | (10 << 24) | 0xC0 | 0x01});
            Cmd(0x40, {1});
            const double c[4][3] = {{-4, 2.5, -1.6}, {-4, 2.5, -8}, {4, 2.5, -8}, {4, 2.5, -1.6}}, down[3] = {0, -1, 0};
            quad(c, down);
            Cmd(0x41);
        }
    }

    if (DSShadowVolume)
    {
        // as games draw them: the volume as a mask (polygon id 0), then again
        // as the shadow (another id), both mode 3, black, translucent
        for (u32 id : {0u, 2u})
        {
            // the mask with its back faces, the shadow with its front faces
            Cmd(0x29, {(3u << 4) | (10u << 16) | (id << 24) | (id == 0 ? 0x40u : 0x80u)});
            Cmd(0x40, {1});
            double x0 = DSShadowVolumeX - 0.4, x1 = DSShadowVolumeX + 0.4, z0 = DSShadowVolumeZ + 0.4, z1 = DSShadowVolumeZ - 0.4, y0 = FloorY - 0.2, y1 = FloorY + 0.2;
            double q[6][4][3] = {
                {{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, {{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}},
                {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}}, {{x1, y0, z1}, {x0, y0, z1}, {x0, y1, z1}, {x1, y1, z1}},
                {{x0, y0, z1}, {x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}}, {{x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0}}};
            for (auto& face : q)
                for (auto& v : face) { Cmd(0x20, {0}); Vertex16(v[0], v[1], v[2]); }
            Cmd(0x41);
        }
    }

    if (CutoutSprite)
    {
        // over the sphere's shadow: left half opaque red, right half clear
        Cmd(0x2A, {6u << 26}); // A5I3, 8x8, address 0
        Cmd(0x2B, {0});
        Cmd(0x29, {(31 << 16) | (5 << 24) | 0x80 | 0x01});
        Cmd(0x40, {1});
        auto texcoord = [](double s, double t) { Cmd(0x22, {((u32)(s * 16) & 0xFFFF) | ((u32)(t * 16) << 16)}); };
        Normal(0, 0, 1); texcoord(0, 8); Vertex16(0.4, FloorY, -3.4);
        Normal(0, 0, 1); texcoord(8, 8); Vertex16(1.6, FloorY, -3.4);
        Normal(0, 0, 1); texcoord(8, 0); Vertex16(1.6, 0.2, -3.4);
        Normal(0, 0, 1); texcoord(0, 0); Vertex16(0.4, 0.2, -3.4);
        Cmd(0x41);
        Cmd(0x2A, {0});
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
    if (TranslucentPanel)
    {
        // a translucent dialog box over the lower left, the sphere's shadow seen through it
        Cmd(0x29, {(12u << 16) | (6 << 24) | 0xC0});
        Cmd(0x40, {1});
        Cmd(0x20, {0x5000}); Vertex16(-0.95, -0.95, 0);
        Cmd(0x20, {0x5000}); Vertex16(0.5, -0.95, 0);
        Cmd(0x20, {0x5000}); Vertex16(0.5, -0.1, 0);
        Cmd(0x20, {0x5000}); Vertex16(-0.95, -0.1, 0);
        Cmd(0x41);
    }

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

    // cut-out sprite texture: A5I3 8x8 in VRAM A (texture slot 0), left half
    // opaque palette colour 1 (red), right half alpha 0
    gpu.MapVRAM_AB(0, 0x83);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) gpu.VRAM_A[y * 8 + x] = x < 4 ? (u8)((31 << 3) | 1) : 0;
    gpu.MapVRAM_E(4, 0x83); // texture palette slot 0
    ((u16*)gpu.VRAM_E)[1] = 0x001F;

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
    // (x8: the terms computed at half resolution, see GLRenderer::MaxLightingScale)
    for (Config c : {Config{"multiplier x16", 1, 4}, Config{"resolution x2", 2, 1}, Config{"resolution x4", 4, 1},
                     Config{"resolution x8", 8, 1}})
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
    {
        // an unlit canopy between the light and the back of the floor casts
        // nothing; the sphere still does
        UnlitCanopy = true;
        r->SetShadows(false);
        auto canopyOff = Frame(*r, gpu);
        r->SetShadows(true);
        Frame(*r, gpu);
        auto canopyOn = Frame(*r, gpu);
        int bx, by, sx, sy;
        // its line to the light crosses the canopy at (-0.6, 1.6, -4.6)
        Project(2.0, FloorY, -5.5, bx, by);
        Project(SphereCenter[0] + 1.1, FloorY, SphereCenter[2] - 0.25, sx, sy);
        double under = Brightness(canopyOn, bx, by, 2) / Brightness(canopyOff, bx, by, 2);
        double shadow = Brightness(canopyOn, sx, sy, 2) / Brightness(canopyOff, sx, sy, 2);
        printf("unlit canopy: floor under it (%d,%d) x%.2f, sphere's shadow x%.2f\n", bx, by, under, shadow);
        check(under > 0.99 && shadow < 0.8, "unlit canopy casts no shadow, the sphere still does");
        UnlitCanopy = false;
    }
    {
        // shadows come from the scene, not from what the camera sees
        auto shadowRatio = [&](bool& flag, double x, double z, const char* what) {
            r->SetShadows(false);
            flag = true;
            auto off = Frame(*r, gpu);
            r->SetShadows(true);
            Frame(*r, gpu);
            auto on = Frame(*r, gpu);
            flag = false;
            int px, py;
            Project(x, FloorY, z, px, py);
            double ratio = Brightness(on, px, py, 2) / Brightness(off, px, py, 2);
            printf("%s: floor at (%d,%d) x%.2f\n", what, px, py, ratio);
            return ratio;
        };
        double offscreen = shadowRatio(OffscreenBox, -2.0, -5.17, "a box outside the view");
        check(offscreen < 0.8, "a box outside the view shadows the visible floor");

        // the panel's winding that the DS culls (front turned away from the camera)
        r->SetShadows(false);
        auto noPanel = Frame(*r, gpu);
        CulledPanel = true;
        for (CulledPanelWinding = 0; CulledPanelWinding < 2; CulledPanelWinding++)
            if (Frame(*r, gpu) == noPanel) break;
        CulledPanel = false;
        check(CulledPanelWinding < 2, "the one-sided panel turned away from the camera isn't drawn");
        double culled = shadowRatio(CulledPanel, -1.6, -4.8, "a panel the DS culls (turned away from the camera)");
        check(culled < 0.8, "a face turned away from the camera still casts");

        int sx, sy;
        Project(SphereCenter[0] + 1.1, FloorY, SphereCenter[2] - 0.25, sx, sy);
        // the side light moves the ceiling's shadow 3.5 to the right: floor from x -0.5 onwards
        double ceilingOpen = shadowRatio(Ceiling, 2.0, -5.5, "a lit ceiling over the scene, open floor under it");
        Ceiling = true;
        r->SetShadows(false);
        auto ceilOff = Frame(*r, gpu);
        r->SetShadows(true);
        Frame(*r, gpu);
        auto ceilOn = Frame(*r, gpu);
        Ceiling = false;
        double ceilingSphere = Brightness(ceilOn, sx, sy, 2) / Brightness(ceilOff, sx, sy, 2);
        printf("with the ceiling: sphere's shadow x%.2f\n", ceilingSphere);
        check(ceilingOpen > 0.97 && ceilingSphere < 0.8, "a ceiling facing into the scene doesn't block the light; the sphere still casts");
        r->SetShadows(true);
    }
    {
        // contact hardening: a shadow is sharp where it meets its caster and
        // softer the further it falls. The right edge of each bar's shadow,
        // where z = -5 (the same distance from the camera for both): its
        // width in pixels between 10% and 90% of the shadow's depth
        SideLight = false;
        Bars = true;
        r->SetShadows(false);
        auto barsOff = Frame(*r, gpu);
        r->SetShadows(true);
        Frame(*r, gpu);
        auto barsOn = Frame(*r, gpu);
        Bars = false;
        auto softEdge = [&](double barX, double height) {
            // inside the shadow: the bar's right side moved along the light
            int sx, sy;
            Project(barX + BarHalf + height * 0.75 - 0.03, FloorY, -5.0, sx, sy);
            auto ratio = [&](int x) { return Brightness(barsOn, x, sy, 0) / std::max(1.0, Brightness(barsOff, x, sy, 0)); };
            double core = 1.0;
            for (int x = sx - 2; x <= sx; x++) core = std::min(core, ratio(x));
            int soft = 0;
            for (int x = sx; x < 256; x++)
            {
                double d = (1.0 - ratio(x)) / std::max(1e-6, 1.0 - core); // 0 lit .. 1 full shadow
                if (d <= 0.1) break;
                if (d < 0.9) soft++;
            }
            printf("bar %.1f above the floor: shadow x%.2f, soft edge %d pixels\n", height, core, soft);
            return std::pair<int, double>(soft, core);
        };
        auto [nearSoft, nearCore] = softEdge(LowBarX, LowBarHeight);
        auto [farSoft, farCore] = softEdge(HighBarX, HighBarHeight);
        check(nearCore < 0.8 && farCore < 0.8, "contact hardening: both bars cast a shadow");
        check(farSoft >= nearSoft + 2, "contact hardening: the high bar's shadow edge is wider (2 pixels or more)");
        SideLight = true;
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

        // the floor left of the sphere mirrors the white wall all the way to
        // the crease: no band where the floor, seen at a grazing angle,
        // stops the rays as if it reflected itself (at x = -1.5 the rays go
        // further left, never towards the sphere)
        int dull = 0, rows = 0;
        for (double z = -5.9; z <= -2.5; z += 0.05)
        {
            int sx, sy;
            Project(-1.5, FloorY, z, sx, sy);
            if (sx < 26) continue; // reflections fade out near the screen's edges, by design
            rows++;
            if (Brightness(reflOn, sx, sy, 0) < Brightness(reflOff, sx, sy, 0) * 1.05) dull++;
        }
        printf("floor mirroring the white wall: %d of %d points not brighter\n", dull, rows);
        check(rows >= 40 && dull == 0, "floor mirrors the wall down to the crease (every point 5% brighter or more)");
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

    // above x4 the effects' terms are computed at half resolution and brought
    // back by an edge-aware filter (GLRenderer::MaxLightingScale): the change
    // the four effects make to the image, averaged back to the DS resolution,
    // must be as close between x8 and x4 as it is between x4 and x2 (terms at
    // full resolution in both; the geometry itself is drawn more finely at
    // each step). Measured: 0.21/255 on average, 723 values over 8, for x8 vs
    // x4; 0.42 and 1500 for x4 vs x2; the change itself is 3.2/255 on average
    {
        const int scales[3] = {2, 4, 8};
        std::vector<int> change[3];
        for (int k = 0; k < 3; k++)
        {
            r->SetRenderSettings(false, scales[k]);
            auto offK = Frame(*r, gpu);
            r->SetAmbientOcclusion(true); r->SetLightBounce(true); r->SetShadows(true); r->SetReflections(true);
            Frame(*r, gpu);
            auto onK = Frame(*r, gpu);
            if (scales[k] == 8) SavePng("lighting_all_x8.png", onK);
            r->SetAmbientOcclusion(false); r->SetLightBounce(false); r->SetShadows(false); r->SetReflections(false);
            for (size_t i = 0; i < onK.size(); i++)
                for (int c = 0; c < 3; c++)
                    change[k].push_back((int)((onK[i] >> (8 * c)) & 0xFF) - (int)((offK[i] >> (8 * c)) & 0xFF));
        }
        auto compare = [&](int a, int b, double& mean, int& over8) {
            double sum = 0;
            over8 = 0;
            for (size_t i = 0; i < change[a].size(); i++)
            {
                int d = std::abs(change[a][i] - change[b][i]);
                sum += d;
                if (d > 8) over8++;
            }
            mean = sum / change[a].size();
        };
        double mean84, mean42;
        int over84, over42;
        compare(2, 1, mean84, over84);
        compare(1, 0, mean42, over42);
        printf("all effects, change to the image: x8 (terms at half resolution) vs x4 %.2f/255 on average, %d values over 8; x4 vs x2 %.2f, %d\n",
               mean84, over84, mean42, over42);
        check(mean84 <= mean42 && over84 <= over42, "all effects at x8: as close to x4 as x4 is to x2");
        r->SetRenderSettings(false, 2);
    }

    // layering: the effects light the opaque geometry; what the DS draws over
    // it (translucent panels, cut-out sprites, fog) keeps its own colours
    {
        SideLight = true;
        auto withEffects = [&](bool on) {
            r->SetAmbientOcclusion(on);
            r->SetShadows(on);
            Frame(*r, gpu); // view data comes with the frame after enabling
            return Frame(*r, gpu);
        };
        auto channel = [](u32 c, int k) { return (double)((c >> (8 * k)) & 0xFF); };
        auto baseOff = withEffects(false), baseOn = withEffects(true);
        check(baseOn != baseOff, "layering: the base scene changes with AO and shadows");

        // the dialog box blends over the scene lit under it: it keeps 19/31 of
        // that scene's change (alpha 12), within a 6-bit colour step and rounding
        TranslucentPanel = true;
        auto panelOff = withEffects(false), panelOn = withEffects(true);
        TranslucentPanel = false;
        int inside = 0; double worst = 0, sceneChange = 0;
        for (int y = 1; y < 191; y++)
            for (int x = 1; x < 255; x++)
            {
                bool covered = true; // the box all round (its border pixels mix both)
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        covered = covered && panelOff[(y+dy)*256+x+dx] != baseOff[(y+dy)*256+x+dx];
                if (!covered) continue;
                int i = y * 256 + x;
                inside++;
                for (int k = 0; k < 3; k++)
                {
                    double expect = channel(panelOff[i], k) + 19.0 / 31 * (channel(baseOn[i], k) - channel(baseOff[i], k));
                    worst = std::max(worst, std::fabs(channel(panelOn[i], k) - expect));
                    sceneChange = std::max(sceneChange, std::fabs(channel(baseOn[i], k) - channel(baseOff[i], k)));
                }
            }
        printf("translucent dialog box: %d pixels, worst error %.1f (scene behind changes up to %.0f)\n", inside, worst, sceneChange);
        check(inside > 1000 && sceneChange > 30 && worst <= 6, "translucent dialog box: only the scene behind it is lit");

        // the cut-out sprite's opaque pixels keep their colour (no lighting
        // data for them: the floor's shadow behind them must not show)
        CutoutSprite = true;
        auto spriteOff = withEffects(false), spriteOn = withEffects(true);
        CutoutSprite = false;
        // its colour: the most common one where it covers the scene (pixels on
        // the seam between its two triangles are blended with the floor)
        std::map<u32, int> colours;
        for (int i = 0; i < 256 * 192; i++)
            if ((spriteOff[i] & 0xFFFFFF) != (baseOff[i] & 0xFFFFFF)) colours[spriteOff[i] & 0xFFFFFF]++;
        u32 spriteColour = 0;
        for (auto& [c, n] : colours) if (n > colours[spriteColour]) spriteColour = c;
        int spritePx = 0, changed = 0, shadowBehind = 0;
        for (int i = 0; i < 256 * 192; i++)
        {
            u32 c = spriteOff[i];
            if ((c & 0xFFFFFF) != spriteColour || (c & 0xFFFFFF) == (baseOff[i] & 0xFFFFFF)) continue;
            spritePx++;
            if ((spriteOn[i] & 0xFFFFFF) != (c & 0xFFFFFF)) changed++;
            if (Luma(baseOn[i]) < Luma(baseOff[i]) - 8) shadowBehind++;
        }
        printf("cut-out sprite: %d pixels, %d changed (%d over the shadow)\n", spritePx, changed, shadowBehind);
        check(spritePx > 100 && shadowBehind > 20 && changed == 0, "cut-out sprite keeps its colours");

        // the game's fake shadow (a DS shadow volume) gives way to the real
        // shadows in the image shown; the game's own image keeps it
        {
            int vx, vy;
            Project(-1.3, FloorY, -4.0, vx, vy);
            r->SetAmbientOcclusion(false);
            r->SetShadows(false);
            Frame(*r, gpu);
            auto plainOff = Frame(*r, gpu);
            DSShadowVolume = true;
            std::vector<u32> capVolume;
            auto volumeOff = Frame(*r, gpu, &capVolume);
            r->SetShadows(true);
            Frame(*r, gpu);
            std::vector<u32> capVolumeOn;
            auto volumeOn = Frame(*r, gpu, &capVolumeOn);
            DSShadowVolume = false;
            Frame(*r, gpu);
            std::vector<u32> capPlainOn;
            auto plainOn = Frame(*r, gpu, &capPlainOn);
            r->SetShadows(false);
            double dark = Brightness(volumeOff, vx, vy, 2) / Brightness(plainOff, vx, vy, 2);
            printf("DS shadow volume (%d,%d): shadows off x%.2f; shadows on, image %s the scene without it\n", vx, vy, dark,
                   volumeOn == plainOn ? "==" : "!=");
            check(dark < 0.9, "DS shadow volume: drawn as the game draws it without real-time shadows");
            check(volumeOn == plainOn, "DS shadow volume: left out of the image with real-time shadows");
            check(capVolumeOn != capPlainOn, "DS shadow volume: still in the game's own image (display capture)");

            // away from anything that casts (a character the DS doesn't
            // light, say): nothing replaces it, the game's shadow stays
            DSShadowVolumeX = -2.2; DSShadowVolumeZ = -5.2;
            Project(DSShadowVolumeX, FloorY, DSShadowVolumeZ, vx, vy);
            r->SetShadows(true);
            Frame(*r, gpu);
            auto lonePlain = Frame(*r, gpu);
            DSShadowVolume = true;
            Frame(*r, gpu);
            auto loneOn = Frame(*r, gpu);
            DSShadowVolume = false;
            r->SetShadows(false);
            DSShadowVolumeX = -1.3; DSShadowVolumeZ = -4.0;
            double lone = Brightness(loneOn, vx, vy, 2) / Brightness(lonePlain, vx, vy, 2);
            printf("DS shadow volume with nothing casting near it (%d,%d), real-time shadows on: x%.2f\n", vx, vy, lone);
            check(lone < 0.9, "DS shadow volume with nothing casting near it: the game's shadow stays");
        }

        // fog at full density hides the lit scene entirely
        FullFog = true;
        auto fogOff = withEffects(false), fogOn = withEffects(true);
        FullFog = false;
        check(fogOn == fogOff, "full fog: nothing of the effects shows through");

        withEffects(false);
        SideLight = false;
    }

    // a game capturing the 3D image every frame (3D on both screens, motion
    // blur) shows the unlit copies: the effects pause, rather than each screen
    // flickering between the lit image and the unlit copy
    {
        auto plain = Frame(*r, gpu);
        r->SetAmbientOcclusion(true);
        Frame(*r, gpu);
        auto lit = Frame(*r, gpu);
        std::vector<u32> copy;
        Frame(*r, gpu, &copy);                     // a capture
        auto afterOne = Frame(*r, gpu, &copy);     // a second one in a row
        auto afterTwo = Frame(*r, gpu, &copy);
        check(lit != plain && afterOne == lit, "capture: one capture doesn't pause the effects");
        check(afterTwo == plain, "capture every frame: the effects pause");
        auto stopped = Frame(*r, gpu); // still sees the last capture
        for (int i = 0; i < 29; i++) stopped = Frame(*r, gpu);
        check(stopped == plain, "capture stopped: still paused for 30 frames");
        check(Frame(*r, gpu) == lit, "capture stopped: the effects come back after 30 frames");
        r->SetAmbientOcclusion(false);
    }

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
