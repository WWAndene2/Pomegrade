#include "OrasSave.h"
#include "OrasEngine.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace remake
{

static constexpr size_t Situation = 4, MoveModels = 10, FooterSize = 0x200;

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
    // the game's file is 0x76000 bytes; one with a grown block 10 (ResizeMoveModels) is longer, its footer still the last 0x200
    if (data.size() < 0x76000 || data.size() % FooterSize || std::memcmp(&data[data.size() - FooterSize + 0x10], "FEEB", 4) != 0)
        throw FormatError("not an ORAS save file (0x76000 bytes or a grown one, FEEB 0x1F0 bytes before its end)");
    OrasSave s;
    s.Data = data;
    const size_t table = s.TableAt(), footer = data.size() - FooterSize;
    size_t offset = 0;
    for (size_t at = table; at + 8 <= data.size(); at += 8)
    {
        const uint32_t length = U32(data, at);
        if (length == 0 || offset + length > footer) break;
        OrasSaveBlock b{offset, length, U16(data, at + 4), U16(data, at + 6)};
        if (Crc16Ccitt(&data[offset], length) != b.Checksum) throw FormatError("save block " + std::to_string(b.Id) + " does not match its checksum");
        s.Blocks.push_back(b);
        offset = (offset + length + 0x1FF) & ~(size_t)0x1FF;
    }
    if (s.Blocks.size() <= MoveModels || s.Blocks[Situation].Offset != 0x1400) throw FormatError("the save has no player block at 0x1400");
    // the game places the footer right after the last block (aligned), so a file whose blocks end elsewhere is not one it wrote
    if (offset != footer) throw FormatError("the save's blocks do not end at its footer (a file of another layout?)");
    return s;
}

int OrasSave::Zone() const { return U16(Data, 0x1402); }
float OrasSave::X() const { float f; std::memcpy(&f, &Data[0x1410], 4); return f; }
float OrasSave::Z() const { float f; std::memcpy(&f, &Data[0x1418], 4); return f; }

void OrasSave::TakePlayerBlockFrom(const OrasSave& other)
{
    const OrasSaveBlock& b = Blocks[Situation];
    if (other.Blocks[Situation].Offset != b.Offset || other.Blocks[Situation].Length != b.Length) throw FormatError("the template save's player block differs in place or size");
    std::memcpy(&Data[b.Offset], &other.Data[b.Offset], b.Length);
    // MoveTo writes the checksum again
}

void OrasSave::TakeBlockFrom(const OrasSave& other, uint16_t id)
{
    for (size_t i = 0; i < Blocks.size(); i++)
        if (Blocks[i].Id == id)
        {
            if (i >= other.Blocks.size() || other.Blocks[i].Offset != Blocks[i].Offset || other.Blocks[i].Length != Blocks[i].Length)
                throw FormatError("the template save's block " + std::to_string(id) + " differs in place or size");
            std::memcpy(&Data[Blocks[i].Offset], &other.Data[Blocks[i].Offset], Blocks[i].Length);
            return;
        }
    throw FormatError("the save has no block " + std::to_string(id));
}

void OrasSave::WriteChecksums()
{
    for (size_t i = 0; i < Blocks.size(); i++)
    {
        const uint16_t crc = Crc16Ccitt(&Data[Blocks[i].Offset], Blocks[i].Length);
        const size_t entry = TableAt() + i * 8 + 6;
        Data[entry] = (uint8_t)crc; Data[entry + 1] = (uint8_t)(crc >> 8);
        Blocks[i].Checksum = crc;
    }
}

size_t OrasMoveModelBytes(size_t records)
{
    if (records < 32 || records > 256) throw FormatError("save records: 32 to 256");
    size_t bytes = records * OrasMoveModelRecord;
    while (!ArmImmediate((uint32_t)bytes)) bytes += 4;
    return bytes;
}

void OrasSave::ResizeMoveModels(size_t records)
{
    const size_t length = OrasMoveModelBytes(records);
    const auto aligned = [](size_t n) { return (n + 0x1FF) & ~(size_t)0x1FF; };
    size_t end = 0;
    for (size_t i = 0; i < Blocks.size(); i++) end = aligned(end + (i == MoveModels ? length : Blocks[i].Length));
    Bytes out(end + FooterSize, 0);
    std::memcpy(&out[end], &Data[Data.size() - FooterSize], FooterSize);
    std::vector<OrasSaveBlock> blocks = Blocks;
    size_t offset = 0;
    for (size_t i = 0; i < blocks.size(); i++)
    {
        OrasSaveBlock& b = blocks[i];
        const size_t newLength = i == MoveModels ? length : b.Length;
        std::memcpy(&out[offset], &Data[b.Offset], std::min(b.Length, newLength));
        b.Offset = offset;
        b.Length = newLength;
        offset = aligned(offset + newLength);
    }
    Data = std::move(out);
    Blocks = std::move(blocks);
    const size_t entry = TableAt() + MoveModels * 8;
    for (int k = 0; k < 4; k++) Data[entry + k] = (uint8_t)(length >> (8 * k));
    WriteChecksums();
}

void OrasSave::MoveTo(int zone, float tileX, float tileZ)
{
    // into another zone: block 10 holds the characters of the zone the save was made in, 0x108 bytes each (u16 +4 the
    // character's number, 0xFF the player; u16 +6 the zone; u16 +8 the model), and the game restores them on loading; kept,
    // a move into another matrix stays black once DllField is loaded (runs matrix2, zA). The player's record stays in its
    // place with the new zone and the others are emptied, so the game places the destination's characters itself: the owner's
    // Littleroot save so moved to Petalburg (zone 13, matrix 2) loads (save_test.sh, run st_zC, 8 October). Emptying them all
    // and moving the player's record to the first place made the save unreadable (run zB). The record layout is seen, not read.
    if (zone != Zone())
        for (const OrasSaveBlock& b : Blocks)
            if (b.Id == 10)
                for (size_t at = b.Offset; at + 0x108 <= b.Offset + b.Length; at += 0x108)
                {
                    if (U16(Data, at + 4) == 0xFF && U16(Data, at + 6) == Zone()) { Data[at + 6] = (uint8_t)zone; Data[at + 7] = (uint8_t)(zone >> 8); }
                    else std::memset(&Data[at], 0, 0x108);
                }
    // the zone is held twice too: +2 and +0xF4, each before its position (+0x10/+0x18 and +0x104/+0x10C). A save made by the game
    // on Route 102 (zone 24, matrix 2) holds 24 at both, the owner's Littleroot save 6 at both; writing only +2 kept the player in
    // place within matrix 1 but a move into matrix 2 never reached the field (run trainer1, 7 October); writing both is not
    // enough for that move (run matrix2, 8 October: still black): block 10 must come from a save made in the target matrix
    // (oras-save --template, runs matrix3-8; ORAS_ENGINE.md 0)
    for (size_t at : {(size_t)0x1402, (size_t)0x14F4}) { Data[at] = (uint8_t)zone; Data[at + 1] = (uint8_t)(zone >> 8); }
    const float x = tileX * 18, z = tileZ * 18;
    // the position is held twice in block 4 (+0x10/+0x18 and +0x104/+0x10C: two saves of the owner, a few steps apart, differ at both
    // with the same values); r5 on the phone kept the player in place when only the first was written
    for (size_t at : {(size_t)0x1410, (size_t)0x1504}) { std::memcpy(&Data[at], &x, 4); std::memcpy(&Data[at + 8], &z, 4); }
    WriteChecksums();
}

}
