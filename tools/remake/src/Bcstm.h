#ifndef REMAKE_BCSTM_H
#define REMAKE_BCSTM_H

// A 3DS stream (BCSTM, "CSTM", version 0x02020000) as Omega Ruby's music has it (sound/bgm_sg_*.dspadpcm.bcstm, read on
// bgm_sg_c_01, 20 October): a 0x40-byte header with three sections, INFO (stream info: DSP-ADPCM, loop flag, channels,
// rate, loop start, frames, 84 blocks of 0x2000 bytes = 14336 samples a channel, the last block's size and padded size,
// 4-byte seek entries every 14336 samples; then per channel 16 predictor coefficients, the first frame's predictor/scale and
// history, the loop frame's), SEEK (each block's history, yn1 and yn2 per channel) and DATA (the channels interleaved block
// by block, from +0x20). Omega Ruby's loops start on a block (bgm_sg_c_01: 200704 = 14 blocks) and end at the last frame.
//
// DSP-ADPCM: 8-byte frames of 14 samples, a header byte (predictor index << 4 | scale) and 14 signed nibbles; a sample is
// ((nibble << scale) << 11 + 1024 + c1 yn1 + c2 yn2) >> 11, clamped to 16 bits.

#include "Bytes.h"

#include <cstdint>
#include <vector>

namespace remake
{

struct StreamAudio
{
    std::vector<std::vector<int16_t>> Channels; // the same length each
    uint32_t Rate = 32728;
    bool Loops = false;
    uint32_t LoopStart = 0; // in samples; the stream ends at the last sample (the loop's end)
};

// the stream as Omega Ruby's BCSTM; with a loop, silence is put in front so its start falls on a block (logged by the caller
// through `paddedBy`, the samples added)
Bytes WriteBcstm(const StreamAudio& audio, uint32_t* paddedBy = nullptr);

// a BCSTM's samples (DSP-ADPCM only), to check an encoding against its source
StreamAudio ReadBcstm(const Bytes& bcstm);

}

#endif // REMAKE_BCSTM_H
