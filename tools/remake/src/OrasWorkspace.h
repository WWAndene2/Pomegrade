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
    // the models an area pack's character list may hold: the game's 31, or the N of oras-engine --character-models that the
    // mod is run with (CharacterList refuses a longer list)
    size_t CharacterModels = 31;
    // characters a zone may place, the --characters N given to oras-engine: the pool holds N with the player, so a zone
    // takes N - 1 (run hEv: 64 people in a pool of 64 stopped in Event_PlaceZoneCharacters, 0x3F80B8); the game's 26
    // without it (ORAS_ENGINE.md 4.5)
    size_t CharacterPool = 0;
    // oras-sandbox --xy: Pokemon X/Y's decrypted .3ds, read for a description's xy-piece lines (OrasXyPiece.h)
    std::string XyRom;

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
    // a zone's text from files already written, one per language in the story text archives' order (a/0/7/9 to a/0/8/6:
    // Japanese kana, kanji, English, French, Italian, German, Spanish, Korean, as X/Y's a/0/8/0-7 are); the member added
    int AddZoneTextFiles(const std::vector<Bytes>& files);
    // a system text file (script message table 8: Script_Native_MsgLoad 0x1029EF60), one per language in the system text
    // archives' order (a/0/7/1 to a/0/7/8, as X/Y's a/0/7/2-9 are); the member added
    int AddSystemTextFiles(const std::vector<Bytes>& files);
    // a place name (header word 14 & 0x3FF): a new line of a/0/7/1-a/0/7/8 member 90 (the eight languages' names); returns it
    int AddPlaceName(const std::string& name);
    // the same with each language's name, in the archives' order (a/0/7/1 to a/0/7/8: Japanese kana, kanji, English, French,
    // Italian, German, Spanish, Korean; X/Y's a/0/7/2-a/0/7/9 member 72 in that order)
    int AddPlaceNames(const std::vector<std::string>& perLanguage);
    // an area pack (a/0/1/4) holding `pack`, in one of the game's placeholder packs no zone uses (0, 1, 39-42, 88, 97, 195),
    // never a pack a game zone draws from, so no Hoenn place changes; its characters' list (a/1/3/7) the one of pack `like`.
    // A pack identical to one the build added shares its slot.
    // Once those are taken, appended past the game's packs (228 of them, then a/0/1/4's per-pack table, extended; run apkA):
    // no limit but the archive's. `models`: character models its zones show, added to its list when `like`'s lacks them
    // (CharacterList)
    int AddAreaPack(const Bytes& pack, int like, const std::vector<int>& models = {});
    // pack `like`'s characters' list (a/1/3/7) with an entry for each of `models` it lacks
    Bytes CharacterList(int like, const std::vector<int>& models);
    Bytes CharacterList(Bytes list, const std::vector<int>& models);
    // game pack `pack`'s list (a/1/3/7) with `models` it lacks appended in place (a game zone given characters)
    void ListCharacters(int pack, const std::vector<int>& models);
    // whether character model `model` swims: its a/1/3/7 entry (the same in every pack listing it) has bytes 2-3 01 06 and byte
    // 6 0. Inferred, not read in the code: the 9 models so listed (259, 273, 274, 278, 279, 345-348) are the only ones every
    // game zone places on water tiles only (sea 0x3D1A0006 and its kin, 81 characters), and their motion pack's idle (slot 1)
    // is a swimming loop of 30-45 frames where a walker's is 1 frame; none of their 5 motions is a standing pose (17
    // October). A model no pack lists does not swim (CharacterList gives it model 284's entry)
    bool Swims(int model);

    // Warps between the build's new zones, by Platinum map header (SINNOH_BUILD.md R4): a step that writes a header's zone
    // registers it with its warps (a tile and the header each leads to); LinkWarps, once every step is done, writes each warp
    // into its zone: its destination the zone of that header, its arrival the destination's warp leading back to this header,
    // its kind by what it joins (OrasNewZone.h: a house's door from outdoors, a mat back out, stairs between interiors), and an
    // interior's header word 13 the outdoor zone its warps lead out to. A warp whose header no step built leads nowhere yet: it
    // is left out and reported
    // Walk: for stairs, the way the player walks into the warp's tile (NewStairsWarp: 0 south, 1 north, 2 east, 3 west)
    struct HeaderWarp { int TileX = 0, TileZ = 0, DestHeader = -1, Walk = 1; };
    void RegisterHeaderZone(int header, int zone, bool interior, const std::vector<HeaderWarp>& warps);
    std::vector<std::string> LinkWarps();

    // the mod under <outDir>/load/mods/<program id>: each changed archive as a BPS patch (romfs_ext/<path>.bps), checked by
    // applying it back, or whole (romfs/<path>) when it came out shorter than the game's, since a shorter patched file would
    // keep the old file's tail (Bps.h). Returns what it wrote, line by line, and the zone header rows the engine is to read
    // (oras-engine --zone-rows) when the zones changed
    std::vector<std::string> Write(const std::string& outDir);

private:
    int AddTextFiles(const std::vector<Bytes>& files, int first);
    struct Archive { Bytes Data; std::unique_ptr<Garc> Game, Now; };
    Archive& Open(const std::string& path);
    N3dsRom& Oras;
    std::vector<bool> PacksTaken; // area packs filled by this build
    std::map<int, Bytes> PacksAdded; // their contents, to share a slot between identical packs
    std::map<int, Bytes> PackCharacters; // their characters' lists (a/1/3/7), likewise
    struct HeaderZone { int Header, Zone; bool Interior; std::vector<HeaderWarp> Warps; };
    std::vector<HeaderZone> HeaderZones;
    std::map<std::string, Archive> Archives;
};

}

#endif // REMAKE_ORASWORKSPACE_H
