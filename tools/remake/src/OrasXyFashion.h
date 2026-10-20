#ifndef REMAKE_ORASXYFASHION_H
#define REMAKE_ORASXYFASHION_H

// X/Y's clothes at the player's mirror (Vaniville, X/Y zone 2), as X/Y natives for Omega Ruby (OrasXyNatives.h). X/Y's
// script (read 20 October, sandbox/vaniville/x2.amx): r = ChangeFashionBefore(); ChangeToDefaultFashionAfter(r); then it
// waits until IsFinishChangingFashion(r) is non-zero. Omega Ruby has no outfits (its player's clothes are fixed), so there is
// nothing to change: Before returns 0, After does nothing, IsFinish returns 1 at once. Their stand-ins until now
// (CommandNOP, and FadeIsEnd for the wait) froze the game at the mirror when the wait got 0 (owner's phone, 19 October), and
// FadeIsEnd still waits while any fade runs; these end the wait at once. X's own code for them (the outfit's models) is not
// read: nothing of it can apply in Omega Ruby.
//
// Checked headless on 20 October (run ng6, a new game with oras-engine --xy-messages, mask 540:0xA43; mr3 from its state):
// at the bedroom's mirror the scene plays through ("...And changed out of his pajamas", the mirror's reflection and
// camera), ends, and the player walks again; the clothes stay Omega Ruby's. Not tested on the phone.

#include "OrasXyNatives.h"

#include <vector>

namespace remake
{

std::vector<XyNative> XyFashionNatives();

}

#endif // REMAKE_ORASXYFASHION_H
