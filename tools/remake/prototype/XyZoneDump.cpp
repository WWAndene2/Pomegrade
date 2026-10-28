// X/Y's zones as text, one file each (28 October: the neutral build's scripts listed, XY_NEUTRAL_SCRIPTS.md): the place
// name (French, a/0/7/5 member 72), every character (model, kind, vanish flag, script), trigger, step event of the fifth
// kind and piece of furniture, then the zone script as amx-asm source (AmxSource). Read by xy_zone_cases.py.
//
//   XyZoneDump <x.3ds> <out dir>      writes <out dir>/z<zone>.txt
#include "Amx.h"
#include "Garc.h"
#include "GameText.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "OrasZone.h"

#include <cstdio>
#include <fstream>

using namespace remake;

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: XyZoneDump <x.3ds> <out dir>\n");
        return 2;
    }
    N3dsRom rom(argv[1]);
    const Garc zones(rom.Read("a/0/1/2"));
    const auto plain = [](Bytes b) { return IsLzCompressed(b) ? LzDecompress(b) : b; };
    const std::vector<std::string> names = ReadGameText(plain(Garc(rom.Read("a/0/7/5")).Sub(72)));
    for (size_t i = 0; i < zones.Count(); i++)
    {
        OrasZone x;
        try { x = OrasZone::Read(plain(zones.Sub(i))); }
        catch (const FormatError&) { continue; }
        std::ofstream f(std::string(argv[2]) + "/z" + std::to_string(i) + ".txt");
        const int name = x.Header[14] & 0x3FF;
        f << "NAME " << (name < (int)names.size() ? names[(size_t)name] : "?") << "\n";
        for (const ZoneCharacter& c : x.Characters)
            f << "CHAR id " << c.Raw[0] << " model " << c.Raw[1] << " kind " << c.Raw[3] << " flag " << c.Raw[4] << " script " << c.Raw[5]
              << " tile " << c.Raw[20] << " " << c.Raw[21] << "\n";
        for (const ZoneTrigger& t : x.Triggers) f << "TRIG script " << t.Raw[0] << " val " << t.Raw[1] << " work " << t.Raw[2] << " tile " << t.Raw[6] << " " << t.Raw[7] << "\n";
        for (const ZoneOther& o : x.Others) f << "STEP script " << o.Raw[0] << " val " << o.Raw[1] << " work " << o.Raw[2] << " tile " << o.Raw[6] << " " << o.Raw[7] << "\n";
        for (const ZoneFurniture& u : x.Furniture) f << "FURN script " << u.Raw[0] << "\n";
        if (x.Script.size() >= 16)
        {
            try { f << "=== SCRIPT\n" << AmxSource(x.Script); }
            catch (const FormatError& e) { f << "script error " << e.what() << "\n"; }
        }
    }
    return 0;
}
