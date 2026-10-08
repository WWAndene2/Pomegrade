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
//   lighting Z        optional: game zone Z's area pack's file 4 (the light's colours, below) in a copy of the sandbox's pack
//   light NAME R G B...
//                     optional: the zone's light, ambient and/or diffuse colours (0-1) of the terrain's light (light set 0;
//                     characters: character-light), in a copy of the sandbox's pack's file 4, every time of day. Checked (run
//                     lit_red2): "light diffuse 1 0 0 ambient 0 0 1" gives the GPU diffuse (254, 0, 0), ambient (0, 0, 254)
//                     and a red and blue field. The game's outdoors: diffuse 1 1 0.5, ambient 1 1 0.8 (seen in Littleroot)
//                     "direction X Y Z": the way the light goes (normalised by the game; outdoors 0 -1 0, from above).
//                     Checked (runs lt_below, lt_sunset, lt_cave): "direction 0 1 0" reverses the GPU's light (from below,
//                     the ground dark), a grazing orange sunset and a dim blue cave light. Example (sunset):
//                     light direction -0.9 -0.25 0.35 diffuse 1 0.55 0.25 ambient 0.45 0.3 0.45
//   character-light NAME R G B...
//                     optional: the same for the characters' light (the second light set of file 4, 0x2D0 further; the
//                     player included). Checked (run lt_char): "diffuse 1 0 0 ambient 0 0 1" reaches the GPU for the
//                     characters only, the field unchanged. The game's: ambient 0.7 0.8 1 (Littleroot), diffuse 1 1 1
//   camera NAME V...  optional: the zone's camera, one or more settings of preset 0 of its pack's file 6, in a copy of the
//                     sandbox's pack (each zone with its own settings takes one of the game's 9 free packs). Read in the code
//                     (Field_CameraComputePos: the camera stands at distance from the point aimed at, along pitch and yaw):
//                     height (the point aimed at, above the player; 15.85), pitch (degrees, negative looks down; -40.74), yaw
//                     (degrees; 0), distance (254.4); by their values and use: fov (degrees; 30), near, far (the clip
//                     planes; 32, 2000). Example: camera pitch -60 distance 320
//   script / init-script
//                     optional: then the zone's own script (file 2) or initialisation script (in file 1) as amx-asm source
//                     (Amx.h), up to a line "end", in place of the template's; natives checked against the game's tables
//                     (tools/remake/ghidra/function_names.tsv, beside this folder) and native mask 0x243. The game runs `main`
//                     with g_mode set: the initialisation script on entering the zone (2 seen), the zone script for an
//                     event, its script number (SINNOH_BUILD.md R1). Example: tools/remake/sandbox/own_scripts.txt
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

#include <functional>
#include <string>
#include <vector>

namespace remake
{

// writes <outDir>/load/mods/<program>/romfs_ext/{a/0/3/9,a/0/4/0,a/0/1/3,a/0/1/4}.bps and sandbox_layout.txt; returns what it
// did, line by line. Throws FormatError on a description it cannot build. `checkNative` (AmxNativeCheck) checks the natives
// of the zones' own scripts; without it a description holding scripts is refused
std::vector<std::string> BuildOrasSandbox(N3dsRom& oras, const std::string& description, const std::string& outDir,
                                          const std::function<std::string(const std::string&)>& checkNative = {});

}

#endif // REMAKE_ORASSANDBOX_H
