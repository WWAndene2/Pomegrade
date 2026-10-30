#ifndef REMAKE_ORASXYSOUND_H
#define REMAKE_ORASXYSOUND_H

// X/Y's sound ids made Omega Ruby's, and X/Y's music and jingles brought into Omega Ruby's sound archive (oras-xy-world
// --music; read 20 October). Both games name a sound by an id, (category << 16) | n, which the code resolves through a
// table of 0x17 categories (Omega Ruby's at 0x592728, X's at 0x54E904, read through FUN_00115984: category bits 16-23 below
// 0x17, n below the category's count): each category a u32 count and a pointer to its list of archive item ids (0x01000000 |
// the sound's index, Bcsar.h). Checked in memory (runs hHeap23, hHeap6): Route 101's header id 0x1000B plays item 0x0100001F
// (STRM_BGM_R_A), Littleroot's 0x10005 item 0x01000013 (STRM_BGM_T_01); X's Vaniville 0x10005 resolves to STRM_BGM_T_01.
// Category 0 is empty in both (count 0, no list).
//
// X/Y's streams (its 225 music tracks and jingles, AAC) are appended to Omega Ruby's archive as DSP-ADPCM streams
// (Bcstm.h), sound index first + k for X/Y's k-th stream, and reached through category 0: id k + 1 (0 left out), the list
// written by oras-engine --xy-sounds N (in the rodata segment's last page, past its 0x71A14 bytes). Any other X/Y sound (its
// sequences: sound effects, cries) takes Omega Ruby's sound of the same name, by its own id, when the two play the same data;
// one of a name Omega Ruby has not keeps X/Y's id (logged), which plays whatever Omega Ruby has there.
//
// X/Y's sound effects whose data differs from Omega Ruby's same-named ones (324, Bcsar.h BcsarDifferentSequences; 29
// October, the owner's rule: what is not identical is ported) are appended to the archive after the streams (when they are
// there) with their own sequences, banks and wave archives (BcsarAppendSequences), and reached through category 0 too: ids
// Streams.size() + 1 + j for the j-th, the list written by oras-engine --xy-sequences.

#include "Bcsar.h"
#include "Bytes.h"
#include "N3dsRom.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace remake
{

struct XySounds
{
    std::vector<BcsarStream> Streams;      // X/Y's, in its archive's order: the k-th appended as Omega Ruby's sound First + k
    size_t First = 0;                      // Omega Ruby's sound count: the first appended sound's index
    std::map<uint32_t, uint32_t> Ids;      // X/Y's sound id -> Omega Ruby's, for every sound id (item type 1) X/Y's categories list
    std::vector<std::string> Unmatched;    // X/Y's sounds of no stream and no name in Omega Ruby
    std::vector<std::pair<size_t, size_t>> Sequences; // X/Y's sound effects ported (X/Y's sound, Omega Ruby's of its name): the j-th id SequenceId + j
    uint32_t SequenceId = 0;               // the first one's id in category 0: Streams.size() + 1
    std::map<uint32_t, uint32_t> OrasItems; // Omega Ruby's sound ids -> their items (category tables: 0x01000000 | the sound)
    // quieter copies of sounds (the footsteps' surfaces, OrasXyFootsteps.h), appended after the ported sound effects: each
    // an id the archive plays (Omega Ruby's, or a ported effect's) and its volume in percent; the k-th plays as id QuietId() + k
    struct QuietSound { uint32_t Id = 0; int Percent = 100; };
    std::vector<QuietSound> Quiet;
    // ids the footsteps play (walking and running, both regions' tables: OrasXyFootsteps.h), their data added to the always
    // loaded group GROUP_FIELD_GLOBAL by WriteXySoundArchive: a sound in no loaded group plays nothing
    std::set<uint32_t> Resident;

    // reads both games' codes (their category tables) and sound archives
    static XySounds Read(const Bytes& orasCode, const Bytes& orasArchive, const Bytes& xCode, const Bytes& xArchive);
    // Omega Ruby's id for X/Y's, or the same id when X/Y's categories do not list it
    uint32_t Translate(uint32_t xId) const;
    // the id of the first quiet copy (after the ported sound effects', in category 0)
    uint32_t QuietId() const { return SequenceId + (uint32_t)Sequences.size(); }
    // the id of the copy of `id` at `percent`, added to Quiet when not there yet
    uint32_t QuietCopy(uint32_t id, int percent);
};

// the archive (Omega Ruby's, with X/Y's streams appended when `streams`) with X/Y's ported sound effects appended
// (Sequences), written as its patch romfs_ext/sound/sango_sound.bcsar.bps under outDir's load/mods/<program id>; with
// muteStreams, every stream sound (the music and jingles) at volume 0; returns the log
std::vector<std::string> WriteXySoundArchive(N3dsRom& oras, const Bytes& original, Bytes archive, const Bytes& xyArchive, const XySounds& sounds,
                                             bool streams, const std::string& outDir, bool muteStreams = false);

// the category table of a code (the "count, list" pairs from category 0, found before the archive's path in the code, as in
// both games); its address in memory (the code from 0x100000)
uint32_t SoundCategoryTable(const Bytes& code, const std::string& archivePath);

}

#endif // REMAKE_ORASXYSOUND_H
