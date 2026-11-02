#ifndef REMAKE_ORASXYACTIONCOMMANDS_H
#define REMAKE_ORASXYACTIONCOMMANDS_H

// X/Y's action commands as Omega Ruby's (2 November, Clemont's quiz rooms; ORAS_ENGINE.md 0.00 "Action commands"): the
// number a zone script gives MdlAcmdSet indexes each game's command table, and the two tables are numbered apart (X 212
// commands, Omega Ruby 260): X/Y's scripts ran Omega Ruby's command of X's number, another action (X's 195, a turn, is
// Omega Ruby's animation 195, which the quiz room's trainer never ended). X/Y's scripts now call XyMdlAcmdSet
// (OrasXyEvents.cpp renames their MdlAcmdSet), a native that translates the command (XyActionCommandsToOras) and calls
// Omega Ruby's MdlAcmdSet (DllField 0x10293B44). The table and how it is made: prototype/xy_action_commands.py.
// Not translated yet: MdlAcmdSet's kin (_MdlAcmdSet, MdlAcmdSetToOtherMdl, IEMdlAcmdSet...) and the commands ported
// code gives.

#include "OrasXyNatives.h"

#include <array>
#include <cstdint>

namespace remake
{

// X/Y's command n -> Omega Ruby's
const std::array<uint8_t, 212>& XyActionCommandsToOras();

// the native XyMdlAcmdSet
XyNative XyMdlAcmdSetNative();

}

#endif // REMAKE_ORASXYACTIONCOMMANDS_H
