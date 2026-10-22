#ifndef REMAKE_ORASXYSEQUENCE_H
#define REMAKE_ORASXYSEQUENCE_H

// An X/Y battle sequence (an "SESD" file: X's a/0/3/6, Omega Ruby's a/0/3/4; a battle's intro among them) made Omega Ruby's
// (read 22 October, ORAS_ENGINE.md 0.1). A sequence is "SESD", u32 2, u32 0, u32 its last frame, then records {u32 start
// frame, u32 end frame, u16, u16, u16 command, arguments} ending with u32 0xFFFFFFFF. The game's player (FUN_003C513C,
// FUN_003C4F98, FUN_003C4DF0) hands each record to DllBattle and moves on by the command's argument size from DllBattle's
// table (Omega Ruby's 213 commands at 0x10108800, X's 186 at 0x100FB1F8), or further when the command read more (some
// commands' arguments vary: those are refused here). Omega Ruby inserted 27 commands into X's list, so X's numbers are
// renumbered (XySequenceCommand); a command whose arguments grew in Omega Ruby gets zeros after X's (seen: X 0x14 and 0x97,
// Omega Ruby's own uses hold 0 there).

#include "Bytes.h"

#include <cstdint>
#include <functional>

namespace remake
{

// X's command n as Omega Ruby's: an ordered alignment of both DllBattles' command handlers by their code (masked branches and
// relocated words), confirmed by the variable-size commands pairing (X 0x6C, 0x19, 0xB8 -> 0x75, 0x1A, 0xD3)
uint16_t XySequenceCommand(uint16_t xCommand);

// xSequence made Omega Ruby's, the commands renumbered and their arguments sized from each game's DllBattle (xSizes: X's
// 186 u32, orasSizes: Omega Ruby's 213); `record` is called for each converted record (its command, Omega Ruby's number,
// and its arguments, which it may change) before it is written
Bytes XySequenceToOras(const Bytes& xSequence, const Bytes& xSizes, const Bytes& orasSizes,
                       const std::function<void(uint16_t command, Bytes& arguments)>& record = {});

}

#endif // REMAKE_ORASXYSEQUENCE_H
