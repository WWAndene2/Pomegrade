#include "OrasFlyMap.h"

#include "Bclim.h"
#include "Darc.h"
#include "PicaTexture.h"

#include <algorithm>
#include <cstring>
#include <optional>
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
// a pane's children; returns how many pictures. With boxes, the boxes (bnd1 children of "s") placed: a pane's
// translation at +0x24 (x, y; from its parent's centre, y up), its size at +0x44
Bytes WithPlacesTransparent(Bytes layout, size_t& pictures, const std::map<uint16_t, std::array<float, 3>>* boxes)
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
            if (boxes && depth == 1 && magic == "bnd1")
            {
                const auto placed = boxes->find((uint16_t)std::stoul(last, nullptr, 36));
                const std::array<float, 3> box = placed == boxes->end() ? std::array<float, 3>{0, 0, 0} : placed->second;
                if (placed != boxes->end())
                {
                    PutFloat(layout, at + 0x24, box[0]);
                    PutFloat(layout, at + 0x28, box[1]);
                }
                PutFloat(layout, at + 0x44, box[2]);
                PutFloat(layout, at + 0x48, box[2]);
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
        if (kind == "flip") { p.Flip = true; continue; }
        std::string flag;
        if (kind == "place" && words >> a >> b >> flag >> x >> y)
        {
            p.Places.push_back({(uint16_t)a, (uint16_t)b, (uint16_t)std::stoul(flag, nullptr, 0), x, y});
            continue;
        }
        if (kind == "pane" && words >> a >> x >> y) { p.Panes[(uint16_t)a] = {x, y}; continue; }
        if (kind == "zone" && words >> a >> b >> x >> y) { p.News.push_back({(uint16_t)a, (uint16_t)b}); p.Panes[(uint16_t)b] = {x, y}; continue; }
        throw FormatError("fly places: a line not read: " + line);
    }
    if (!p.Width || !p.Height || (p.Panes.empty() && p.Places.empty())) throw FormatError("fly places: no size or no place");
    return p;
}

// a pixel of a drawing in the coordinates of "s": the drawing scaled into its 320 x 210, centred (fly_background.py)
static std::array<float, 3> BoxAt(const FlyPlaces& p, float px, float py)
{
    const float scale = std::min(320.0f / (float)p.Width, 210.0f / (float)p.Height);
    if (p.Flip) py = (float)p.Height - py;
    return {(px - (float)p.Width / 2) * scale, -(py - (float)p.Height / 2) * scale, 12};
}

Bytes FlyPlacesRecords(const Bytes& records, const FlyPlaces& hoenn, const FlyPlaces* kalos, std::map<uint16_t, std::array<float, 3>>& boxes,
                       FlySwitch& sw)
{
    constexpr size_t Record = 60;
    if (records.empty() || records.size() % Record) throw FormatError("fly places: a/0/6/6 is not 60-byte records");
    const auto pane = [](const Bytes& b, size_t r) { return (uint16_t)(b[r * Record + 4] | b[r * Record + 5] << 8); };
    const auto setU16 = [](Bytes& b, size_t at, uint16_t v) { b[at] = (uint8_t)v; b[at + 1] = (uint8_t)(v >> 8); };
    const size_t count = records.size() / Record;
    std::set<uint16_t> taken;
    for (const FlyPlaces::NewPlace& n : hoenn.News) taken.insert(n.Pane);
    uint16_t hidden = 0;
    for (size_t r = 0; r < count && !hidden; r++)
        if (!hoenn.Panes.count(pane(records, r)) && !taken.count(pane(records, r))) hidden = pane(records, r);
    if (!hidden) throw FormatError("fly places: no box left to hide the others in");
    boxes.clear();
    for (const auto& [id, at] : hoenn.Panes) boxes[id] = BoxAt(hoenn, at.first, at.second);
    Bytes out = records;
    const auto add = [&](size_t from, uint16_t zone, uint16_t box, uint16_t flight, std::optional<uint16_t> flag)
    {
        Bytes added(records.begin() + (long)(from * Record), records.begin() + (long)((from + 1) * Record));
        setU16(added, 0, zone);
        setU16(added, 2, zone);
        setU16(added, 4, box);
        setU16(added, 16, flight);
        if (flag) { setU16(added, 20, *flag); setU16(added, 28, *flag); }
        setU16(added, 32, 0);
        out.insert(out.end(), added.begin(), added.end());
    };
    for (const FlyPlaces::NewPlace& n : hoenn.News)
    {
        size_t from = count;
        for (size_t r = 0; r < count && from == count; r++) if (pane(records, r) == n.Pane) from = r;
        if (from == count) throw FormatError("fly places: no record names box " + std::to_string(n.Pane));
        add(from, n.Zone, n.Pane, 0, std::nullopt);
    }
    for (size_t r = 0; r < count; r++)
        if (!hoenn.Panes.count(pane(records, r)) || taken.count(pane(records, r))) setU16(out, r * Record + 4, hidden);
    sw = FlySwitch();
    sw.Hidden = hidden;
    for (size_t r = 0; r < out.size() / Record; r++) sw.Records.push_back(pane(out, r));
    std::map<uint16_t, std::array<float, 3>> onKalos;
    if (kalos)
    {
        // each a copy of the first route record (u16 4 zero: a town's record names its town by it, and its copies showed
        // "Bourg-en-Vol", run pk3; a route's is named by its zone, as Hoenn's added places were, run pl1)
        size_t copied = count;
        for (size_t r = 0; r < count && copied == count; r++) if (!(records[r * Record + 8] | records[r * Record + 9])) copied = r;
        if (copied == count) throw FormatError("fly places: no record named by its zone to copy");
        std::vector<uint16_t> free, shared;
        for (size_t r = 0; r < count; r++)
        {
            const uint16_t p = pane(records, r);
            if (p != hidden && !hoenn.Panes.count(p) && !taken.count(p) && std::find(free.begin(), free.end(), p) == free.end()) free.push_back(p);
        }
        for (const auto& [id, at] : boxes) shared.push_back(id);
        size_t f = 0, h = 0;
        for (const FlyPlaces::Place& k : kalos->Places)
        {
            const bool own = f < free.size();
            if (!own && h >= shared.size()) throw FormatError("fly places: more Kalos places than boxes");
            const uint16_t box = own ? free[f++] : shared[h++];
            const std::array<float, 3> at = BoxAt(*kalos, k.X, k.Y);
            add(copied, k.Zone, own ? box : hidden, k.Flight, k.Flag);
            sw.Records.push_back((uint16_t)(0x8000 | box));
            onKalos[box] = at;
            if (own) boxes[box] = {at[0], at[1], 0};
        }
    }
    for (const auto& [id, at] : boxes)
    {
        FlySwitch::Box b{id, {at[0], at[1], at[2]}, {at[0], at[1], 0}};
        if (onKalos.count(id)) for (int k = 0; k < 3; k++) b.Kalos[k] = onKalos[id][k];
        sw.Boxes.push_back(b);
    }
    return out;
}

Bytes FlyMapWithBackgrounds(const Bytes& member, const Bytes& hoenn, const Bytes& kalos, const std::map<uint16_t, std::array<float, 3>>* boxes)
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
            f.Data = WithPlacesTransparent(f.Data, pictures, boxes);
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

std::vector<CodeWord> FlyMapSwitchWords(uint32_t cave, const FlySwitch* sw)
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
    //    (+0x34) and the slot's address (+4) moved by 0x10001;
    //  - with sw, then: each box of sw.Boxes (the same layout's, FUN_004C8F3C) given its place on the map now shown (r5:
    //    the address's low bit), its translation at +0x28 (x, y) and its size at +0x48 (w, h) as the picture "0" shows
    //    them in memory (run lr4); and each record (this + 0x94, its +4: a u32 count then the 60-byte records, FUN_004C6CFC
    //    reads them so; the count 175 in memory, run pl1) its box (+4) from sw.Records, Hidden for a record of the other
    //    map. A record's zone stays: the screen reads the records' zones (their names) when it opens (zones swapped by L/R
    //    showed the swapped-out zone's name, run pk4);
    //  - with sw, once the screen has put the player's mark (Hidden's z, +0x30, 0 in the file, set to 1): if the mark is on
    //    a box only Kalos's map shows, Kalos is shown at once. FUN_107F2E54 puts the mark and the cursor on the box of the
    //    player's zone's first record and keeps the mark's place in the cursor's object (this + 0xA4) at +0x90 (x, y + 15:
    //    run pk7, -14.953, 19.43 on Camphrier's box); its +0xEC, the box, is the hovered one, -1 off a box (run pk7).
    // The tables follow the literals, reached from the pc (the module is loaded anywhere)
    constexpr uint32_t hook = 0x107F5434, push = 0xE92D4010;
    constexpr uint32_t eq = 0x0, ne = 0x1, al = 0xE;
    std::vector<uint32_t> code;
    std::map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> branches, literalLoads, tableAdds; // fixed once every label is known
    const auto label = [&](const std::string& name) { labels[name] = code.size(); };
    const auto op = [&](uint32_t word) { code.push_back(word); };
    const auto b = [&](uint32_t condition, const std::string& to) { branches.push_back({code.size(), to}); op(condition << 28 | 0x0A000000u); };
    const auto bl = [&](const std::string& to) { branches.push_back({code.size(), to}); op(0xEB000000u); };
    const auto load = [&](uint32_t rd, const std::string& literal) { literalLoads.push_back({code.size(), literal}); op(0xE59F0000u | rd << 12); };
    const auto call = [&](const std::string& f) { load(12, f); op(0xE12FFF3C); }; // ldr r12, =f; blx r12
    const auto tables = [&](uint32_t rd) { tableAdds.push_back({code.size(), "tables"}); op(0xE28F0F00u | rd << 12); }; // add rd, pc, #...
    const std::vector<std::pair<std::string, uint32_t>> literals = {{"Manager", 0x0010E088}, {"Pressed", 0x004C84F0}, {"LayoutOf", 0x004C4BE4},
        {"PaneOf", 0x004C8F3C}, {"Width", 0x43BE0000 /* 380.0 */}, {"Step", 0x00010001}, {"One", 0x3F800000 /* 1.0 */}, {"Hidden", sw ? sw->Hidden : 0u}};

    op(push);
    op(0xE92D01E0);                                   // push {r5-r8}
    op(0xE1A04000);                                   // mov r4, r0: this
    op(0xE59400A4);                                   // ldr r0, [r4, #0xA4]: the cursor's object
    op(0xE3500000); b(eq, "done");
    op(0xE3A01000);                                   // mov r1, #0
    call("LayoutOf");
    op(0xE3500000); b(eq, "done");
    op(0xE1A07000);                                   // mov r7, r0: the layout
    if (sw)
    {
        op(0xE1A00007);                               // mov r0, r7
        load(1, "Hidden");
        call("PaneOf");
        op(0xE3500000); b(eq, "keys");
        op(0xE5901030);                               // ldr r1, [r0, #0x30]: Hidden's z
        op(0xE3510000); b(ne, "keys");                // cmp r1, #0: opened before
        op(0xE59420A4);                               // ldr r2, [r4, #0xA4]
        op(0xE5921090);                               // ldr r1, [r2, #0x90]: the player's mark, x
        op(0xE5923094);                               // ldr r3, [r2, #0x94]: y
        op(0xE191C003); b(eq, "keys");                // orrs r12, r1, r3: not put yet (the first frames, run pk6)
        load(2, "One");
        op(0xE5802030);                               // str r2, [r0, #0x30]
        tables(6);
        op(0xE4968004);                               // ldr r8, [r6], #4: the boxes' count
        label("scan");
        op(0xE3580000); b(eq, "keys");                // cmp r8, #0
        op(0xE596200C);                               // ldr r2, [r6, #12]: its side on Hoenn's map
        op(0xE3520000); b(ne, "scanNext");
        op(0xE5962018);                               // ldr r2, [r6, #24]: on Kalos's
        op(0xE3520000); b(eq, "scanNext");
        op(0xE5962010);                               // ldr r2, [r6, #16]: its x on Kalos's
        op(0xE1520001); b(ne, "scanNext");            // cmp r2, r1
        op(0xE596201C);                               // ldr r2, [r6, #28]: the mark's y on it
        op(0xE1520003); b(ne, "scanNext");            // cmp r2, r3
        bl("toggle");                                 // the player on a box only Kalos's map shows: Kalos
        b(al, "done");
        label("scanNext");
        op(0xE2866020);                               // add r6, r6, #32
        op(0xE2488001);                               // sub r8, r8, #1
        b(al, "scan");
    }
    label("keys");
    call("Manager");
    op(0xE5900024);                                   // ldr r0, [r0, #0x24]: the button device
    op(0xE3A01C03);                                   // mov r1, #0x300: L or R
    op(0xE3A02000);                                   // mov r2, #0
    call("Pressed");
    op(0xE3500000); b(eq, "done");                    // cmp r0, #0
    // the press taken from the game: L and R cleared from the device's pressed bits (+4) in each of its states (+0x30,
    // +0x34, +0x44: FUN_004C84F0 reads one), so the screen's own use of them is not run
    call("Manager");
    op(0xE5900024);                                   // ldr r0, [r0, #0x24]
    for (const uint32_t state : {0x30u, 0x34u, 0x44u})
    {
        op(0xE5902000 | state);                       // ldr r2, [r0, #state]
        op(0xE3520000);                               // cmp r2, #0
        op(0x15921004);                               // ldrne r1, [r2, #4]
        op(0x13C11C03);                               // bicne r1, r1, #0x300
        op(0x15821004);                               // strne r1, [r2, #4]
    }
    bl("toggle");
    label("done");
    op(0xE8BD01E0);                                   // pop {r5-r8}
    op(0xE1A00004);                                   // mov r0, r4
    branches.push_back({code.size(), "@hook"}); op(0xEA000000u); // back after the push

    // toggle (r4 this, r7 the layout): the other map
    label("toggle");
    op(0xE92D4000);                                   // push {lr}
    op(0xE1A00007);                                   // mov r0, r7
    op(0xE3A01000);                                   // mov r1, #0: the pane "0"
    call("PaneOf");
    op(0xE3500000); b(eq, "return");
    op(0xE5901048);                                   // ldr r1, [r0, #0x48]: its width
    load(2, "Width");
    op(0xE1510002); b(ne, "return");                  // cmp r1, r2
    op(0xE590013C);                                   // ldr r0, [r0, #0x13C]: its material
    op(0xE3500000); b(eq, "return");
    op(0xE5900034);                                   // ldr r0, [r0, #0x34]: its texture slot
    op(0xE3500000); b(eq, "return");
    op(0xE5901004);                                   // ldr r1, [r0, #4]: the texture's address
    load(2, "Step");
    op(0xE3110001);                                   // tst r1, #1
    op(0x00811002);                                   // addeq r1, r1, r2: Kalos
    op(0x10411002);                                   // subne r1, r1, r2: Hoenn
    op(0xE5801004);                                   // str r1, [r0, #4]
    if (sw)
    {
        op(0xE2015001);                               // and r5, r1, #1: the map now shown
        tables(6);
        op(0xE4968004);                               // ldr r8, [r6], #4: the boxes' count
        label("box");
        op(0xE3580000); b(eq, "records");             // cmp r8, #0
        op(0xE1A00007);                               // mov r0, r7
        op(0xE5961000);                               // ldr r1, [r6]: the box's id
        call("PaneOf");
        op(0xE3500000); b(eq, "boxNext");
        op(0xE2862004);                               // add r2, r6, #4
        op(0xE0853085);                               // add r3, r5, r5, lsl #1
        op(0xE0822103);                               // add r2, r2, r3, lsl #2: its place on the map shown
        op(0xE5921000); op(0xE5801028);               // x
        op(0xE5921004); op(0xE580102C);               // y
        op(0xE5921008); op(0xE5801048); op(0xE580104C); // w, h
        label("boxNext");
        op(0xE2866020);                               // add r6, r6, #32
        op(0xE2488001);                               // sub r8, r8, #1
        b(al, "box");
        label("records");
        op(0xE5940094);                               // ldr r0, [r4, #0x94]
        op(0xE3500000); b(eq, "return");
        op(0xE5900004);                               // ldr r0, [r0, #4]
        op(0xE3500000); b(eq, "return");
        op(0xE4908004);                               // ldr r8, [r0], #4: the records' count
        op(0xE4963004);                               // ldr r3, [r6], #4: the table's
        op(0xE1580003);                               // cmp r8, r3
        op(0x81A08003);                               // movhi r8, r3: the fewer
        op(0xE4962004);                               // ldr r2, [r6], #4: Hidden
        label("record");
        op(0xE3580000); b(eq, "return");              // cmp r8, #0
        op(0xE0D610B2);                               // ldrh r1, [r6], #2: box, map (bit 15)
        op(0xE1A037A1);                               // mov r3, r1, lsr #15: its map
        op(0xE1530005);                               // cmp r3, r5
        op(0x03C11902);                               // biceq r1, r1, #0x8000: its box
        op(0x11A01002);                               // movne r1, r2: Hidden
        op(0xE1C010B4);                               // strh r1, [r0, #4]
        op(0xE280003C);                               // add r0, r0, #60
        op(0xE2488001);                               // sub r8, r8, #1
        b(al, "record");
    }
    label("return");
    op(0xE8BD8000);                                   // pop {pc}

    label("pool");
    for (const auto& [name, value] : literals) { labels[name] = code.size(); op(value); }
    if (sw)
    {
        // the tables: the boxes' count, each box (u32 id, then x, y, side on Hoenn's map and on Kalos's, and the
        // player's mark's y on Kalos's: the box's y + 15, the y of "s", as the screen adds them), the records' count,
        // Hidden, each record's u16
        label("tables");
        const auto bits = [](float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; };
        op((uint32_t)sw->Boxes.size());
        for (const FlySwitch::Box& box : sw->Boxes)
        {
            op(box.Id);
            for (const float f : box.Hoenn) op(bits(f));
            for (const float f : box.Kalos) op(bits(f));
            op(bits(box.Kalos[1] + 15.0f));
        }
        op((uint32_t)sw->Records.size());
        op(sw->Hidden);
        for (size_t k = 0; k < sw->Records.size(); k += 2)
            op(sw->Records[k] | (k + 1 < sw->Records.size() ? (uint32_t)sw->Records[k + 1] << 16 : 0));
    }
    const auto address = [&](size_t index) { return cave + 4 * (uint32_t)index; };
    for (const auto& [at, to] : branches)
    {
        const uint32_t target = to == "@hook" ? hook + 4 : address(labels.at(to));
        code[at] |= ((target - address(at) - 8) >> 2) & 0xFFFFFF;
    }
    for (const auto& [at, literal] : literalLoads) code[at] |= address(labels.at(literal)) - address(at) - 8;
    for (const auto& [at, unused] : tableAdds)
    {
        const uint32_t offset = address(labels.at("tables")) - address(at) - 8;
        if (offset > 1020 || offset % 4) throw FormatError("fly map: the L/R tables out of an add's reach");
        code[at] |= offset >> 2;
    }
    if (code.size() * 4 > 0x1000) throw FormatError("fly map: the L/R cave past its page");
    std::vector<CodeWord> words{{hook, push, 0xEA000000u | (((cave - hook - 8) >> 2) & 0xFFFFFF), "the fly screen's update: to the L/R cave"}};
    for (uint32_t k = 0; k < code.size(); k++) words.push_back({cave + 4 * k, 0, code[k], "the fly screen's L/R (cave)"});
    return words;
}

std::vector<CodeWord> NaviMapWords(uint32_t cave, uint32_t firstKalos)
{
    // FUN_102B5164 (DllField: the Navi-Map's player mark and scroll, by the player's zone's place record, as the fly
    // screen's; read 28 October) begins with push {r4, r5, r6, lr}, made a branch to the cave, which does it and comes
    // back after it. Its object's +0xC holds the player's zone at +0xF0 (the function compares it so; calling
    // FUN_004D8398 without its zone argument stopped the field, run nv2), its +0x10 the Navi-Map's
    // view, whose +0xA8 is the layout (its panes 0x20 and 0xB, FUN_004C8F3C). The picture "0" of that layout, 380 wide,
    // its material's texture slot's address (found in memory, run nv1: a poke of +0x10001 showed Kalos's half) is set
    // to Kalos's half or Hoenn's as the zone is a Kalos one (the mod's Kalos zones from the first one up) or not
    constexpr uint32_t hook = 0x102B5164, push = 0xE92D4070;
    std::vector<uint32_t> code;
    std::vector<std::pair<size_t, int>> loads, toDone;
    const auto load = [&](uint32_t rd, int literal) { loads.push_back({code.size(), literal}); code.push_back(0xE59F0000u | rd << 12); };
    const auto call = [&](int literal) { load(12, literal); code.push_back(0xE12FFF3C); };
    const auto doneIf = [&](uint32_t condition) { toDone.push_back({code.size(), 0}); code.push_back(condition << 28 | 0x0A000000u); };
    constexpr uint32_t eq = 0x0, ne = 0x1;
    enum { PaneOf, Width, Step, First };
    const uint32_t literals[] = {0x004C8F3C, 0x43BE0000 /* 380.0 */, 0x00010001, firstKalos};
    code.push_back(0xE92D40F0);                       // push {r4-r7, lr}
    code.push_back(0xE1A04000);                       // mov r4, r0
    code.push_back(0xE594000C);                       // ldr r0, [r4, #0xC]
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE1D05FB0);                       // ldrh r5, [r0, #0xF0]: the player's zone, as the function reads it
    code.push_back(0xE5940010);                       // ldr r0, [r4, #0x10]: the Navi-Map's view
    code.push_back(0xE3500000); doneIf(eq);
    code.push_back(0xE59000A8);                       // ldr r0, [r0, #0xA8]: its layout
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
    load(2, First);
    code.push_back(0xE1550002);                       // cmp r5, r2
    code.push_back(0x23A03001);                       // movhs r3, #1: Kalos
    code.push_back(0x33A03000);                       // movlo r3, #0: Hoenn
    code.push_back(0xE201C001);                       // and r12, r1, #1: the half shown
    code.push_back(0xE15C0003); doneIf(eq);           // cmp r12, r3
    load(2, Step);
    code.push_back(0xE3530000);                       // cmp r3, #0
    code.push_back(0x10811002);                       // addne r1, r1, r2
    code.push_back(0x00411002);                       // subeq r1, r1, r2
    code.push_back(0xE5801004);                       // str r1, [r0, #4]
    const uint32_t done = cave + 4 * (uint32_t)code.size();
    code.push_back(0xE1A00004);                       // mov r0, r4
    code.push_back(0xE8BD40F0);                       // pop {r4-r7, lr}
    code.push_back(push);                             // the hook's own push {r4, r5, r6, lr}
    code.push_back(0xEA000000u | (((hook + 4 - (cave + 4 * (uint32_t)code.size()) - 8) >> 2) & 0xFFFFFF));
    const uint32_t pool = cave + 4 * (uint32_t)code.size();
    for (const uint32_t l : literals) code.push_back(l);
    for (const auto& [at, literal] : loads) code[at] |= pool + 4 * (uint32_t)literal - (cave + 4 * (uint32_t)at) - 8;
    for (const auto& [at, unused] : toDone) code[at] |= ((done - (cave + 4 * (uint32_t)at) - 8) >> 2) & 0xFFFFFF;
    std::vector<CodeWord> words{{hook, push, 0xEA000000u | (((cave - hook - 8) >> 2) & 0xFFFFFF), "the Navi-Map's update: to its cave"}};
    for (uint32_t k = 0; k < code.size(); k++) words.push_back({cave + 4 * k, 0, code[k], "the Navi-Map's region (cave)"});
    return words;
}

}
