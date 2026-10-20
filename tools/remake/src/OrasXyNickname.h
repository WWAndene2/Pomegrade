#ifndef REMAKE_ORASXYNICKNAME_H
#define REMAKE_ORASXYNICKNAME_H

// X/Y's nickname the player's friends give (Vaniville, X/Y zone 8), as X/Y natives for Omega Ruby (OrasXyNatives.h). Read
// in X's DllField on 20 October (its modules linked by prototype/cro_link.py): the script loads its message file (MsgLoad
// 21), lists three of its lines as nicknames, and PlayerNickNameSelectFix(line) (X 0x10290BA4) copies that line, its tags
// expanded, into the player's status: the 13 characters at +0x6A of the save's status block (X's at save + 0x129A8).
// Omega Ruby keeps the same field: its WordSetPlayerNickName (0x1029C7D8) shows the 13 characters at +0x6A of save +
// 0x129A0 (FUN_00451510, FUN_004ea884). Every function X's native calls has a byte-identical copy in Omega Ruby's .code
// (compared 20 October): the native is X's, with Omega Ruby's addresses and its script context's message holder (+0x88 of
// the context's +0x1C object, where Omega Ruby's MsgLoad 0x1029EF60 keeps it; X's +0x7C).
//
// Not provided yet (CommandNOP): PlayerNickNameLoad and PlayerNickNameUnload (the keyboard's context), CallInput and
// InputFix (the nickname typed: the menu's fourth choice leaves the nickname as it was), WordSetPlayerNickNamePrefix
// (the nickname's first letters as a word, X 0x1029144C).

#include "OrasXyNatives.h"

#include <vector>

namespace remake
{

std::vector<XyNative> XyNicknameNatives();

}

#endif // REMAKE_ORASXYNICKNAME_H
