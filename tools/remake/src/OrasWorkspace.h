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

    // What a new zone takes, each the next free number as the build stands (shared by every tool, SINNOH_BUILD.md R3):
    // the zone itself (a ZO container, OrasNewZone.h) appended to a/0/1/3, with its row of the header table (member 536, 56 bytes
    // a zone number; the rows 536 and 537 of the table members filled with the first new zone's) and its file of the encounter
    // container (member 537) (ORAS_ENGINE.md 2); returns its number
    int AddZone(const Bytes& zone, const Bytes& encounters);
    int NextZone() { return (int)Edited("a/0/1/3").Count(); }
    // its own text file (header word 3): a new member of each of the eight story text archives (a/0/7/9-a/0/8/6, one per
    // language), the same lines in all; returns the member
    int AddZoneText(const std::vector<std::string>& lines);
    // a place name (header word 14 & 0x3FF): a new line of a/0/7/1-a/0/7/8 member 90 (the eight languages' names); returns it
    int AddPlaceName(const std::string& name);
    // an area pack (a/0/1/4) holding `pack`, in one of the game's placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195),
    // never a pack a game zone draws from, so no Hoenn place changes; its characters' list (a/1/3/7) the one of pack `like`.
    // A pack appended past the game's 229 is refused: its zone sent the game into its fatal-error loop (runs light1-light4)
    int AddAreaPack(const Bytes& pack, int like);

    // the mod under <outDir>/load/mods/<program id>: each changed archive as a BPS patch (romfs_ext/<path>.bps), checked by
    // applying it back, or whole (romfs/<path>) when it came out shorter than the game's, since a shorter patched file would
    // keep the old file's tail (Bps.h). Returns what it wrote, line by line, and the zone header rows the engine is to read
    // (oras-engine --zone-rows) when the zones changed
    std::vector<std::string> Write(const std::string& outDir);

private:
    struct Archive { Bytes Data; std::unique_ptr<Garc> Game, Now; };
    Archive& Open(const std::string& path);
    N3dsRom& Oras;
    std::vector<bool> PacksTaken; // area packs filled by this build
    std::map<std::string, Archive> Archives;
};

}

#endif // REMAKE_ORASWORKSPACE_H
