#ifndef REMAKE_ORASNEWZONE_H
#define REMAKE_ORASNEWZONE_H

// A zone of Omega Ruby / Alpha Sapphire written from nothing (SINNOH_BUILD.md R2): its header word by word, its event
// entries field by field, scripts of our own, the ZO container laid out as the game's. No zone of the game is read.
//
// The header's 28 u16 words (offsets are the game's accessors' at 0x4D83CC-0x4D896C and 0x4DBE10, each word at twice its
// number; measured on the 536 game zones, 8 October):
//   0      the zone's kind, low byte (Zone_HeaderGetByte0, Field_ZoneSetup_Step): 0 in 127 of the 193 outdoor zones, 3 in 289
//          of the 343 interiors
//   1      the area pack (a/0/1/4; Event_LoadZoneCharacterModels, Zone_HeaderGetAreaId)
//   2      the map matrix (a/0/4/0; Map_StartMatrixLoad)
//   3      the zone's text file: a member of the story text archives a/0/7/9-a/0/8/6, one per language (Msg_ResolveTextSource;
//          zone 6's 66 holds Littleroot's lines, ORAS_ENGINE.md 6); every zone has its own
//   4, 5   the music, one u32 (Zone_GetMapMatrixForZone reads +8 as the track, misnamed: Zone_ChangeBgmOnEnter plays it):
//          word 4 the track (52 values), word 5 always 1
//   6-11   1 5 1 5 1 14 in every zone (no reader found)
//   12     one value per zone, 0-537 (two missing): the zones ranked by word 3, falling (an id in the developers' order,
//          inferred); Zone_ChangeLoadStep reads it (Zone_GetHeaderField18), what for is not read
//   13     the outdoor zone the zone belongs to: its own number for the 193 outdoor zones, its town's for an interior
//          (Littleroot's houses 223-227 hold 6); the low byte is read by Zone_GetHeaderByte1A (the characters' handlers)
//   14     the place name, low 10 bits (Zone_GetLocationNameId: a line of a/0/7/x member 90), and 6 bits above it
//          (Field_HasUpperBits1C, Zone_ChangeLoad; 1 in Littleroot, 0 in its houses)
//   15     low 5 bits the camera mode (Field_SetCameraMode), bits 5, 6, 7-13 read by the field and the warps
//   16     low 5 bits the location type (Warp_DoorSequenceStep), bit 0x4000 music by time of day (Zone_GetMapMatrixForZone),
//          bits 0x400, 0x800 read by the warps
//   17, 18 0 in every zone
//   19     -1 in 446 zones; 10 marks a Secret Base spot (Zone_HeaderGetField26 == 10), others the mirror natives'
//   20, 21 one u32 of flags at +0x28 (bits 0, 1, 3, 4-10, 11-14, 16, 17-20 each read by one accessor); word 21's upper bits
//          differ in nearly every zone, not read
//   22-24  the spawn, x y z in pixels (18 a tile; y the height), and 25-27 the same again (equal in 524 zones)
// The words whose meaning is not read take the values of two headers every sandbox run has loaded (ORAS_ENGINE.md 2):
// Littleroot's (zone 6) for an outdoor zone, its first house's (zone 223) for an interior, written here as numbers.

#include "OrasZone.h"

#include <array>
#include <string>
#include <vector>

namespace remake
{

enum class NewZoneKind { Outdoor, Interior };

struct NewZoneFields
{
    NewZoneKind Kind = NewZoneKind::Outdoor;
    int Number = -1;           // the zone's member of a/0/1/3
    int AreaPack = -1, Matrix = -1, Text = -1;
    int Overworld = -1;        // word 13: the outdoor zone it belongs to; -1 for an outdoor zone, which belongs to itself
    int NameLine = -1;         // a line of a/0/7/x member 90; -1: none of its own (word 14 & 0x3FF 0)
    int Music = -1;            // word 4; -1: Littleroot's (5) outdoors, its house's (65) inside
    float SpawnX = 0, SpawnZ = 0; // the tile where a warp-less arrival lands (header words 22, 24, 25, 27)
};

// the header's 28 words for these fields (above); throws FormatError on a field left out
std::array<uint16_t, 28> NewZoneHeader(const NewZoneFields& f);

// A character's 24 words (OrasZone.h): the known ones set, the others the value most of the game's 2,904 characters hold
// (word 4 0 in 1,805, 8-11 0, 12-13 a 1 x 1 range in 2,610, 14-16 -1 in 2,898, 17-19 and 22-23 0). kind 1 a trainer
// (script 3000 + its id, ORAS_ENGINE.md 6)
ZoneCharacter NewCharacter(int id, int model, int tileX, int tileZ, int facing, int script, int movement, int kind, int sight);

// A warp's 12 words: destination zone and warp, the kind and its byte above (0x0301 a house's door, as Littleroot's; 0x0200 an
// interior's way out, as its houses'), the position in pixels (a tile's centre), and its width and height in tiles (1 x 1 a
// door, 3 x 1 a house's mat)
ZoneDoor NewDoorWarp(int toZone, int toWarp, int tileX, int tileZ);
ZoneDoor NewExitWarp(int toZone, int toWarp, int pixelX, int pixelZ);

// Scripts that do nothing, as the game's smallest (zone 80): main returns 0 for every command (SINNOH_BUILD.md R1), for a zone
// given none of its own
extern const char* const EmptyZoneScript;
extern const char* const EmptyInitScript;

// The zone's ZO container, written from nothing: file 0 the header, 1 the events (the arrays, then the initialisation script,
// padded to 4), 2 the zone script, 3 the encounters (empty: none), 4 twelve zero bytes (511 of the 536 zones). `zone` holds the
// header, the entries and the two scripts (assembled)
Bytes WriteNewZone(const OrasZone& zone, const Bytes& encounters);

}

#endif // REMAKE_ORASNEWZONE_H
