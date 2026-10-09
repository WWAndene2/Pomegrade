#ifndef REMAKE_ORASREGION_H
#define REMAKE_ORASREGION_H

// A region of Platinum rebuilt as a new ORAS map matrix: a rectangle of Sinnoh's piece grid, cut as `oras-world` cuts it
// (ORAS piece (X, Y) holds Platinum's matrix tiles 40X..40X+39, 40Y..40Y+39), every piece built as `oras-town` builds one
// (BuildTownPiece) and appended to a/0/3/9, a matrix appended to a/0/4/0 holding them with its zone grid filled from Platinum's
// map headers, and the ORAS zones that stand for those headers moved onto it with their warps on Platinum's doors.
//
// Zones cannot be appended (ORAS_LITTLEROOT.md 11, a4), so each Platinum header used is given an existing ORAS zone number
// (Zones); that zone keeps its own area pack (rule 6), which receives the textures of the pieces it lies on. What else the
// reused zone holds (characters, furniture, triggers, scripts: Hoenn's) is kept and reported, not rebuilt.
//
// Global grid, not a window per place: Sinnoh is rebuilt matrix by matrix out of one grid, so neighbouring matrices meet without
// re-cutting (the owner: long term, Sinnoh as Platinum lays it out).

#include "N3dsRom.h"
#include "OrasWorkspace.h"
#include "NdsRom.h"
#include "OrasTown.h"

#include <map>
#include <string>
#include <vector>

namespace remake
{

// a door of the region, in matrix tiles, with its Platinum destination header and the ORAS zone it leads into (-1: none of its own)
struct RegionDoor { int X = 0, Y = 0, DestHeader = 0, Interior = -1; };

struct OrasRegionOptions
{
    // the kits, textures and design rules of each piece, as oras-town takes them; its window, cell, zone and area pack are not used
    OrasTownOptions Town;
    int Left = 0, Top = 0, Width = 0, Height = 0;   // the rectangle, in ORAS pieces of Sinnoh's grid
    // Platinum map header -> ORAS zone (a/0/1/3 member); -1: the header's blocks are left out of the matrix (0xFFFF). A header with
    // no block in the rectangle and a door leading to it (a house) is that door's interior: the door's warp leads into the zone,
    // whose warp 0 leads back out to the door; one zone per door
    std::map<int, int> Zones;
    // a zone for every header Zones does not name: Hoenn's overworld zones first, then the empty ones; header 0 left out
    bool AutoZones = false;
    // the zones moved onto the matrix have Hoenn's triggers (tiles that start a script when stepped on) moved off the map: r5 froze on the phone
    // with the field running and no emulator error, and its zones 6 and 23 kept Littleroot's and Route 101's triggers at their
    // Hoenn tiles, whose scripts look for Hoenn's characters and places (suspect, untested)
    bool NoTriggers = false;
    // Hoenn's characters moved off the map the same way: r7a (Route 201, zone 23, left out) ended a freeze nearing Route 201 that
    // all5a/all5b/all6 had with zone 23 keeping Route 101's 12 characters and scripts; this separates the characters from the pieces
    bool NoCharacters = false;
    // test of the Route 201 freeze (ORAS_LITTLEROOT.md 0): a built piece whose 1600 tiles are all a blocking wall
    // (0x01000021) gets every tile set to this value instead; 0: off
    uint32_t SolidPieceTiles = 0;
    // a built piece whose 1600 tiles are all wall (0x01000021: forest, the region's edge) is left out, its cell without a
    // piece as for a left-out header: two such pieces of Route 201's northern edge are what the game loads right before
    // its fatal error (ORAS_LITTLEROOT.md 0); a cell without a piece next to the player did not freeze (r7a)
    bool SkipSolidPieces = false;
    // test of the Route 201 freeze: every tile of one value in the built pieces set to another (--tile-replace FROM:TO)
    std::vector<std::pair<uint32_t, uint32_t>> TileReplace;
    bool OthersOut = false;     // a header Zones does not name is left out, as with -1 (a large matrix holding a few places)
    size_t MatrixTemplate = 1;  // the matrix whose file 1 (meaning unknown) the new matrix copies: Littleroot's (164, a house's, on
                                // an interior's or a cave's matrix)
    int ModelMatrix = 15;       // the pieces' model names, world<NN>_<x>_<y> (the game's run from world01 to world14)
    bool PlanOnly = false;      // print the rectangle's map headers, block by block, and build nothing
    // SINNOH_BUILD.md R2b and R4: every header given a zone gets a new zone written from nothing (OrasNewZone.h), numbered from
    // the next free one, instead of moving that ORAS zone onto the matrix: --zone H:Z then only names the game zone whose area
    // pack the header's pieces are built with. The area packs the pieces add to are new packs (OrasWorkspace::AddAreaPack), so no
    // Hoenn place changes. Every Platinum warp is kept, linked by header once the build is done (OrasWorkspace::LinkWarps): a
    // house's door into the interior a step built from Platinum's matrix (--matrix M --header H), a mat back out, stairs between
    // floors. On an interior's or a cave's matrix (not 0) no house is built and the tiles no warp reaches are made solid. No game
    // zone's header, events, scripts or text is copied; the spawn tile is the named zone's outdoors, by the first warp inside
    bool NewZones = false;
    int Header = -1; // --header H: the map header of a matrix that names none per cell (Platinum's interiors and caves, R4)
    std::map<int, std::string> Names; // --name H:TEXT, with --new-zones: header H's place name
    std::string OutDir;
};

// adds the region to the build's archives (OrasWorkspace.h: OrasWorkspace::Write then writes the mod) and writes
// <OutDir>/region_preview.gltf and region_plan.txt; returns what it did, line by line
std::vector<std::string> BuildOrasRegion(const NdsRom& platinum, OrasWorkspace& ws, const OrasRegionOptions& options);

// The plan for the whole of Sinnoh as oras-region builds it, nothing written: Sinnoh's piece grid cut into strips of `stripWidth`
// columns (each a matrix, trimmed to its used rows), the oras-region rectangle and map headers of each, the edge warps between
// neighbouring strips, and how many ORAS zones the headers need against the ones that can be reused
std::vector<std::string> PlanSinnoh(const NdsRom& platinum, N3dsRom& oras, int stripWidth);

}

#endif // REMAKE_ORASREGION_H
