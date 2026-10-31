#ifndef REMAKE_ORASXYGYMNATIVES_H
#define REMAKE_ORASXYGYMNATIVES_H

// X/Y's gym natives (the scripts' side of the gym modules, OrasXyGymModules.h), as X/Y natives for Omega Ruby
// (OrasXyNatives.h). Read 20 October in X's DllField: most are a call of a function of the zone's gym module with the
// module's object, which X's DllField reaches through its imports of that module and Omega Ruby's has none of; the native
// finds both where Field_LoadAreaSpecificModule's caller keeps them, in the field ([script context + 0xC] + 0x48): the
// object at +0x1A0 (X's +0x198, read in Omega Ruby's 0x102DF954 and X's 0x1029E69C), the module's handle, its mapped
// address, at +0x1A4 (OrasXyGymModules.h); the function at the handle + its offset in the module (the port moves no code).
//
// Clemont's (Lumiose, DllUSGymElec), thin ones: _GymElecPlayDoorAnm(a, b), _GymElecBtmPlayDoorAnm(a),
// _GymElecPlayMissAnm(a), _GymElecIsEndDoorAnm(a), _GymElecIsEndMissAnm(), _GymElecSetBlackFace(),
// _GymElecBtmIsEndDoorAnm(). Its six natives building event objects (the lifts, the stand, black and answer animations)
// are not here yet. Not run in the game: the module needs its archive (OrasXyGymModules.h).
//
// Ramos's (Coumarine, DllUSGymGrass): _GymGrassPlayGardAnm(), _GymGrassIsEndGardAnm(). His three event natives
// (GymGrassSlideEvent, GymGrassHangMoveEvent, GymGrassRopeUpDownEvent) are not here yet.

// Olympia's (Anistar, DllUSGymEsper): GymEsperWarpEvent, X's 0x10294528 (the arrival effect: X's DllField class the
// ported module builds, its init copied there), as XyGymEsperWarpEvent: Omega Ruby has a native of that name (Mossdeep's
// gym, another class), so X's scripts call X's under its own name (OrasXyEvents.cpp).
#include "OrasXyNatives.h"

#include <vector>

namespace remake
{

std::vector<XyNative> XyGymNatives();

}

#endif // REMAKE_ORASXYGYMNATIVES_H
