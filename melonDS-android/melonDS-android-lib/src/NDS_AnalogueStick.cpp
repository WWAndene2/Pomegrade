#include "NDS_AnalogueStick.h"

#include "NDS.h"
#include "NDS_AnaloguePatches.h"

#include <algorithm>
#include <cmath>

namespace melonDS
{

void AnalogueStick::SetPosition(float x, float y) noexcept
{
    auto pack = [](float v) { return (u32)(u16)(s16)std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f); };
    Position = pack(x) | (pack(y) << 16);
}

void AnalogueStick::RunFrame(NDS& nds)
{
    ActivePatch = nullptr;
    const NDSCart::CartCommon* cart = nds.NDSCartSlot.GetCart();
    if (!cart)
        return;
    const AnaloguePatch* patch = FindAnaloguePatch(cart->GetHeader().GameCode);
    if (!patch)
        return;

    // the code may not be loaded (another overlay in its place) or loaded
    // again since the last frame: compare word for word before writing
    auto matches = [&](const std::array<u32, AnaloguePatch::Length>& words) {
        for (int i = 0; i < AnaloguePatch::Length; i++)
        {
            u32 addr = patch->Address + i * 4;
            if (*(const u32*)&nds.MainRAM[addr & nds.MainRAMMask] != words[i])
                return false;
        }
        return true;
    };
    // through the ARM9 bus, as cheat codes write: the JIT drops what it
    // compiled from these addresses
    auto write = [&](const std::array<u32, AnaloguePatch::Length>& words) {
        for (int i = 0; i < AnaloguePatch::Length; i++)
            nds.ARM9Write32(patch->Address + i * 4, words[i]);
    };

    if (Enabled)
    {
        if (matches(patch->Original))
            write(patch->Patched);
        if (matches(patch->Patched))
            ActivePatch = patch;
    }
    else if (matches(patch->Patched))
        write(patch->Original);
}

u32 AnalogueStick::Read32(u32 addr, u32 keyinput) const noexcept
{
    if (!ActivePatch || addr < RegisterBase || addr >= RegisterEnd)
        return 0;

    u32 pos = Position;
    float x = (s16)(pos & 0xFFFF) / 32767.0f, y = (s16)(pos >> 16) / 32767.0f;
    if (std::sqrt(x * x + y * y) < DeadZone)
    {
        // the D-pad (KEYINPUT, active low: right 4, left 5, up 6, down 7):
        // the same values as the game's own 8 directions
        x = (float)(!(keyinput & (1 << 4))) - (float)(!(keyinput & (1 << 5)));
        y = (float)(!(keyinput & (1 << 6))) - (float)(!(keyinput & (1 << 7)));
    }

    s32 out[2] = {0, 0};
    if (x != 0 || y != 0)
        ActivePatch->Registers(x, y, out);
    return (u32)out[(addr - RegisterBase) >> 2];
}

}
