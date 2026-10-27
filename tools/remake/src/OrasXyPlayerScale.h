#ifndef REMAKE_ORASXYPLAYERSCALE_H
#define REMAKE_ORASXYPLAYERSCALE_H

// The player drawn larger in X/Y's zones (owner's decision, 27 October): X/Y's world is built around a taller player, X's
// standing 35.8 units from its feet to the top of its hair (its parts, a/0/8/8, no hat) against May's 33.0 (a/0/2/1
// member 171, its ribbon left out): 8 % more. The world keeps X/Y's size (the same 18-unit tiles, the props the two games
// share the same size), so the player is scaled instead, which on screen is the same as X/Y's world 8 % smaller.
// Read headless (runs pc1, sc3, 27 October): the player's model instance (class 0x81AC18) holds its scale at + 0x168 and
// a copy at + 0x19C (three floats each; 1.5 there draws the player 1.5 times larger, and nothing writes them back each
// frame); the path to it from DllField's global 0x10383E40 (in its data, read through the word its code holds at
// 0x10263D84, which the loader relocates): + 0x1C the zone, + 0x18 an object whose + 0xC is the player, whose + 0x98C is
// its model. The hook: the camera area search (0x102CE4B8, each frame on the field), its second instruction `mvn r2, #0`
// calling a cave that writes 1.08 when the zone is in [first, last], and 1.0 elsewhere where the scale is still 1.08 (so
// Omega Ruby's own scales are left alone). Run once after the player's creation (DllField 0x102597B8) it found no model
// yet (run ps2). Checked headless (27 October): the Village's player at 1.08 (run ps3), the Hoenn hub's at 1.0 (run ps4).
// Not read: the model's other users of these floats (the shadow, a follower)

#include "CodePatch.h"

#include <vector>

namespace remake
{

constexpr uint32_t XyPlayerScaleBits = 0x3F8A3D71; // 1.08f

std::vector<CodeWord> XyPlayerScaleFieldPatchAt(uint32_t cave, int firstZone, int lastZone);

}

#endif // REMAKE_ORASXYPLAYERSCALE_H
