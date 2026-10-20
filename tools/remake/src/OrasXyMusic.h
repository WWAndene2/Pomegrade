#ifndef REMAKE_ORASXYMUSIC_H
#define REMAKE_ORASXYMUSIC_H

// X/Y's music and jingles in Omega Ruby (oras-xy-world --music, the owner's choice of 20 October: all 225 of X/Y's streams,
// at Omega Ruby's own 32728 Hz stereo, beside its music). Each of X/Y's stream files (sound/*.aac, 48000 Hz) is decoded by
// ffmpeg (on the PATH) to 16-bit stereo at 32728 Hz, cut at its loop's end (X/Y's archive gives the loop in samples at the
// file's rate, scaled), encoded as Omega Ruby's BCSTM (Bcstm.h, its loop put on a block) and written beside Omega Ruby's
// under romfs/sound (the same name, .dspadpcm.bcstm for .aac; X/Y's silence, a name Omega Ruby's archive already plays,
// is Omega Ruby's file); the archive gains one stream sound each (Bcsar.h, copies of STRM_BGM_C_01 with X/Y's volume), as
// romfs_ext/sound/sango_sound.bcsar.bps. Their ids are category 0's (OrasXySound.h), which oras-engine --xy-sounds sets up.
// Not done: X/Y's AAC carries an encoder delay that a loop's start and end both keep (not measured).

#include "N3dsRom.h"
#include "OrasXySound.h"

#include <string>
#include <vector>

namespace remake
{

// writes the streams and the archive's patch under outDir (the mod's load/mods/<id>/); returns the log, ending with the
// oras-engine option the ids need
std::vector<std::string> BuildXyMusic(N3dsRom& oras, N3dsRom& xy, const XySounds& sounds, const std::string& outDir, const std::string& scratchDir);

}

#endif // REMAKE_ORASXYMUSIC_H
