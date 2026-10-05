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

static float AsFloat(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }

PicaCommands PicaCommands::Parse(const std::vector<uint32_t>& words)
{
    PicaCommands out;
    uint32_t uniformIndex = 0;
    bool uniform32 = false;
    uint32_t f24[3] = {};
    auto uniformData = [&](uint32_t param) {
        const uint32_t u = (uniformIndex >> 2) % 96;
        if (uniform32)
        {
            out.VertexUniforms[u][3 - (uniformIndex & 3)] = AsFloat(param); // w, z, y, x
        }
        else
        {
            f24[uniformIndex & 3] = param;
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
    auto add = [&](uint16_t reg, std::vector<uint32_t> params) {
        if (reg == 0x2C0) { uniformIndex = (params[0] & 0xFF) << 2; uniform32 = (params[0] >> 31) != 0; }
        else if (reg >= 0x2C1 && reg <= 0x2C8) for (uint32_t p : params) uniformData(p);
        out.List.push_back({reg, std::move(params)});
    };

    size_t i = 0;
    while (i + 1 < words.size())
    {
        uint32_t param = words[i++];
        const uint32_t header = words[i++];
        uint16_t id = header & 0xFFFF;
        const uint32_t extra = (header >> 20) & 0x7FF;
        if (i + extra > words.size()) throw FormatError("PICA commands: a command runs past its list");
        if (header >> 31) // consecutive registers, one parameter each
        {
            for (uint32_t k = 0; k <= extra; k++)
            {
                add(id++, {param});
                if (k < extra) param = words[i++];
            }
        }
        else
        {
            std::vector<uint32_t> params{param};
            for (uint32_t k = 0; k < extra; k++) params.push_back(words[i++]);
            add(id, std::move(params));
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
