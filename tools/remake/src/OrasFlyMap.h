#ifndef REMAKE_ORASFLYMAP_H
#define REMAKE_ORASFLYMAP_H

// The fly screen's map drawn from images (owner, 28 October: the X/Y-style maps of tools/remake/map as the fly screen's
// backgrounds, the simplest way: no highlight; L/R switching Hoenn and Kalos). Omega Ruby's fly screen (DllTownmap; its
// files a/0/6/7 member 0, a header and a DARC at 0x5900, read 28 October) draws Hoenn from its layout map_lower.bclyt: a
// background picture "0" (380 x 270, poke_navi_map.bclim, ETC1 stored 512 x 512: the sea alone) and, in the pane
// "s" (320 x 210), one bounding pane per place (where the cursor names it) holding three pictures, the place's square or
// road shown, lit and dimmed (a/0/6/6 names them). The images given become the background, in the same format, and every
// picture of "s" is made transparent (alpha 0): the bounding panes stay, so the cursor still names the places where Omega
// Ruby's map had them.
//
// Two maps in the one texture, its size kept: the picture shows the image's 380 x Height part (its texture slot's size,
// from the file's; u, v 0 to 1 over it) at the start of the texture's memory, which holds the stored rows from the
// bottom (PicaTextureDecode's row 511 first). The image is declared 380 x 256: Hoenn in the first 256 stored rows, Kalos
// in the next 256, 0x10000 bytes on (ETC1, 64 tiles of 32 bytes a band of 8 rows); 256 rows drawn on the picture's 270.
// L or R adds 0x10001 to the texture slot's address, or takes it off (the low bit marks Kalos; the GPU reads the address
// >> 3). Checked in the game (run lr1, 28 October): the slot's address changed by a poke changes the picture drawn at
// once; the picture's texture coordinates and its material's texture SRT, poked, change nothing.

#include "Bytes.h"
#include "CodePatch.h"

#include <vector>

namespace remake
{

constexpr uint32_t FlyMapWidth = 380, FlyMapHeight = 256; // each map's image (RGBA rows, map/fly_background.py)
constexpr uint32_t DllTownmapBase = 0x107F2000;           // where prototype/cro_link.py and the Ghidra program place it

// a/0/6/7 member 0 (plain) with `hoenn` as its background, `kalos` (empty: none) after it, and the places' pictures
// transparent
Bytes FlyMapWithBackgrounds(const Bytes& member, const Bytes& hoenn, const Bytes& kalos);

// DllTownmap's words (linked addresses): L or R switching the map shown, in a cave at `cave` (past the code, grown)
std::vector<CodeWord> FlyMapSwitchWords(uint32_t cave);

}

#endif // REMAKE_ORASFLYMAP_H
