#include "NDS_Inspector.h"

#include "NDS.h"
#include "NDSCart.h"
#include "DMA.h"
#include "GPU3D.h"
#include "xxhash/xxhash.h"

#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <cstring>

namespace melonDS
{

namespace
{
// source tags 1 to 4: the ARM9 DMAs (rewritten when each is started, so a
// long command DMA keeps its tag); the CPU's writes cycle through the rest
constexpr u16 FirstCpuSource = 5;

u32 Read32(const u8* rom, u32 length, u32 offset)
{
    if (offset + 4 > length) return 0;
    u32 v; memcpy(&v, rom + offset, 4); return v;
}

u16 Read16(const u8* rom, u32 length, u32 offset)
{
    if (offset + 2 > length) return 0;
    u16 v; memcpy(&v, rom + offset, 2); return v;
}

const char* CommandName(u8 cmd)
{
    switch (cmd)
    {
    case 0x10: return "MTX_MODE";
    case 0x11: return "MTX_PUSH";
    case 0x12: return "MTX_POP";
    case 0x13: return "MTX_STORE";
    case 0x14: return "MTX_RESTORE";
    case 0x15: return "MTX_IDENTITY";
    case 0x16: return "MTX_LOAD_4x4";
    case 0x17: return "MTX_LOAD_4x3";
    case 0x18: return "MTX_MULT_4x4";
    case 0x19: return "MTX_MULT_4x3";
    case 0x1A: return "MTX_MULT_3x3";
    case 0x1B: return "MTX_SCALE";
    case 0x1C: return "MTX_TRANS";
    case 0x20: return "COLOR";
    case 0x21: return "NORMAL";
    case 0x22: return "TEXCOORD";
    case 0x23: return "VTX_16";
    case 0x24: return "VTX_10";
    case 0x25: return "VTX_XY";
    case 0x26: return "VTX_XZ";
    case 0x27: return "VTX_YZ";
    case 0x28: return "VTX_DIFF";
    case 0x29: return "POLYGON_ATTR";
    case 0x2A: return "TEXIMAGE_PARAM";
    case 0x2B: return "PLTT_BASE";
    case 0x30: return "DIF_AMB";
    case 0x31: return "SPE_EMI";
    case 0x32: return "LIGHT_VECTOR";
    case 0x33: return "LIGHT_COLOR";
    case 0x34: return "SHININESS";
    case 0x40: return "BEGIN_VTXS";
    case 0x41: return "END_VTXS";
    case 0x50: return "SWAP_BUFFERS";
    case 0x60: return "VIEWPORT";
    case 0x70: return "BOX_TEST";
    case 0x71: return "POS_TEST";
    case 0x72: return "VEC_TEST";
    default: return nullptr;
    }
}

// printable ASCII as is, other bytes (Shift-JIS names...) as \xNN: the
// report stays plain text, valid for Java's string conversion
std::string Printable(const std::string& in)
{
    std::string out;
    for (unsigned char c : in)
    {
        if (c >= 0x20 && c < 0x7F) { out += (char)c; continue; }
        char b[8]; snprintf(b, sizeof(b), "\\x%02X", c); out += b;
    }
    return out;
}

// a well spread hash of a key, for colours
u32 Mix(u32 x)
{
    x ^= x >> 16; x *= 0x7FEB352D;
    x ^= x >> 15; x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}
}

void Inspector::BeginFrame() noexcept
{
    FrameNumber++;
    bool requested = RequestedEnabled;
    if (requested == Enabled) return;

    Enabled = requested;
    // JIT code writes ARM::StorePC only while this is on
    if (NDS.IsJITEnabled())
        NDS.JIT.SetTrackStoreSites(Enabled);

    Current = FrameTables();
    PrevTexTranslation.clear();
    PrevObjects.clear();
    ObjectRanks.clear();
    LastSource = 0;
    for (auto& s : Sources) s = Source();
    if (Enabled)
    {
        std::lock_guard<std::mutex> guard(Lock);
        Last = FrameTables();
        FileReads.clear();
        PositionFields.clear();
        Parity.Clear();
        FileReadsTruncated = false;
    }
    if (Enabled) OnCartChanged();
}

void Inspector::OnCartChanged() noexcept
{
    std::lock_guard<std::mutex> guard(Lock);
    Files.clear();
    FileNames.clear();
    GameCode.clear();

    const NDSCart::CartCommon* cart = NDS.GetNDSCart();
    if (!cart) return;
    const u8* rom = cart->GetROM();
    const u32 len = cart->GetROMLength();
    if (!rom || len < 0x200) return;

    GameCode.assign((const char*)rom + 0x0C, 4);
    const u32 fntOff = Read32(rom, len, 0x40), fntSize = Read32(rom, len, 0x44);
    const u32 fatOff = Read32(rom, len, 0x48), fatSize = Read32(rom, len, 0x4C);
    const u32 fileCount = std::min<u32>(fatSize / 8, 0x10000);

    FileNames.assign(fileCount, std::string());
    for (u32 id = 0; id < fileCount; id++)
    {
        u32 start = Read32(rom, len, fatOff + id * 8), end = Read32(rom, len, fatOff + id * 8 + 4);
        if (end > start) Files.push_back({start, end, id});
    }
    std::sort(Files.begin(), Files.end(), [](const CartFile& a, const CartFile& b) { return a.Start < b.Start; });

    // NitroFS names: directory tables, then each directory's entries
    if (fntOff && fntSize >= 8 && fntOff + fntSize <= len)
    {
        u32 dirCount = std::min<u32>(Read16(rom, len, fntOff + 6), 0x1000);
        std::vector<std::string> dirPath(dirCount);
        std::vector<int> dirParent(dirCount, -1);
        std::vector<std::string> dirName(dirCount);
        for (u32 d = 1; d < dirCount; d++)
            dirParent[d] = (int)(Read16(rom, len, fntOff + d * 8 + 6) & 0xFFF);

        // names of sub-directories are found in their parent's entries
        for (u32 d = 0; d < dirCount; d++)
        {
            u32 pos = fntOff + Read32(rom, len, fntOff + d * 8);
            while (pos < fntOff + fntSize)
            {
                u8 l = rom[pos++];
                if (l == 0) break;
                u32 nameLen = l & 0x7F;
                if (pos + nameLen > len) break;
                std::string name((const char*)rom + pos, nameLen);
                pos += nameLen;
                if (l & 0x80)
                {
                    u32 sub = Read16(rom, len, pos) & 0xFFF;
                    pos += 2;
                    if (sub < dirCount) dirName[sub] = name;
                }
            }
        }
        for (u32 d = 1; d < dirCount; d++)
        {
            // walk up to the root (a broken table can't loop forever)
            std::string path;
            int cur = (int)d;
            for (int guard = 0; cur > 0 && guard < 64; guard++)
            {
                path = dirName[cur] + "/" + path;
                cur = dirParent[cur];
            }
            dirPath[d] = path;
        }
        for (u32 d = 0; d < dirCount; d++)
        {
            u32 pos = fntOff + Read32(rom, len, fntOff + d * 8);
            u32 id = Read16(rom, len, fntOff + d * 8 + 4);
            while (pos < fntOff + fntSize)
            {
                u8 l = rom[pos++];
                if (l == 0) break;
                u32 nameLen = l & 0x7F;
                if (pos + nameLen > len) break;
                std::string name((const char*)rom + pos, nameLen);
                pos += nameLen;
                if (l & 0x80) { pos += 2; continue; }
                if (id < fileCount) FileNames[id] = dirPath[d] + name;
                id++;
            }
        }
    }

    // overlays have file ids but no names
    for (int cpu = 0; cpu < 2; cpu++)
    {
        u32 off = Read32(rom, len, 0x50 + cpu * 8), size = Read32(rom, len, 0x54 + cpu * 8);
        for (u32 i = 0; i + 32 <= size; i += 32)
        {
            u32 ov = Read32(rom, len, off + i), id = Read32(rom, len, off + i + 24);
            if (id < fileCount && FileNames[id].empty())
                FileNames[id] = std::string(cpu ? "(overlay ARM7 " : "(overlay ARM9 ") + std::to_string(ov) + ")";
        }
    }
}

u32 Inspector::CpuSite() const noexcept
{
    const ARMv5& arm9 = NDS.ARM9;
    if (NDS.IsJITEnabled())
        return arm9.StorePC;
    return arm9.R[15] | ((arm9.CPSR >> 5) & 1);
}

u32 Inspector::CpuCaller() const noexcept
{
    const ARMv5& arm9 = NDS.ARM9;
    return NDS.IsJITEnabled() ? arm9.StoreLR : arm9.R[14];
}

u16 Inspector::NewSource(const Source& source) noexcept
{
    if (LastSource >= FirstCpuSource)
    {
        const Source& last = Sources[LastSource];
        if (last.Site == source.Site && last.Caller == source.Caller && last.ListHash == source.ListHash && last.ListAddr == source.ListAddr)
            return LastSource;
    }
    u16 next = LastSource + 1;
    if (next < FirstCpuSource || next >= SourceRing) next = FirstCpuSource;
    Sources[next] = source;
    LastSource = next;
    return next;
}

u16 Inspector::CommandSource() noexcept
{
    if (!Enabled) return 0;
    // a DMA copying commands, else the CPU
    for (int i = 0; i < 4; i++)
        if (NDS.DMAs[i].IsExecuting())
            return NDS.DMAs[i].InspectorSource;
    u32 site = CpuSite();
    if (!site) return 0;
    Source s;
    s.Site = site;
    s.Caller = CpuCaller();
    return NewSource(s);
}

void Inspector::OnDmaEnabled(DMA& dma, u32 srcAddr, u32 dstAddr, u32 count) noexcept
{
    dma.InspectorSource = 0;
    if (dstAddr < 0x04000400 || dstAddr >= 0x040005CC) return; // not geometry commands

    const bool words32 = dma.Cnt & 0x04000000;
    if (!count) count = 0x200000;
    u32 words = words32 ? count : (count + 1) / 2;

    Source s;
    s.Site = CpuSite();
    s.Caller = CpuCaller();
    s.ListAddr = srcAddr;
    s.ListWords = words;
    s.Draw = ++DmaDraws;
    if ((srcAddr >> 24) == 0x02)
    {
        // the list's content, read once (main RAM wraps at its mask)
        XXH32_state_t* state = XXH32_createState();
        XXH32_reset(state, 0);
        for (u32 i = 0; i < words; i++)
        {
            u32 w;
            memcpy(&w, &NDS.MainRAM[(srcAddr + i * 4) & NDS.MainRAMMask & ~3u], 4);
            XXH32_update(state, &w, 4);
        }
        s.ListHash = XXH32_digest(state);
        XXH32_freeState(state);
    }
    else
        s.ListHash = srcAddr; // not in main RAM: the address stands for it

    // tags 1-4 for the 4 ARM9 DMAs
    u16 tag = 0;
    for (int i = 0; i < 4; i++)
        if (&NDS.DMAs[i] == &dma) tag = (u16)(i + 1);
    if (!tag) return;
    Sources[tag] = s;
    dma.InspectorSource = tag;
}

void Inspector::OnCommand(u8 command, u32 param, u16 source) noexcept
{
    const Source& s = Sources[source < SourceRing ? source : 0];
    Current.Sites[s.Site].Commands++;
    if (Current.Trace.size() < MaxTrace)
        Current.Trace.push_back({command, param, s.Site});
    else
        Current.TraceTruncated = true;
}

void Inspector::OnPolygon(Polygon& poly, u16 source, s32 texX, s32 texY, const s32* posMatrix) noexcept
{
    const Source& s = Sources[source < SourceRing ? source : 0];

    // a new source (another display list, or a CPU write after it) is a new
    // draw; consecutive CPU writes from one site stay one draw
    const u64 key = s.ListHash ? s.ListHash : (s.Site | 1ull << 32);
    if (Current.Objects.empty() || key != Current.Objects.back().Key || s.Draw != CurrentDraw)
    {
        if (Current.Objects.size() < MaxObjects)
        {
            ObjectDraw draw;
            draw.Key = key;
            draw.Rank = ObjectRanks[key]++;
            if (posMatrix)
                for (int i = 0; i < 3; i++) draw.Translation[i] = posMatrix[12 + i];
            Current.Objects.push_back(draw);
        }
    }
    CurrentDraw = s.Draw;
    if (!Current.Objects.empty()) Current.Objects.back().Polygons++;
    poly.CallSite = s.Site;
    poly.Caller = s.Caller;
    poly.ListHash = s.ListHash;

    SiteStats& site = Current.Sites[s.Site];
    site.Polygons++;
    site.Callers[s.Caller]++;
    if (s.ListAddr || s.ListHash)
    {
        site.Lists[s.ListHash]++;
        ListStats& list = Current.Lists[s.ListHash];
        list.Addr = s.ListAddr;
        list.Words = s.ListWords;
        list.Site = s.Site;
        list.Polygons++;
    }
    Current.PolygonIds[(poly.Attr >> 24) & 0x3F]++;
    Current.Polygons++;

    // textured polygons: texgen mode and texture matrix (address, size,
    // format bits of the texture parameters identify the texture)
    if ((poly.TexParam >> 26) & 0x7)
    {
        TexUse& use = Current.Textures[((u64)s.Site << 32) | (poly.TexParam & 0x3FFFFFFF)];
        use.Polygons[poly.TexParam >> 30]++;
        use.TexX = texX;
        use.Palette = poly.TexPalette;
        const u32 alpha = (poly.Attr >> 16) & 0x1F;
        if (alpha && alpha < 31) use.Translucent++;
        // material registers in effect (DIF_AMB, SPE_EMI): they shade lit polygons only
        if (poly.Attr & 0xF)
        {
            const GPU3D& g = NDS.GPU.GPU3D;
            use.Lit++;
            use.Specular = std::max({use.Specular, g.MatSpecular[0], g.MatSpecular[1], g.MatSpecular[2]});
            use.Emission = std::max({use.Emission, g.MatEmission[0], g.MatEmission[1], g.MatEmission[2]});
            use.Shininess = use.Shininess || g.UseShininessTable;
        }
        use.TexY = texY;
    }
}

std::vector<u32> Inspector::PaletteHistogram(u32 texParam) const
{
    // bits per texel and index mask by format: A3I5, 4, 16, 256 colours, -, A5I3
    static const int bits[8] = {0, 8, 2, 4, 8, 0, 8, 0};
    static const u32 mask[8] = {0, 0x1F, 0x3, 0xF, 0xFF, 0, 0x7, 0};
    const u32 format = (texParam >> 26) & 7;
    if (!bits[format]) return {};
    const u32 width = 8u << ((texParam >> 20) & 7), height = 8u << ((texParam >> 23) & 7);
    const u32 addr = (texParam & 0xFFFF) << 3;
    std::vector<u32> counts(mask[format] + 1, 0);
    const u32 texels = width * height;
    for (u32 i = 0; i < texels; i++)
    {
        u32 bit = i * bits[format];
        u8 byte = NDS.GPU.ReadVRAM_Texture<u8>(addr + (bit >> 3));
        counts[(byte >> (bit & 7)) & mask[format]]++;
    }
    return counts;
}

void Inspector::OnFlush() noexcept
{
    // palette indices of each paletted texture drawn this frame
    for (auto& [key, use] : Current.Textures)
    {
        u32 param = (u32)key;
        if (!Current.PaletteHistograms.count(param))
        {
            std::vector<u32> h = PaletteHistogram(param);
            if (!h.empty()) Current.PaletteHistograms[param] = std::move(h);
        }
    }

    // scrolling: the translation compared with the frame before
    std::map<u64, std::pair<s32, s32>> translations;
    for (auto& [key, use] : Current.Textures)
    {
        auto prev = PrevTexTranslation.find(key);
        if (prev != PrevTexTranslation.end() && (prev->second.first != use.TexX || prev->second.second != use.TexY))
        {
            use.Moved = true;
            use.DeltaX = use.TexX - prev->second.first;
            use.DeltaY = use.TexY - prev->second.second;
        }
        translations[key] = {use.TexX, use.TexY};
    }
    PrevTexTranslation.swap(translations);

    // materials: the evidence of every call site drawing the texture
    {
        std::map<u64, TextureEvidence> evidence;
        for (auto& [key, use] : Current.Textures)
        {
            TextureEvidence& e = evidence[((u64)use.Palette << 32) | (u32)key];
            for (u32 n : use.Polygons) e.Polygons += n;
            e.Translucent += use.Translucent;
            e.Scrolling = e.Scrolling || use.Moved;
            e.Lit += use.Lit;
            e.Specular = std::max(e.Specular, use.Specular);
            e.Emission = std::max(e.Emission, use.Emission);
            e.Shininess = e.Shininess || use.Shininess;
        }
        Current.MaterialEvidence.swap(evidence);
    }

    // objects: the same (key, rank) as in the previous frame keeps its id
    std::map<std::pair<u64, u32>, ObjectHistory> objects;
    for (ObjectDraw& draw : Current.Objects)
    {
        auto prev = PrevObjects.find({draw.Key, draw.Rank});
        if (prev != PrevObjects.end())
        {
            draw.Id = prev->second.Id;
            draw.Frames = prev->second.Frames + 1;
            memcpy(draw.PrevTranslation, prev->second.Translation, sizeof(draw.PrevTranslation));
            draw.HasPrev = true;
        }
        else
        {
            draw.Id = NextObjectId++;
            draw.Frames = 1;
        }
        ObjectHistory h {draw.Id, draw.Frames, {}};
        memcpy(h.Translation, draw.Translation, sizeof(h.Translation));
        objects[{draw.Key, draw.Rank}] = h;
    }
    PrevObjects.swap(objects);
    ObjectRanks.clear();
    {
        std::lock_guard<std::mutex> guard(Lock);
        FindPositionFields();
        std::swap(Last, Current);
        LastFrameNumber = FrameNumber;
    }
    Current.Sites.clear();
    Current.Lists.clear();
    memset(Current.PolygonIds, 0, sizeof(Current.PolygonIds));
    Current.Polygons = 0;
    Current.Trace.clear();
    Current.TraceTruncated = false;
    Current.Textures.clear();
    Current.Objects.clear();
    Current.Materials.clear();
    Current.MaterialEvidence.clear();
    Current.PaletteHistograms.clear();
}

void Inspector::OnCartRead(u32 addr, u32 len) noexcept
{
    std::lock_guard<std::mutex> guard(Lock);
    // the file holding this address (0xFFFFFFFF: none, e.g. the header)
    u32 file = 0xFFFFFFFF, offset = addr;
    auto it = std::upper_bound(Files.begin(), Files.end(), addr, [](u32 a, const CartFile& f) { return a < f.Start; });
    if (it != Files.begin())
    {
        --it;
        if (addr < it->End) { file = it->Id; offset = addr - it->Start; }
    }

    // a read that continues the previous one extends it
    if (!FileReads.empty())
    {
        FileRead& last = FileReads.back();
        if (last.File == file && last.Offset + last.Length == offset)
        {
            last.Length += len;
            return;
        }
    }
    if (FileReads.size() >= MaxFileReads) { FileReadsTruncated = true; return; }
    FileReads.push_back({FrameNumber, file, offset, len});
}

u32 Inspector::ViewColour(const Polygon& poly) const noexcept
{
    u32 key;
    switch (GetView())
    {
    case View::PolygonId: key = (poly.Attr >> 24) & 0x3F; break;
    case View::CallSite: key = poly.CallSite ? poly.CallSite ^ Mix(poly.Caller) : 0; break; // a library routine differs by caller
    case View::DisplayList: key = poly.ListHash; break;
    default: return 0;
    }
    // 0 (no list, unknown site) in grey; the rest in bright, distinct colours
    if (!key && GetView() != View::PolygonId) return 12 | (12 << 5) | (12 << 10);
    u32 h = Mix(key + 0x9E3779B9);
    u32 r = 6 + (h & 0x1F) % 26, g = 6 + ((h >> 5) & 0x1F) % 26, b = 6 + ((h >> 10) & 0x1F) % 26;
    return r | (g << 5) | (b << 10);
}

std::string Inspector::FileName(u32 id) const
{
    if (id == 0xFFFFFFFF) return "(no file: header, ARM9/ARM7 binaries or tables)";
    if (id < FileNames.size() && !FileNames[id].empty()) return Printable(FileNames[id]);
    return "(file " + std::to_string(id) + ")";
}

void Inspector::OnRendered(GPU& gpu, Renderer3D& renderer)
{
    if (!Enabled) return;
    {
        // materials of the frame just drawn: the renderer has brought the
        // flat texture VRAM the classifier decodes up to date (doing it here
        // would take the VRAM changes from the renderer's texture cache).
        // Statistics per frame: the texels at an address change with the scene
        std::lock_guard<std::mutex> guard(Lock);
        Classifier.Clear();
        Last.Materials.clear();
        for (auto& [key, e] : Last.MaterialEvidence)
        {
            FrameTables::Material m;
            m.Result = Classifier.Classify(gpu, (u32)key, (u32)(key >> 32), e);
            m.Stats = Classifier.Statistics(gpu, (u32)key, (u32)(key >> 32));
            m.Evidence = e;
            Last.Materials[key] = m;
        }
    }
    if (!Enabled || !renderer.Accelerated || FrameNumber - LastParityFrame < ParityInterval) return;
    LastParityFrame = FrameNumber;
    std::lock_guard<std::mutex> guard(Lock);
    Parity.Check(gpu, renderer, FrameNumber);
}

void Inspector::FindPositionFields()
{
    // translations from the trace: MTX_TRANS (3 parameters), MTX_LOAD/MULT
    // 4x4 (parameters 12-14) and 4x3 (9-11)
    std::unordered_map<s32, std::vector<std::pair<s32, s32>>> byX;
    u8 cmd = 0;
    u32 index = 0;
    s32 v[3] = {};
    for (const TraceEntry& e : Current.Trace)
    {
        if (e.Command != cmd) { cmd = e.Command; index = 0; }
        else index++;
        u32 first;
        switch (cmd)
        {
        case 0x1C: first = 0; index %= 3; break;
        case 0x16: case 0x18: first = 12; index %= 16; break;
        case 0x17: case 0x19: first = 9; index %= 12; break;
        default: continue;
        }
        if (index < first || index > first + 2) continue;
        v[index - first] = (s32)e.Param;
        // zero is everywhere in RAM: an origin says nothing
        if (index == first + 2 && (v[0] | v[1] | v[2]) && byX.size() < 1024)
            byX[v[0]].push_back({v[1], v[2]});
    }
    if (byX.empty()) return;

    const u32* ram = (const u32*)NDS.MainRAM;
    const u32 words = (NDS.MainRAMMask + 1) / 4;
    for (u32 i = 0; i + 2 < words; i++)
    {
        auto it = byX.find((s32)ram[i]);
        if (it == byX.end()) continue;
        for (auto& [y, z] : it->second)
        {
            if ((s32)ram[i + 1] != y || (s32)ram[i + 2] != z) continue;
            u32 addr = 0x02000000 + i * 4;
            auto field = PositionFields.find(addr);
            if (field == PositionFields.end())
            {
                if (PositionFields.size() >= MaxPositionFields) break;
                field = PositionFields.emplace(addr, PositionField{}).first;
            }
            PositionField& f = field->second;
            if (f.FramesFound && (f.Value[0] != (s32)ram[i] || f.Value[1] != y || f.Value[2] != z)) f.Changes++;
            f.FramesFound++;
            f.LastFrame = FrameNumber;
            f.Value[0] = (s32)ram[i]; f.Value[1] = y; f.Value[2] = z;
            break;
        }
    }
}

std::string Inspector::Report() const
{
    std::lock_guard<std::mutex> guard(Lock);
    std::string out;
    char line[256];
    auto add = [&](const char* fmt, auto... args) { snprintf(line, sizeof(line), fmt, args...); out += line; };
    auto site = [&](u32 s) -> std::string {
        if (!s) return "unknown";
        char b[32];
        u32 a = InstructionAddress(s);
        snprintf(b, sizeof(b), "%08X%s", a & ~1u, (a & 1) ? " (Thumb)" : "");
        return b;
    };

    add("Pomegrade inspector report\n");
    add("Game code: %s\n", GameCode.empty() ? "?" : Printable(GameCode).c_str());
    add("Frame: %u (3D of the last finished frame)\n", LastFrameNumber);
    add("Call site: address of the ARM9 instruction that wrote the 3D command, or that started the DMA copying it.\n");
    add("Called from: the return address (LR) at that instruction, in the caller (for a library routine: the game code using it).\n");
    add("Display list: a block of 3D commands copied by DMA (address, length in words, hash of its content).\n\n");

    add("== Polygons: %u ==\n\n", Last.Polygons);

    add("== Call sites (%zu) ==\n", Last.Sites.size());
    add("%-22s %9s %9s  %s\n", "site", "polygons", "commands", "display lists (hash:polygons)");
    std::vector<std::pair<u32, const SiteStats*>> sites;
    for (auto& [k, v] : Last.Sites) sites.push_back({k, &v});
    std::sort(sites.begin(), sites.end(), [](auto& a, auto& b) { return a.second->Polygons > b.second->Polygons; });
    for (auto& [k, v] : sites)
    {
        std::string lists;
        int n = 0;
        for (auto& [h, c] : v->Lists)
        {
            if (n++ == 8) { lists += " ..."; break; }
            char b[32]; snprintf(b, sizeof(b), " %08X:%u", h, c); lists += b;
        }
        add("%-22s %9u %9u %s\n", site(k).c_str(), v->Polygons, v->Commands, lists.c_str());
        // callers (LR at the store, a return address): which function called
        // the code at this site; the most polygons first
        std::vector<std::pair<u32, u32>> callers(v->Callers.begin(), v->Callers.end());
        std::sort(callers.begin(), callers.end(), [](auto& a, auto& b) { return a.second > b.second; });
        int shown = 0;
        for (auto& [lr, c] : callers)
        {
            if (!c) continue;
            if (shown++ == 12) { add("      ... %zu more callers\n", callers.size() - 12); break; }
            add("      called from %08X%s: %u polygons\n", lr & ~1u, (lr & 1) ? " (Thumb)" : "", c);
        }
    }

    add("\n== Display lists (%zu) ==\n", Last.Lists.size());
    add("%-9s %-9s %7s %9s  %s\n", "hash", "address", "words", "polygons", "started by");
    std::vector<std::pair<u32, const ListStats*>> lists;
    for (auto& [k, v] : Last.Lists) lists.push_back({k, &v});
    std::sort(lists.begin(), lists.end(), [](auto& a, auto& b) { return a.second->Polygons > b.second->Polygons; });
    for (auto& [k, v] : lists)
        add("%08X  %08X  %7u %9u  %s\n", k, v->Addr, v->Words, v->Polygons, site(v->Site).c_str());

    add("\n== Polygon IDs ==\n");
    for (int i = 0; i < 64; i++)
        if (Last.PolygonIds[i]) add("id %2d: %u polygons\n", i, Last.PolygonIds[i]);

    add("\n== Textures by call site (%zu) ==\n", Last.Textures.size());
    add("Texgen: how texture coordinates are made. normal = a fake reflection (sphere map), position = a projected texture\n");
    add("(often a fake shadow or light). Scrolls: the texture matrix translation changed since the frame before (per frame,\n");
    add("raw 1/4096 units; a flowing texture: water, lava, conveyors).\n");
    add("%-22s %-9s %-11s %6s %8s %7s %9s  %s\n", "site", "address", "size/format", "none", "texcoord", "normal", "position", "scrolls");
    for (auto& [key, use] : Last.Textures)
    {
        u32 param = (u32)key;
        char scroll[48] = "";
        if (use.Moved) snprintf(scroll, sizeof(scroll), "%+d, %+d", use.DeltaX, use.DeltaY);
        char fmt[24];
        snprintf(fmt, sizeof(fmt), "%dx%d f%u", 8 << ((param >> 20) & 7), 8 << ((param >> 23) & 7), (param >> 26) & 7);
        add("%-22s %08X  %-11s %6u %8u %7u %9u  %s\n", site((u32)(key >> 32)).c_str(), (param & 0xFFFF) << 3, fmt,
            use.Polygons[0], use.Polygons[1], use.Polygons[2], use.Polygons[3], scroll);
    }

    add("\n== Palette indices of paletted textures (%zu) ==\n", Last.PaletteHistograms.size());
    add("Texels per palette index, most used first (index:percent). Indices an artist gives one material (eyes, trim, glow)\n");
    add("show as their own entries; textures of one model using the same indices for the same parts can share masks.\n");
    for (auto& [param, counts] : Last.PaletteHistograms)
    {
        u32 total = 0, used = 0;
        std::vector<std::pair<u32, u32>> order;
        for (u32 i = 0; i < counts.size(); i++)
        {
            total += counts[i];
            if (counts[i]) { used++; order.push_back({counts[i], i}); }
        }
        std::sort(order.begin(), order.end(), [](auto& a, auto& b) { return a.first > b.first || (a.first == b.first && a.second < b.second); });
        std::string top;
        for (size_t k = 0; k < order.size() && k < 12; k++)
        {
            char b[24]; snprintf(b, sizeof(b), " %u:%u%%", order[k].second, (order[k].first * 100 + total / 2) / total); top += b;
        }
        add("%08X  %dx%d f%u  %u of %zu indices:%s%s\n", (param & 0xFFFF) << 3, 8 << ((param >> 20) & 7), 8 << ((param >> 23) & 7),
            (param >> 26) & 7, used, counts.size(), top.c_str(), order.size() > 12 ? " ..." : "");
    }

    add("\n== Objects (%zu draws%s) ==\n", Last.Objects.size(), Last.Objects.size() >= MaxObjects ? ", list full" : "");
    add("A draw is a display list, or a run of polygons from one call site. Its id stays the same while the same thing is drawn\n");
    add("in the same rank from frame to frame. Translation of its position matrix (model-view, 20.12), and its move since the previous frame.\n");
    add("%-6s %-7s %-22s %-6s %-9s  %s\n", "id", "frames", "drawn from", "rank", "polygons", "translation (move)");
    for (const ObjectDraw& d : Last.Objects)
    {
        char from[32];
        if (d.Key >> 32) snprintf(from, sizeof(from), "site %s", site((u32)d.Key).c_str());
        else snprintf(from, sizeof(from), "list %08X", (u32)d.Key);
        char move[64] = "new";
        if (d.HasPrev)
            snprintf(move, sizeof(move), "%+.3f, %+.3f, %+.3f", (d.Translation[0] - d.PrevTranslation[0]) / 4096.0,
                     (d.Translation[1] - d.PrevTranslation[1]) / 4096.0, (d.Translation[2] - d.PrevTranslation[2]) / 4096.0);
        add("%-6u %-7u %-22s %-6u %-9u  %.3f, %.3f, %.3f (%s)\n", d.Id, d.Frames, from, d.Rank, d.Polygons,
            d.Translation[0] / 4096.0, d.Translation[1] / 4096.0, d.Translation[2] / 4096.0, move);
    }

    // fields that changed come first: an object that moved is the best evidence
    std::vector<std::pair<u32, const PositionField*>> fields;
    for (auto& [addr, f] : PositionFields) fields.push_back({addr, &f});
    std::sort(fields.begin(), fields.end(), [](auto& a, auto& b) {
        return a.second->Changes != b.second->Changes ? a.second->Changes > b.second->Changes : a.second->FramesFound > b.second->FramesFound;
    });
    add("\n== Position fields in main RAM (%zu%s) ==\n", fields.size(), fields.size() >= MaxPositionFields ? ", list full" : "");
    add("Three words (x, y, z, 20.12) equal to a matrix translation the game sent. Moved: matched with a new value.\n");
    add("%-10s %-7s %-6s %-6s  %s\n", "address", "frames", "moved", "last", "x, y, z");
    for (size_t i = 0; i < fields.size() && i < 64; i++)
    {
        const PositionField& f = *fields[i].second;
        add("%08X   %-7u %-6u %-6u  %.3f, %.3f, %.3f\n", fields[i].first, f.FramesFound, f.Changes, f.LastFrame,
            f.Value[0] / 4096.0, f.Value[1] / 4096.0, f.Value[2] / 4096.0);
    }

    {
        add("\n== Materials (%zu textures) ==\n", Last.Materials.size());
        add("From render state and texture statistics (no manifest or file names yet); unknown below %.2f confidence.\n", MaterialClassifier::MinConfidence);
        add("%-8s %-9s %-5s %-8s %-8s %-4s  %-31s %s\n", "texture", "size", "fmt", "palette", "class", "conf", "hue sat bright detail grain var", "lit spec emis  cues");
        std::vector<std::pair<u64, const FrameTables::Material*>> list;
        for (auto& [key, m] : Last.Materials) list.push_back({key, &m});
        std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.second->Evidence.Polygons > b.second->Evidence.Polygons; });
        for (auto& [key, m] : list)
        {
            const u32 param = (u32)key;
            const auto& s = m->Stats;
            add("%08X %3ux%-5u f%u   %08X %-8s %.2f  %3.0f %.2f %.2f %.3f %.2f %.2f  %3u%% %2u%s %2u   %s\n", (param & 0xFFFF) << 3, 8 << ((param >> 20) & 7), 8 << ((param >> 23) & 7),
                (param >> 26) & 7, (u32)(key >> 32), MaterialClassifier::Name(m->Result.Class), m->Result.Confidence,
                s.Hue, s.Saturation, s.Brightness, s.Detail, s.Grain, s.Variety,
                m->Evidence.Polygons ? m->Evidence.Lit * 100 / m->Evidence.Polygons : 0, m->Evidence.Specular, m->Evidence.Shininess ? "s" : " ", m->Evidence.Emission, m->Result.Cues.c_str());
        }
    }

    out += Parity.Report();

    add("\n== Cartridge reads since the inspector was turned on (%zu%s) ==\n", FileReads.size(), FileReadsTruncated ? ", list full" : "");
    add("%-8s %-10s %-10s  %s\n", "frame", "offset", "bytes", "file");
    for (const FileRead& r : FileReads)
        add("%-8u 0x%08X 0x%08X  %s\n", r.Frame, r.Offset, r.Length, FileName(r.File).c_str());

    add("\n== 3D command trace of the frame (%zu commands%s) ==\n", Last.Trace.size(), Last.TraceTruncated ? ", truncated" : "");
    add("Vertex, colour, normal and texture coordinate commands are counted, not listed.\n");
    u32 skipped = 0;
    u32 lastSite = 0xFFFFFFFF;
    for (const TraceEntry& e : Last.Trace)
    {
        const char* name = CommandName(e.Command);
        bool detail = e.Command < 0x20 || e.Command >= 0x29;
        if (!detail) { skipped++; continue; }
        if (skipped) { add("    ... %u vertex/colour/normal/texcoord commands\n", skipped); skipped = 0; }
        if (e.Site != lastSite) { add("[%s]\n", site(e.Site).c_str()); lastSite = e.Site; }
        if (name) add("    %-14s %08X\n", name, e.Param);
        else add("    cmd %02X         %08X\n", e.Command, e.Param);
    }
    if (skipped) add("    ... %u vertex/colour/normal/texcoord commands\n", skipped);
    return out;
}

}
