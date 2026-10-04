// Save states: a state whose WRAMCNT differs from the console it is loaded
// on gets its shared WRAM mapping back. Before the fix, loading read WRAMCNT
// straight into the field MapSharedWRAM compares against, so the remap was
// skipped and the ARM7 kept the loading console's mapping (Dragon Quest
// Monsters: Joker's state, loaded on a freshly started console, ran its ARM7
// on the wrong memory and hung).
#include "NDS.h"
#include "Savestate.h"
#include <cstdio>
#include <memory>
#include <string>
using namespace melonDS;

namespace
{
bool ok = true;
void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

std::unique_ptr<NDS> MakeNDS()
{
    NDSArgs args;
    args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    nds->Reset();
    nds->Start();
    return nds;
}
}

int main()
{
    constexpr u32 WRAMCNT = 0x04000247, Marker = 0x5EB0A7ED;

    // WRAMCNT 3: all 32 KB of shared WRAM to the ARM7, at 0x03000000
    auto saved = MakeNDS();
    saved->ARM9Write8(WRAMCNT, 3);
    saved->ARM7Write32(0x03000000, Marker);
    check(saved->ARM7Read32(0x03000000) == Marker, "the ARM7 writes its shared WRAM");
    Savestate state;
    check(saved->DoSavestate(&state) && !state.Error, "state saved");
    state.Finish();

    // loaded on a console with WRAMCNT 0 (all shared WRAM to the ARM9: the
    // ARM7 sees its own WRAM at 0x03000000)
    auto loaded = MakeNDS();
    loaded->ARM9Write8(WRAMCNT, 0);
    Savestate in(state.Buffer(), state.Length(), false);
    check(loaded->DoSavestate(&in) && !in.Error, "state loaded");
    check(loaded->ARM9Read8(WRAMCNT) == 3, "WRAMCNT restored");
    check(loaded->ARM7Read32(0x03000000) == Marker, "the ARM7 sees the shared WRAM again (mapping restored)");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
