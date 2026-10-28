// The fly screen's L/R switch (OrasFlyMap.h): the hook's branch into the cave, every branch and literal load of the cave
// inside it, and its way back after the push it replaces; the places' records (the maps themselves need the game: checked
// by oras-engine)
#include "OrasFlyMap.h"
#include <cstdio>
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

    // the places: two records on box 10 and 11, a third on box 12; box 10 placed, a new place on box 11
    Bytes records(3 * 60, 0);
    for (int r = 0; r < 3; r++) { records[r * 60] = (uint8_t)(5 + r); records[r * 60 + 4] = (uint8_t)(10 + r); records[r * 60 + 16] = 7; }
    const FlyPlaces places = ReadFlyPlaces("size 100 50\npane 10 50 25 # a town\nzone 77 11 10 10\n");
    const Bytes out = FlyPlacesRecords(records, places);
    check(out.size() == 4 * 60, "the new place appended");
    check(out[3 * 60] == 77 && out[3 * 60 + 4] == 11 && out[3 * 60 + 16] == 0 && out[3 * 60 + 2] == 77, "its zone, its box, no flight");
    check(out[4] == 10 && out[64] == 12 && out[124] == 12, "the placed box kept; the box given away and the unplaced one hidden in the unplaced one");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
