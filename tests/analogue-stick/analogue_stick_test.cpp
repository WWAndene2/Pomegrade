// Analogue movement: the patch for Dragon Quest Monsters: Joker (AJRP) is
// written only over the game's original instructions, the stick registers
// give the game's own values for its 8 D-pad directions, and switching off
// restores the code. A cartridge with the game's code and no game data; the
// patched code itself was checked in the real game (see the commit message).
#include "NDS.h"
#include "NDSCart.h"
#include "NDS_AnaloguePatches.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace melonDS;

static std::unique_ptr<NDSCart::CartCommon> CartWithCode(const char* code)
{
    std::vector<u8> rom(0x20000, 0);
    memcpy(&rom[0x0C], code, 4);
    return NDSCart::ParseROM(rom.data(), (u32)rom.size());
}

int main()
{
    bool ok = true;
    auto check = [&](bool cond, const char* what) { printf("%s: %s\n", what, cond ? "yes" : "NO"); ok = ok && cond; };

    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    nds->Reset();
    nds->SetNDSCart(CartWithCode("AJRP"));
    AnalogueStick& stick = nds->AnalogueStick;

    const char code[4] = {'A', 'J', 'R', 'P'};
    const AnaloguePatch* patch = FindAnaloguePatch(code);
    check(patch != nullptr, "AJRP has a patch");
    if (!patch) return 1;
    auto word = [&](int i) { return *(u32*)&nds->MainRAM[(patch->Address + i * 4) & nds->MainRAMMask]; };
    auto holds = [&](const std::array<u32, AnaloguePatch::Length>& words) {
        for (int i = 0; i < AnaloguePatch::Length; i++) if (word(i) != words[i]) return false;
        return true;
    };
    auto place = [&](const std::array<u32, AnaloguePatch::Length>& words) {
        for (int i = 0; i < AnaloguePatch::Length; i++) *(u32*)&nds->MainRAM[(patch->Address + i * 4) & nds->MainRAMMask] = words[i];
    };

    // another overlay where the movement code would be: nothing written
    for (int i = 0; i < AnaloguePatch::Length; i++) *(u32*)&nds->MainRAM[(patch->Address + i * 4) & nds->MainRAMMask] = 0xE1A00000 + i;
    stick.SetEnabled(true);
    stick.RunFrame(*nds);
    check(word(0) == 0xE1A00000 && word(8) == 0xE1A00008, "other code in its place: left alone");
    check(stick.Read32(AnalogueStick::RegisterBase, 0x3FF & ~(1 << 6)) == 0, "registers read 0 without the patch");

    // off: the original code stays
    place(patch->Original);
    stick.SetEnabled(false);
    stick.RunFrame(*nds);
    check(holds(patch->Original), "off: the original code stays");

    // on: patched
    stick.SetEnabled(true);
    stick.RunFrame(*nds);
    check(holds(patch->Patched), "on: the movement code is patched");

    // the game's own values for its 8 directions (from its switch at 021BC154)
    struct Dir { const char* name; u32 keys; float x, y; s32 angle, turn; } dirs[] = {
        {"down",       1 << 7,              0,  -1, 0,         0},
        {"right",      1 << 4,              1,   0, 0x5A000,  -0x2800},
        {"left",       1 << 5,             -1,   0, -0x5A000,  0x2800},
        {"up",         1 << 6,              0,   1, 0xB4000,   0},
        {"up+right",   (1 << 6) | (1 << 4), 1,   1, 0x87000,  -0x1400},
        {"up+left",    (1 << 6) | (1 << 5), -1,  1, -0x87000,  0x1400},
        {"down+right", (1 << 7) | (1 << 4), 1,  -1, 0x2D000,  -0x1400},
        {"down+left",  (1 << 7) | (1 << 5), -1, -1, -0x2D000,  0x1400},
    };
    bool dpad = true, sticks = true;
    for (auto& d : dirs)
    {
        u32 keyinput = 0x3FF & ~d.keys; // active low
        stick.SetPosition(0, 0);
        s32 a = (s32)stick.Read32(AnalogueStick::RegisterBase, keyinput), t = (s32)stick.Read32(AnalogueStick::RegisterBase + 4, keyinput);
        if (a != d.angle || t != d.turn) { dpad = false; printf("  D-pad %s: %X %X, game %X %X\n", d.name, a, t, d.angle, d.turn); }
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        stick.SetPosition(d.x / len, d.y / len);
        a = (s32)stick.Read32(AnalogueStick::RegisterBase, keyinput); t = (s32)stick.Read32(AnalogueStick::RegisterBase + 4, keyinput);
        // the stick is stored in 16 bits: within 1/100 degree
        if (std::abs(a - d.angle) > 41 || std::abs(t - d.turn) > 2) { sticks = false; printf("  stick %s: %X %X, game %X %X\n", d.name, a, t, d.angle, d.turn); }
    }
    check(dpad, "D-pad, stick released: the game's 8 values exactly");
    check(sticks, "stick in the 8 directions: the game's 8 values");

    // in between: the stick's own angle
    stick.SetPosition(0.5f, 0.8660254f); // 150 degrees from towards the camera
    s32 a = (s32)stick.Read32(AnalogueStick::RegisterBase, 0x3FF & ~(1 << 6));
    printf("stick at 150 degrees: %.3f\n", a / 4096.0);
    check(std::fabs(a / 4096.0 - 150) < 0.01, "stick at 150 degrees: 150");
    stick.SetPosition(0.1f, 0.1f); // inside the dead zone: the D-pad decides
    check((s32)stick.Read32(AnalogueStick::RegisterBase, 0x3FF & ~(1 << 6)) == 0xB4000, "dead zone: the D-pad decides");

    // off again: restored
    stick.SetEnabled(false);
    stick.RunFrame(*nds);
    check(holds(patch->Original), "off again: the original code is back");
    check(stick.Read32(AnalogueStick::RegisterBase, 0x3FF & ~(1 << 6)) == 0, "off: registers read 0");

    // another game: untouched
    nds->SetNDSCart(CartWithCode("ABCD"));
    place(patch->Original);
    stick.SetEnabled(true);
    stick.RunFrame(*nds);
    check(holds(patch->Original), "another game: untouched");

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
