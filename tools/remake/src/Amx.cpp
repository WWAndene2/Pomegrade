#include "Amx.h"

namespace remake
{

AmxInfo AmxInfo::Read(const Bytes& b)
{
    if (b.size() < 56) throw FormatError("AMX: header too short");
    AmxInfo a;
    a.Size = U32(b, 0); a.Magic = U16(b, 4); a.FileVersion = b[6]; a.AmxVersion = b[7]; a.Flags = U16(b, 8); a.DefSize = U16(b, 10);
    if (a.Magic != 0xF1E0) throw FormatError("AMX: magic " + std::to_string(a.Magic) + " is not 0xF1E0");
    a.Cod = U32(b, 12); a.Dat = U32(b, 16); a.Hea = U32(b, 20); a.Stp = U32(b, 24); a.Cip = U32(b, 28);
    a.Publics = U32(b, 32); a.Natives = U32(b, 36); a.Libraries = U32(b, 40);
    if (a.Size > b.size()) throw FormatError("AMX: size " + std::to_string(a.Size) + " past the " + std::to_string(b.size()) + " bytes given");
    if (a.DefSize == 0 || a.Publics > a.Natives || a.Natives > a.Libraries || a.Cod > a.Dat || a.Dat > a.Hea)
        throw FormatError("AMX: header fields out of order");
    return a;
}

}
