#include "Bcstm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace remake
{

namespace
{
constexpr uint32_t FrameSamples = 14, FrameBytes = 8, BlockBytes = 0x2000, BlockSamples = BlockBytes / FrameBytes * FrameSamples;

void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }
void Put32(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b.at(at + k) = (uint8_t)(v >> (8 * k)); }

int16_t Clamp16(int64_t v) { return (int16_t)std::clamp<int64_t>(v, -32768, 32767); }

using Coefs = std::array<std::array<int16_t, 2>, 8>;

// the 8 predictors of a channel: each frame's least-squares two-tap predictor on its 14 samples (with the two before),
// clustered by k-means weighted by the frame's energy, in fixed point 11 bits
Coefs FindCoefs(const std::vector<int16_t>& s)
{
    struct Frame { double a1, a2, weight; };
    std::vector<Frame> frames;
    for (size_t f = 0; f * FrameSamples < s.size(); f++)
    {
        double r11 = 0, r12 = 0, r22 = 0, r01 = 0, r02 = 0, energy = 0;
        for (size_t k = 0; k < FrameSamples; k++)
        {
            const size_t i = f * FrameSamples + k;
            if (i >= s.size()) break;
            const double x = s[i], y1 = i >= 1 ? s[i - 1] : 0, y2 = i >= 2 ? s[i - 2] : 0;
            r11 += y1 * y1; r12 += y1 * y2; r22 += y2 * y2; r01 += x * y1; r02 += x * y2; energy += x * x;
        }
        const double det = r11 * r22 - r12 * r12;
        if (energy < 1 || std::fabs(det) < 1e-6) continue;
        double a1 = (r01 * r22 - r02 * r12) / det, a2 = (r02 * r11 - r01 * r12) / det;
        a1 = std::clamp(a1, -3.9, 3.9); a2 = std::clamp(a2, -3.9, 3.9);
        frames.push_back({a1, a2, std::log1p(energy)});
    }
    Coefs out{};
    // starting predictors: silence-friendly zero, then the classic first and second order ones
    std::array<std::array<double, 2>, 8> c = {{{0, 0}, {1, 0}, {2, -1}, {1.5, -0.6}, {1.8, -0.85}, {0.9, 0}, {1.95, -0.97}, {1.2, -0.3}}};
    if (!frames.empty())
        for (int pass = 0; pass < 24; pass++)
        {
            std::array<std::array<double, 3>, 8> sum{};
            for (const Frame& f : frames)
            {
                size_t best = 0;
                double bestDistance = std::numeric_limits<double>::max();
                for (size_t k = 0; k < 8; k++)
                {
                    const double d = (f.a1 - c[k][0]) * (f.a1 - c[k][0]) + (f.a2 - c[k][1]) * (f.a2 - c[k][1]);
                    if (d < bestDistance) { bestDistance = d; best = k; }
                }
                sum[best][0] += f.a1 * f.weight; sum[best][1] += f.a2 * f.weight; sum[best][2] += f.weight;
            }
            for (size_t k = 0; k < 8; k++) if (sum[k][2] > 0) c[k] = {sum[k][0] / sum[k][2], sum[k][1] / sum[k][2]};
        }
    for (size_t k = 0; k < 8; k++)
        for (size_t j = 0; j < 2; j++) out[k][j] = (int16_t)std::clamp<long>(std::lround(c[k][j] * 2048), -32768, 32767);
    return out;
}

struct Encoded { Bytes Data; std::vector<std::array<int16_t, 2>> History; uint8_t FirstHeader = 0; }; // History: per frame, before it

// the channel's frames, each the predictor and scale giving the least squared error against the decoder's own history
Encoded EncodeChannel(const std::vector<int16_t>& s, const Coefs& coefs)
{
    Encoded e;
    const size_t frames = (s.size() + FrameSamples - 1) / FrameSamples;
    e.Data.assign(frames * FrameBytes, 0);
    int32_t yn1 = 0, yn2 = 0;
    for (size_t f = 0; f < frames; f++)
    {
        e.History.push_back({(int16_t)yn1, (int16_t)yn2});
        int64_t bestError = std::numeric_limits<int64_t>::max();
        std::array<uint8_t, FrameBytes> best{};
        int32_t bestYn1 = 0, bestYn2 = 0;
        for (size_t p = 0; p < 8; p++)
        {
            const int32_t c1 = coefs[p][0], c2 = coefs[p][1];
            // the largest residual with the source's own history, for a first scale
            int32_t peak = 0;
            for (size_t k = 0; k < FrameSamples; k++)
            {
                const size_t i = f * FrameSamples + k;
                const int32_t x = i < s.size() ? s[i] : 0;
                const int32_t h1 = k >= 1 ? s[i - 1] : (k == 0 ? yn1 : 0), h2 = k >= 2 ? s[i - 2] : (k == 0 ? yn2 : yn1);
                peak = std::max(peak, std::abs(x - (int32_t)(((int64_t)c1 * h1 + (int64_t)c2 * h2) >> 11)));
            }
            int first = 0;
            while (first < 12 && (7 << first) < peak) first++;
            for (int scale = std::max(0, first - 1); scale <= std::min(12, first + 1); scale++)
            {
                std::array<uint8_t, FrameBytes> frame{};
                frame[0] = (uint8_t)(p << 4 | scale);
                int32_t h1 = yn1, h2 = yn2;
                int64_t error = 0;
                for (size_t k = 0; k < FrameSamples; k++)
                {
                    const size_t i = f * FrameSamples + k;
                    const int32_t x = i < s.size() ? s[i] : 0;
                    const int64_t predicted = (int64_t)c1 * h1 + (int64_t)c2 * h2;
                    const double ideal = ((double)x * 2048 - predicted - 1024) / (2048.0 * (1 << scale));
                    const int nibble = (int)std::clamp<long>(std::lround(ideal), -8, 7);
                    const int16_t out = Clamp16((((int64_t)nibble << scale) * 2048 + 1024 + predicted) >> 11);
                    error += (int64_t)(out - x) * (out - x);
                    h2 = h1; h1 = out;
                    frame[1 + k / 2] |= (uint8_t)((nibble & 0xF) << (k % 2 ? 0 : 4));
                }
                if (error < bestError) { bestError = error; best = frame; bestYn1 = h1; bestYn2 = h2; }
            }
        }
        std::memcpy(&e.Data[f * FrameBytes], best.data(), FrameBytes);
        if (f == 0) e.FirstHeader = best[0];
        yn1 = bestYn1; yn2 = bestYn2;
    }
    return e;
}
}

Bytes WriteBcstm(const StreamAudio& audio, uint32_t* paddedBy)
{
    if (audio.Channels.empty() || audio.Channels.size() > 8) throw FormatError("a stream of 1 to 8 channels");
    std::vector<std::vector<int16_t>> ch = audio.Channels;
    uint32_t pad = 0;
    if (audio.Loops && audio.LoopStart % BlockSamples) pad = BlockSamples - audio.LoopStart % BlockSamples;
    for (const auto& c : ch) if (c.size() != ch[0].size()) throw FormatError("a stream's channels of different lengths");
    for (auto& c : ch) c.insert(c.begin(), pad, 0);
    if (paddedBy) *paddedBy = pad;
    const uint32_t samples = (uint32_t)ch[0].size(), loopStart = audio.Loops ? audio.LoopStart + pad : 0;
    if (samples == 0 || loopStart >= samples) throw FormatError("a stream with no samples, or its loop starting past its end");
    const uint32_t channels = (uint32_t)ch.size(), blocks = (samples + BlockSamples - 1) / BlockSamples;
    const uint32_t lastSamples = samples - (blocks - 1) * BlockSamples;
    const uint32_t lastSize = (lastSamples + FrameSamples - 1) / FrameSamples * FrameBytes, lastPadded = (lastSize + 0x1F) & ~0x1Fu;

    std::vector<Coefs> coefs;
    std::vector<Encoded> encoded;
    for (const auto& c : ch) { coefs.push_back(FindCoefs(c)); encoded.push_back(EncodeChannel(c, coefs.back())); }

    // INFO: header, three references, the stream info, the channel table, each channel's DSP-ADPCM info (as bgm_sg_c_01's)
    const size_t infoBody = 0x48, streamInfo = infoBody + 0x18, channelTable = infoBody + 0x5C;
    const size_t channelRefs = channelTable + 4 + 8 * channels, dspInfos = channelRefs + 8 * channels;
    const size_t infoEnd = (dspInfos + 0x2E * channels + 0x1F) & ~(size_t)0x1F, infoSize = infoEnd - 0x40;
    const size_t seekAt = infoEnd, seekSize = (8 + 4 * channels * (size_t)blocks + 0x1F) & ~(size_t)0x1F;
    const size_t dataAt = seekAt + seekSize, dataSize = 0x20 + (size_t)(blocks - 1) * BlockBytes * channels + (size_t)lastPadded * channels;
    Bytes b(dataAt + dataSize, 0);
    std::memcpy(&b[0], "CSTM", 4);
    Put16(b, 4, 0xFEFF); Put16(b, 6, 0x40); Put32(b, 8, 0x02020000); Put32(b, 0x0C, (uint32_t)b.size()); Put16(b, 0x10, 3);
    const size_t sections[3][3] = {{0x4000, 0x40, infoSize}, {0x4001, seekAt, seekSize}, {0x4002, dataAt, dataSize}};
    for (size_t k = 0; k < 3; k++) { Put16(b, 0x14 + 12 * k, (uint16_t)sections[k][0]); Put32(b, 0x18 + 12 * k, (uint32_t)sections[k][1]); Put32(b, 0x1C + 12 * k, (uint32_t)sections[k][2]); }

    std::memcpy(&b[0x40], "INFO", 4); Put32(b, 0x44, (uint32_t)infoSize);
    Put16(b, infoBody, 0x4100); Put32(b, infoBody + 4, 0x18);              // the stream info
    Put16(b, infoBody + 8, 0); Put32(b, infoBody + 12, 0xFFFFFFFF);        // no track info
    Put16(b, infoBody + 16, 0x0101); Put32(b, infoBody + 20, 0x5C);        // the channel table
    b[streamInfo] = 2; b[streamInfo + 1] = audio.Loops ? 1 : 0; b[streamInfo + 2] = (uint8_t)channels;
    Put32(b, streamInfo + 4, audio.Rate); Put32(b, streamInfo + 8, loopStart); Put32(b, streamInfo + 12, samples);
    Put32(b, streamInfo + 16, blocks); Put32(b, streamInfo + 20, BlockBytes); Put32(b, streamInfo + 24, BlockSamples);
    Put32(b, streamInfo + 28, lastSize); Put32(b, streamInfo + 32, lastSamples); Put32(b, streamInfo + 36, lastPadded);
    Put32(b, streamInfo + 40, 4); Put32(b, streamInfo + 44, BlockSamples);
    Put16(b, streamInfo + 48, 0x1F00); Put32(b, streamInfo + 52, 0x18);   // the samples: DATA + 0x18 past its body
    Put16(b, streamInfo + 56, 0x0100);                                     // as bgm_sg_c_01: 00 01 00 00, then 0
    Put32(b, channelTable, channels);
    for (uint32_t c = 0; c < channels; c++)
    {
        const size_t ref = channelRefs + 8 * c, dsp = dspInfos + 0x2E * c;
        Put16(b, channelTable + 4 + 8 * c, 0x4102); Put32(b, channelTable + 8 + 8 * c, (uint32_t)(ref - channelTable));
        Put16(b, ref, 0x0300); Put32(b, ref + 4, (uint32_t)(dsp - ref));
        for (size_t k = 0; k < 8; k++) { Put16(b, dsp + 4 * k, (uint16_t)coefs[c][k][0]); Put16(b, dsp + 4 * k + 2, (uint16_t)coefs[c][k][1]); }
        Put16(b, dsp + 0x20, encoded[c].FirstHeader);
        const size_t loopFrame = loopStart / FrameSamples;
        Put16(b, dsp + 0x26, encoded[c].Data[loopFrame * FrameBytes]);
        Put16(b, dsp + 0x28, (uint16_t)encoded[c].History[loopFrame][0]); Put16(b, dsp + 0x2A, (uint16_t)encoded[c].History[loopFrame][1]);
    }

    std::memcpy(&b[seekAt], "SEEK", 4); Put32(b, seekAt + 4, (uint32_t)seekSize);
    for (uint32_t k = 0; k < blocks; k++)
        for (uint32_t c = 0; c < channels; c++)
        {
            const auto& h = encoded[c].History[(size_t)k * BlockSamples / FrameSamples];
            Put16(b, seekAt + 8 + 4 * ((size_t)k * channels + c), (uint16_t)h[0]); Put16(b, seekAt + 10 + 4 * ((size_t)k * channels + c), (uint16_t)h[1]);
        }

    std::memcpy(&b[dataAt], "DATA", 4); Put32(b, dataAt + 4, (uint32_t)dataSize);
    size_t at = dataAt + 0x20;
    for (uint32_t k = 0; k < blocks; k++)
    {
        const size_t size = k + 1 < blocks ? BlockBytes : lastSize, step = k + 1 < blocks ? BlockBytes : lastPadded;
        for (uint32_t c = 0; c < channels; c++) { std::memcpy(&b[at], &encoded[c].Data[(size_t)k * BlockBytes], size); at += step; }
    }
    return b;
}

StreamAudio ReadBcstm(const Bytes& b)
{
    if (b.size() < 0x40 || std::memcmp(&b[0], "CSTM", 4) != 0) throw FormatError("not a BCSTM");
    const size_t info = U32(b, 0x18), data = U32(b, 0x30), body = info + 8;
    const size_t si = body + U32(b, body + 4);
    if (b.at(si) != 2) throw FormatError("a BCSTM not in DSP-ADPCM");
    StreamAudio a;
    const uint32_t channels = b[si + 2];
    a.Loops = b[si + 1] != 0; a.Rate = U32(b, si + 4); a.LoopStart = U32(b, si + 8);
    const uint32_t samples = U32(b, si + 12), blocks = U32(b, si + 16), blockBytes = U32(b, si + 20), blockSamples = U32(b, si + 24);
    const uint32_t lastPadded = U32(b, si + 36);
    const size_t table = body + U32(b, body + 20);
    a.Channels.assign(channels, {});
    for (uint32_t c = 0; c < channels; c++)
    {
        const size_t ref = table + U32(b, table + 8 + 8 * c), dsp = ref + U32(b, ref + 4);
        int32_t coef[16];
        for (int k = 0; k < 16; k++) coef[k] = (int16_t)U16(b, dsp + 2 * k);
        int32_t yn1 = (int16_t)U16(b, dsp + 0x22), yn2 = (int16_t)U16(b, dsp + 0x24);
        std::vector<int16_t>& out = a.Channels[c];
        for (uint32_t k = 0; k < blocks && out.size() < samples; k++)
        {
            const size_t start = data + 0x20 + (size_t)k * blockBytes * channels + (size_t)c * (k + 1 < blocks ? blockBytes : lastPadded);
            for (uint32_t f = 0; f * FrameSamples < blockSamples && out.size() < samples; f++)
            {
                const uint8_t header = b.at(start + f * FrameBytes);
                const int p = header >> 4, scale = header & 0xF;
                for (uint32_t n = 0; n < FrameSamples && out.size() < samples; n++)
                {
                    const uint8_t byte = b.at(start + f * FrameBytes + 1 + n / 2);
                    int nibble = n % 2 ? byte & 0xF : byte >> 4;
                    if (nibble >= 8) nibble -= 16;
                    const int16_t s = Clamp16((((int64_t)nibble << scale) * 2048 + 1024 + (int64_t)coef[2 * p] * yn1 + (int64_t)coef[2 * p + 1] * yn2) >> 11);
                    out.push_back(s); yn2 = yn1; yn1 = s;
                }
            }
        }
    }
    return a;
}

}
