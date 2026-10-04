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
// - RAM-map discovery (5.2): the translations the game gives its matrices
//   (MTX_TRANS, and the last row of loaded/multiplied matrices) are searched
//   in main RAM as three consecutive words (x, y, z); the addresses found,
//   frame after frame, are likely the position fields of game objects;
// - objects (5.12 step 4): each draw (a display list, or a run of polygons
//   from one call site) is an object, identified across frames by what it
//   draws and its rank among the draws of the same thing; it keeps its id,
//   and its position (model-view) matrix translation this frame and the previous one;
// - per paletted texture: how many texels use each palette index (artists
//   paint each material with its own indices: eyes, trim, glow) (5.3);
// - with a hardware renderer, once a second the frame is checked against
//   the software renderer (GPU3D_ParityOracle.h, 5.12 step 6);
// - each texture drawn gets a material class (water, foliage, wood...) from
//   its render state and statistics (GPU3D_MaterialClassifier.h, step 7);
// - display captures (5.4): where the game captures the screen to, and how
//   it uses that VRAM bank after (as a texture, shown, as a background), and
//   the fog settings (5.5);
// - the joint trees of the last frame's skinned models, from its trace
//   (GPU3D_SkeletonRecovery.h, step 10);
// - the lights the frame set, from its trace (GPU3D_LightRecovery.h, step 11);
// - Report() writes all of it as text.
// Off by default; off, it costs nothing (the JIT emits no extra code).

#include "types.h"
#include "GPU3D_ParityOracle.h"
#include "GPU3D_MaterialClassifier.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace melonDS
{
class NDS;
class GPU;
class Renderer3D;
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
    // R[14] (LR) at that store: the return address into the function's caller
    [[nodiscard]] u32 CpuCaller() const noexcept;
    // the source tag of a 3D command written now (0: none)
    [[nodiscard]] u16 CommandSource() noexcept;
    // a DMA was enabled by the CPU: records who started it and, for the
    // geometry FIFO, the display list it copies
    void OnDmaEnabled(DMA& dma, u32 srcAddr, u32 dstAddr, u32 count) noexcept;
    // the geometry engine runs a command
    void OnCommand(u8 command, u32 param, u16 source) noexcept;
    // a polygon is made from the command whose source was given, with the
    // texture matrix's translation (TexMatrix[12], [13]) and the position
    // matrix (PosMatrix, 4x4 20.12; null: unknown) in effect
    void OnPolygon(Polygon& poly, u16 source, s32 texX, s32 texY, const s32* posMatrix = nullptr) noexcept;
    // the polygon lists were handed to the renderer: this frame's tables
    // become the last frame's
    void OnFlush() noexcept;
    // the renderer drew this frame's 3D (emulator thread, its context current)
    void OnRendered(GPU& gpu, Renderer3D& renderer);
    // the cartridge is read (command B7)
    void OnCartRead(u32 addr, u32 len) noexcept;

    // the colour a polygon gets in the current view (5 bits per channel)
    [[nodiscard]] u32 ViewColour(const Polygon& poly) const noexcept;

    // any thread
    [[nodiscard]] std::string Report() const;
    // the material manifest's text (GPU3D_MaterialClassifier.h); its
    // unreadable lines are listed in the report
    void SetMaterialManifest(const std::string& text);
    // the inserted cartridge's game code ("" without one): names its manifest
    [[nodiscard]] std::string CartGameCode() const;

    // a call site as an instruction address (bit 0 still says Thumb)
    static u32 InstructionAddress(u32 site) noexcept { return site & 1 ? (site & ~1u) - 4 + 1 : site - 8; }

private:
    struct Source
    {
        u32 Site = 0;      // CPU store or DMA start, R[15] form
        u32 Caller = 0;    // LR at that store (return address, bit 0: Thumb)
        u32 ListAddr = 0;  // display list (DMA source), 0 for CPU writes
        u32 ListWords = 0;
        u32 ListHash = 0;
        u32 Draw = 0;      // DMA start count: each DMA is a draw of its own (objects)
    };
    struct SiteStats { u32 Polygons = 0; u32 Commands = 0; std::map<u32, u32> Lists; std::map<u32, u32> Callers; };
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
        u32 Palette = 0;          // TexPalette (last polygon)
        u32 Translucent = 0;      // polygons with alpha 1-30
        u32 Lit = 0;              // polygons with a light enabled
        u8 Specular = 0, Emission = 0; // largest material channel (0-31) of lit polygons
        bool Shininess = false;   // the shininess table used
        u64 PolygonIds = 0;       // bit n: polygon ID n drawn with it
    };
    // one draw of an object in a frame
    struct ObjectDraw
    {
        u64 Key = 0;          // display list hash, or call site | 1 << 32
        u32 Rank = 0;         // among this frame's draws with the same key
        u32 Id = 0;           // the same from frame to frame
        u32 Frames = 0;       // consecutive frames it was drawn
        u32 Polygons = 0;
        s32 Translation[3] = {};
        s32 PrevTranslation[3] = {};
        bool HasPrev = false;
    };
    struct ObjectHistory { u32 Id; u32 Frames; s32 Translation[3]; };
    struct FrameTables
    {
        std::map<u32, SiteStats> Sites;
        std::map<u32, ListStats> Lists;     // by hash
        u32 PolygonIds[64] = {};
        u32 LitPolygons[4] = {}; // polygons with each light enabled
        u32 Polygons = 0;
        std::vector<TraceEntry> Trace;
        bool TraceTruncated = false;
        std::map<u64, TexUse> Textures;     // (site << 32) | texture parameters
        // palette-index histograms of the paletted textures drawn, by texture
        // parameters (address, size, format); texel counts per index
        std::map<u32, std::vector<u32>> PaletteHistograms;
        std::vector<ObjectDraw> Objects;
        // material of each texture drawn, by (palette << 32) | parameters
        struct Material { MaterialResult Result; MaterialClassifier::TextureStats Stats; TextureEvidence Evidence; std::vector<MaterialClassifier::Region> Regions; };
        std::map<u64, Material> Materials;
        std::map<u64, TextureEvidence> MaterialEvidence; // same keys
    };
    struct CartFile { u32 Start, End, Id; };
    // a VRAM bank (A-D) the game captures the screen to
    struct CaptureBank
    {
        u32 Captures = 0;      // frames captured to it
        u32 LastCnt = 0;       // DISPCAPCNT of the last capture
        u32 AsTexture = 0;     // frames mapped as texture image after a capture
        u32 Displayed = 0;     // frames shown from VRAM (display mode 2)
        u32 AsBackground = 0;  // frames mapped as a background
    };
    // a main RAM address holding (x, y, z) words a matrix was translated by
    struct PositionField
    {
        u32 FramesFound = 0;  // frames its value matched a translation
        u32 LastFrame = 0;    // the last of them
        u32 Changes = 0;      // matches with a value different from the last
        s32 Value[3] = {};
    };

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

    std::map<u32, PositionField> PositionFields; // by address
    // objects of the previous frame by (key, rank), and the next new id
    std::map<std::pair<u64, u32>, ObjectHistory> PrevObjects;
    std::map<u64, u32> ObjectRanks; // this frame: draws per key so far
    u32 CurrentDraw = 0;            // Source::Draw of the object being drawn
    u32 DmaDraws = 0;
    u32 NextObjectId = 1;

    ParityOracle Parity; // under Lock
    MaterialClassifier Classifier; // under Lock
    CaptureBank CaptureBanks[4];   // under Lock
    // this frame's display capture and VRAM use
    void RecordCapture() noexcept;
    std::string ManifestErrors;
    u32 LastParityFrame = 0;
    static constexpr u32 ParityInterval = 60;
    static constexpr size_t MaxObjects = 4096;
    static constexpr size_t MaxPositionFields = 4096;

    std::vector<CartFile> Files; // sorted by start
    std::vector<std::string> FileNames; // by file id
    std::string GameCode;
    std::vector<FileRead> FileReads;
    bool FileReadsTruncated = false;

    mutable std::mutex Lock; // Last, FileReads, Files, FileNames

    u16 NewSource(const Source& source) noexcept;
    // this frame's matrix translations searched in main RAM
    void FindPositionFields();
    // texel counts per palette index of a paletted texture in VRAM (empty
    // for direct colour and compressed formats)
    std::vector<u32> PaletteHistogram(u32 texParam) const;
    [[nodiscard]] std::string FileName(u32 id) const;
};

}

#endif // NDS_INSPECTOR_H
