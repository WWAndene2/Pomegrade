#ifndef REMAKE_ORASSANDBOX_H
#define REMAKE_ORASSANDBOX_H

// New zones built from a text description, written from nothing (OrasNewZone.h: header, entries, scripts, text file), the
// game's assets aside (area packs, building pieces, an interior's map) (ORAS_ENGINE.md 0, the sandbox): its ground is a grid of tile roles drawn by hand (TownLayout's letters), built into map
// pieces as oras-town builds Platinum's (BuildTownPiece), set on a new map matrix whose every block is the zone, and the
// zone appended to a/0/1/3 as member 538 and up. The game reads it with
// `oras-engine --zone-rows` (the zone bound, the header table's size and the script masks: ORAS_ENGINE.md 2 and 6).
//
// The description, one statement a line ('#' starts a comment). Each zone opens with its number and is followed by its own
// statements; then the map, shared by the zones:
//   zone [N]          a zone: the next free member of a/0/1/3 as the build stands (538 on the game's archive, after the zones of
//                     earlier steps in an oras-build), the next ones one after the other; N, when given, must be that number
//   pack P            the first zone: the area pack (a/0/1/4 member) whose textures every zone's pieces draw from (8: Littleroot's).
//                     The zones are written from nothing (OrasNewZone.h: every header word, entry and script), no game zone copied
//   music N           optional: the track (header word 4; 5 Littleroot's, the default)
//   line TEXT         optional, repeatable: a line of the zone's own text file (header word 3, a new member of a/0/7/9-a/0/8/6),
//                     line 0 first, for its scripts' messages
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
//                     (Amx.h), up to a line "end", in place of ones that do nothing; natives checked against the game's tables
//                     (tools/remake/ghidra/function_names.tsv, beside this folder) and native mask 0x243. The game runs `main`
//                     with g_mode set: the initialisation script on entering the zone (2 seen), the zone script for an
//                     event, its script number (SINNOH_BUILD.md R1). Example: tools/remake/sandbox/own_scripts.txt
//   scene             optional, instead of script: then the zone script as a scene (SceneScript.h: "on N", "say L", "wait F",
//                     "flag N on|off", "if [not] flag N" ... "else" ... "endif"), up to a line "end"; compiled on Littleroot's
//                     zone script, whose talking and waiting routines it calls. Example: tools/remake/sandbox/scene_demo.txt
//   encounters Z      optional: game zone Z's encounter file (its file 3) copied; none without it
//   xy-encounters Z   optional, instead: X/Y's zone Z's, made Omega Ruby's as oras-xy-world makes it (OrasXyEncounters.h,
//                     X/Y's battle types: --xy, and --onto a mod built with oras-engine --xy-wild-effects)
//   character MODEL X Z [face F] [script S] [move M]
//                     a person standing on tile X, Z of the map (model: a character model number, as oras-inspect zone prints
//                     them; face 0-3; script: the command its zone script is run with, g_mode, 0 none; move: the movement, 0 standing).
//                     A swimmer model (OrasWorkspace::Swims: 259, 273, 274, 278, 279, 345-348) has no standing motion, its idle
//                     swims, and water is solid here: it is drawn half in the ground, and the build warns
//   trainer ID MODEL X Z [face F] [sight N] [move M]
//                     a trainer of a/0/3/6 (script 3000 + ID, kind 1, sight 4 tiles unless given: inferred, OrasZone.h)
//   team ID [class C] [name TEXT]
//                     then trainer ID's new team (a/0/3/6, a/0/3/8; with --onto, a trainer the mod added too: oras-xy-world's
//                     X trainers, 950 and up), up to "end": a line each, "pokemon SPECIES LEVEL
//                     [form F] [item I] [moves A B C D] [difficulty D]" (1 to 6; national dex numbers, the game's item and
//                     move numbers; difficulty 0-255 as the game's teams hold it, 0 by default); the format follows the
//                     items and moves given; class C (a/0/3/7: the battle's portrait and title, 178 Team Magma's leader,
//                     174 Team Aqua's) and name TEXT (line ID of a/0/7/1-8 member 22, all eight languages) when given;
//                     the rest of the record kept
//   edit Z            instead of a new zone: then line, character, trigger, restyle and scene statements added to zone Z
//                     of a/0/1/3, its own kept (a game zone, or with --onto one the mod added, X/Y's 538 and up)
//   class C battle-type T
//                     trainer class C's battles take battle type T (its music and victory jingle; a/0/3/7 byte +2): every
//                     trainer of the class, the portrait and title kept
//   restyle N model M in an "edit Z": the zone's own character N (word 0, as oras-inspect zone prints it) given model M,
//                     added to its pack's character list (a gym leader's look)
//   trigger X Z [W H] script N [height P]
//                     stepping on the W x H tiles (1 x 1 by default) from tile X, Z runs the zone script with g_mode N (a
//                     scene's "on N"): words 0, 6-9 of the game's trigger records, no condition; P the ground's height
//                     there in pixels (word 10, 0 by default: Littleroot's ground), as a door's "height H"
//   door X Z house Z2 a door ('D' on the map) into a new zone after the described ones, written from nothing on game interior
//                     Z2's map (its matrix, area pack and the tile of its way out); its one warp leads back out
//   door X Z xy ZX    on an xy-piece: the inside made of X/Y's zone ZX (its piece, props, textures and way out: XyInsideToOras),
//                     X's own door model being the piece's (no 'D' on the map). Either form may end with "height H", the warp's
//                     height in pixels (0 by default; X's Vaniville stands at 144)
//   pieces W H        the size in map pieces (40 x 40 tiles each)
//   xy-piece X Y M P  piece X, Y is Pokemon X/Y's map piece M (a/0/4/1 member) made Omega Ruby's, its textures from X/Y's area
//                     pack P (a/0/1/3 member, the pack its zone draws from), instead of one built from the map's rows (which are
//                     then not used for it): OrasXyPiece.h. Needs oras-sandbox --xy <x.3ds>
//   xy-stairs ZA X Z W H WAY ZB X Z W H WAY
//                     stairs X/Y runs by script (no warp of its own: X's zone 2, the player's house, to its upper floor 3):
//                     X/Y zone ZA's W x H tiles from X, Z go up to zone ZB, whose tiles go back down, written as X/Y's own
//                     stairs warps (kind 0x500 + the way). WAY: how the tiles are walked into, down, up, right or left (the
//                     game sets the player out the other way). ZA is a door ... xy's inside; ZB becomes a floor of its own.
//                     The tiles are the solid stair tiles, walked into, as X/Y's own stairs warps sit (zones 4-5): beside the
//                     tiles of X's stairs triggers (zone 2's trigger records 1 and 2, at 31 35 and 9 15: run hSt1, 18 October)
//   blocks            with several zones: then 4 H rows of 4 W digits, the zone (0 the first given, 1 the next...) of each
//                     10 x 10 block; one zone alone takes every block
//   map               then 40 H rows of 40 W letters: '.' grass, 'g' tall grass, ':' path, 's' pale grass, '*' flowers,
//                     '~' water, 't' a tree, 'T' forest, 'H' a house's wall, 'D' its door, 'F' a fence, 'L' a ledge
//                     jumped down southward; all but grass, tall grass, paths and flowers are solid (a ledge from the north)
//
// A save moved into a zone (oras-save) shows none of its characters: a continued save restores them from its own block 10,
// which oras-save empties on a zone change (ORAS_ENGINE.md 0); walking in from another zone loads them from the zone's file.

#include "OrasWorkspace.h"

#include <functional>
#include <string>
#include <vector>

namespace remake
{

// adds the described zones to the build's archives (OrasWorkspace.h: OrasWorkspace::Write then writes the mod) and writes
// <outDir>/sandbox_layout.txt; returns what it did, line by line. Throws FormatError on a description it cannot build.
// `checkNative` (AmxNativeCheck) checks the natives of the zones' scripts; without it nothing is built. `descriptionPath`,
// the description's file: X/Y zones' scripts (xy-events, door ... xy) are read from x<zone>.amx and x<zone>.init.amx in the
// folder of its name beside it when there (tools/remake/sandbox/vaniville/ for vaniville.txt: remake_tool xy-script-export
// writes them), else translated (OrasXyScript.h); the natives are named from ../ghidra/function_names.tsv beside it
std::vector<std::string> BuildOrasSandbox(OrasWorkspace& ws, const std::string& description, const std::string& outDir,
                                          const std::function<std::string(const std::string&)>& checkNative = {},
                                          const std::string& descriptionPath = "");

}

#endif // REMAKE_ORASSANDBOX_H
