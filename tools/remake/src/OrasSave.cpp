#include "OrasSave.h"

#include <cstring>
#include <string>

namespace remake
{

static constexpr size_t TableAt = 0x75E14, Situation = 4;

uint16_t Crc16Ccitt(const uint8_t* data, size_t size)
{
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < size; i++)
    {
        c ^= (uint16_t)(data[i] << 8);
        for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

OrasSave OrasSave::Read(const Bytes& data)
{
    if (data.size() != 0x76000 || data.size() < TableAt || std::memcmp(&data[TableAt - 4], "FEEB", 4) != 0)
        throw FormatError("not an ORAS save file (0x76000 bytes, block table at 0x75E14)");
    OrasSave s;
    s.Data = data;
    size_t offset = 0;
    for (size_t at = TableAt; at + 8 <= data.size(); at += 8)
    {
        const uint32_t length = U32(data, at);
        if (length == 0 || offset + length > TableAt) break;
        OrasSaveBlock b{offset, length, U16(data, at + 4), U16(data, at + 6)};
        if (Crc16Ccitt(&data[offset], length) != b.Checksum) throw FormatError("save block " + std::to_string(b.Id) + " does not match its checksum");
        s.Blocks.push_back(b);
        offset = (offset + length + 0x1FF) & ~(size_t)0x1FF;
    }
    if (s.Blocks.size() <= Situation || s.Blocks[Situation].Offset != 0x1400) throw FormatError("the save has no player block at 0x1400");
    return s;
}

int OrasSave::Zone() const { return U16(Data, 0x1402); }
float OrasSave::X() const { float f; std::memcpy(&f, &Data[0x1410], 4); return f; }
float OrasSave::Z() const { float f; std::memcpy(&f, &Data[0x1418], 4); return f; }

void OrasSave::MoveTo(int zone, float tileX, float tileZ)
{
    // the zone is held twice too: +2 and +0xF4, each before its position (+0x10/+0x18 and +0x104/+0x10C). A save made by the game
    // on Route 102 (zone 24, matrix 2) holds 24 at both, the owner's Littleroot save 6 at both; writing only +2 kept the player in
    // place within matrix 1 but a move into matrix 2 never reached the field (run trainer1, 7 October); writing both is not
    // enough for that move (run matrix2, 8 October: still black): other words of the block differ (ORAS_ENGINE.md 0, item 1)
    for (size_t at : {(size_t)0x1402, (size_t)0x14F4}) { Data[at] = (uint8_t)zone; Data[at + 1] = (uint8_t)(zone >> 8); }
    const float x = tileX * 18, z = tileZ * 18;
    // the position is held twice in block 4 (+0x10/+0x18 and +0x104/+0x10C: two saves of the owner, a few steps apart, differ at both
    // with the same values); r5 on the phone kept the player in place when only the first was written
    for (size_t at : {(size_t)0x1410, (size_t)0x1504}) { std::memcpy(&Data[at], &x, 4); std::memcpy(&Data[at + 8], &z, 4); }
    const OrasSaveBlock& b = Blocks[Situation];
    const uint16_t crc = Crc16Ccitt(&Data[b.Offset], b.Length);
    const size_t entry = TableAt + Situation * 8 + 6;
    Data[entry] = (uint8_t)crc; Data[entry + 1] = (uint8_t)(crc >> 8);
    Blocks[Situation].Checksum = crc;
}

}
