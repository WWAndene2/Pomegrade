#ifndef REMAKE_PICACOMMANDS_H
#define REMAKE_PICACOMMANDS_H

// The 3DS GPU's (PICA200) command lists, as BCH models store their vertex
// layouts, index buffers and textures: pairs of words (a parameter, then a
// header: register id in bits 0-15, mask 16-19, extra parameter count 20-30,
// bit 31 "consecutive": the extra parameters go to the following registers),
// each command padded to 8 bytes. The vertex shader's float uniforms are
// tracked as they are loaded (register 0x2C0 index, 0x2C1-0x2C8 data; 32-bit
// floats when the index's bit 31 is set, else 24-bit floats packed 4 in 3
// words). Layout from SPICA (public domain), PICACommandReader.

#include <cstdint>
#include <vector>

namespace remake
{

struct PicaCommand
{
    uint16_t Register = 0;
    std::vector<uint32_t> Params;
    size_t At = 0; // the word index of Params[0] in the list (a writer patches it there)
};

struct PicaCommands
{
    std::vector<PicaCommand> List;
    float VertexUniforms[96][4] = {}; // x, y, z, w

    static PicaCommands Parse(const std::vector<uint32_t>& words);
    // the last value written to a register (0 when none)
    uint32_t Last(uint16_t reg) const;
};

// a PICA 24-bit float (1 sign, 7 exponent, 16 mantissa bits)
float PicaFloat24(uint32_t v);

}

#endif // REMAKE_PICACOMMANDS_H
