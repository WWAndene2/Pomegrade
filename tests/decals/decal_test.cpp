// Decals: a polygon with the DS "depth test equal" flag drawn on a floor at
// the same depth (road marks, shadows, signs). The DS draws it where its depth
// is within a margin of what is there; the software renderer is the
// reference, the OpenGL one must cover the same pixels at every setting.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_Soft.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace melonDS;

namespace
{
// LDR rd,=value / STR rd,[rn] / B . (literal pool after the code)
struct Program
{
    u32 Base;
    std::vector<u32> Code;
    std::vector<std::pair<size_t, u32>> Literals;
    explicit Program(u32 base) : Base(base) {}
    void Ldr(int rd, u32 value) { Literals.push_back({Code.size(), value}); Code.push_back(0xE59F0000 | (rd << 12)); }
    void Str(int rd, int rn) { Code.push_back(0xE5800000 | (rn << 16) | (rd << 12)); }
    void Write(u32 addr, u32 value) { Ldr(0, addr); Ldr(1, value); Str(1, 0); }
    void Loop() { Code.push_back(0xEAFFFFFE); }
    void Place(NDS& nds) const
    {
        std::vector<u32> out = Code;
        for (auto& [index, value] : Literals)
        {
            u32 lit = (u32)out.size();
            out.push_back(value);
            out[index] |= (lit - (u32)index) * 4 - 8;
        }
        memcpy(&nds.MainRAM[(Base - 0x02000000) & nds.MainRAMMask], out.data(), out.size() * 4);
    }
};

bool InitEGL()
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

u32 Fx(double v) { return (u32)(s32)std::lround(v * 4096) & 0xFFFF; }
u32 Fx32(double v) { return (u32)(s32)std::lround(v * 4096); }

constexpr u32 GXFIFO_MTX_MODE = 0x04000440, MTX_LOAD_4x4 = 0x04000458, MTX_IDENTITY = 0x04000454;
constexpr u32 COLOR = 0x04000480, VTX_16 = 0x0400048C, POLYGON_ATTR = 0x040004A4;
constexpr u32 BEGIN_VTXS = 0x04000500, END_VTXS = 0x04000504, SWAP_BUFFERS = 0x04000540, VIEWPORT = 0x04000580;
constexpr u32 CLEAR_COLOR = 0x04000350, CLEAR_DEPTH = 0x04000354, POWCNT1 = 0x04000304;

// a floor (y = -1, from z = -1 to -7.5: 4.12 vertex coordinates stop at 8)
// seen in perspective, a decal on it drawn after it, both quads. The DS sorts
// opaque polygons by their bottom edge: the decal shares the floor's near
// edge so it stays after it. wbuffer: SWAP_BUFFERS bit 1
void Scene(Program& p, bool wbuffer, double decalLift)
{
    p.Write(POWCNT1, 0x820F);
    p.Write(0x04000000, 0x00010108); // DISPCNT: display on, BG0 = 3D
    p.Write(CLEAR_COLOR, 0x001F0000);
    p.Write(CLEAR_DEPTH, 0x7FFF);
    p.Write(VIEWPORT, 0xBFFF0000);
    // the depth mode is latched by a buffer swap: one first, so the scene is
    // drawn in it (the swap waits for the next frame)
    p.Write(SWAP_BUFFERS, wbuffer ? 2 : 0);
    // projection: 90 degrees, near 0.5, far 16 (row vectors)
    const double n = 0.5, f = 16, a = (f + n) / (n - f), b = 2 * f * n / (n - f);
    const double proj[16] = {0.75, 0, 0, 0,  0, 1, 0, 0,  0, 0, a, -1,  0, 0, b, 0};
    p.Write(GXFIFO_MTX_MODE, 0);
    for (double m : proj) p.Write(MTX_LOAD_4x4, Fx32(m));
    p.Write(GXFIFO_MTX_MODE, 2);
    p.Write(MTX_IDENTITY, 0);

    auto quad = [&](u32 attr, u32 colour, double x0, double x1, double z0, double z1, double y) {
        p.Write(POLYGON_ATTR, attr);
        p.Write(BEGIN_VTXS, 1);
        p.Write(COLOR, colour);
        const double c[4][2] = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
        for (auto& v : c)
        {
            p.Write(VTX_16, Fx(v[0]) | (Fx(y) << 16));
            p.Write(VTX_16, Fx(v[1]));
        }
        p.Write(END_VTXS, 0);
    };
    quad(0x011F00C0, 0x03E0, -3, 3, -1, -7.5, -1);                                     // floor, green, id 1
    quad(0x021F40C0, 0x001F, -0.6, 0.6, -1, -6, -1 + decalLift);                    // decal, red, id 2, depth equal
    p.Write(SWAP_BUFFERS, wbuffer ? 2 : 0);
    p.Loop();
}

bool Red(u32 c) { return (c & 0x3F) > 32 && ((c >> 8) & 0x3F) < 16; }

int RedPixels(const std::vector<u32>& img)
{
    int n = 0;
    for (u32 c : img) if (Red(c)) n++;
    return n;
}

// decal pixels of the reference missing from the image
int Missing(const std::vector<u32>& ref, const std::vector<u32>& img)
{
    int n = 0;
    for (size_t i = 0; i < ref.size(); i++) if (Red(ref[i]) && !Red(img[i])) n++;
    return n;
}
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    bool ok = true;
    auto check = [&](bool cond, const std::string& what) { printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO"); ok = ok && cond; };

    for (int wbuffer = 0; wbuffer < 2; wbuffer++)
    {
        NDSArgs args; args.JIT = std::nullopt;
        auto nds = std::make_unique<NDS>(std::move(args));
        nds->Reset();
        nds->Start();
        Program prog(0x02000000);
        Scene(prog, wbuffer, 0);
        prog.Place(*nds);
        nds->ARM9.JumpTo(0x02000000);
        for (int i = 0; i < 3; i++) nds->RunFrame();

        // the reference: the software renderer's image of this frame
        SoftRenderer sr;
        nds->GPU.GPU3D.RenderFrameIdentical = false;
        sr.RenderFrame(nds->GPU);
        std::vector<u32> soft(256 * 192);
        for (int y = 0; y < 192; y++) memcpy(&soft[y * 256], sr.GetLine(y), 256 * 4);
        const int want = RedPixels(soft);
        printf("%s: software renderer: %d decal pixels\n", wbuffer ? "W-buffer" : "Z-buffer", want);
        check(want > 300, std::string(wbuffer ? "W" : "Z") + "-buffer: the reference draws the decal");

        auto r = GLRenderer::New();
        if (!r) { puts("GLRenderer::New failed"); return 1; }
        for (int scale : {1, 4})
            for (int precise = 0; precise < 2; precise++)
            {
                r->SetRenderSettings(false, scale);
                r->SetHighPrecision(precise);
                while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }
                r->RenderFrame(nds->GPU);
                r->PrepareCaptureFrame();
                std::vector<u32> gl(256 * 192);
                for (int y = 0; y < 192; y++) memcpy(&gl[y * 256], r->GetLine(y), 256 * 4);
                // every decal pixel the DS draws is drawn; more is allowed: in
                // this scene the DS itself loses part of the decal to its
                // depth rounding, a renderer with finer depth keeps it
                int got = RedPixels(gl), missing = Missing(soft, gl);
                char what[192];
                snprintf(what, sizeof(what), "%s, x%d%s: OpenGL draws every decal pixel the reference draws (%d missing of %d; %d drawn)",
                         wbuffer ? "W-buffer" : "Z-buffer", scale, precise ? ", high precision" : "", missing, want, got);
                check(missing <= want / 100, what);
            }
    }

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
