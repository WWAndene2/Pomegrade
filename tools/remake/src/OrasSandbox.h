#ifndef REMAKE_ORASSANDBOX_H
#define REMAKE_ORASSANDBOX_H

// A new zone built from a text description, nothing taken from a Hoenn place but a template zone's header words and scripts
// (ORAS_ENGINE.md 0, the sandbox): its ground is a grid of tile roles drawn by hand (TownLayout's letters), built into map
// pieces as oras-town builds Platinum's (BuildTownPiece), set on a new map matrix whose every block is the zone, and the
// zone appended to a/0/1/3 as member 538 and up with no furniture, characters, warps or triggers. The game reads it with
// `oras-engine --zone-rows` (the zone bound, the header table's size and the script masks: ORAS_ENGINE.md 2 and 6).
//
// The description, one statement a line ('#' starts a comment):
//   zone N            the zone's number: the next free member of a/0/1/3 (538 on the game's archive)
//   template Z        the zone whose header (music, weather... kept raw), scripts and area pack the new zone starts from
//   spawn X Z         the tile the zone's header names (where a new game and a save's warp land, inferred)
//   name TEXT         optional: the place name (Navi-Map, the sign on entering), added to the eight languages' names
//   encounters Z      optional: zone Z's encounter file (its file 3) copied; none without it
//   character MODEL X Z [face F] [script S] [move M]
//                     a person standing on tile X, Z (model: a character model number, as oras-inspect zone prints them;
//                     face 0-3; script: a number of the template zone's script, 0 none; move: the movement, 0 standing)
//   trainer ID MODEL X Z [face F] [sight N] [move M]
//                     a trainer of a/0/3/6 (script 3000 + ID, kind 1, sight 4 tiles unless given: inferred, OrasZone.h)
//   pieces W H        the size in map pieces (40 x 40 tiles each)
//   map               then 40 H rows of 40 W letters: '.' grass, 'g' tall grass, ':' path, 's' pale grass, '*' flowers,
//                     '~' water, 't' a tree, 'T' forest; trees, forest and water are solid
//
// Houses, fences, ledges and doors are not built yet.

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
