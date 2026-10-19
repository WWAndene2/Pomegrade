#include "XyCharacter.h"

#include "Bch.h"
#include "BchUpgrade.h"
#include "BinLinker.h"
#include "GfMotionPack.h"

#include <cctype>

namespace remake
{

Bytes XyCharacterToOras(const Bytes& member, std::vector<std::string>& report)
{
    if (member.size() < 4 || !std::isalpha(member[0]) || !std::isalpha(member[1])) throw FormatError("an X/Y character is a 2-letter container");
    const std::string tag(member.begin(), member.begin() + 2);
    BinLinker pack = BinLinker::Read(member, tag);
    for (size_t i = 0; i < pack.Files.size(); i++)
    {
        Bytes& f = pack.Files[i];
        if (Bch::Is(f) && f.size() > 4 && f[4] == 0x07)
        {
            try { f = BchUpgradeTo21(f); report.push_back("file " + std::to_string(i) + ": BCH 0x07 -> 0x21"); }
            catch (const FormatError& e) { report.push_back("file " + std::to_string(i) + " kept as it is: " + e.what()); }
        }
        else if (IsGfMotionPack(f, 173))
        {
            std::string own;
            for (uint32_t k = 0; k < 173; k++)
            {
                bool isOwn = false;
                const int to = U32(f, 4 + 4 * k) ? XyMotionSlotInOras(k, &isOwn) : -1;
                if (isOwn) own += (own.empty() ? "" : ", ") + std::to_string(k) + " -> " + std::to_string(to);
            }
            f = XyMotionPackToOras(f);
            report.push_back("file " + std::to_string(i) + ": motion pack, 173 slots -> 239" +
                             (own.empty() ? std::string() : "; X's own motions in free ORAS slots: " + own));
        }
    }
    return pack.Write();
}

std::map<int, int> XyModelMembers(const Bytes& dllField)
{
    for (size_t at = 0; at + 16 <= dllField.size(); at += 4)
    {
        if (U32(dllField, at) != 0 || U32(dllField, at + 4) != 0 || U32(dllField, at + 8) != 1 || U32(dllField, at + 12) != 1) continue;
        std::map<int, int> members;
        size_t i = 0;
        for (; at + 8 * i + 8 <= dllField.size() && U32(dllField, at + 8 * i + 4) == i; i++)
        {
            const uint32_t number = U32(dllField, at + 8 * i);
            if (number >= 0x3000 || (i > 0 && number == 0)) break;
            members[(int)number] = (int)i;
        }
        if (members.count(0x2000) && members.count(0x1000)) return members;
    }
    throw FormatError("X/Y's DllField.cro has no table of model numbers");
}

}

