#include "MapMatrix.h"

namespace remake
{

MapMatrix MapMatrix::Read(const Bytes& d)
{
    MapMatrix m;
    m.Width = U8(d, 0);
    m.Height = U8(d, 1);
    const bool headers = U8(d, 2) != 0, heights = U8(d, 3) != 0;
    const size_t nameLength = U8(d, 4);
    m.Name = Text(d, 5, nameLength);
    const size_t cells = (size_t)m.Width * m.Height;
    if (cells == 0) throw FormatError("map matrix: empty");
    size_t at = 5 + nameLength;
    m.Headers.assign(cells, -1);
    m.Heights.assign(cells, 0);
    m.LandData.assign(cells, -1);
    if (headers) for (size_t i = 0; i < cells; i++, at += 2) m.Headers[i] = U16(d, at);
    if (heights) for (size_t i = 0; i < cells; i++, at += 1) m.Heights[i] = U8(d, at);
    for (size_t i = 0; i < cells; i++, at += 2)
    {
        const uint16_t v = U16(d, at);
        m.LandData[i] = v == 0xFFFF ? -1 : v;
    }
    if (at != d.size()) throw FormatError("map matrix: " + std::to_string(d.size() - at) + " bytes left over");
    return m;
}

}
