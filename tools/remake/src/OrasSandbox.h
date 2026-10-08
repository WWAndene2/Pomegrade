#ifndef REMAKE_ORASSANDBOX_H
#define REMAKE_ORASSANDBOX_H

// A new zone built from a text description, nothing taken from a Hoenn place but a template zone's header words and scripts
// (ORAS_ENGINE.md 0, the sandbox): its ground is a grid of tile roles drawn by hand (TownLayout's letters), built into map
// pieces as oras-town builds Platinum's (BuildTownPiece), set on a new map matrix whose every block is the zone, and the
// zone appended to a/0/1/3 as member 538 and up with no furniture, characters, warps or triggers. The game reads it with
// `oras-engine --zone-rows` (the zone bound, the header table's size and the script masks: ORAS_ENGINE.md 2 and 6).
//
// The description, one statement a line ('#' starts a comment). Each zone opens with its number and is followed by its own
// statements; then the map, shared by the zones:
//   zone N            a zone: the next free member of a/0/1/3 (538 on the game's archive), the next ones one after the other
//   template Z        the game zone whose header (music, weather... kept raw) and scripts the zone starts from; every zone draws
//                     its textures from the first zone's template's area pack
//   spawn X Z         the tile the zone's header names (where a new game and a save's warp land, inferred), on its own blocks
//   name TEXT         optional: the place name (Navi-Map, the sign on entering), added to the eight languages' names
//   encounters Z      optional: game zone Z's encounter file (its file 3) copied; none without it
//   character MODEL X Z [face F] [script S] [move M]
//                     a person standing on tile X, Z of the map (model: a character model number, as oras-inspect zone prints
//                     them; face 0-3; script: a number of the template zone's script, 0 none; move: the movement, 0 standing)
//   trainer ID MODEL X Z [face F] [sight N] [move M]
//                     a trainer of a/0/3/6 (script 3000 + ID, kind 1, sight 4 tiles unless given: inferred, OrasZone.h)
//   door X Z house Z2 a door ('D' on the map) into a new zone after the described ones, a copy of game interior Z2 (no
//                     characters or triggers; its warp 0 leads back out)
//   pieces W H        the size in map pieces (40 x 40 tiles each)
//   blocks            with several zones: then 4 H rows of 4 W digits, the zone (0 the first given, 1 the next...) of each
//                     10 x 10 block; one zone alone takes every block
//   map               then 40 H rows of 40 W letters: '.' grass, 'g' tall grass, ':' path, 's' pale grass, '*' flowers,
//                     '~' water, 't' a tree, 'T' forest, 'H' a house's wall, 'D' its door, 'F' a fence, 'L' a ledge
//                     jumped down southward; all but grass, tall grass, paths and flowers are solid (a ledge from the north)
//
// A save moved into a zone (oras-save) shows none of its characters: a continued save restores them from its own block 10,
// which oras-save empties on a zone change (ORAS_ENGINE.md 0); walking in from another zone loads them from the zone's file.

#include "N3dsRom.h"

#include <string>
#include <vector>

namespace remake
{

// writes <outDir>/load/mods/<program>/romfs_ext/{a/0/3/9,a/0/4/0,a/0/1/3,a/0/1/4}.bps and sandbox_layout.txt; returns what it
// did, line by line. Throws FormatError on a description it cannot build
std::vector<std::string> BuildOrasSandbox(N3dsRom& oras, const std::string& description, const std::string& outDir);

}

#endif // REMAKE_ORASSANDBOX_H
