#include "CodePatch.h"

namespace remake
{

static uint32_t Word(const Bytes& b, size_t at) { return b.at(at) | b.at(at + 1) << 8 | b.at(at + 2) << 16 | (uint32_t)b.at(at + 3) << 24; }

Bytes CodePatchIps(const Bytes& code, const std::vector<CodeWord>& words)
{
    Bytes ips = {'P', 'A', 'T', 'C', 'H'};
    for (const CodeWord& w : words)
    {
        const size_t at = w.Address - CodeBase;
        if (w.Address < CodeBase || at + 4 > code.size() || at >= 0x1000000) throw FormatError("code patch: address outside the code");
        if (Word(code, at) != w.Expected)
            throw FormatError("code patch (" + w.Why + "): the game's word is not the expected one; another version of the game?");
        const uint8_t record[] = {(uint8_t)(at >> 16), (uint8_t)(at >> 8), (uint8_t)at, 0, 4,
                                  (uint8_t)w.Value, (uint8_t)(w.Value >> 8), (uint8_t)(w.Value >> 16), (uint8_t)(w.Value >> 24)};
        ips.insert(ips.end(), record, record + sizeof record);
    }
    ips.insert(ips.end(), {'E', 'O', 'F'});
    return ips;
}

Bytes CodePatchApply(const Bytes& code, const Bytes& ips)
{
    Bytes out = code;
    for (size_t at = 5; at + 3 <= ips.size() && !(ips[at] == 'E' && ips[at + 1] == 'O' && ips[at + 2] == 'F');)
    {
        const size_t offset = ips[at] << 16 | ips[at + 1] << 8 | ips[at + 2], length = ips[at + 3] << 8 | ips[at + 4];
        if (!length || offset + length > out.size()) throw FormatError("code patch: a record Azahar would refuse");
        std::copy(ips.begin() + (ptrdiff_t)at + 5, ips.begin() + (ptrdiff_t)(at + 5 + length), out.begin() + (ptrdiff_t)offset);
        at += 5 + length;
    }
    return out;
}

}
