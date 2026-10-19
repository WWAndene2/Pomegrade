#ifndef REMAKE_XYCHARACTER_H
#define REMAKE_XYCHARACTER_H

// A Pokemon X/Y character (an a/0/2/1 member: a 2-letter container of its model, its motion pack and, for some, a BCH
// of material animations such as blinking) made Omega Ruby's: every BCH 0x07 converted (BchUpgrade.h), the motion
// pack moved to ORAS's slots (GfMotionPack.h). Checked live (headless, runs xchar2, xchar3, xmove, 16-17 October): X's
// trainers 10 and 31 in place of ORAS's character 284 are drawn, lit as ORAS's characters, and turn on their own.

#include "Bytes.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

// the member converted; report gets a line for each file changed or kept as it is (a BCH the conversion can't carry)
Bytes XyCharacterToOras(const Bytes& member, std::vector<std::string>& report);

// X/Y's model numbers -> their a/0/2/1 member, from X/Y's field module (DllField.cro): a table of (u32 number, u32 member)
// pairs, as Omega Ruby's (read 19 October: DllField 0x1027A640 looks a number up in 510 pairs at 0x10337CFC; a number not
// in it is its own member, as Omega Ruby's appended characters are). X/Y's has 269, each pair's member its index: 0-170
// themselves, 0x2000-0x2046 members 171-241, 0x1000-0x101A members 242-268 (the class models a zone's characters name,
// 0x1000 and up). Found as the longest run of pairs (i, i) from (0, 0) followed by 0x2000; throws FormatError when none
std::map<int, int> XyModelMembers(const Bytes& dllField);

}

#endif // REMAKE_XYCHARACTER_H
