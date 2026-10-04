// Cross-checks the remake tool's texture names against the DS core itself:
// random textures of the seven formats are put in the emulator's texture
// VRAM and decoded by melonDS::DecodeTexture, then hashed as the core names
// its dumps; the tool, given the same bytes as a ROM file holds them, must
// produce the same name.
#include "EmulatorTextureName.h"

#include "NDS.h"
#include "GPU.h"
#include "GPU3D_Texcache.h"
#include "GPU3D_TextureReplacement.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

using namespace melonDS;

int main()
{
    NDSArgs args; args.JIT = std::nullopt;
    auto nds = std::make_unique<NDS>(std::move(args));
    GPU& gpu = nds->GPU;
    std::mt19937 rng(7);
    int failures = 0, checks = 0;

    for (int format = 1; format <= 7; format++)
        for (int sizeCase = 0; sizeCase < 3; sizeCase++)
            for (int transparent = 0; transparent < 2; transparent++)
            {
                const u32 ws = sizeCase, hs = (sizeCase + 1) % 3; // 8x16, 16x32, 32x8
                const u32 w = 8u << ws, h = 8u << hs;
                // texture at 0x100 (8-byte units: 0x20), 4x4 data in slot 0 so its info is in slot 1
                const u32 texAddr = 0x100, palBase = 3;
                const u32 texParam = (texAddr / 8) | ws << 20 | hs << 23 | (u32)format << 26 | (u32)transparent << 29;
                const int bpp[8] = {0, 8, 2, 4, 8, 2, 8, 16};
                remake::Bytes texels((size_t)w * h * bpp[format] / 8), info, palette(format == 5 ? 0x400 : 512);
                for (auto& b : texels) b = (uint8_t)rng();
                for (auto& b : palette) b = (uint8_t)rng();
                memset(gpu.VRAMFlat_Texture, 0, sizeof gpu.VRAMFlat_Texture);
                memset(gpu.VRAMFlat_TexPal, 0, sizeof gpu.VRAMFlat_TexPal);
                memcpy(gpu.VRAMFlat_Texture + texAddr, texels.data(), texels.size());
                if (format == 5)
                {
                    // block info: palette offsets inside the palette written, every mode
                    info.resize((size_t)w * h / 16 * 2);
                    for (size_t i = 0; i < info.size(); i += 2)
                    {
                        const uint16_t v = (uint16_t)((rng() % 0x80) | (rng() % 4) << 14);
                        info[i] = v & 0xFF; info[i + 1] = v >> 8;
                    }
                    memcpy(gpu.VRAMFlat_Texture + 0x20000 + (texAddr >> 1), info.data(), info.size());
                }
                // the 4-colour format's palette address is in 8-byte units, the others' in 16
                const u32 palAddr = format == 2 ? palBase * 8 : palBase * 16;
                memcpy(gpu.VRAMFlat_TexPal + palAddr, palette.data(), palette.size());

                std::vector<u32> decoded(w * h);
                TexSource source;
                DecodeTexture(gpu, texParam, palBase, decoded.data(), source);
                char want[64];
                snprintf(want, sizeof want, "tex_%ux%u_%016llx", w, h,
                         (unsigned long long)TextureReplacement::HashDecoded(decoded.data(), w, h));

                remake::TextureFormat f{format, w, h, transparent != 0};
                const std::string got = remake::EmulatorTextureName(f, texels, palette, info);
                checks++;
                if (got != want)
                {
                    failures++;
                    const std::vector<uint32_t> mine = remake::DecodeRgb6a5(f, texels, palette, info);
                    size_t i = 0;
                    while (i < mine.size() && mine[i] == decoded[i]) i++;
                    printf("format %d %ux%u transparent %d: %s, core %s (texel %zu: %08x vs %08x)\n", format, w, h, transparent,
                           got.c_str(), want, i, i < mine.size() ? mine[i] : 0, i < mine.size() ? decoded[i] : 0);
                }
            }
    printf("%d/%d names match the core\n", checks - failures, checks);
    printf(failures ? "FAILED\n" : "ALL OK\n");
    return failures != 0;
}
