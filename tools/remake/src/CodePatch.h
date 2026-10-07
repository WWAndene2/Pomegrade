#ifndef REMAKE_CODEPATCH_H
#define REMAKE_CODEPATCH_H

// Patches to the game's own code (ORAS_ENGINE.md: the engine's limits lifted where the code sets them), written as the IPS
// file Azahar applies to the decompressed ExeFS code from a mod folder (load/mods/<program id>/exefs/code.ips,
// azahar/src/core/file_sys/ncch_container.cpp). Each word names the value the game ships there: a patch for another version
// of the game, or one aimed at the wrong place, is refused instead of written.

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct CodeWord
{
    uint32_t Address;  // where the game maps it (the code is loaded at 0x100000)
    uint32_t Expected; // the game's own word there
    uint32_t Value;    // the patched word
    std::string Why;
};

// the IPS patch of those words; throws FormatError when a word of code is not the expected one
Bytes CodePatchIps(const Bytes& code, const std::vector<CodeWord>& words);

// the code with the patch applied, as Azahar's ApplyIpsPatch does (to check a patch before it leaves the tool)
Bytes CodePatchApply(const Bytes& code, const Bytes& ips);

constexpr uint32_t CodeBase = 0x100000;

}

#endif // REMAKE_CODEPATCH_H
