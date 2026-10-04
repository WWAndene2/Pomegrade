#include "GxDisplayList.h"

namespace remake
{

int GxParamCount(uint8_t c)
{
    switch (c)
    {
    case 0x00: case 0x11: case 0x15: case 0x41: return 0;
    case 0x10: case 0x12: case 0x13: case 0x14: return 1;
    case 0x16: case 0x18: return 16;
    case 0x17: case 0x19: return 12;
    case 0x1A: return 9;
    case 0x1B: case 0x1C: return 3;
    case 0x20: case 0x21: case 0x22: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: return 1;
    case 0x23: return 2;
    case 0x29: case 0x2A: case 0x2B: return 1;
    case 0x30: case 0x31: case 0x32: case 0x33: return 1;
    case 0x34: return 32;
    case 0x40: case 0x50: case 0x60: return 1;
    case 0x70: return 3;
    case 0x71: return 2;
    case 0x72: return 1;
    default: return -1;
    }
}

static int Sext(uint32_t v, int bits) { return (int)(v << (32 - bits)) >> (32 - bits); }

GxMesh DecodeDisplayList(const Bytes& list)
{
    GxMesh mesh;
    GxVertex cur;
    int pos[3] = {};           // 4.12
    int primitive = -1;        // 0 triangles, 1 quads, 2 triangle strip, 3 quad strip
    std::vector<uint32_t> prim; // vertex indices of the current primitive
    auto emit = [&]() {
        for (int k = 0; k < 3; k++) cur.Position[k] = pos[k] / 4096.0f;
        const uint32_t index = (uint32_t)mesh.Vertices.size();
        mesh.Vertices.push_back(cur);
        prim.push_back(index);
        const size_t n = prim.size();
        auto tri = [&](uint32_t a, uint32_t b, uint32_t c) { mesh.Triangles.insert(mesh.Triangles.end(), {a, b, c}); };
        switch (primitive)
        {
        case 0: if (n % 3 == 0) tri(prim[n - 3], prim[n - 2], prim[n - 1]); break;
        case 1: if (n % 4 == 0) { tri(prim[n - 4], prim[n - 3], prim[n - 2]); tri(prim[n - 4], prim[n - 2], prim[n - 1]); } break;
        case 2: // strip: every other triangle reversed to keep the winding
            if (n >= 3) { if (n % 2) tri(prim[n - 3], prim[n - 2], prim[n - 1]); else tri(prim[n - 2], prim[n - 3], prim[n - 1]); }
            break;
        case 3: // quad strip: each new pair closes a quad (v0 v1 v3 v2)
            if (n >= 4 && n % 2 == 0) { tri(prim[n - 4], prim[n - 3], prim[n - 1]); tri(prim[n - 4], prim[n - 1], prim[n - 2]); }
            break;
        default: throw FormatError("display list: vertex outside BEGIN_VTXS");
        }
    };

    size_t at = 0;
    while (at + 4 <= list.size())
    {
        const uint32_t packed = U32(list, at);
        at += 4;
        for (int slot = 0; slot < 4; slot++)
        {
            const uint8_t c = (packed >> (8 * slot)) & 0xFF;
            const int n = GxParamCount(c);
            if (n < 0) throw FormatError("display list: unknown command " + std::to_string(c));
            if (c == 0) continue;
            mesh.Commands++;
            std::vector<uint32_t> p(n);
            for (int i = 0; i < n; i++) { p[i] = U32(list, at); at += 4; }
            switch (c)
            {
            case 0x14: cur.Joint = p[0] & 31; break;
            case 0x20: cur.Colour = p[0] & 0x7FFF; break;
            case 0x21: for (int k = 0; k < 3; k++) cur.Normal[k] = Sext(p[0] >> (10 * k), 10) / 512.0f; break;
            case 0x22: cur.TexCoord[0] = (int16_t)(p[0] & 0xFFFF) / 16.0f; cur.TexCoord[1] = (int16_t)(p[0] >> 16) / 16.0f; break;
            case 0x23: pos[0] = (int16_t)(p[0] & 0xFFFF); pos[1] = (int16_t)(p[0] >> 16); pos[2] = (int16_t)(p[1] & 0xFFFF); emit(); break;
            case 0x24: for (int k = 0; k < 3; k++) pos[k] = Sext(p[0] >> (10 * k), 10) << 6; emit(); break;
            case 0x25: pos[0] = (int16_t)(p[0] & 0xFFFF); pos[1] = (int16_t)(p[0] >> 16); emit(); break;
            case 0x26: pos[0] = (int16_t)(p[0] & 0xFFFF); pos[2] = (int16_t)(p[0] >> 16); emit(); break;
            case 0x27: pos[1] = (int16_t)(p[0] & 0xFFFF); pos[2] = (int16_t)(p[0] >> 16); emit(); break;
            case 0x28: for (int k = 0; k < 3; k++) pos[k] += Sext(p[0] >> (10 * k), 10); emit(); break;
            case 0x40: primitive = p[0] & 3; prim.clear(); break;
            case 0x41: prim.clear(); break;
            default: break; // matrices, materials, lights: not geometry
            }
        }
    }
    return mesh;
}

}
