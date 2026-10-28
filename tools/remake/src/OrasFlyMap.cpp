#include "OrasFlyMap.h"

#include "Bclim.h"
#include "Darc.h"
#include "PicaTexture.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <sstream>
#include <string>

namespace remake
{

namespace
{
constexpr uint32_t GameWidth = 380, GameHeight = 270; // the game's image, as poke_navi_map.bclim declares it
constexpr uint8_t PicaEtc1 = 0x0C;                    // PICA's ETC1 (CLIM's 10)

uint32_t Word(const Bytes& b, size_t at) { return (uint32_t)(b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24); }

void PutFloat(Bytes& b, size_t at, float v)
{
    uint32_t u;
    std::memcpy(&u, &v, 4);
    for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(u >> 8 * k);
}

// the layout with every picture inside the pane "s" made transparent: its sections in order, pas1/pae1 opening and closing
// a pane's children; returns how many pictures. With places, its boxes (bnd1 children of "s") placed: a pane's
// translation at +0x24 (x, y; from its parent's centre, y up), its size at +0x44
Bytes WithPlacesTransparent(Bytes layout, size_t& pictures, const FlyPlaces* places)
{
    if (layout.size() < 0x14 || std::memcmp(layout.data(), "CLYT", 4) != 0) throw FormatError("fly map: map_lower.bclyt is not a CLYT layout");
    size_t at = (uint16_t)(layout[6] | layout[7] << 8);
    std::string last;  // the last pane's name
    int depth = -1;    // the children's depth inside "s", -1 outside it
    pictures = 0;
    while (at + 8 <= layout.size())
    {
        const std::string magic(layout.begin() + (long)at, layout.begin() + (long)at + 4);
        const uint32_t size = Word(layout, at + 4);
        if (size < 8 || at + size > layout.size()) throw FormatError("fly map: a layout section past the file");
        const bool pane = magic == "pan1" || magic == "pic1" || magic == "bnd1" || magic == "txt1" || magic == "wnd1";
        if (pane)
        {
            last = std::string(reinterpret_cast<const char*>(&layout[at + 12]), strnlen(reinterpret_cast<const char*>(&layout[at + 12]), 16));
            if (depth > 0 && magic == "pic1") { layout[at + 10] = 0; pictures++; }
            if (places && depth == 1 && magic == "bnd1")
            {
                const auto placed = places->Panes.find((uint16_t)std::stoul(last, nullptr, 36));
                float x = 0, y = 0, side = 0;
                if (placed != places->Panes.end())
                {
                    // the drawing scaled into the 320 x 210 of "s", centred (fly_background.py)
                    const float scale = std::min(320.0f / (float)places->Width, 210.0f / (float)places->Height);
                    x = (placed->second.first - (float)places->Width / 2) * scale;
                    y = -(placed->second.second - (float)places->Height / 2) * scale;
                    side = 12;
                    PutFloat(layout, at + 0x24, x);
                    PutFloat(layout, at + 0x28, y);
                }
                PutFloat(layout, at + 0x44, side);
                PutFloat(layout, at + 0x48, side);
            }
        }
        else if (magic == "pas1")
        {
            if (depth >= 0) depth++;
            else if (last == "s") depth = 1;
        }
        else if (magic == "pae1" && depth >= 0) depth--;
        if (depth == 0) depth = -1;
        at += size;
    }
    if (!pictures) throw FormatError("fly map: no picture under the pane s (another layout?)");
    return layout;
}
}

FlyPlaces ReadFlyPlaces(const std::string& text)
{
    FlyPlaces p;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        line = line.substr(0, line.find('#'));
        std::istringstream words(line);
        std::string kind;
        if (!(words >> kind)) continue;
        float x = 0, y = 0;
        unsigned a = 0, b = 0;
        if (kind == "size" && words >> p.Width >> p.Height) continue;
        if (kind == "pane" && words >> a >> x >> y) { p.Panes[(uint16_t)a] = {x, y}; continue; }
        if (kind == "zone" && words >> a >> b >> x >> y) { p.News.push_back({(uint16_t)a, (uint16_t)b}); p.Panes[(uint16_t)b] = {x, y}; continue; }
        throw FormatError("fly places: a line not read: " + line);
    }
    if (!p.Width || !p.Height || p.Panes.empty()) throw FormatError("fly places: no size or no place");
    return p;
}

Bytes FlyPlacesRecords(const Bytes& records, const FlyPlaces& places)
{
    constexpr size_t Record = 60;
    if (records.empty() || records.size() % Record) throw FormatError("fly places: a/0/6/6 is not 60-byte records");
    const auto pane = [](const Bytes& b, size_t r) { return (uint16_t)(b[r * Record + 4] | b[r * Record + 5] << 8); };
    const auto setU16 = [](Bytes& b, size_t at, uint16_t v) { b[at] = (uint8_t)v; b[at + 1] = (uint8_t)(v >> 8); };
    const size_t count = records.size() / Record;
    std::set<uint16_t> taken;
    for (const FlyPlaces::NewPlace& n : places.News) taken.insert(n.Pane);
    uint16_t hidden = 0;
    for (size_t r = 0; r < count && !hidden; r++)
        if (!places.Panes.count(pane(records, r)) && !taken.count(pane(records, r))) hidden = pane(records, r);
    if (!hidden) throw FormatError("fly places: no box left to hide the others in");
    Bytes out = records;
    for (const FlyPlaces::NewPlace& n : places.News)
    {
        size_t from = count;
        for (size_t r = 0; r < count && from == count; r++) if (pane(records, r) == n.Pane) from = r;
        if (from == count) throw FormatError("fly places: no record names box " + std::to_string(n.Pane));
        Bytes added(records.begin() + (long)(from * Record), records.begin() + (long)((from + 1) * Record));
        setU16(added, 0, n.Zone);
        setU16(added, 2, n.Zone);
        setU16(added, 16, 0);
        out.insert(out.end(), added.begin(), added.end());
    }
    for (size_t r = 0; r < count; r++)
        if (!places.Panes.count(pane(records, r)) || taken.count(pane(records, r))) setU16(out, r * Record + 4, hidden);
    return out;
}

Bytes FlyMapWithBackgrounds(const Bytes& member, const Bytes& hoenn, const Bytes& kalos, const FlyPlaces* places)
{
    const size_t imageSize = (size_t)FlyMapWidth * FlyMapHeight * 4;
    if (hoenn.size() != imageSize || (!kalos.empty() && kalos.size() != imageSize)) throw FormatError("fly map: a map is 380 x 256 RGBA");
    size_t darcAt = 0;
    for (; darcAt + 4 <= member.size() && std::memcmp(&member[darcAt], "darc", 4) != 0; darcAt += 4) {}
    if (darcAt + 4 > member.size()) throw FormatError("fly map: no DARC in a/0/6/7 member 0");
    std::vector<DarcFile> files = ReadDarc(Bytes(member.begin() + (long)darcAt, member.end()));
    bool background = false, layout = false;
    for (DarcFile& f : files)
    {
        if (f.Path.find("poke_navi_map.bclim") != std::string::npos)
        {
            const ClimImage old = ClimImage::Read(f.Data);
            if (old.Width != GameWidth || old.Height != GameHeight || old.Format != 10 || old.StoredWidth != 512 || old.StoredHeight != 2 * FlyMapHeight)
                throw FormatError("fly map: poke_navi_map.bclim is not 380 x 270 ETC1 stored 512 x 512");
            // Hoenn in the stored rows 511 down to 256 as PicaTextureDecode numbers them (the texture's memory from its
            // start), Kalos in 255 down to 0: an image put the right way up in the decoded rows showed upside down (run
            // fm5, 28 October), one in the top-left corner, where ClimImage::Rgba crops, black (run fm4)
            Bytes padded((size_t)old.StoredWidth * old.StoredHeight * 4, 0);
            const auto place = [&](const Bytes& rgba, uint32_t firstRow)
            {
                for (uint32_t y = 0; y < FlyMapHeight; y++)
                    std::copy(rgba.begin() + (long)(y * FlyMapWidth * 4), rgba.begin() + (long)((y + 1) * FlyMapWidth * 4),
                              padded.begin() + (long)((size_t)(firstRow - y) * old.StoredWidth * 4));
            };
            place(hoenn, old.StoredHeight - 1);
            if (!kalos.empty()) place(kalos, FlyMapHeight - 1);
            const Bytes pixels = PicaTextureEncode(padded, old.StoredWidth, old.StoredHeight, PicaEtc1);
            if (pixels.size() != old.Pixels.size()) throw FormatError("fly map: the encoded background's size differs from the game's");
            std::copy(pixels.begin(), pixels.end(), f.Data.begin());
            // the image declared 380 x 256 (the footer's "imag" block: u32 size, u16 width, u16 height)
            size_t imag = f.Data.size() >= 0x28 ? f.Data.size() - 0x28 : 0;
            for (; imag + 12 <= f.Data.size() && std::memcmp(&f.Data[imag], "imag", 4) != 0; imag++) {}
            if (imag + 12 > f.Data.size()) throw FormatError("fly map: no imag block in poke_navi_map.bclim");
            f.Data[imag + 10] = (uint8_t)FlyMapHeight;
            f.Data[imag + 11] = (uint8_t)(FlyMapHeight >> 8);
            background = true;
        }
        else if (f.Path.find("map_lower.bclyt") != std::string::npos)
        {
            size_t pictures = 0;
            f.Data = WithPlacesTransparent(f.Data, pictures, places);
            layout = true;
        }
    }
    if (!background || !layout) throw FormatError("fly map: poke_navi_map.bclim or map_lower.bclyt not in the DARC");
    const Bytes darc = WriteDarc(files);
    if (darc.size() != member.size() - darcAt) throw FormatError("fly map: the rewritten DARC's size differs (the header before it would need its offsets)");
    Bytes out(member.begin(), member.begin() + (long)darcAt);
    out.insert(out.end(), darc.begin(), darc.end());
    return out;
}

std::vector<CodeWord> FlyMapSwitchWords(uint32_t cave)
{
    // FUN_107F5434 (the fly screen's update, called every frame: its touch handler FUN_107F3F7C, then FUN_0033DC40), its
    // first instruction (push {r4, lr}) made a branch to the cave, which does it and comes back after it. In the cave:
    //  - L or R pressed: FUN_004C84F0 (device, mask, 0), the pressed bits (state +4) of a button device, as DllTownmap
    //    reads its keys (0x10, 0x20, 0x1000); the device is the input manager's (FUN_0010E088) +0x24 (FUN_001175D0's
    //    0). The masks are the game's (L 0x100, R 0x200: the device's state in the memory of runs lr1 and lr6). The
    //    manager's +0xC (FUN_004C7EE4's) is a touch device: its pressed bits stayed 0 (run lr5);
    //  - the map's picture: the layout FUN_107F3F7C draws the cursor in (this + 0xA4, FUN_004C4BE4) and its pane id 0
    //    (FUN_004C8F3C; the ids are not the panes' rank in the file, where "0" is the fourth: the layout's cache of panes
    //    held it at 0, run lr4), checked to be 380 wide (+0x48); its material (+0x13C), the material's first texture slot
    //    (+0x34) and the slot's address (+4) moved by 0x10001
    constexpr uint32_t hook = 0x107F5434, push = 0xE92D4010;
    std::vector<uint32_t> code;
    std::vector<std::pair<size_t, int>> literalLoads, toDone; // (word, literal) a ldr rd, [pc]; a branch to the end
    const auto here = [&] { return cave + 4 * (uint32_t)code.size(); };
    const auto load = [&](uint32_t rd, int literal) { literalLoads.push_back({code.size(), literal}); code.push_back(0xE59F0000u | rd << 12); };
    const auto call = [&](int literal) { load(12, literal); code.push_back(0xE12FFF3C); }; // ldr r12, =f; blx r12
    const auto doneIf = [&](uint32_t condition) { toDone.push_back({code.size(), 0}); code.push_back(condition << 28 | 0x0A000000u); };
    constexpr uint32_t eq = 0x0, ne = 0x1;
    const uint32_t literals[] = {0x0010E088, 0x004C84F0, 0x004C4BE4, 0x004C8F3C, 0x43BE0000 /* 380.0 */, 0x00010001};
    enum { Manager, Pressed, LayoutOf, PaneOf, Width, Step };
    code.push_back(push);
    code.push_back(0xE1A04000);                       // mov r4, r0: this
    call(Manager);
    code.push_back(0xE5900024);                       // ldr r0, [r0, #0x24]: the button device
    code.push_back(0xE3A01C03);                       // mov r1, #0x300: L or R
    code.push_back(0xE3A02000);                       // mov r2, #0
    call(Pressed);
    code.push_back(0xE3500000); doneIf(eq);           // cmp r0, #0
    code.push_back(0xE59400A4);                       // ldr r0, [r4, #0xA4]
    code.push_back(0xE3A01000);                       // mov r1, #0
    call(LayoutOf);
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE3A01000);                       // mov r1, #0: the pane "0"
    call(PaneOf);
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE5901048);                       // ldr r1, [r0, #0x48]: its width
    load(2, Width);
    code.push_back(0xE1510002); doneIf(ne);           // cmp r1, r2
    code.push_back(0xE590013C);                       // ldr r0, [r0, #0x13C]: its material
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE5900034);                       // ldr r0, [r0, #0x34]: its texture slot
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE5901004);                       // ldr r1, [r0, #4]: the texture's address
    load(2, Step);
    code.push_back(0xE3110001);                       // tst r1, #1
    code.push_back(0x00811002);                       // addeq r1, r1, r2: Kalos
    code.push_back(0x10411002);                       // subne r1, r1, r2: Hoenn
    code.push_back(0xE5801004);                       // str r1, [r0, #4]
    const uint32_t done = here();
    code.push_back(0xE1A00004);                       // mov r0, r4
    code.push_back(0xEA000000u | (((hook + 4 - here() - 8) >> 2) & 0xFFFFFF)); // back after the push
    const uint32_t pool = here();
    for (const uint32_t l : literals) code.push_back(l);
    for (const auto& [at, literal] : literalLoads) code[at] |= pool + 4 * (uint32_t)literal - (cave + 4 * (uint32_t)at) - 8;
    for (const auto& [at, unused] : toDone) code[at] |= ((done - (cave + 4 * (uint32_t)at) - 8) >> 2) & 0xFFFFFF;
    std::vector<CodeWord> words{{hook, push, 0xEA000000u | (((cave - hook - 8) >> 2) & 0xFFFFFF), "the fly screen's update: to the L/R cave"}};
    for (uint32_t k = 0; k < code.size(); k++) words.push_back({cave + 4 * k, 0, code[k], "the fly screen's L/R (cave)"});
    return words;
}

}
