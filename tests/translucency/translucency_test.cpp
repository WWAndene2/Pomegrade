// Translucent polygons: the DS blends a translucent polygon over a pixel only
// if the last translucent polygon there had another polygon ID, in the sort
// order the game chose. The software renderer is the reference; the OpenGL
// one must blend the same, at every resolution.
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

// an opaque grey background, then three translucent quads (alpha 16 of 31):
// A (red) and B (green) with polygon ID 10, overlapping; C (blue), ID 20,
// over both. The DS blends a translucent polygon over a pixel only if the
// last translucent polygon there had another ID: B is not drawn where A is,
// C is everywhere. manualSort: SWAP_BUFFERS bit 0 (submission order, not
// sorted by Y)
void Scene(Program& p, bool manualSort)
{
    p.Write(POWCNT1, 0x820F);
    p.Write(0x04000000, 0x00010108); // DISPCNT: display on, BG0 = 3D
    p.Write(0x04000060, 0x0008);     // DISP3DCNT: alpha blending
    p.Write(CLEAR_COLOR, 0x001F0000);
    p.Write(CLEAR_DEPTH, 0x7FFF);
    p.Write(VIEWPORT, 0xBFFF0000);
    p.Write(SWAP_BUFFERS, manualSort ? 1 : 0);
    p.Write(GXFIFO_MTX_MODE, 0);
    p.Write(MTX_IDENTITY, 0);
    p.Write(GXFIFO_MTX_MODE, 2);
    p.Write(MTX_IDENTITY, 0);

    auto quad = [&](u32 attr, u32 colour, double x0, double y0, double x1, double y1, double z) {
        p.Write(POLYGON_ATTR, attr);
        p.Write(BEGIN_VTXS, 1);
        p.Write(COLOR, colour);
        const double c[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        for (auto& v : c)
        {
            p.Write(VTX_16, Fx(v[0]) | (Fx(v[1]) << 16));
            p.Write(VTX_16, Fx(z));
        }
        p.Write(END_VTXS, 0);
    };
    // front and back drawn: bits 6-7; alpha: bits 16-20; ID: bits 24-29
    quad(0x011F00C0, 0x4210, -0.9, -0.9, 0.9, 0.9, 0.5);                    // background, grey, opaque
    quad(0x0A1000C0 | (1 << 11), 0x001F, -0.6, -0.5, 0.2, 0.3, 0.2);       // A, red, ID 10
    quad(0x0A1000C0 | (1 << 11), 0x03E0, -0.2, -0.3, 0.6, 0.5, 0.1);       // B, green, ID 10
    quad(0x141000C0 | (1 << 11), 0x7C00, -0.4, -0.1, 0.4, 0.1, 0.0);       // C, blue, ID 20
    p.Write(SWAP_BUFFERS, manualSort ? 1 : 0);
    p.Loop();
}
}

int main()
{
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    bool ok = true;
    auto check = [&](bool cond, const std::string& what) { printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO"); ok = ok && cond; };

    for (int manualSort = 0; manualSort < 2; manualSort++)
    {
        NDSArgs args; args.JIT = std::nullopt;
        auto nds = std::make_unique<NDS>(std::move(args));
        nds->Reset();
        nds->Start();
        Program prog(0x02000000);
        Scene(prog, manualSort);
        prog.Place(*nds);
        nds->ARM9.JumpTo(0x02000000);
        for (int i = 0; i < 3; i++) nds->RunFrame();

        SoftRenderer sr;
        nds->GPU.GPU3D.RenderFrameIdentical = false;
        sr.RenderFrame(nds->GPU);
        std::vector<u32> soft(256 * 192);
        for (int y = 0; y < 192; y++) memcpy(&soft[y * 256], sr.GetLine(y), 256 * 4);
        const std::string mode = manualSort ? "manual sort" : "auto sort";

        // the reference itself: where A and B overlap (and not C), one of them
        // only, as it blends alone (which one depends on the sort order)
        auto at = [&](const std::vector<u32>& img, double x, double y) {
            int px = (int)std::lround((x + 1) * 128), py = (int)std::lround((1 - y) * 96);
            return img[py * 256 + px] & 0x3F3F3F;
        };
        const u32 both = at(soft, 0.0, 0.25), aOnly = at(soft, -0.5, -0.4), bOnly = at(soft, 0.4, 0.4);
        check(aOnly != bOnly && (both == aOnly || both == bOnly),
              mode + ": the reference: A and B (one ID) overlap blended once, " + (both == aOnly ? "A" : "B") + " first");

        auto r = GLRenderer::New();
        if (!r) { puts("GLRenderer::New failed"); return 1; }
        for (int scale : {1, 4})
        {
            r->SetRenderSettings(false, scale);
            while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }
            r->RenderFrame(nds->GPU);
            r->PrepareCaptureFrame();
            std::vector<u32> gl(256 * 192);
            for (int y = 0; y < 192; y++) memcpy(&gl[y * 256], r->GetLine(y), 256 * 4);
            // pixels inside the quads (polygon edges follow other rules), and
            // the largest colour difference there
            int inside = 0, same = 0, worst = 0;
            for (int y = 0; y < 192; y++)
                for (int x = 0; x < 256; x++)
                {
                    const u32 a = soft[y * 256 + x], b = gl[y * 256 + x];
                    // skip pixels with a neighbour of another colour in the reference (edges)
                    bool edge = false;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            int nx = std::clamp(x + dx, 0, 255), ny = std::clamp(y + dy, 0, 191);
                            if ((soft[ny * 256 + nx] & 0x3F3F3F) != (a & 0x3F3F3F)) edge = true;
                        }
                    if (edge || !(a >> 24)) continue;
                    inside++;
                    int d = 0;
                    for (int c = 0; c < 24; c += 8) d = std::max(d, std::abs((int)((a >> c) & 0x3F) - (int)((b >> c) & 0x3F)));
                    if (d == 0) same++;
                    worst = std::max(worst, d);
                }
            char what[160];
            snprintf(what, sizeof(what), "%s, x%d: OpenGL blends as the DS inside the quads (%d of %d exact, largest difference %d)", mode.c_str(), scale, same, inside, worst);
            check(inside > 20000 && worst <= 1, what);
        }
    }
    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
