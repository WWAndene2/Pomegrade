#ifndef REMAKE_BCSAR_H
#define REMAKE_BCSAR_H

// A 3DS sound archive (BCSAR, "CSAR"), read as far as Omega Ruby's (sound/sango_sound.bcsar, version 0x02030100) and X/Y's
// (sound/xy_sound.bcsar, 0x02030000) stream sounds need (20 October): sections STRG (names), INFO, FILE. INFO's body holds
// eight references (u16 type, u16 0, u32 offset from the body): the sound table (0x2100), sound groups, banks, wave
// archives, groups, players, the file table (0x2106) and the project info. A table: u32 count, then references (offsets
// from the table) to its entries. A sound entry: u32 file, u32 player, u8 volume, u8 remote filter, u16 0, a reference to
// its details (0x2201 a stream, relative to the entry), u32 option flags and one u32 a set flag (bit 0: the name's string
// index). A stream's details: u16 track mask, u16 channels, a reference to its tracks; in X/Y then u32 flags and one u32
// a flag: bit 0 a value (low byte the channels, byte 1 the loop flag), bit 1 the loop start, bit 2 the loop end, in samples
// (X/Y's music is AAC, which carries no loop; Omega Ruby's BCSTM holds its own). A file entry: a reference 0x220D (an
// external file) to its name, a path from sound/, NUL-terminated, padded to 4.
//
// A sound's number is its index in the sound table. The game's own music numbers are (category << 16) | n, n counted from
// the category's first sound (sounddebug/out.txt: category 1 the music, from 0; the last, 0x10, to the table's end).

#include "Bytes.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace remake
{

struct BcsarStream
{
    size_t Index = 0;      // in the sound table
    std::string Name, File;
    uint8_t Volume = 0;
    bool HasLoop = false;  // a loop given in the archive (X/Y)
    uint32_t LoopStart = 0, LoopEnd = 0;
};

// the archive's stream sounds (their file external), in the sound table's order
std::vector<BcsarStream> BcsarStreams(const Bytes& bcsar);

// the number of sounds in its sound table
size_t BcsarSoundCount(const Bytes& bcsar);

// a sound's volume (its entry's u8, 0-127 as the archive's own sounds use it), and the same set
uint8_t BcsarSoundVolume(const Bytes& bcsar, size_t sound);
void BcsarSetSoundVolume(Bytes& bcsar, size_t sound, uint8_t volume);

// the files a sound's data is in: its own (a sequence's), its banks' and their wave archives'
std::vector<uint32_t> BcsarSoundFiles(const Bytes& bcsar, size_t sound);

// The archive with these files added to group `groupName`'s list (a linked group: the game loads each listed file from the
// archive when it loads the group), the group's file rewritten in place and the files after it moved; `added` counts the
// files not listed before (30 October: the footstep sounds the mod plays added to GROUP_FIELD_GLOBAL, which holds the
// grass's; a sound in no loaded group stays silent, the other grounds' sounds loading with their own groups)
Bytes BcsarWithGroupFiles(const Bytes& bcsar, const std::string& groupName, const std::vector<uint32_t>& files, size_t& added);

// every sound's name (its option bit 0's string; empty without one), in the sound table's order
std::vector<std::string> BcsarSoundNames(const Bytes& bcsar);

struct NewStreamSound { std::string File; uint8_t Volume = 0x7F; };

// the archive with these stream sounds appended to its sound table, each a copy of stream sound `like` (its entry and
// details, name included) playing its own new external file; returns the archive, the first new sound's index in `first`
Bytes BcsarAppendStreams(const Bytes& bcsar, size_t like, const std::vector<NewStreamSound>& sounds, size_t& first);

// X/Y's sequence sounds (sound effects) whose name Omega Ruby's archive has but whose data differs (prototype/
// sound_compare.py's comparison, 29 October: the sequence file, then each bank, its wave archive ids compared by the
// archives' contents; each file's version word +8 set aside): pairs of X/Y's sound and Omega Ruby's of its name
std::vector<std::pair<size_t, size_t>> BcsarDifferentSequences(const Bytes& oras, const Bytes& xy);

// The archive with `xy`'s sequence sounds appended to its sound table (the k-th pair's as sound first + k), beside its
// own, with their own data: each sound's sequence file, its banks and their wave archives, appended to the file table and
// to FILE. A bank or wave archive whose contents the archive already holds is shared (its own id); the others are added,
// their ids renumbered (a bank's wave id table, a sequence's bank table). A sound takes X/Y's entry, Omega Ruby's
// same-named sound's player and name; an added bank or wave archive has no name. `added` counts the banks and wave
// archives added
Bytes BcsarAppendSequences(const Bytes& bcsar, const Bytes& xy, const std::vector<std::pair<size_t, size_t>>& sounds, size_t& first,
                           std::string& added);

}

#endif // REMAKE_BCSAR_H
