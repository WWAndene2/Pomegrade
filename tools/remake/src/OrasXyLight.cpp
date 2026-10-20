#include "OrasXyLight.h"

#include <cstring>

namespace remake
{

Bytes XyLightToOras(const Bytes& x, const Bytes& oras)
{
    constexpr size_t Sets = 4, Planes = 15, Kinds = 3, Times = 4;
    constexpr size_t OrasPlane = 0x30, OrasSet = Planes * OrasPlane, XyPlane = 0x40, XySet = Planes * XyPlane;
    if (oras.size() < Sets * OrasSet) throw FormatError("an Omega Ruby light file of " + std::to_string(oras.size()) + " bytes, not 2944");
    if (x.size() < Sets * XySet) throw FormatError("an X/Y light file of " + std::to_string(x.size()) + " bytes, not 9728");
    Bytes out = oras;
    for (size_t s = 0; s < Sets; s++)
        for (size_t p = 0; p < Planes; p++)
            for (size_t k = 0; k < Kinds; k++)
                std::memcpy(&out[s * OrasSet + p * OrasPlane + k * Times * 4], &x[s * XySet + p * XyPlane + k * Times * 4], Times * 4);
    return out;
}

}
