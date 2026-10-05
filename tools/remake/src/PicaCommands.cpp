#include "PicaCommands.h"

#include "Bytes.h"

#include <cstring>

namespace remake
{

float PicaFloat24(uint32_t v)
{
    uint32_t bits = 0;
    if (v & 0x7FFFFF) bits = (v & 0xFFFF) << 7 | (((v >> 16) & 0x7F) + 64) << 23 | ((v >> 23) & 1) << 31;
    else bits = ((v >> 23) & 1) << 31;
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

uint32_t ToPicaFloat24(float f)
{
    uint32_t bits;
    memcpy(&bits, &f, 4);
    const uint32_t sign = bits >> 31;
    const int exponent = (int)((bits >> 23) & 0xFF) - 127 + 63;
    if ((bits & 0x7FFFFFFF) == 0 || exponent <= 0) return sign << 23; // zero (and values too small for 24 bits)
    if (exponent >= 127) throw FormatError("PICA float24: value out of range");
    return sign << 23 | (uint32_t)exponent << 16 | ((bits >> 7) & 0xFFFF);
}

static float AsFloat(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }

PicaCommands PicaCommands::Parse(const std::vector<uint32_t>& words)
{
    PicaCommands out;
    for (auto& u : out.UniformWords) for (int& w : u) w = -1;
    uint32_t uniformIndex = 0;
    bool uniform32 = false;
    uint32_t f24[3] = {};
    auto uniformData = [&](uint32_t param, size_t word) {
        const uint32_t u = (uniformIndex >> 2) % 96;
        out.Uniform32[u] = uniform32;
        if (uniform32)
        {
            out.VertexUniforms[u][3 - (uniformIndex & 3)] = AsFloat(param); // w, z, y, x
            out.UniformWords[u][3 - (uniformIndex & 3)] = (int)word;
        }
        else
        {
            f24[uniformIndex & 3] = param;
            out.UniformWords[u][uniformIndex & 3] = (int)word;
            if ((uniformIndex & 3) == 2)
            {
                out.VertexUniforms[u][0] = PicaFloat24(f24[2] & 0xFFFFFF);
                out.VertexUniforms[u][1] = PicaFloat24((f24[2] >> 24) | ((f24[1] & 0xFFFF) << 8));
                out.VertexUniforms[u][2] = PicaFloat24((f24[1] >> 16) | ((f24[0] & 0xFF) << 16));
                out.VertexUniforms[u][3] = PicaFloat24(f24[0] >> 8);
                uniformIndex++; // 3 words carry the 4 components
            }
        }
        uniformIndex++;
    };
    // at: the word index of params[0]; the others follow its header (non-consecutive commands only have more than one)
    auto add = [&](uint16_t reg, std::vector<uint32_t> params, size_t at) {
        if (reg == 0x2C0) { uniformIndex = (params[0] & 0xFF) << 2; uniform32 = (params[0] >> 31) != 0; }
        else if (reg >= 0x2C1 && reg <= 0x2C8)
            for (size_t k = 0; k < params.size(); k++) uniformData(params[k], k ? at + 1 + k : at);
        out.List.push_back({reg, std::move(params), at});
    };

    size_t i = 0;
    while (i + 1 < words.size())
    {
        size_t paramAt = i;
        uint32_t param = words[i++];
        const uint32_t header = words[i++];
        uint16_t id = header & 0xFFFF;
        const uint32_t extra = (header >> 20) & 0x7FF;
        if (i + extra > words.size()) throw FormatError("PICA commands: a command runs past its list");
        if (header >> 31) // consecutive registers, one parameter each
        {
            for (uint32_t k = 0; k <= extra; k++)
            {
                add(id++, {param}, paramAt);
                if (k < extra) { paramAt = i; param = words[i++]; }
            }
        }
        else
        {
            std::vector<uint32_t> params{param};
            for (uint32_t k = 0; k < extra; k++) params.push_back(words[i++]);
            add(id, std::move(params), paramAt);
        }
        if (i & 1) i++; // commands are padded to 8 bytes
    }
    return out;
}

uint32_t PicaCommands::Last(uint16_t reg) const
{
    for (auto it = List.rbegin(); it != List.rend(); ++it)
        if (it->Register == reg) return it->Params[0];
    return 0;
}

}
