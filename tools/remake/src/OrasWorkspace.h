#ifndef REMAKE_ORASWORKSPACE_H
#define REMAKE_ORASWORKSPACE_H

// The game's archives as one build leaves them (SINNOH_BUILD.md R3): every tool that adds to the game (oras-region,
// oras-sandbox) edits the same copies, so the numbers they take (zones, matrices, map pieces, area packs, text members) are
// always the next free ones of the archive as the build stands, and the build is one mod. Two mods each patching a/0/1/3
// cannot be merged (the Remake mod workflow's merge refuses a file in two mods): the tools run in one workspace instead
// (oras-build).

#include "Bytes.h"
#include "Garc.h"
#include "N3dsRom.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace remake
{

class OrasWorkspace
{
public:
    explicit OrasWorkspace(N3dsRom& oras) : Oras(oras) {}

    N3dsRom& Rom() { return Oras; }
    // an archive as the game ships it, and as the build has left it so far (each read from the game once, on first use)
    const Garc& Original(const std::string& path);
    Garc& Edited(const std::string& path);

    // the mod under <outDir>/load/mods/<program id>: each changed archive as a BPS patch (romfs_ext/<path>.bps), checked by
    // applying it back, or whole (romfs/<path>) when it came out shorter than the game's, since a shorter patched file would
    // keep the old file's tail (Bps.h). Returns what it wrote, line by line, and the zone header rows the engine is to read
    // (oras-engine --zone-rows) when the zones changed
    std::vector<std::string> Write(const std::string& outDir);

private:
    struct Archive { Bytes Data; std::unique_ptr<Garc> Game, Now; };
    Archive& Open(const std::string& path);
    N3dsRom& Oras;
    std::map<std::string, Archive> Archives;
};

}

#endif // REMAKE_ORASWORKSPACE_H
