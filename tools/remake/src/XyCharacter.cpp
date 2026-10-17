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
            f = XyMotionPackToOras(f);
            report.push_back("file " + std::to_string(i) + ": motion pack, 173 slots -> 239");
        }
    }
    return pack.Write();
}

}
