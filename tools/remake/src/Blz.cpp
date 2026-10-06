#include "Blz.h"

namespace remake
{

Bytes BlzDecompress(const Bytes& in)
{
    if (in.size() < 8) throw FormatError("BLZ: shorter than its footer");
    const uint32_t bounds = U32(in, in.size() - 8), added = U32(in, in.size() - 4);
    const size_t footer = bounds >> 24, region = bounds & 0xFFFFFF;
    if (footer > in.size() || region > in.size() || footer > region) throw FormatError("BLZ: footer out of bounds");
    Bytes out(in.begin(), in.end());
    out.resize(in.size() + added);
    size_t src = in.size() - footer, dst = out.size();
    const size_t stop = in.size() - region;
    while (src > stop)
    {
        uint8_t control = out[--src];
        for (int k = 0; k < 8 && src > stop; k++, control <<= 1)
        {
            if (control & 0x80)
            {
                if (src < stop + 2) throw FormatError("BLZ: a reference runs past the packed region");
                src -= 2;
                const uint32_t seg = out[src] | out[src + 1] << 8;
                const size_t length = ((seg >> 12) & 0xF) + 3, distance = (seg & 0xFFF) + 2;
                for (size_t j = 0; j < length; j++)
                {
                    if (dst + distance >= out.size() || dst == 0) throw FormatError("BLZ: a reference out of the output");
                    const uint8_t b = out[dst + distance]; // read before the step, as ctrtool does
                    out[--dst] = b;
                }
            }
            else
            {
                if (dst == 0) throw FormatError("BLZ: output underrun");
                out[--dst] = out[--src];
            }
        }
    }
    return out;
}

}
