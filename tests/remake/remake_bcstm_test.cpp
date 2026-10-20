// The remake tooling's stream encoder (Bcstm.h): a stereo signal written as Omega Ruby's BCSTM (DSP-ADPCM) and read back
// decodes close to its source, keeps its rate and length, puts its loop on a block (silence in front), and fills the
// header, sections and the last block as the game's files have them (bgm_sg_c_01: 0x2000-byte blocks of 14336 samples).
#include "Bcstm.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static double SnrDb(const std::vector<int16_t>& a, const std::vector<int16_t>& b, size_t offset)
{
    double s = 0, n = 0;
    for (size_t i = 0; i < a.size(); i++) { const double x = a[i], y = b.at(i + offset); s += x * x; n += (x - y) * (x - y); }
    return 10 * std::log10(s / (n + 1e-9));
}

int main()
{
    StreamAudio a;
    a.Rate = 32728;
    const size_t n = 3 * 14336 + 5000; // three full blocks and a short last one
    a.Channels.assign(2, std::vector<int16_t>(n));
    for (size_t i = 0; i < n; i++)
    {
        const double t = (double)i / a.Rate;
        a.Channels[0][i] = (int16_t)(9000 * std::sin(2 * M_PI * 440 * t) + 3000 * std::sin(2 * M_PI * 1250 * t));
        a.Channels[1][i] = (int16_t)(12000 * std::sin(2 * M_PI * 220 * t) * std::exp(-t));
    }
    a.Loops = true;
    a.LoopStart = 20000; // not on a block: 8672 samples of silence go in front

    uint32_t pad = 0;
    const Bytes file = WriteBcstm(a, &pad);
    check(std::memcmp(&file[0], "CSTM", 4) == 0 && file[8] == 0 && file[9] == 0 && file[10] == 2 && file[11] == 2, "a CSTM of version 0x02020000");
    check(U32(file, 0x0C) == file.size(), "its size in the header");
    check(pad == 14336 * 2 - 20000, "silence in front, to put the loop on a block");
    const StreamAudio b = ReadBcstm(file);
    check(b.Channels.size() == 2 && b.Rate == 32728, "two channels at 32728 Hz read back");
    check(b.Channels[0].size() == n + pad, "the length with the silence");
    check(b.Loops && b.LoopStart == 20000 + pad && b.LoopStart % 14336 == 0, "the loop on block 2");
    bool silent = true;
    for (size_t i = 0; i < pad; i++) silent = silent && std::abs(b.Channels[0][i]) < 2 && std::abs(b.Channels[1][i]) < 2;
    check(silent, "the silence decodes silent");
    const double s0 = SnrDb(a.Channels[0], b.Channels[0], pad), s1 = SnrDb(a.Channels[1], b.Channels[1], pad);
    printf("  SNR %.1f dB and %.1f dB\n", s0, s1);
    check(s0 > 30 && s1 > 30, "both channels decode above 30 dB of their source");
    // the last block: its samples, its bytes (8 a 14 samples) padded to 0x20, as the game's stream info gives them
    const size_t info = U32(file, 0x18), si = info + 8 + U32(file, info + 12);
    const uint32_t blocks = U32(file, si + 16), lastSamples = U32(file, si + 32), lastSize = U32(file, si + 28), lastPadded = U32(file, si + 36);
    check(blocks == 4 && lastSamples == n + pad - 3 * 14336, "four blocks (56680 samples), the last one short");
    check(lastSize == (lastSamples + 13) / 14 * 8 && lastPadded % 0x20 == 0 && lastPadded >= lastSize, "the last block's size and padding");

    bool refused = false;
    try { StreamAudio bad = a; bad.Channels[1].pop_back(); WriteBcstm(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "channels of different lengths: refused");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
