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
// WordSetPlayerNickNamePrefix(slot) (X 0x1029144C, through X's 0x427974) sets a word to the first letter of the player's
// name (two characters in Japanese), the friends' suggestion; X's helper has no copy in Omega Ruby, so the native does its
// work with Omega Ruby's copies of the functions it calls.
//
// The typed nickname (the menu's fourth choice): PlayerNickNameLoad and Unload make and free a holder of the keyboard's
// parameters in the context's +0x9C (X's +0x90); CallInput opens the name keyboard (application 2, mode 5) over a buffer of
// its own, through the calls X makes, each found in Omega Ruby (OrasXyNickname.cpp); InputFix copies the buffer into the
// nickname. Its keyboard knows mode 5 (checked: titled "Quel est votre surnom ?"); that the slot is free in Omega Ruby is
// inferred (nothing found using it).
//
// Checked headless on 20 October, X/Y's Aquacorde scene (zone 541, a save moved there with work 16790 set, a step onto
// its trigger; with oras-engine --xy-messages, the scene's MsgLoad files): "P'tite A, ça te plaît, hein ?" then "Ok ! Va
// pour P'tite A !" (Prefix and SelectFix); the menu's fourth choice opens the keyboard, and three typed characters come
// back as the nickname ("Va pour ♂♀+ !": CallInput, InputFix). Not tested on the phone.

#include "OrasXyNatives.h"

#include <vector>

namespace remake
{

std::vector<XyNative> XyNicknameNatives();

}

#endif // REMAKE_ORASXYNICKNAME_H
