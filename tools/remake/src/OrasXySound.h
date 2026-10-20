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
// sequences: sound effects, cries) takes Omega Ruby's sound of the same name, by its own id; one of a name Omega Ruby has
// not keeps X/Y's id (logged), which plays whatever Omega Ruby has there.

#include "Bcsar.h"
#include "Bytes.h"

#include <map>
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

    // reads both games' codes (their category tables) and sound archives
    static XySounds Read(const Bytes& orasCode, const Bytes& orasArchive, const Bytes& xCode, const Bytes& xArchive);
    // Omega Ruby's id for X/Y's, or the same id when X/Y's categories do not list it
    uint32_t Translate(uint32_t xId) const;
};

// the category table of a code (the "count, list" pairs from category 0, found before the archive's path in the code, as in
// both games); its address in memory (the code from 0x100000)
uint32_t SoundCategoryTable(const Bytes& code, const std::string& archivePath);

}

#endif // REMAKE_ORASXYSOUND_H
