#ifndef REMAKE_ORASXYSCRIPT_H
#define REMAKE_ORASXYSCRIPT_H

// Pokemon X/Y's zone scripts (a/0/1/2 member, file 2 main, file 3 init: Pawn (AMX) as Omega Ruby's, ORAS_ENGINE.md 6)
// made Omega Ruby's, first step: the natives. A script calls the game by a native's name hash (AmxNameHash); X and Omega
// Ruby share most names. Read 19 October on Vaniville's zones 0, 2-7 (526 native calls, 115 natives, named from X's code
// strings hashed and from Omega Ruby's tables in ghidra/function_names.tsv): 102 have Omega Ruby's name, 8 were renamed
// (XyNativeRenames), _Suspend is the script machine's own, 3 change the player's outfit (X's mirror; Omega Ruby has
// none) and one hash (0x9AB8B7EE) is not named in X's code strings.
//
// Not done here, the next steps: X's story flags and works (FlagSet 604, 614, 2745, works 0x4000 and up, read in zone 0
// and 2) are X's numbering, which in Omega Ruby are its own story's; X's text (MsgLoad) and zones (MapChangeFake) are X's
// numbers too. A translated script is not yet run in the game.

#include "Bytes.h"

#include <string>
#include <utility>
#include <vector>

namespace remake
{

// X/Y's native -> Omega Ruby's native doing the same, under another name (each pair found by its name's words, read in
// neither game's code: their arguments are not compared yet)
const std::vector<std::pair<std::string, std::string>>& XyNativeRenames();

struct XyScriptNatives
{
    Bytes Script;                       // the script, its native table rewritten
    std::vector<std::string> Renamed;   // "X name -> Omega Ruby name"
    std::vector<std::string> Missing;   // natives the zone's mask does not register in Omega Ruby: "#HASH (X name or ?): why"
};

// xScript: an X/Y zone script; namesFile: tools/remake/ghidra/function_names.tsv (Omega Ruby's natives and their mask
// bits); mask: the native mask of the zone it will run in (0x243 for zones 536 and up unless oras-engine --script-mask)
XyScriptNatives XyScriptToOras(const Bytes& xScript, const std::string& namesFile, uint32_t mask);

}

#endif // REMAKE_ORASXYSCRIPT_H
