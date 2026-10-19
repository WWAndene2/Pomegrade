#ifndef REMAKE_ORASXYEVENTS_H
#define REMAKE_ORASXYEVENTS_H

#include "Bytes.h"
#include "OrasXyScript.h"
#include "OrasZone.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

class OrasWorkspace;

// An X/Y zone's people, objects, triggers, text and scripts made Omega Ruby's (xy-events, and the insides of door ... xy):
// characters' models (X/Y's a/0/2/1 member of their number, XyModelMembers: class models 0x1000 and up through X/Y's table)
// converted (XyCharacter.h) and appended to a/0/2/1 once each (modelOf, by X/Y member); flags (characters' word 4) and works (triggers'
// word 2) moved by `numbers`; tiles shifted by (dx, dz) from X/Y's matrix to the new one; the text, X/Y's a/0/8/0-7 member
// of the zone's header word 3, added as it is in each language; the scripts read from <scriptDir>/x<zone>.amx and
// x<zone>.init.amx when there, else translated (XyScriptToOras, XyScriptSource), and assembled against the native mask they
// need (0x243 and the bits their natives are registered under: `mask`, which oras-engine --script-mask must give the zone
// when it is not 0x243). X/Y's fifth kind of event (zone 0 has 2) is left out (logged).
struct XyEvents
{
    std::vector<ZoneFurniture> Furniture;
    std::vector<ZoneCharacter> Characters;
    std::vector<ZoneTrigger> Triggers;
    Bytes Script, InitScript;
    int Text = -1;
    uint32_t Mask = 0x243;
    std::vector<int> Models; // the Omega Ruby models of its characters (plain ones), for the area pack's list
};

XyEvents XyEventsToOras(OrasWorkspace& ws, int xZone, int dx, int dz, const XyNumbers& numbers, std::map<int, int>& modelOf,
                       const std::string& namesFile, const std::string& scriptDir, int orasZone, std::vector<std::string>& log);

}

#endif // REMAKE_ORASXYEVENTS_H
