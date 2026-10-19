#ifndef REMAKE_ORASXYSCRIPT_H
#define REMAKE_ORASXYSCRIPT_H

// Pokemon X/Y's zone scripts (a/0/1/2 member, file 2 main, file 3 init: Pawn (AMX) as Omega Ruby's, ORAS_ENGINE.md 6)
// made Omega Ruby's, first step: the natives. A script calls the game by a native's name hash (AmxNameHash); X and Omega
// Ruby share most names. Read 19 October on Vaniville's zones 0, 2-7 (526 native calls, 115 natives, named from X's code
// strings hashed and from Omega Ruby's tables in ghidra/function_names.tsv): 102 have Omega Ruby's name, 8 were renamed
// (XyNativeRenames), _Suspend is the script machine's own, 3 change the player's outfit (X's mirror; Omega Ruby has
// none) and one hash (0x9AB8B7EE) is not named in X's code strings.
//
// Second step, XyScriptSource: the script as amx-asm source to read and edit (Amx.h's format), its natives named, every call
// commented with the native's name, and X's story numbers moved out of Omega Ruby's: X's flags (FlagSet 604, 614, 2745...
// in Vaniville) and event works (0x4000-0x7FFF, kept in the save) are X's story, which in Omega Ruby are its own. Flags go
// to 0xD00 and up, works to 0x4178 and up: past the game's own (its flags end at 0xCFF, its works at 0x4177), in save
// block 47, which oras-engine --story large opens (OrasEngine.cpp: X names 880 flags and 176 works, more than the 247
// flags and 56 works the game leaves free; without the patch a flag of 0xD00 or more stops the game). Not moved: a flag or work a script computes (reported), X's text files (MsgLoad)
// and zones (MapChangeFake: X's numbers, reported by the caller).

#include "Bytes.h"

#include <map>
#include <set>
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

// where an X/Y zone is in the mod: its zone number and the shift of its tiles (X/Y's matrix to the mod's)
struct XyZonePlace { int Zone = 0, Dx = 0, Dz = 0; };

// X/Y's flags and event works -> Omega Ruby's numbers (XyNumbersFor), and X/Y's zones -> the mod's (filled by the builder)
struct XyNumbers
{
    std::map<int, int> Flags, Works;
    std::map<int, XyZonePlace> Zones;
};

// the numbers XyNumbersFor gives from
constexpr int XyFirstFlag = 0xD00, XyLastFlag = 0x2C3F, XyFirstWork = 0x4178, XyLastWork = 0x436B;

// X's flags and works the scripts (after XyScriptToOras) name, with the extra flags (characters' vanish flags, word 4: zone 0's character 3 hides on 615) and
// works (triggers' conditions, word 2), numbered from XyFirstFlag and XyFirstWork in increasing order; throws FormatError
// past XyLastFlag or XyLastWork
XyNumbers XyNumbersFor(const std::vector<Bytes>& scripts, const std::set<int>& extraFlags, const std::set<int>& extraWorks);

// the script (after XyScriptToOras) as editable amx-asm source: natives by name (Omega Ruby's tables, namesFile) or,
// when Omega Ruby has none, a stand-in with the X hash in a comment (`notes` says so: FadeIsEnd when a loop waits on its
// result, else CommandNOP); each sysreq.n commented with its
// native; flags and works moved by `numbers`; a flag or work given by a variable added to `notes`
std::string XyScriptSource(const Bytes& script, const std::string& namesFile, const XyNumbers& numbers, std::vector<std::string>& notes);

// the zones in a script's source (XyScriptSource's, or one edited from it) moved to the mod's: the first argument of
// _MapChangeCore, _MapChangeAndDemoCall and _MapChangeRailCore (a zone, Omega Ruby's natives read it as a u16: Script_Native_
// _MapChangeCore 0x10296AA4 and the others, read 19 October) when a constant, and with the first two the destination tile
// (arguments 2 and 4, x and z in tiles) shifted by that zone's Dx, Dz. Natives are known by the names in the source's
// native lines. Not moved, in `notes`: a zone computed, a zone not in `zones`, and the natives whose zone arguments are
// not read (PlayerGetZoneID's result compared with X/Y numbers, EventMapReplace, CallMapConnect, GymEsperWarpEvent)
std::string XyMoveZones(const std::string& source, const std::map<int, XyZonePlace>& zones, std::vector<std::string>& notes);

// xScript: an X/Y zone script; namesFile: tools/remake/ghidra/function_names.tsv (Omega Ruby's natives and their mask
// bits); mask: the native mask of the zone it will run in (0x243 for zones 536 and up unless oras-engine --script-mask)
XyScriptNatives XyScriptToOras(const Bytes& xScript, const std::string& namesFile, uint32_t mask);

}

#endif // REMAKE_ORASXYSCRIPT_H
