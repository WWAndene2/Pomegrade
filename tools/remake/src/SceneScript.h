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
//   flag N on|off     sets or clears story flag N (FlagSet / FlagReset)
//   if flag N | if not flag N   then statements, an optional "else" and its statements, up to "endif"; nests
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
