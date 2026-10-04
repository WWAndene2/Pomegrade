#ifndef NDS_INSPECTOR_H
#define NDS_INSPECTOR_H

// Inspector (Pomegrade): shows how a game draws, the first step of the DS
// engine remake (DS_ENGINE_REMAKE.md, 5.2 and 5.12 step 1). While it is on:
// - every 3D command carries its source through the geometry FIFO: the ARM9
//   instruction that wrote it (call site), or, for a command DMA, the
//   instruction that started the DMA and the display list it copies (address,
//   length, hash of its content);
// - each polygon keeps the call site and display list it was drawn from, and
//   the renderer can colour polygons by polygon ID, call site or display list;
// - the last frame's polygons per call site and per display list, its polygon
//   IDs and its 3D command trace (matrix stack operations included) are kept,
//   as is a log of the cartridge files the game reads (NitroFS names);
// - per call site and texture: how texture coordinates are generated (none,
//   from texcoords, normals: a fake reflection, positions: a projected
//   texture) and whether the texture matrix moves from frame to frame (a
//   scrolling texture: water, lava, conveyors) (DS_ENGINE_REMAKE.md 5.3);
// - Report() writes all of it as text.
// Off by default; off, it costs nothing (the JIT emits no extra code).

#include "types.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace melonDS
{
class NDS;
class DMA;
struct Polygon;

class Inspector
{
public:
    enum class View : int { Off = 0, PolygonId = 1, CallSite = 2, DisplayList = 3 };

    explicit Inspector(melonDS::NDS& nds) noexcept : NDS(nds) {}

    // any thread; applied at the start of the next frame
    void SetEnabled(bool enabled) noexcept { RequestedEnabled = enabled; }
    void SetView(View view) noexcept { CurrentView = (int)view; }
    [[nodiscard]] View GetView() const noexcept { return Enabled ? (View)CurrentView.load() : View::Off; }
    [[nodiscard]] bool IsEnabled() const noexcept { return Enabled; }

    // emulator thread, once per frame before it runs: applies SetEnabled
    void BeginFrame() noexcept;
    // a cartridge was inserted or removed: reads its file tables
    void OnCartChanged() noexcept;

    // R[15] of the ARM9 instruction writing to hardware right now (bit 0:
    // Thumb), or 0 when unknown
    [[nodiscard]] u32 CpuSite() const noexcept;
    // the source tag of a 3D command written now (0: none)
    [[nodiscard]] u16 CommandSource() noexcept;
    // a DMA was enabled by the CPU: records who started it and, for the
    // geometry FIFO, the display list it copies
    void OnDmaEnabled(DMA& dma, u32 srcAddr, u32 dstAddr, u32 count) noexcept;
    // the geometry engine runs a command
    void OnCommand(u8 command, u32 param, u16 source) noexcept;
    // a polygon is made from the command whose source was given, with the
    // texture matrix's translation (TexMatrix[12], [13]) in effect
    void OnPolygon(Polygon& poly, u16 source, s32 texX, s32 texY) noexcept;
    // the polygon lists were handed to the renderer: this frame's tables
    // become the last frame's
    void OnFlush() noexcept;
    // the cartridge is read (command B7)
    void OnCartRead(u32 addr, u32 len) noexcept;

    // the colour a polygon gets in the current view (5 bits per channel)
    [[nodiscard]] u32 ViewColour(const Polygon& poly) const noexcept;

    // any thread
    [[nodiscard]] std::string Report() const;

    // a call site as an instruction address (bit 0 still says Thumb)
    static u32 InstructionAddress(u32 site) noexcept { return site & 1 ? (site & ~1u) - 4 + 1 : site - 8; }

private:
    struct Source
    {
        u32 Site = 0;      // CPU store or DMA start, R[15] form
        u32 ListAddr = 0;  // display list (DMA source), 0 for CPU writes
        u32 ListWords = 0;
        u32 ListHash = 0;
    };
    struct SiteStats { u32 Polygons = 0; u32 Commands = 0; std::map<u32, u32> Lists; };
    struct ListStats { u32 Addr = 0; u32 Words = 0; u32 Site = 0; u32 Polygons = 0; };
    struct TraceEntry { u8 Command; u32 Param; u32 Site; };
    struct FileRead { u32 Frame; u32 File; u32 Offset; u32 Length; };
    // a texture as one call site uses it
    struct TexUse
    {
        u32 Polygons[4] = {}; // by texgen mode: none, texcoord, normal, position
        s32 TexX = 0, TexY = 0;   // texture matrix translation (last polygon)
        bool Moved = false;       // translation changed since the previous frame
        s32 DeltaX = 0, DeltaY = 0;
    };
    struct FrameTables
    {
        std::map<u32, SiteStats> Sites;
        std::map<u32, ListStats> Lists;     // by hash
        u32 PolygonIds[64] = {};
        u32 Polygons = 0;
        std::vector<TraceEntry> Trace;
        bool TraceTruncated = false;
        std::map<u64, TexUse> Textures;     // (site << 32) | texture parameters
    };
    struct CartFile { u32 Start, End, Id; };

    static constexpr int SourceRing = 1024; // > the commands the FIFOs can hold
    static constexpr size_t MaxTrace = 65536;
    static constexpr size_t MaxFileReads = 8192;

    melonDS::NDS& NDS;
    std::atomic<bool> RequestedEnabled = false;
    bool Enabled = false;
    std::atomic<int> CurrentView = 0;
    u32 FrameNumber = 0;

    Source Sources[SourceRing];
    u16 LastSource = 0;

    FrameTables Current;
    FrameTables Last;
    u32 LastFrameNumber = 0;
    // texture matrix translations of the previous frame, for scrolling
    std::map<u64, std::pair<s32, s32>> PrevTexTranslation;

    std::vector<CartFile> Files; // sorted by start
    std::vector<std::string> FileNames; // by file id
    std::string GameCode;
    std::vector<FileRead> FileReads;
    bool FileReadsTruncated = false;

    mutable std::mutex Lock; // Last, FileReads, Files, FileNames

    u16 NewSource(const Source& source) noexcept;
    [[nodiscard]] std::string FileName(u32 id) const;
};

}

#endif // NDS_INSPECTOR_H
