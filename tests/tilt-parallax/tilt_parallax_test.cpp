// Drives the real DS 3D engine with geometry commands (as a game would) and
// checks the tilt parallax: triangles at three depths in perspective move by
// depth around the scene's average one, an orthographic triangle and the
// game's position test don't move, the polygon multiplier's sub-vertices
// move with their polygon, and the projection the reflections use matches
// the moved vertices.
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace melonDS;

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

static void Triangle(double x, double y, double z, double size)
{
    Cmd(0x40, {0}); // triangles
    // lit with diverging normals: the multiplier only splits curved lit polygons
    Cmd(0x21, {N10(-0.4) | (N10(-0.4) << 10) | (N10(0.82) << 20)});
    Vertex16(x - size, y - size, z);
    Cmd(0x21, {N10(0.4) | (N10(-0.4) << 10) | (N10(0.82) << 20)});
    Vertex16(x + size, y - size, z);
    Cmd(0x21, {N10(0.0) | (N10(0.57) << 10) | (N10(0.82) << 20)});
    Vertex16(x, y + size, z);
    Cmd(0x41);
}

// a shallow scene, as a game's far camera gives: the gain scales the shift up
static const double Depths[3] = {-4.0, -5.0, -6.0}; // within the 4.12 vertex range (±8)

// an orthographic scene, as Platinum's overworld: two triangles at different depths
static bool OrthoScene = false;

static void SubmitFrame()
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
    Cmd(0x32, {N10(0.0) | (N10(0.0) << 10) | (N10(-1.0) << 20)}); // light 0 towards the scene
    Cmd(0x33, {0x7FFF});
    Cmd(0x30, {0x7FFF | (0x2108u << 16)});
    Cmd(0x29, {(31 << 16) | 0xC0 | 0x01}); // both faces, opaque, light 0
    for (double z : Depths)
        Triangle(0.0, 0.0, z, 0.1 * -z);

    // the game's position test, at the far depth
    Cmd(0x24, {(Fx(0.5) & 0xFFFF) | ((Fx(0.0) & 0xFFFF) << 16), Fx(-6.0) & 0xFFFF});

    // orthographic: a flat 3D menu (one depth), or a scene with depth
    double ortho[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, -1, 0,  0, 0, 0, 1};
    Cmd(0x10, {0}); LoadMatrix(ortho);
    Cmd(0x10, {2}); Cmd(0x15);
    Triangle(0.5, 0.5, -0.5, 0.2);
    if (OrthoScene)
        Triangle(-0.5, -0.5, -0.9, 0.2);

    Cmd(0x50, {0});
    g.VBlank();
}

struct Result { double x[4]; s32 posTest[4]; };

static Result Measure()
{
    GPU3D& g = Nds->GPU.GPU3D;
    Result r {};
    for (int i = 0; i < 4; i++) r.posTest[i] = g.PosTestResult[i];
    if (g.RenderNumPolygons != 4) { printf("expected 4 polygons, got %u\n", g.RenderNumPolygons); return r; }
    // the render list is Y-sorted: find each triangle by its own W
    for (u32 i = 0; i < g.RenderNumPolygons; i++)
    {
        Polygon* p = g.RenderPolygonRAM[i];
        double sx = 0, w = p->Vertices[0]->Position[3];
        for (u32 j = 0; j < p->NumVertices; j++)
            sx += (double)p->Vertices[j]->Position[0] / p->Vertices[j]->Position[3];
        sx /= p->NumVertices;
        int slot = 3; // orthographic: W = 1.0
        for (int d = 0; d < 3; d++)
            if (std::fabs(w / 4096.0 + Depths[d]) < 0.05) slot = d;
        r.x[slot] = sx;
    }
    return r;
}

// worst gap between each perspective vertex and its view position through the
// projection the renderer's reflections use, as a fraction of W
static double ReprojectionError(u32 count, Polygon* const* polygons)
{
    const GPU3D& g = Nds->GPU.GPU3D;
    double worst = 0;
    for (u32 i = 0; i < count; i++)
    {
        const Polygon* p = polygons[i];
        if (p->Vertices[0]->Orthographic) continue;
        for (u32 j = 0; j < p->NumVertices; j++)
        {
            const Vertex* v = p->Vertices[j];
            const double view[4] = {v->ViewPosition[0], v->ViewPosition[1], v->ViewPosition[2], 4096.0};
            for (int c = 0; c < 4; c++)
            {
                double clip = (view[0]*g.RenderProjMatrix[c] + view[1]*g.RenderProjMatrix[4+c] + view[2]*g.RenderProjMatrix[8+c] + view[3]*g.RenderProjMatrix[12+c]) / 4096.0;
                worst = std::max(worst, std::fabs(clip - v->Position[c]) / v->Position[3]);
            }
        }
    }
    printf("      worst reprojection error: %.5f of W\n", worst);
    return worst;
}

static bool Check(bool cond, const char* what)
{
    printf("%s: %s\n", cond ? "ok  " : "FAIL", what);
    return cond;
}

int main()
{
    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    Nds = nds.get();
    GPU3D& g = nds->GPU.GPU3D;
    g.Reset();
    g.SetEnabled(true, true);
    g.SetViewDataCapture(true);

    bool ok = true;
    SubmitFrame(); SubmitFrame();
    Result off = Measure();

    const float shift = 0.05f;
    g.SetTiltParallax(shift, 0.0f);
    SubmitFrame(); // latches the tilt at its end
    Result latched = Measure();
    ok &= Check(latched.x[2] == off.x[2], "the tilt starts at a frame boundary, not mid-frame");
    SubmitFrame();
    Result on = Measure();

    // pivot = the average W of the perspective vertices, gain = pivot / (2 * their standard deviation)
    double pivot = 0, squares = 0;
    for (double z : Depths) { pivot += -z / 3.0; squares += z * z / 3.0; }
    const double gain = std::clamp(pivot / (2.0 * std::sqrt(squares - pivot * pivot)), 1.0, 20.0);
    printf("      pivot %.3f, gain %.3f\n", pivot, gain);
    ok &= Check(gain > 2.0, "the shallow scene's shift is scaled up");
    for (int d = 0; d < 3; d++)
    {
        double expected = shift * gain * (1.0 - pivot / -Depths[d]);
        double got = on.x[d] - off.x[d];
        printf("      depth %.0f: moved %+.4f, expected %+.4f\n", -Depths[d], got, expected);
        ok &= Check(std::fabs(got - expected) < 0.002, "perspective triangle moved by its depth");
    }
    ok &= Check(on.x[0] < off.x[0] && on.x[2] > off.x[2], "near goes left, far goes right");
    ok &= Check(on.x[3] == off.x[3], "orthographic triangle unchanged");
    bool sameTest = true;
    for (int i = 0; i < 4; i++) sameTest &= on.posTest[i] == off.posTest[i];
    ok &= Check(sameTest, "the game's position test unchanged");

    // the reflections' projection reproduces the moved vertices
    ok &= Check(ReprojectionError(g.RenderNumPolygons, g.RenderPolygonRAM.data()) < 0.002,
                "reflection projection matches the moved vertices");

    // polygon multiplier: its sub-vertices (built from view space, not from
    // the game's vertices) move like the rest of the scene
    g.SetPolygonMultiplier(2);
    SubmitFrame(); SubmitFrame();
    printf("      multiplier: %u hardware polygons -> %u rendered polygons\n", g.RenderNumPolygons, g.GetRenderNumPolygons());
    ok &= Check(g.GetRenderNumPolygons() > g.RenderNumPolygons &&
                ReprojectionError(g.GetRenderNumPolygons(), g.GetRenderPolygons()) < 0.002,
                "multiplied sub-vertices moved with the scene");

    // back to off: nothing moves
    g.SetPolygonMultiplier(1);
    g.SetTiltParallax(0.0f, 0.0f);
    SubmitFrame(); SubmitFrame();
    Result back = Measure();
    ok &= Check(back.x[0] == off.x[0] && back.x[1] == off.x[1] && back.x[2] == off.x[2], "off again: the scene back in place");

    // orthographic scene: the nearer triangle goes left, the farther right,
    // each by the shift (two triangles: each is one spread from the average)
    OrthoScene = true;
    SubmitFrame(); SubmitFrame();
    double orthoOff[2] {}, orthoOn[2] {};
    auto orthoX = [&](double* out) {
        for (u32 i = 0; i < g.RenderNumPolygons; i++)
        {
            Polygon* p = g.RenderPolygonRAM[i];
            if (p->Vertices[0]->Position[3] != 4096) continue; // orthographic: W = 1.0
            double sx = 0;
            for (u32 j = 0; j < p->NumVertices; j++) sx += (double)p->Vertices[j]->Position[0] / 4096.0;
            out[p->Vertices[0]->Position[2] > 0.7 * 4096 ? 1 : 0] += sx / p->NumVertices; // clip z 0.5 (near) and 0.9 (far)
        }
    };
    orthoX(orthoOff);
    g.SetTiltParallax(shift, 0.0f);
    SubmitFrame(); SubmitFrame();
    orthoX(orthoOn);
    printf("      orthographic: near moved %+.4f, far moved %+.4f, expected -/+%.4f\n",
           orthoOn[0] - orthoOff[0], orthoOn[1] - orthoOff[1], shift / 2.0);
    ok &= Check(std::fabs(orthoOn[0] - orthoOff[0] + shift / 2.0) < 0.002 && std::fabs(orthoOn[1] - orthoOff[1] - shift / 2.0) < 0.002,
                "orthographic scene moved by its depth");
    g.SetTiltParallax(0.0f, 0.0f);
    OrthoScene = false;

    printf(ok ? "ALL PASSED\n" : "FAILED\n");
    return ok ? 0 : 1;
}
