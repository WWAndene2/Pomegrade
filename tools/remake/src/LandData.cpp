#include "LandData.h"

namespace remake
{

static float Fx32(const Bytes& d, size_t at) { return (float)(int32_t)U32(d, at) / 65536.0f; }

LandData LandData::Read(const Bytes& d)
{
    const uint32_t permSize = U32(d, 0), buildSize = U32(d, 4), modelSize = U32(d, 8), heightSize = U32(d, 12);
    if (permSize != LandTiles * LandTiles * 2) throw FormatError("land data: permissions of " + std::to_string(permSize) + " bytes");
    if (buildSize % 0x30) throw FormatError("land data: buildings of " + std::to_string(buildSize) + " bytes");
    LandData l;
    size_t at = 16;
    for (uint32_t i = 0; i < LandTiles * LandTiles; i++) l.Permissions.push_back(U16(d, at + i * 2));
    at += permSize;
    for (size_t b = 0; b < buildSize / 0x30; b++)
    {
        const size_t e = at + b * 0x30;
        LandBuilding x;
        x.Model = U32(d, e);
        for (int k = 0; k < 3; k++) x.Position[k] = Fx32(d, e + 4 + k * 4);
        for (int k = 0; k < 8; k++) x.Raw[k] = U32(d, e + 16 + k * 4);
        l.Buildings.push_back(x);
    }
    at += buildSize;
    l.Model = Slice(d, at, modelSize);
    at += modelSize;
    l.Heights = Slice(d, at, heightSize);
    return l;
}

}
