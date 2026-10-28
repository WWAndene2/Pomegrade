// The fly screen's L/R switch (OrasFlyMap.h): the hook's branch into the cave, every branch and literal load of the cave
// inside it, and its way back after the push it replaces; the places' records (the maps themselves need the game: checked
// by oras-engine)
#include "OrasFlyMap.h"
#include <cstdio>
#include <map>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static uint32_t Target(uint32_t at, uint32_t word) { return at + 8 + (uint32_t)(((int32_t)(word << 8)) >> 6); }

int main()
{
    constexpr uint32_t cave = 0x10800000, hook = 0x107F5434;
    const std::vector<CodeWord> w = FlyMapSwitchWords(cave);
    check(!w.empty() && w[0].Address == hook && w[0].Expected == 0xE92D4010 && (w[0].Value & 0xFF000000) == 0xEA000000 &&
          Target(hook, w[0].Value) == cave, "the update's push made a branch to the cave");
    check(w.size() > 1 && w[1].Address == cave && w[1].Value == 0xE92D4010, "the cave does the push first");
    const uint32_t end = cave + 4 * (uint32_t)(w.size() - 1);
    bool inside = true, back = false, loads = true;
    for (size_t k = 1; k < w.size() && !back; k++) // the instructions, to the branch back; its literals after it
    {
        const uint32_t at = w[k].Address, v = w[k].Value;
        if (w[k].Expected != 0) inside = false;
        if ((v & 0x0F000000) == 0x0A000000) // b<cond>
        {
            const uint32_t to = Target(at, v);
            if (to == hook + 4 && (v & 0xF0000000) == 0xE0000000) back = true;
            else if (to < cave || to >= end) inside = false;
        }
        if ((v & 0x0FFF0000) == 0x059F0000) // ldr rd, [pc, #offset]
        {
            const uint32_t literal = at + 8 + (v & 0xFFF);
            if (literal < cave || literal >= end) loads = false;
        }
    }
    check(inside, "every branch of the cave stays in it, every word of it expected zero");
    check(back, "the cave goes back after the push");
    check(loads, "every literal it loads is in the cave");

    // the places: records on boxes 10, 11, 12 and 13; box 10 a Hoenn place, a new Hoenn place on box 11, two Kalos places
    Bytes records(4 * 60, 0);
    for (int r = 0; r < 4; r++) { records[r * 60] = (uint8_t)(5 + r); records[r * 60 + 4] = (uint8_t)(10 + r); records[r * 60 + 16] = 7; }
    const FlyPlaces hoenn = ReadFlyPlaces("size 100 50\npane 10 50 25 # a town\nzone 77 11 10 10\n");
    const FlyPlaces kalos = ReadFlyPlaces("size 100 50\nflip\nplace 600 82 0x17B1 50 0\nplace 601 0 0x17A0 0 50\n");
    std::map<uint16_t, std::array<float, 3>> boxes;
    FlySwitch sw;
    const Bytes out = FlyPlacesRecords(records, hoenn, &kalos, boxes, sw);
    const auto u16 = [&](size_t at) { return out[at] | out[at + 1] << 8; };
    check(out.size() == 7 * 60, "the new Hoenn place and the two Kalos places appended");
    check(u16(4 * 60) == 77 && u16(4 * 60 + 4) == 11 && u16(4 * 60 + 16) == 0, "the Hoenn place: its zone, its box, no flight");
    check(u16(4) == 10 && u16(64) == 12 && u16(124) == 12 && u16(184) == 12, "the placed box kept; the box given away and the unplaced ones hidden in box 12");
    check(u16(5 * 60) == 600 && u16(5 * 60 + 4) == 13 && u16(5 * 60 + 16) == 82 && u16(5 * 60 + 20) == 0x17B1 && u16(5 * 60 + 32) == 0,
          "the first Kalos place on the one free box, with its flight, flag and an empty description");
    check(u16(6 * 60) == 601 && u16(6 * 60 + 4) == 12, "the second on a Hoenn place's box, hidden until Kalos shows");
    check(sw.Hidden == 12 && sw.Records.size() == 7 && sw.Records[5] == (0x8000 | 13) && sw.Records[6] == (0x8000 | 10) && sw.Records[0] == 10 && sw.Records[1] == 12,
          "the records' boxes on either map");
    bool own = false, moved = false;
    for (const FlySwitch::Box& box : sw.Boxes)
    {
        if (box.Id == 13) own = box.Hoenn[2] == 0 && box.Kalos[2] == 12 && box.Kalos[0] == 0 && box.Kalos[1] == -80;  // (50, 0) turned: the bottom; scale 3.2
        if (box.Id == 10) moved = box.Hoenn[2] == 12 && box.Kalos[2] == 12 && box.Kalos[0] == -160 && box.Kalos[1] == 80;
    }
    check(own && moved && boxes.at(13)[2] == 0, "the boxes' places on either map (the Kalos drawing turned upside down)");
    const std::vector<CodeWord> both = FlyMapSwitchWords(cave, &sw);
    check(both.size() > w.size() && both[0].Value == w[0].Value, "the cave with the places' tables");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
