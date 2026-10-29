#ifndef REMAKE_ORASHOENNNEUTRAL_H
#define REMAKE_ORASHOENNNEUTRAL_H

// Hoenn without its story (owner, 28 October: the map as if it had no story, kept: the gym leaders, items, ordinary people,
// trainers, gifts and services, wild battles; the path blockers opened), the rule of X/Y's neutral zones (OrasXyEvents.h,
// --story off) applied to Omega Ruby's own 538 zones. Read 29 October over every zone's characters (3,440): their script
// gives their kind (0-1999 the zone's own talk, 2000 a scene's actor with no talk of its own, 3000-6999 trainers, 7000-7999
// items, 10000 up the game's shared scripts) and word 4 a vanish flag the story sets or clears. Left out:
//  - a character of the zone's talk or of script 2000 with a vanish flag (370 and 327): the story's people;
//  - the field-move obstacles, the rocks to smash (script 10050, model 8204), the trees to cut (10052, 8203) and the
//    boulders (10054, 8201): 190;
//  - the triggers (the story's steps);
//  - the scenes a zone starts by itself on entering (GetSceneChangeData's table, ended at its first triple).
// Kept: trainers, items, the shared scripts' characters past 10000 (static Pokemon, Kecleon...) and the people without a
// flag. The zones' initialisation scripts are kept, unlike X/Y's: Omega Ruby's also lay their maps out (which objects show
// by the story's flags), not checked one by one.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

class OrasWorkspace;

// Omega Ruby's zones 0 to `zones` - 1 in the workspace's a/0/1/3 made so; returns the log's lines
std::vector<std::string> HoennWithoutStory(OrasWorkspace& ws, int zones);

// the script with GetSceneChangeData's table ended at its first triple (`dropped` 1 when it had one, else 0)
Bytes ScriptWithoutSceneChanges(const Bytes& script, size_t& dropped);

}

#endif // REMAKE_ORASHOENNNEUTRAL_H
