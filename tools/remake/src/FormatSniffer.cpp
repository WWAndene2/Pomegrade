#include "FormatSniffer.h"

#include "NitroCompression.h"

namespace remake
{

FileKind Sniff(const Bytes& data)
{
    struct Signature { const char* Magic; const char* Id; const char* Description; };
    static const Signature signatures[] = {
        {"NARC", "narc", "Nitro archive (NARC)"},
        {"BMD0", "nsbmd", "3D models (NSBMD)"},
        {"BTX0", "nsbtx", "3D textures (NSBTX)"},
        {"BCA0", "nsbca", "skeletal animation (NSBCA)"},
        {"BTP0", "nsbtp", "texture pattern animation (NSBTP)"},
        {"BTA0", "nsbta", "texture coordinate animation (NSBTA)"},
        {"BMA0", "nsbma", "material animation (NSBMA)"},
        {"BVA0", "nsbva", "visibility animation (NSBVA)"},
        {"RGCN", "ncgr", "2D tiles (NCGR)"},
        {"RLCN", "nclr", "palette (NCLR)"},
        {"RCSN", "nscr", "2D screen / tilemap (NSCR)"},
        {"RECN", "ncer", "2D sprite cells (NCER)"},
        {"RNAN", "nanr", "2D sprite animation (NANR)"},
        {"SDAT", "sdat", "sound data (SDAT)"},
        {"CRAG", "garc", "3DS archive (GARC)"},
        {"BCH\0", "bch", "3DS models and textures (BCH)"},
        {"CGFX", "cgfx", "3DS graphics (CGFX)"},
    };
    if (data.size() >= 4)
        for (const Signature& s : signatures)
            if (data[0] == (uint8_t)s.Magic[0] && data[1] == (uint8_t)s.Magic[1] && data[2] == (uint8_t)s.Magic[2] && data[3] == (uint8_t)s.Magic[3])
                return {s.Id, s.Description};
    if (IsLzCompressed(data))
        return {data[0] == 0x10 ? "lz10" : "lz11", data[0] == 0x10 ? "LZ10-compressed" : "LZ11-compressed"};
    if (data.empty()) return {"empty", "empty"};
    return {"unknown", "unknown"};
}

}
