// Inspector: polygons are attributed to the ARM9 instruction that wrote their
// 3D commands, or to the DMA that copied them (with the display list's
// address, length and content hash); the trace lists matrix stack
// operations; cartridge reads are named after NitroFS files. Real ARM code in
// main RAM, run with the interpreter and with the JIT (x86-64 back end here;
// the ARM64 one mirrors it). Also: nothing is recorded while it is off.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "NDS.h"
#include "GPU.h"
#include "GPU3D.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_MaterialClassifier.h"
#include "GPU3D_SkeletonRecovery.h"
#include "GPU3D_Soft.h"
#include "NDSCart.h"
#include "NDS_Inspector.h"
#include "xxhash/xxhash.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace melonDS;

namespace
{
// a tiny ARM assembler: LDR rd,=value (literal pool after the code), STR, B .
struct Program
{
    u32 Base;
    std::vector<u32> Code;
    std::vector<std::pair<size_t, u32>> Literals; // instruction index, value

    explicit Program(u32 base) : Base(base) {}
    u32 Here() const { return Base + (u32)Code.size() * 4; }
    void Ldr(int rd, u32 value) { Literals.push_back({Code.size(), value}); Code.push_back(0xE59F0000 | (rd << 12)); }
    // returns the instruction's address
    u32 Str(int rd, int rn) { u32 at = Here(); Code.push_back(0xE5800000 | (rn << 16) | (rd << 12)); return at; }
    void Loop() { Code.push_back(0xEAFFFFFE); }
    // r9 = count; the code from LoopStart to LoopEnd runs count times. The JIT
    // interprets a block while it compiles it: the first pass (in the block
    // starting at the program's entry) and the second (a new block starting at
    // the loop) are interpreted, the third runs compiled code
    u32 LoopStart(u32 count) { Code.push_back(0xE3A09000 | count); return Here(); }
    // BL target, BX LR, and a forward B patched later
    u32 Bl(u32 target) { u32 at = Here(); Code.push_back(0xEB000000 | (((target - (at + 8)) / 4) & 0xFFFFFF)); return at; }
    void BxLr() { Code.push_back(0xE12FFF1E); }
    size_t BForward() { Code.push_back(0xEA000000); return Code.size() - 1; }
    void PatchB(size_t index, u32 target) { Code[index] |= ((target - (Base + (u32)index * 4 + 8)) / 4) & 0xFFFFFF; }
    void LoopEnd(u32 start)
    {
        Code.push_back(0xE2599001); // SUBS r9, r9, #1
        u32 at = Here();
        Code.push_back(0x1A000000 | (((start - (at + 8)) / 4) & 0xFFFFFF)); // BNE start
    }
    // store value to addr, through r0 and r1; returns the STR's address
    u32 Write(u32 addr, u32 value) { Ldr(0, addr); Ldr(1, value); return Str(1, 0); }
    void Place(NDS& nds) const
    {
        std::vector<u32> out = Code;
        for (auto& [index, value] : Literals)
        {
            u32 litIndex = (u32)out.size();
            out.push_back(value);
            u32 offset = (litIndex - (u32)index) * 4 - 8;
            out[index] |= offset;
        }
        for (size_t i = 0; i < out.size(); i++)
            memcpy(&nds.MainRAM[(Base - 0x02000000 + i * 4) & nds.MainRAMMask], &out[i], 4);
    }
};

constexpr u32 POWCNT1 = 0x04000304, POLYGON_ATTR = 0x040004A4, BEGIN_VTXS = 0x04000500, END_VTXS = 0x04000504;
constexpr u32 VTX_16 = 0x0400048C, SWAP_BUFFERS = 0x04000540, MTX_PUSH = 0x04000444, MTX_POP = 0x04000448;
constexpr u32 DMA0SAD = 0x040000B0, DMA0DAD = 0x040000B4, DMA0CNT = 0x040000B8;
constexpr u32 TEXIMAGE_PARAM = 0x040004A8;
// texture at 0x2000 (param 0x400 in 8-byte units), 16x16, direct colour (7), texgen from normals (2)
constexpr u32 TexParamNormal = (2u << 30) | (7u << 26) | (1u << 23) | (1u << 20) | 0x400;
constexpr u32 Attr = 0x051F00C0; // polygon ID 5, opaque, front and back drawn
const u32 Vertices[3][2] = {{0, 0}, {0x0800, 0}, {0x08000000, 0}};

std::unique_ptr<NDS> MakeNDS(bool jit)
{
    NDSArgs args;
    if (jit) args.JIT = JITArgs{}; else args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    nds->Reset();
    nds->Start();
    return nds;
}

void Run(NDS& nds, u32 entry, int frames)
{
    nds.ARM9.JumpTo(entry);
    for (int i = 0; i < frames; i++) nds.RunFrame();
}

std::string Hex(u32 v) { char b[16]; snprintf(b, sizeof(b), "%08X", v); return b; }

// the polygons the report gives a call site
u32 SitePolygons(const std::string& report, u32 instr)
{
    size_t at = report.find("\n" + Hex(instr) + " ");
    if (at == std::string::npos) return 0;
    unsigned polys = 0;
    sscanf(report.c_str() + at + 1 + 8, " %u", &polys);
    return polys;
}

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

// a full-screen quad with an 8x8 direct-colour texture (texel x,y = red x*4, green y*4)
Vertex QuadVerts[4];
Polygon Quad;
void SetupTexturedQuad(GPU& gpu, u32 callSite)
{
    gpu.MapVRAM_AB(0, 0x83);
    u16* tex = (u16*)gpu.VRAM_A;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            tex[y*8+x] = (x*4) | ((y*4) << 5) | 0x8000;
    int pos[4][2] = {{0,0},{256,0},{256,192},{0,192}};
    int tc[4][2] = {{0,0},{8*16,0},{8*16,8*16},{0,8*16}};
    memset(&Quad, 0, sizeof(Quad));
    for (int i = 0; i < 4; i++)
    {
        Vertex& v = QuadVerts[i];
        memset(&v, 0, sizeof(v));
        v.FinalPosition[0] = pos[i][0]; v.FinalPosition[1] = pos[i][1];
        v.HiresPosition[0] = pos[i][0] << 4; v.HiresPosition[1] = pos[i][1] << 4;
        v.FinalColor[0] = v.FinalColor[1] = v.FinalColor[2] = 510;
        v.TexCoords[0] = tc[i][0]; v.TexCoords[1] = tc[i][1];
        Quad.Vertices[i] = &v;
        Quad.FinalZ[i] = 0x1000;
        Quad.FinalW[i] = 0x1000;
    }
    Quad.NumVertices = 4;
    Quad.Attr = (31 << 16) | (1 << 24);
    Quad.TexParam = (7u << 26);
    Quad.FacingView = true;
    Quad.CallSite = callSite;
    gpu.GPU3D.RenderPolygonRAM[0] = &Quad;
    gpu.GPU3D.RenderNumPolygons = 1;
    gpu.GPU3D.RenderDispCnt = 1;
    gpu.GPU3D.RenderClearAttr1 = 0;
    gpu.GPU3D.RenderClearAttr2 = 0x7FFF;
}

// a cartridge with a NitroFS holding data/a.bin and data/sub/b.bin, an overlay
std::unique_ptr<NDSCart::CartCommon> CartWithFiles()
{
    std::vector<u8> rom(0x40000, 0);
    auto w32 = [&](u32 off, u32 v) { memcpy(&rom[off], &v, 4); };
    auto w16 = [&](u32 off, u16 v) { memcpy(&rom[off], &v, 2); };
    memcpy(&rom[0x0C], "TEST", 4);
    // FAT: file 0 = overlay 0, 1 = data/a.bin, 2 = data/sub/b.bin
    const u32 fat = 0x1000, fnt = 0x1100, ovt = 0x1200;
    w32(0x48, fat); w32(0x4C, 3 * 8);
    w32(fat + 0, 0x8000);  w32(fat + 4, 0x9000);
    w32(fat + 8, 0x10000); w32(fat + 12, 0x12000);
    w32(fat + 16, 0x20000); w32(fat + 20, 0x20100);
    // FNT: root (dir F000) holds "data"; data (F001) holds a.bin and "sub"; sub (F002) holds b.bin
    w32(0x40, fnt); w32(0x44, 0x80);
    w32(fnt + 0, 0x18); w16(fnt + 4, 1); w16(fnt + 6, 3);
    w32(fnt + 8, 0x20); w16(fnt + 12, 1); w16(fnt + 14, 0xF000);
    w32(fnt + 16, 0x30); w16(fnt + 20, 2); w16(fnt + 22, 0xF001);
    u32 p = fnt + 0x18;
    rom[p++] = 0x84; memcpy(&rom[p], "data", 4); p += 4; w16(p, 0xF001); p += 2; rom[p++] = 0;
    p = fnt + 0x20;
    rom[p++] = 0x05; memcpy(&rom[p], "a.bin", 5); p += 5;
    rom[p++] = 0x83; memcpy(&rom[p], "sub", 3); p += 3; w16(p, 0xF002); p += 2; rom[p++] = 0;
    p = fnt + 0x30;
    rom[p++] = 0x05; memcpy(&rom[p], "b.bin", 5); p += 5; rom[p++] = 0;
    // ARM9 overlay table: overlay 7 is file 0
    w32(0x50, ovt); w32(0x54, 32);
    w32(ovt + 0, 7); w32(ovt + 24, 0);
    return NDSCart::ParseROM(rom.data(), (u32)rom.size());
}
}

int main()
{
    bool ok = true;
    auto check = [&](bool cond, const std::string& what) { printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO"); ok = ok && cond; };

    for (int jit = 0; jit < 2; jit++)
    {
        const std::string mode = jit ? "JIT" : "interpreter";

        // 1. CPU writes: polygon from the store of the last vertex word
        {
            auto nds = MakeNDS(jit);
            nds->Inspector.SetEnabled(true);
            nds->Inspector.SetView(Inspector::View::CallSite);
            Program prog(0x02000000);
            prog.Write(POWCNT1, 0x820F);
            u32 pushSite = prog.Write(MTX_PUSH, 0);
            prog.Write(POLYGON_ATTR, Attr);
            prog.Write(TEXIMAGE_PARAM, TexParamNormal);
            u32 loop = prog.LoopStart(3);
            prog.Write(BEGIN_VTXS, 0);
            u32 lastVertex = 0;
            prog.Ldr(0, VTX_16);
            for (auto& v : Vertices)
            {
                prog.Ldr(1, v[0]); prog.Str(1, 0);
                prog.Ldr(2, v[1]); lastVertex = prog.Str(2, 0);
            }
            prog.Write(END_VTXS, 0);
            prog.LoopEnd(loop);
            prog.Write(MTX_POP, 1);
            prog.Write(SWAP_BUFFERS, 0);
            prog.Loop();
            prog.Place(*nds);
            Run(*nds, 0x02000000, 3);

            std::string report = nds->Inspector.Report();
            check(SitePolygons(report, lastVertex) == 3, mode + ": all 3 polygons (interpreted twice, then compiled) belong to the store of their last vertex word (" + Hex(lastVertex) + ")");
            check(report.find("id  5: 3 polygons") != std::string::npos, mode + ": its polygon ID is counted");
            check(report.find("[" + Hex(pushSite) + "]\n    MTX_PUSH") != std::string::npos, mode + ": the trace has MTX_PUSH at its call site");
            check(report.find("MTX_POP        00000001") != std::string::npos, mode + ": and MTX_POP with its parameter");
            char tex[96];
            snprintf(tex, sizeof(tex), "%-22s %08X  %-11s %6u %8u %7u %9u", Hex(lastVertex).c_str(), 0x2000u, "16x16 f7", 0u, 0u, 3u, 0u);
            check(report.find(tex) != std::string::npos, mode + ": its texture, generated from normals (a fake reflection)");
            if (!ok) printf("%s\n", report.c_str());
        }

        // 2. a command DMA: the polygon belongs to the DMA's start, with its display list
        {
            auto nds = MakeNDS(jit);
            nds->Inspector.SetEnabled(true);
            nds->Inspector.SetView(Inspector::View::DisplayList);
            // packed commands: BEGIN_VTXS, then 3 x VTX_16, then END_VTXS
            std::vector<u32> list = {0x23232340, 0};
            for (auto& v : Vertices) { list.push_back(v[0]); list.push_back(v[1]); }
            list.push_back(0x00000041);
            const u32 listAddr = 0x02010000;
            memcpy(&nds->MainRAM[listAddr & nds->MainRAMMask], list.data(), list.size() * 4);
            u32 hash = XXH32(list.data(), list.size() * 4, 0);

            Program prog(0x02000000);
            prog.Write(POWCNT1, 0x820F);
            prog.Write(POLYGON_ATTR, Attr);
            prog.Write(DMA0SAD, listAddr);
            prog.Write(DMA0DAD, 0x04000400);
            u32 loop = prog.LoopStart(3);
            // 32-bit, destination fixed, start now
            u32 dmaSite = prog.Write(DMA0CNT, 0x84400000 | (u32)list.size());
            prog.LoopEnd(loop);
            prog.Write(SWAP_BUFFERS, 0);
            prog.Loop();
            prog.Place(*nds);
            Run(*nds, 0x02000000, 3);

            std::string report = nds->Inspector.Report();
            check(SitePolygons(report, dmaSite) == 3, mode + ": all 3 DMAs' polygons belong to the store that started it (" + Hex(dmaSite) + ")");
            char line[96];
            snprintf(line, sizeof(line), "%08X  %08X  %7u %9u  %08X", hash, listAddr, (u32)list.size(), 3u, dmaSite);
            check(report.find(line) != std::string::npos, mode + ": its display list: hash of its content, address, length, polygons, started by");
            // objects: the list drawn 3 times, as 3 draws ranked 0 to 2
            for (u32 rank = 0; rank < 3; rank++)
            {
                snprintf(line, sizeof(line), "%-6u %-7u list %08X          %-6u %-9u", rank + 1, 1u, hash, rank, 1u);
                check(report.find(line) != std::string::npos, mode + ": object " + std::to_string(rank + 1) + ": the list's draw of rank " + std::to_string(rank));
            }
            if (!ok) printf("%s\n", report.c_str());
        }
    }

    // 2b. a library-like routine starting the DMA, called from two places:
    // the call site is the routine, "called from" names each caller
    for (int jit = 0; jit < 2; jit++)
    {
        const std::string mode = jit ? "JIT" : "interpreter";
        auto nds = MakeNDS(jit);
        nds->Inspector.SetEnabled(true);
        std::vector<u32> list = {0x23232340, 0};
        for (auto& v : Vertices) { list.push_back(v[0]); list.push_back(v[1]); }
        list.push_back(0x00000041);
        memcpy(&nds->MainRAM[0x02010000 & nds->MainRAMMask], list.data(), list.size() * 4);

        Program prog(0x02000000);
        size_t toMain = prog.BForward();
        // the routine: starts the DMA, returns
        u32 routine = prog.Here();
        prog.Write(DMA0SAD, 0x02010000);
        prog.Write(DMA0DAD, 0x04000400);
        u32 dmaSite = prog.Write(DMA0CNT, 0x84400000 | (u32)list.size());
        prog.BxLr();
        prog.PatchB(toMain, prog.Here());
        prog.Write(POWCNT1, 0x820F);
        prog.Write(POLYGON_ATTR, Attr);
        u32 loop = prog.LoopStart(3);
        u32 call1 = prog.Bl(routine);
        prog.LoopEnd(loop);
        u32 call2 = prog.Bl(routine);
        prog.Write(SWAP_BUFFERS, 0);
        prog.Loop();
        prog.Place(*nds);
        Run(*nds, 0x02000000, 3);

        std::string report = nds->Inspector.Report();
        check(SitePolygons(report, dmaSite) == 4, mode + ": the routine's DMA start is the call site of all 4 polygons");
        check(report.find("called from " + Hex(call1 + 4) + ": 3 polygons") != std::string::npos, mode + ": 3 called from the loop (" + Hex(call1 + 4) + ")");
        check(report.find("called from " + Hex(call2 + 4) + ": 1 polygons") != std::string::npos, mode + ": 1 from the other call (" + Hex(call2 + 4) + ")");
        if (!ok) printf("%s\n", report.c_str());
    }

    // 3. off: nothing recorded, and polygons carry no source
    {
        auto nds = MakeNDS(true);
        Program prog(0x02000000);
        prog.Write(POWCNT1, 0x820F);
        prog.Write(POLYGON_ATTR, Attr);
        prog.Write(BEGIN_VTXS, 0);
        prog.Ldr(0, VTX_16);
        for (auto& v : Vertices) { prog.Ldr(1, v[0]); prog.Str(1, 0); prog.Ldr(2, v[1]); prog.Str(2, 0); }
        prog.Write(SWAP_BUFFERS, 0);
        prog.Loop();
        prog.Place(*nds);
        Run(*nds, 0x02000000, 3);
        check(nds->ARM9.StorePC == 0, "off: the JIT writes no store sites");
        check(nds->Inspector.Report().find("== Polygons: 0 ==") != std::string::npos, "off: no polygons recorded");
        check(nds->GPU.GPU3D.GetRenderNumPolygons() == 1 && nds->GPU.GPU3D.GetRenderPolygons()[0]->CallSite == 0,
              "off: the polygon is drawn, with no call site");
    }

    // 4b. scrolling: the texture matrix's translation moving from frame to frame
    {
        auto nds = MakeNDS(false);
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        Polygon poly {};
        poly.TexParam = (1u << 30) | (7u << 26) | 0x400;
        for (s32 x : {0, 64, 128})
        {
            nds->Inspector.OnPolygon(poly, 0, x, -32);
            nds->Inspector.OnFlush();
        }
        std::string report = nds->Inspector.Report();
        check(report.find("+64, +0") != std::string::npos, "a texture whose matrix moves 64 per frame scrolls");
        nds->Inspector.OnPolygon(poly, 0, 128, -32);
        nds->Inspector.OnFlush();
        report = nds->Inspector.Report();
        size_t at = report.find("== Textures by call site (1) ==");
        size_t end = report.find("\n== ", at + 1);
        check(at != std::string::npos && report.substr(at, end - at).find("+") == std::string::npos, "still: no scrolling");
    }

    // 4d. RAM-map discovery: an object whose position (x, y, z words in main
    // RAM) is the translation the game draws it with, moving each frame; a
    // translation found nowhere in RAM, and an origin (everywhere), give nothing
    {
        auto nds = MakeNDS(false);
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        for (s32 x : {0x1000, 0x1800, 0x2000})
        {
            const s32 pos[3] = {x, -0x345, 0x6789};
            memcpy(&nds->MainRAM[0x100010], pos, sizeof(pos));
            for (s32 v : pos) nds->Inspector.OnCommand(0x1C, (u32)v, 0);
            for (s32 v : {0x7123, 0x7456, 0x7789}) nds->Inspector.OnCommand(0x1C, (u32)v, 0);
            for (int i = 0; i < 3; i++) nds->Inspector.OnCommand(0x1C, 0, 0);
            nds->Inspector.OnFlush();
        }
        std::string report = nds->Inspector.Report();
        check(report.find("02100010   3       2      ") != std::string::npos, "RAM map: the object's position field, found 3 frames, moved twice");
        check(report.find("== Position fields in main RAM (1) ==") != std::string::npos, "RAM map: nothing else");
    }

    // 4e. objects: a draw keeps its id from frame to frame, with the move of
    // its position matrix's translation
    {
        auto nds = MakeNDS(false);
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        Polygon poly {};
        s32 m[16] = {};
        for (s32 x : {0x1000, 0x1800})
        {
            m[12] = x;
            nds->Inspector.OnPolygon(poly, 0, 0, 0, m);
            nds->Inspector.OnFlush();
        }
        std::string report = nds->Inspector.Report();
        check(report.find("1      2       site unknown           0      1          1.500, 0.000, 0.000 (+0.500, +0.000, +0.000)") != std::string::npos,
              "objects: the same draw keeps id 1 over 2 frames, moved by 0.5");
    }

    // 4f. materials: direct-colour 16x16 textures in VRAM bank A, classified
    // from their statistics and the evidence of the polygons drawing them
    {
        auto nds = MakeNDS(false);
        u32 seed = 12345;
        auto noise = [&]() { seed = seed * 1103515245 + 12345; return (int)((seed >> 16) & 0x7FFF) % 9 - 4; };
        auto rgb = [](int r, int g, int b) {
            auto c = [](int v) { return (u16)std::clamp(v, 0, 31); };
            return (u16)(0x8000 | c(r) | (c(g) << 5) | (c(b) << 10));
        };
        u32 slot = 0;
        // a texture from a function of (x, y), 0 = transparent
        auto texture = [&](auto texel) {
            const u32 addr = slot++ * 512;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    u16 t = texel(x, y);
                    memcpy(&nds->GPU.VRAM_A[addr + (y * 16 + x) * 2], &t, 2);
                }
            return (7u << 26) | (1u << 20) | (1u << 23) | (addr >> 3);
        };
        TextureEvidence none, scrolling, translucent, selfLit;
        selfLit.Polygons = 4; selfLit.Lit = 4; selfLit.Emission = 31;
        TextureEvidence ownId, sceneryId;
        ownId.PolygonIds = 1ull << 6; ownId.SceneryId = 63;
        sceneryId.PolygonIds = 1ull << 63; sceneryId.SceneryId = 63;
        scrolling.Scrolling = true;
        translucent.Polygons = 4; translucent.Translucent = 4;
        struct Case { const char* what; u32 param; TextureEvidence e; MaterialClass want; };
        const Case cases[] = {
            {"green leaves with holes", texture([&](int x, int y) { return (x * 7 + y * 3) % 10 < 3 ? (u16)0 : rgb(6 + noise(), 20 + 2 * noise(), 5 + noise()); }), none, MaterialClass::Foliage},
            {"deep blue water, scrolling", texture([&](int, int) { return rgb(2, 10 + noise() / 2, 24 + noise() / 2); }), scrolling, MaterialClass::Water},
            {"deep blue water, translucent", texture([&](int, int) { return rgb(2, 10 + noise() / 2, 24 + noise() / 2); }), translucent, MaterialClass::Water},
            {"grey stone (light and dark spots of one colour)", texture([&](int, int) { int n = 2 * noise(); return rgb(16 + n, 16 + n, 15 + n); }), none, MaterialClass::Stone},
            {"brown planks, grain along x", texture([&](int, int y) { int v = (y % 4 == 0) ? -5 : 0; return rgb(18 + v, 10 + v, 3); }), none, MaterialClass::Wood},
            {"bright sky", texture([&](int, int y) { return rgb(12 + y / 4, 20 + y / 4, 31); }), none, MaterialClass::Sky},
            {"dim blue sky, self-lit (full emission)", texture([&](int, int y) { return rgb(8 + y / 4, 14 + y / 4, 22); }), selfLit, MaterialClass::Sky},
            {"the same, not self-lit: deep blue water", texture([&](int, int y) { return rgb(8 + y / 4, 14 + y / 4, 22); }), none, MaterialClass::Water},
            {"a character's atlas, its own polygon ID", texture([&](int x, int y) { return rgb((x * 5) % 32, (y * 7) % 32, ((x + y) * 3) % 32); }), ownId, MaterialClass::Character},
            {"the same atlas in the scenery's ID", texture([&](int x, int y) { return rgb((x * 5) % 32, (y * 7) % 32, ((x + y) * 3) % 32); }), sceneryId, MaterialClass::Unknown},
            {"one-colour grey noise, its own ID: stone, not a character", texture([&](int, int) { int n = 2 * noise(); return rgb(16 + n, 16 + n, 15 + n); }), ownId, MaterialClass::Stone},
            {"red lava, scrolling", texture([&](int, int) { return rgb(30, 8 + noise(), 2); }), scrolling, MaterialClass::Lava},
            {"a character's atlas of many colours", texture([&](int x, int y) { return rgb((x * 5) % 32, (y * 7) % 32, ((x + y) * 3) % 32); }), none, MaterialClass::Unknown},
        };
        // the bank mapped for textures once filled (mapping marks it changed),
        // then, as a renderer does before drawing, the flat texture VRAM made current
        nds->GPU.MapVRAM_AB(0, 0x83);
        auto dirty = nds->GPU.VRAMDirty_Texture.DeriveState(nds->GPU.VRAMMap_Texture, nds->GPU);
        nds->GPU.MakeVRAMFlat_TextureCoherent(dirty);
        MaterialClassifier classifier;
        auto classify = [&](u32 param, TextureEvidence e) { e.Polygons = e.Polygons ? e.Polygons : 4; return classifier.Classify(nds->GPU, param, 0, e); };
        for (const Case& c : cases)
        {
            MaterialResult r = classify(c.param, c.e);
            check(r.Class == c.want, std::string("material: ") + c.what + ": " + MaterialClassifier::Name(r.Class) + " (" + std::to_string(r.Confidence).substr(0, 4) + ", " + r.Cues + ")");
        }

        // the manual manifest: a texture named by its hash takes the class
        // given, over the classifier's (the brown planks, wood, named stone)
        {
            const u32 planks = cases[4].param;
            char text[160];
            snprintf(text, sizeof(text), "# Pomegrade/Materials/TEST.txt\n%08X stone  # the pier\n\nzz water\n12345678 dragon\n", classifier.Statistics(nds->GPU, planks, 0).Hash);
            MaterialManifest manifest;
            std::string errors = MaterialClassifier::ParseManifest(text, manifest);
            check(manifest.size() == 1 && errors == "zz water\n12345678 dragon\n", "manifest: one entry read, 2 bad lines reported");
            classifier.SetManifest(manifest);
            MaterialResult r = classify(planks, none);
            check(r.Class == MaterialClass::Stone && r.Confidence == 1.0f && r.Cues == "manifest", "manifest: the named texture is stone (was wood)");
            check(classify(cases[3].param, none).Cues != "manifest", "manifest: the others are still classified");
            classifier.SetManifest({});
        }

        // palette-index segmentation: a 16-colour texture of three materials,
        // each painted with a ramp of three shades: skin (entries 1-3), red
        // cloth (4-6, dark to bright: one colour, shaded), grey metal (7-9)
        {
            const u16 palette[10] = {0, rgb(28, 19, 13), rgb(24, 15, 9), rgb(20, 11, 6),
                                     rgb(10, 0, 0), rgb(20, 1, 1), rgb(30, 2, 2),
                                     rgb(8, 8, 9), rgb(16, 16, 17), rgb(26, 26, 27)};
            memcpy(nds->GPU.VRAM_E, palette, sizeof(palette));
            nds->GPU.MapVRAM_E(4, 0x83);
            // 16x16, 4 bits a texel at 0x8000: skin 1/2, red 1/4, grey 1/4
            const u32 addr = 0x8000;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x += 2)
                {
                    auto index = [&](int px) { int shade = (px + y) % 3; return x < 8 ? 1 + shade : y < 8 ? 4 + shade : 7 + shade; };
                    nds->GPU.VRAM_A[addr + y * 8 + x / 2] = (u8)(index(x) | (index(x + 1) << 4));
                }
            const u32 param = (3u << 26) | (1u << 20) | (1u << 23) | (addr >> 3);
            auto regions = classifier.Segment(nds->GPU, param, 0);
            bool skin = false, red = false, grey = false;
            for (auto& r : regions)
            {
                std::vector<u8> ids(r.Indices);
                if (ids == std::vector<u8>{1, 2, 3}) skin = std::string(r.Label) == "skin" && r.Share > 0.49f && r.Share < 0.51f;
                if (ids == std::vector<u8>{4, 5, 6}) red = std::string(r.Label).empty();
                if (ids == std::vector<u8>{7, 8, 9}) grey = std::string(r.Label).empty();
            }
            check(regions.size() == 3 && skin && red && grey,
                  "segmentation: 3 regions, one per material and its shades; skin labelled, half the texels (" + std::to_string(regions.size()) + " regions)");
        }
    }

    // 4g. display captures: to bank B then mapped as a texture (render to
    // texture), to bank C then shown from VRAM (motion blur, fades)
    {
        auto nds = MakeNDS(false);
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        GPU2D::Unit& a = nds->GPU.GPU2D_A;
        for (int frame = 0; frame < 2; frame++)
        {
            a.CaptureCnt = (1u << 31) | (1u << 16) | (3u << 20);   // bank B, 256x192, source A
            nds->GPU.VRAMCNT[1] = 0x83;                             // bank B: texture image
            nds->Inspector.BeginFrame();
        }
        a.CaptureCnt = (1u << 31) | (2u << 16) | (1u << 29);       // bank C, source B
        nds->GPU.VRAMCNT[2] = 0x80;                                 // LCDC
        a.DispCnt = (2u << 16) | (2u << 18);                        // display mode 2: bank C
        nds->Inspector.BeginFrame();
        std::string report = nds->Inspector.Report();
        check(report.find("Bank B: 2 captures of the 3D or the screen (256x192, offset 0x0); after, used as a texture 3 frames") != std::string::npos &&
              report.find("    render to texture") != std::string::npos, "captures: bank B captured twice, then used as a texture (render to texture)");
        check(report.find("Bank C: 1 captures of VRAM or main memory") != std::string::npos && report.find("shown 1") != std::string::npos &&
              report.find("    the frame shown from VRAM") != std::string::npos, "captures: bank C shown from VRAM");
        if (!ok) printf("%s\n", report.substr(report.find("== Display captures")).substr(0, 600).c_str());
    }

    // 4h. skeleton recovery from a command trace, the NitroSDK way: a model
    // starts with the projection loaded; joints are stored after their
    // parent's matrix is restored (or chained); a scratch slot may hold two
    // joints in turn; vertices go to the joint restored last (model scale
    // after it); a billboard joint is loaded from the joint restored last
    {
        std::vector<GeometryCommand> t;
        auto cmd = [&](u8 c, u32 p = 0, int params = 1) { for (int i = 0; i < params; i++) t.push_back({c, p, 0x02001000}); };
        auto vertices = [&](int n) { for (int i = 0; i < n; i++) cmd(0x23, 0, 2); };
        for (int model = 0; model < 2; model++)
        {
            cmd(0x10, 0); cmd(0x16, 0, 16);           // projection
            cmd(0x10, 2); cmd(0x17, 0, 12);           // camera
            cmd(0x19, 0, 12); cmd(0x13, 0);           // root, slot 0              joint 0
            cmd(0x14, 0); cmd(0x19, 0, 12); cmd(0x13, 1);   // hips, slot 1         joint 1
            cmd(0x19, 0, 12); cmd(0x13, 2);           // spine, chained, slot 2    joint 2
            cmd(0x14, 1); cmd(0x19, 0, 12); cmd(0x13, 0);   // leg in scratch slot 0 joint 3
            cmd(0x14, 2); cmd(0x10, 1); cmd(0x17, 0, 12);   // billboard from spine
            cmd(0x10, 2); cmd(0x13, 3);                      //                    joint 4
            // a skinned polygon: 2 vertices on the spine, 1 on the leg
            cmd(0x40, 0);
            cmd(0x14, 2); cmd(0x1B, 0, 3); vertices(2);
            cmd(0x14, 0); cmd(0x1B, 0, 3); vertices(1);
            cmd(0x41, 0);
        }
        const std::vector<Skeleton> sk = RecoverSkeletons(t);
        bool shape = sk.size() == 2;
        for (const Skeleton& s : sk)
        {
            const auto& j = s.Joints;
            shape = shape && j.size() == 5 && j[0].Parent == -1 && j[1].Parent == 0 && j[2].Parent == 1 && j[3].Parent == 1 &&
                    j[3].Slot == 0 && j[4].Parent == 2 && j[2].Vertices == 2 && j[3].Vertices == 1 && s.Depth() == 4;
        }
        if (!shape) for (const Skeleton& s : sk) printf("%s", SkeletonTree(s).c_str());
        check(shape, "skeletons: 2 models (split at the projection load), 5 joints each: chain, scratch slot, billboard, skinned vertices");
    }

    // 4c. palette indices    // 4c. palette indices    // 4c. palette indices of a paletted texture: 8x8, 16 colours (4 bits per
    // texel), 48 texels on index 1, 16 on index 5
    {
        auto nds = MakeNDS(false);
        nds->GPU.MapVRAM_AB(0, 0x83); // bank A: texture slot 0
        u8* vram = nds->GPU.VRAM_A;
        for (int i = 0; i < 64; i += 2)
        {
            u8 lo = i < 48 ? 1 : 5, hi = (i + 1) < 48 ? 1 : 5;
            vram[i / 2] = lo | (hi << 4);
        }
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        Polygon poly {};
        poly.TexParam = (3u << 26); // 16 colours, 8x8, address 0
        nds->Inspector.OnPolygon(poly, 0, 0, 0);
        nds->Inspector.OnFlush();
        std::string report = nds->Inspector.Report();
        check(report.find("00000000  8x8 f3  2 of 16 indices: 1:75% 5:25%") != std::string::npos, "palette indices: 2 of 16 used, 75% and 25%");
        if (!ok) printf("%s\n", report.c_str());
    }

    // 4. cartridge reads named after NitroFS files
    {
        auto nds = MakeNDS(false);
        nds->SetNDSCart(CartWithFiles());
        nds->Inspector.SetEnabled(true);
        nds->Inspector.BeginFrame();
        nds->Inspector.OnCartRead(0x10200, 0x200);
        nds->Inspector.OnCartRead(0x10400, 0x200); // continues: one entry
        nds->Inspector.OnCartRead(0x20000, 0x100);
        nds->Inspector.OnCartRead(0x8000, 0x200);
        nds->Inspector.OnCartRead(0x0, 0x200);
        std::string report = nds->Inspector.Report();
        check(report.find("0x00000200 0x00000400  data/a.bin") != std::string::npos, "reads: offset in the file, continuous reads merged");
        check(report.find("data/sub/b.bin") != std::string::npos, "reads: a file in a sub-directory");
        check(report.find("(overlay ARM9 7)") != std::string::npos, "reads: an overlay");
        check(report.find("(no file: header") != std::string::npos, "reads: outside any file");
        check(report.find("(4,") == std::string::npos && report.find("reads since the inspector was turned on (4)") != std::string::npos, "reads: 4 entries");
        check(report.find("Game code: TEST") != std::string::npos, "game code");
        if (!ok) printf("%s\n", report.c_str());
    }

    // 5. the renderer's view: a polygon flat, in its call site's colour, untextured
    if (!InitEGL()) { puts("EGL init failed"); return 1; }
    {
        NDSArgs args; args.JIT = std::nullopt;
        auto nds = std::make_unique<NDS>(std::move(args));
        GPU& gpu = nds->GPU;
        const u32 site = 0x02001238;
        SetupTexturedQuad(gpu, site);
        auto r = GLRenderer::New();
        if (!r) { puts("GLRenderer::New failed"); return 1; }
        r->SetRenderSettings(false, 1);
        while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }
        auto render = [&]() {
            r->RenderFrame(gpu);
            r->PrepareCaptureFrame();
            std::vector<u32> out(256*192);
            for (int y = 0; y < 192; y++) memcpy(&out[y*256], r->GetLine(y), 256*4);
            return out;
        };
        auto channels = [](u32 c) { return std::array<u32, 3>{c & 0x3F, (c >> 8) & 0x3F, (c >> 16) & 0x3F}; };

        auto normal = render();
        check(normal[3*256+4] != normal[150*256+200], "view off: the texture is drawn");

        nds->Inspector.SetEnabled(true);
        nds->Inspector.SetView(Inspector::View::CallSite);
        nds->Inspector.BeginFrame();
        auto coloured = render();
        u32 c = nds->Inspector.ViewColour(Quad);
        std::array<u32, 3> want = {(c & 0x1F) * 2, ((c >> 5) & 0x1F) * 2, ((c >> 10) & 0x1F) * 2};
        bool flat = coloured[3*256+4] == coloured[150*256+200];
        auto got = channels(coloured[100*256+128]);
        bool near = true;
        for (int i = 0; i < 3; i++) near = near && (got[i] + 1 >= want[i] && got[i] <= want[i] + 1);
        printf("view colour: want %u %u %u, got %u %u %u\n", want[0], want[1], want[2], got[0], got[1], got[2]);
        check(flat && near, "call site view: flat, in the call site's colour");

        Quad.CallSite = 0x02004444;
        auto other = render();
        check(other[100*256+128] != coloured[100*256+128], "another call site: another colour");

        nds->Inspector.SetView(Inspector::View::Off);
        auto back = render();
        check(back == normal, "view back to off: the original picture");

    }

    // 6. parity oracle: once a second while recording, the frame a hardware
    // renderer drew against the software renderer's
    {
        auto nds = MakeNDS(false);
        GPU& gpu = nds->GPU;
        nds->Inspector.SetEnabled(true);
        // textured, modulated by the vertex colour: a direct-colour 8x8
        // texture of one texel (5, 16, 27) in VRAM bank A, vertex colour
        // (31, 20, 9). OpenGL blended these in floats, up to a level darker
        nds->GPU.MapVRAM_AB(0, 0x83);
        for (int i = 0; i < 64; i++)
        {
            const u16 texel = 0x8000 | 5 | (16 << 5) | (27 << 10);
            memcpy(&nds->GPU.VRAM_A[i * 2], &texel, 2);
        }
        Program prog(0x02000000);
        prog.Write(POWCNT1, 0x820F);
        prog.Write(0x04000060, 1); // DISP3DCNT: textures on
        prog.Write(TEXIMAGE_PARAM, 7u << 26);
        prog.Write(0x04000480, 31 | (20 << 5) | (9 << 10)); // COLOR
        prog.Write(0x04000580, 0xBFFF0000); // VIEWPORT: the whole screen
        prog.Write(POLYGON_ATTR, Attr);
        prog.Write(BEGIN_VTXS, 0);
        for (auto& v : Vertices) { prog.Write(VTX_16, v[0]); prog.Write(VTX_16, v[1]); }
        prog.Write(END_VTXS, 0);
        prog.Write(SWAP_BUFFERS, 0);
        prog.Loop();
        prog.Place(*nds);
        Run(*nds, 0x02000000, 3);

        auto r = GLRenderer::New();
        if (!r) { puts("GLRenderer::New failed"); return 1; }
        r->SetRenderSettings(false, 4);
        while (r->NeedsShaderCompile()) { int cur, cnt; r->ShaderCompileStep(cur, cnt); }
        struct Blank : Renderer3D
        {
            u32 Line[256] {};
            Blank() : Renderer3D(true) {}
            void Reset(GPU&) override {}
            void RenderFrame(GPU&) override {}
            u32* GetLine(int) override { return Line; }
        } blank;
        auto parity = [&](Renderer3D& renderer) {
            for (int i = 0; i < 60; i++) nds->Inspector.BeginFrame();
            renderer.RenderFrame(gpu);
            nds->Inspector.OnRendered(gpu, renderer);
            std::string report = nds->Inspector.Report();
            size_t at = report.find("== Parity");
            return report.substr(at, report.find("\n== ", at + 1) - at);
        };
        auto differing = [](const std::string& section) {
            size_t at = section.find("Last: ");
            return at == std::string::npos ? -1 : atoi(section.c_str() + at + 6);
        };
        // the triangle: 1568 pixels. OpenGL fills polygon edges by its own
        // rule, not the DS's: a few edge pixels differ, the rest must not
        std::string same = parity(*r);
        printf("%s", same.c_str());
        int n = differing(same);
        check(same.find("1 checks.") != std::string::npos && n >= 0 && n <= 1568 / 20 && same.find("largest colour difference 0/63") != std::string::npos,
              "parity: OpenGL (x4) draws the textured triangle in the software renderer's colours, but for edge pixels (" + std::to_string(n) + ")");
        // exact: the oracle's tolerance (2 levels) would hide a level's error
        {
            SoftRenderer sr;
            gpu.GPU3D.RenderFrameIdentical = false;
            sr.RenderFrame(gpu);
            r->RenderFrame(gpu);
            r->PrepareCaptureFrame();
            int both = 0, exact = 0;
            for (int y = 0; y < 192; y++)
            {
                const u32* h = r->GetLine(y);
                const u32* d = sr.GetLine(y);
                for (int x = 0; x < 256; x++)
                    if ((h[x] >> 24) && (d[x] >> 24)) { both++; if ((h[x] & 0x3F3F3F) == (d[x] & 0x3F3F3F)) exact++; }
            }
            check(both > 1400 && exact == both, "textured triangle: every pixel in exactly the DS colour (" + std::to_string(exact) + " of " + std::to_string(both) + ")");
        }
        std::string none = parity(blank);
        printf("%s", none.c_str());
        check(none.find("2 checks.") != std::string::npos && differing(none) >= 1500 && none.find("....9") != std::string::npos,
              "parity: a renderer that draws nothing is caught, in the triangle's tiles");
    }

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
