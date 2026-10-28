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

#include <array>
#include <map>
#include <string>
#include <vector>

namespace remake
{

constexpr uint32_t FlyMapWidth = 380, FlyMapHeight = 256; // each map's image (RGBA rows, map/fly_background.py)
constexpr uint32_t DllTownmapBase = 0x107F2000;           // where prototype/cro_link.py and the Ghidra program place it

// The places on a drawing (map/hoenn_xy_places.txt, map/kalos_xy_places.txt): the cursor names a place where its box is,
// a bnd1 pane under "s" (named by its id in base 36) that the place's a/0/6/6 record names. A record (60 bytes): u16 0 and
// 1 its zone, u16 2 its box, u16 4 a town's own name (0: the zone's), u16 8 its flight (Omega Ruby's places table entry
// + 1, 0 none), u16 10 and 14 flags (the place's visited flag), u16 16 its description (a line of a/0/7/4 member 33).
// Without a town name its name is its zone's place name: the zone header's halfword 14 bits 0-9, a line of a/0/7/4
// member 90 (read 28 October)
struct FlyPlaces
{
    uint32_t Width = 0, Height = 0;                 // the drawing's, in pixels
    bool Flip = false;                              // the drawing shown upside down (fly_background.py --flip)
    std::map<uint16_t, std::pair<float, float>> Panes; // a box of the game's place on a pixel of the drawing
    struct NewPlace { uint16_t Zone, Pane; };       // a record added: a zone's name on a box (placed in Panes)
    std::vector<NewPlace> News;
    struct Place { uint16_t Zone, Flight, Flag; float X, Y; }; // a record added on a pixel, its box chosen (Kalos)
    std::vector<Place> Places;
};
FlyPlaces ReadFlyPlaces(const std::string& text);

// What L/R switches besides the texture (FlyMapSwitchWords): each box's place (x, y from the centre of "s", side; side 0:
// nothing to point at) on either map, and each record's box (bits 0-14) and map (bit 15 set for a Kalos record); a record
// not on the shown map names Hidden (a 0 x 0 box)
struct FlySwitch
{
    struct Box { uint16_t Id; float Hoenn[3], Kalos[3]; };
    std::vector<Box> Boxes;
    uint16_t Hidden = 0;
    std::vector<uint16_t> Records;
};

// a/0/6/6 member 0 (60-byte records) for Hoenn's places and, given, Kalos's: Hoenn's records whose box is not placed (the
// routes...) naming one such box, Hidden; Hoenn's new places appended, each a copy of the record that named its box
// before; then Kalos's, copies of the first record named by its zone (u16 4 zero). A Kalos place takes a box no Hoenn
// place holds (on its point, 0 x 0 until Kalos is shown: the screen opens on the player's box, so a player in Kalos is
// found there) while some are left (28 of 31), then one of Hoenn's places (moved by L/R). An added record takes its
// zone, flight and flag, and description line 0 (empty). `boxes` gets each box's place in the file, `sw` what L/R switches
Bytes FlyPlacesRecords(const Bytes& records, const FlyPlaces& hoenn, const FlyPlaces* kalos, std::map<uint16_t, std::array<float, 3>>& boxes,
                       FlySwitch& sw);

// a/0/6/7 member 0 (plain) with `hoenn` as its background, `kalos` (empty: none) after it, and the places' pictures
// transparent; with `boxes`, every box of "s" placed so (x, y, side), the others made 0 x 0
Bytes FlyMapWithBackgrounds(const Bytes& member, const Bytes& hoenn, const Bytes& kalos,
                            const std::map<uint16_t, std::array<float, 3>>* boxes = nullptr);

// DllTownmap's words (linked addresses): L or R switching the map shown, in a cave at `cave` (past the code, grown), and
// with `sw` the boxes and records too
std::vector<CodeWord> FlyMapSwitchWords(uint32_t cave, const FlySwitch* sw = nullptr);

// DllField's words (linked addresses): the field's Navi-Map (the lower screen's map, the same texture) showing Kalos's
// half when the player's zone is `firstKalos` or above, in a cave at `cave` (DllField's room)
std::vector<CodeWord> NaviMapWords(uint32_t cave, uint32_t firstKalos);

}

#endif // REMAKE_ORASFLYMAP_H
