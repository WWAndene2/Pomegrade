#ifndef REMAKE_SCENESCRIPT_H
#define REMAKE_SCENESCRIPT_H

// A zone script written as a scene (a sandbox description's `scene` block, OrasSandbox.h) instead of assembler: what each
// event does, one statement a line, compiled into a zone script (amx-asm source, Amx.h) on a game zone's script whose
// routines it calls (the library: talking, waiting), taken from the player's own game when the mod is built.
//
// The scene, one statement a line ('#' starts a comment; numbers in decimal or 0x hex):
//   on N              the event N's statements follow, up to the next "on" (N: the script number a character or sign
//                     runs, its g_mode, SINNOH_BUILD.md R1; a sandbox `character ... script N`)
//   say L             the person talked to says line L of the zone's text (its `line` statements, 0 first), in their
//                     balloon; the game turns them to face the player, waits for A, then closes the window
//   wait F            waits F frames
//   act WHO CODE DIR [N]   a person's movement command, N times (1 by default), then waits for its end: WHO -1 the
//                     person talked to, 255 the player (as the game's scripts give them), else a character's number;
//                     CODE and DIR as the game's command lists hold them (MdlAcmdInit, MdlAcmdSet CODE DIR 1 0,
//                     MdlAcmdSetEnd, then MdlAcmdUpdate each frame until done). WHO may be "player" or "person", DIR
//                     "north" (0), "south" (1), "west" (2), "east" (3). Seen in the game (run sceneD1, 16 October): codes
//                     19, 13, 12 and 17 move one tile (19 in all four directions, the person and the player; their speeds
//                     not compared), 6 turns (the player, north); 3, 5, 232, 233 moved nothing (a "say" turns the person
//                     back to the player, so turning was not seen on them); 7 not understood
//   walk WHO DIR [N]  act WHO 19 DIR N: walks N tiles
//   face WHO DIR      act WHO 6 DIR: turns
//   fade out|in KIND [F]   the screen fades from the field (FadeRequestOut 0 KIND F) or back to it (the library's
//                     routine: waits for the map's blocks, then _FadeRequestIn KIND F) over F frames, 30 by default, and
//                     the scene waits for its end (FadeIsEnd); KIND "both" (2: both screens darken) or "top" (0: the
//                     top screen only), seen in run sceneE1; fading in waits for the map, so it takes up to 60 frames more
//   camera X Z H TX TZ TH [FOV [F]]   the camera moves over F frames (60 by default) to stand at tile X, Z, H tiles
//                     up, looking at tile TX, TZ, TH tiles up, field of view FOV degrees (30 by default); a tile is 18 world
//                     units (Littleroot's camera tables, read 16 October: position, target, fov, roll)
//   camera back [F]   the field's camera again (EVCameraEnd F)
//   flag N on|off     sets or clears story flag N (FlagSet / FlagReset)
//   battle T          a battle against trainer T (a/0/3/6), as the scripts that start one do (_CallTrainerBattleCore 0 T
//                     0 0 0 0 0); lost (_BattleGetResult's first cell 0), the game's handler takes over (CallTrainerLose)
//                     and the event ends; won, the scene goes on
//   give item I [N]   N of item I (1 by default) into the bag (ItemAdd), with no message: a "say" tells it
//   give pokemon N    entry N of the game's 37 gift Pokemon (PokePartyAdd N 0; DllField 0x1033B06C, read 16 October: 0-2
//                     Hoenn's starters level 5, 3-5 Kanto's 10, 6-14 and 17-18 fossils 20, 15 Castform 30, 16 Lucario 32,
//                     19-20 Cosplay Pikachu, 21 Beldum 1, 22 Camerupt 40, 23 Sharpedo 40, 24 Latios 30, 25 Latias 30,
//                     26-27 Wynaut's and Togepi's eggs, 28-36 the other regions' starters 5); nothing when the party is full
//   heal              the party restored (PokePartyRecoverAll)
//   if flag N | if not flag N | if item I | if not item I   then statements (item: at least one in the bag,
//                     ItemGetNum), an optional "else" and its statements, up to "endif"; nests
// Story flags: 1 to 0xCFF are saved; 0xD00 to 0x2FFF are fatal; no game script names 0xC09-0xCFF as a constant
// (ORAS_ENGINE.md 6: 2,056 constant uses of 699 flags in the 1,072 zone scripts, 16 October), so a mod's story takes
// them from the top (0xCE0-0xCFF). Not read in the game's code beyond the scripts: a flag there may yet have a use.
//
// The library (`librarySource`, AmxSource of a game zone's script: Littleroot's, zone 6) keeps all its code; the scene
// adds its own `main` (the header's cip moved to it) that runs the events given, CommandNOP for any other and nothing for
// -1, as the game's mains do. Its routines are found by their code, not their address, and the compile fails when one is
// missing: talking ("say": the routine every talking script has, arguments line, 1, -1 the person talked to) and
// waiting (the _Suspend wrapper).

#include <string>

namespace remake
{

// the scene compiled into amx-asm source; throws FormatError naming the scene's line, or the library routine not found
std::string CompileScene(const std::string& scene, const std::string& librarySource);

}

#endif // REMAKE_SCENESCRIPT_H
