#ifndef REMAKE_LAYOUT_H
#define REMAKE_LAYOUT_H

// BCLYT (a 3DS layout: screens, the title logo, menus) and BCLAN (its animations), described as text. Both are a
// header (magic "CLYT"/"CLAN", u16 BOM 0xFEFF, u16 header size, u32 version, u32 file size, u16 section count, u16 0)
// then sections of (4-char magic, u32 size incl. its 8 header bytes). Read in detail: lyt1 (screen size), txl1/fnl1
// (texture and font names), mat1 (material names), pan1/pic1/txt1/wnd1/bnd1 (panes: name, position, rotation, scale,
// size, alpha; pas1/pae1 nest them), grp1 (groups), pat1/pai1 (an animation's name, frames, the panes it animates).
// Other sections are listed by size. The layout is the community's (Kuriimu, Every File Explorer); checked here only
// against itself (remake_layout_test) until a run reads the game's.

#include "Bytes.h"

#include <string>

namespace remake
{

std::string DescribeLayout(const Bytes& file);

}

#endif // REMAKE_LAYOUT_H
