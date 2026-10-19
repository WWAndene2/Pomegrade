#ifndef REMAKE_ORASXYWORLD_H
#define REMAKE_ORASXYWORLD_H

// Pokemon X/Y's world in Omega Ruby (oras-xy-world): X/Y's zones (a/0/1/2), their map matrices (a/0/4/2, the same format as
// Omega Ruby's a/0/4/0: ORAS_ENGINE.md 7), map pieces (a/0/4/1) and area packs (a/0/1/3) made Omega Ruby's as whole matrices,
// X/Y's tile coordinates kept, so a zone's spawn, warps, people and script tiles need no shift. Measured on X (19 October):
// 360 zones on 227 matrices; the eight outdoor matrices 0-7 (18 x 21 to 27 x 19 pieces) hold the towns and routes, a few
// zones each (Vaniville and its neighbours on matrix 0); the rest are interiors and small places, some shared (matrix 119,
// one piece, by 16 zones under four packs); each of the 1,030 pieces is used once; 160 area packs.
//
// For each matrix and pack a zone pairs (a group): a new a/0/4/0 member, X/Y's with its pieces converted (XyPieceToOras, at
// their own cells, their props through XyPropsToOras) and its zone grid renumbered; a piece's prop types depend on the pack,
// hence a matrix per pair. For each X/Y pack, a new area pack (AddAreaPack): a copy of Omega Ruby's pack 8 (Littleroot's)
// for one an outdoor zone uses first, else of pack 112 (Littleroot's houses'), with the textures and props of its groups'
// pieces and the models its zones' characters wear. For each zone, a new zone (NewZoneHeader): interior when X/Y's header word
// 0 is 3 (270 of the 360), else outdoor, with Omega Ruby's camera and location type for that kind (X/Y's 24 camera modes and
// 6 location types are not Omega Ruby's 5 and 3: not kept); its area pack, matrix, place name (X/Y's a/0/7/2-9 member 72 line
// of word 14 & 0x3FF, added in the eight languages), the outdoor zone it belongs to (word 13, renumbered), its spawn (X/Y's
// words 22 and 26: x and z in pixels, each the low half of a u32); X/Y's warps with their destination renumbered (a warp to a
// zone not taken is left out, logged); its people, objects, triggers, text and scripts (XyEventsToOras, no shift). X/Y's
// story flags and works are numbered over all 360 zones, so a flag keeps its number whatever part a build takes. Music: the
// header's default (Littleroot's outdoors, its house's inside): X/Y's tracks are not Omega Ruby's.

#include "OrasWorkspace.h"

#include <set>
#include <string>
#include <vector>

namespace remake
{

struct XyWorldOptions
{
    std::set<int> Matrices;  // X/Y's matrices whose zones are taken (empty: all)
    std::string NamesFile;   // tools/remake/ghidra/function_names.tsv (Omega Ruby's natives)
    std::string ScriptDir;   // a folder of edited scripts x<zone>.amx / x<zone>.init.amx, read in place of the translation
};

// builds into the workspace (ws.XyRom: X/Y's .3ds) and returns the log, ending with the oras-engine options the mod needs
std::vector<std::string> BuildXyWorld(OrasWorkspace& ws, const XyWorldOptions& options);

}

#endif // REMAKE_ORASXYWORLD_H
