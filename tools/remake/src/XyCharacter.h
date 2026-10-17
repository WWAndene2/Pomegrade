#ifndef REMAKE_XYCHARACTER_H
#define REMAKE_XYCHARACTER_H

// A Pokemon X/Y character (an a/0/2/1 member: a 2-letter container of its model, its motion pack and, for some, a BCH
// of material animations such as blinking) made Omega Ruby's: every BCH 0x07 converted (BchUpgrade.h), the motion
// pack moved to ORAS's slots (GfMotionPack.h). Checked live (headless, runs xchar2, xchar3, xmove, 16-17 October): X's
// trainers 10 and 31 in place of ORAS's character 284 are drawn, lit as ORAS's characters, and turn on their own.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

// the member converted; report gets a line for each file changed or kept as it is (a BCH the conversion can't carry)
Bytes XyCharacterToOras(const Bytes& member, std::vector<std::string>& report);

}

#endif // REMAKE_XYCHARACTER_H
