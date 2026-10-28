#ifndef REMAKE_ORASFLYMAP_H
#define REMAKE_ORASFLYMAP_H

// The fly screen's map drawn from an image (owner, 28 October: the X/Y-style maps of tools/remake/map as the fly screen's
// backgrounds, the simplest way: no highlight). Omega Ruby's fly screen (DllTownmap; its files a/0/6/7 member 0, a header
// and a DARC at 0x5900, read 28 October) draws Hoenn from its layout map_lower.bclyt: a background picture "0" (380 x 270,
// poke_navi_map.bclim, ETC1: the sea alone) and, in the pane "s" (320 x 210), one bounding pane per place (where the
// cursor names it) holding three pictures, the place's square or road shown, lit and dimmed (a/0/6/6 names them). The
// image given becomes the background, in the same format, and every picture of "s" is made transparent (alpha 0): the
// bounding panes stay, so the cursor still names the places where Omega Ruby's map had them.

#include "Bytes.h"

namespace remake
{

// a/0/6/7 member 0 (plain) with `rgba` (380 x 270 RGBA rows) as its background and the places' pictures transparent
Bytes FlyMapWithBackground(const Bytes& member, const Bytes& rgba);

}

#endif // REMAKE_ORASFLYMAP_H
