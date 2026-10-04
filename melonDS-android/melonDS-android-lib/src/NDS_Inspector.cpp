#include "NDS_Inspector.h"

#include "NDS.h"
#include "NDSCart.h"
#include "DMA.h"
#include "GPU3D.h"
#include "xxhash/xxhash.h"

#include <algorithm>
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
    LastSource = 0;
    for (auto& s : Sources) s = Source();
    if (Enabled)
    {
        std::lock_guard<std::mutex> guard(Lock);
        Last = FrameTables();
        FileReads.clear();
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

u16 Inspector::NewSource(const Source& source) noexcept
{
    if (LastSource >= FirstCpuSource)
    {
        const Source& last = Sources[LastSource];
        if (last.Site == source.Site && last.ListHash == source.ListHash && last.ListAddr == source.ListAddr)
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
    s.ListAddr = srcAddr;
    s.ListWords = words;
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

void Inspector::OnPolygon(Polygon& poly, u16 source) noexcept
{
    const Source& s = Sources[source < SourceRing ? source : 0];
    poly.CallSite = s.Site;
    poly.ListHash = s.ListHash;

    SiteStats& site = Current.Sites[s.Site];
    site.Polygons++;
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
}

void Inspector::OnFlush() noexcept
{
    {
        std::lock_guard<std::mutex> guard(Lock);
        std::swap(Last, Current);
        LastFrameNumber = FrameNumber;
    }
    Current.Sites.clear();
    Current.Lists.clear();
    memset(Current.PolygonIds, 0, sizeof(Current.PolygonIds));
    Current.Polygons = 0;
    Current.Trace.clear();
    Current.TraceTruncated = false;
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
    case View::CallSite: key = poly.CallSite; break;
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
