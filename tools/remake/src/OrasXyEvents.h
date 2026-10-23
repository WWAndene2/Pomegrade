#ifndef REMAKE_ORASXYEVENTS_H
#define REMAKE_ORASXYEVENTS_H

#include "Bytes.h"
#include "OrasXyScript.h"
#include "OrasZone.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace remake
{

class OrasWorkspace;
class N3dsRom;

// An X/Y zone's people, objects, triggers, text and scripts made Omega Ruby's (xy-events, and the insides of door ... xy):
// characters' models (X/Y's a/0/2/1 member of their number, XyModelMembers: class models 0x1000 and up through X/Y's table)
// converted (XyCharacter.h) and appended to a/0/2/1 once each (modelOf, by X/Y member); flags (characters' word 4) and works (triggers'
// word 2) moved by `numbers`; tiles shifted by (dx, dz) from X/Y's matrix to the new one; the text, X/Y's a/0/8/0-7 member
// of the zone's header word 3, added as it is in each language; the scripts read from <scriptDir>/x<zone>.amx and
// x<zone>.init.amx when there, else translated (XyScriptToOras, XyScriptSource), and assembled against the native mask they
// need (0x243 and the bits their natives are registered under: `mask`, which oras-engine --script-mask must give the zone
// when it is not 0x243). X/Y's fifth kind of event (zone 0 has 2) is left out (logged). Without `story`: its triggers, the
// characters with a vanish flag (the story's, shown or hidden by it) and its init script (the scenes on entering) are left out;
// its zone script (people's talk, signs, objects) is kept, a talk's story branches taking their default course. With
// `sounds`, the scripts' sound ids are Omega Ruby's (XyMoveSounds; X/Y's music and jingles only with `music`). Trainers (a
// trainer character's script, the scripts' battles: XyMoveTrainers) are Omega Ruby's numbers of X/Y's (OrasXyTrainers.h);
// with `backgrounds`, the battles' backgrounds too (XyMoveBackgrounds, numbered there: OrasXyBackgrounds.h). With `scriptIds`,
// the shared scripts' ids its furniture (word 0), characters (word 5) and triggers (word 0) name are the mod's
// (OrasXySharedScripts.h).
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

// an X/Y script's source (XyScriptSource's) made Omega Ruby's and assembled, as a zone's is: its zones, sounds (with
// `sounds`), trainers, backgrounds (with `backgrounds`), badges, scripted wild battles and message files (X/Y's appended
// once a run) moved, with `scriptIds` the shared scripts' ids it gives natives (XyMoveScriptIds), then `edit`; assembled
// against the native mask it needs, added to `mask` (`what` names it in errors)
Bytes XyAssembleScript(OrasWorkspace& ws, N3dsRom& xRom, std::string source, const XyNumbers& numbers, const std::string& namesFile,
                       const struct XySounds* sounds, bool music, struct XyBackgroundNumbers* backgrounds, const class XyScriptIds* scriptIds,
                       const std::function<std::string(const std::string&)>& edit, const std::string& what, uint32_t& mask,
                       std::vector<std::string>& notes, std::vector<std::string>& log);

XyEvents XyEventsToOras(OrasWorkspace& ws, int xZone, int dx, int dz, const XyNumbers& numbers, std::map<int, int>& modelOf,
                       const std::string& namesFile, const std::string& scriptDir, int orasZone, std::vector<std::string>& log,
                       bool story = true,
                        const struct XySounds* sounds = nullptr, bool music = false, struct XyBackgroundNumbers* backgrounds = nullptr,
                        const class XyScriptIds* scriptIds = nullptr);

}

#endif // REMAKE_ORASXYEVENTS_H
