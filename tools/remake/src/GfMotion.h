#ifndef REMAKE_GF_MOTION_H
#define REMAKE_GF_MOTION_H

// The compact skeleton motions of ORAS's Pokemon motion packs and of the title's packs (ORAS_TITLE.md 1b), the layout
// SPICA reads as GF1Motion (Formats/GFL/Motion/GF1Motion.cs):
//   u16 code count, u16 frame count; 3-bit codes packed 8 to a little-endian 24-bit word; codes 0 and 1 of the list
//   unread, then one code per bone element (translation xyz, rotation xyz, scale xyz) or 1 for three skipped elements;
//   the key frame lists, one per code 6 or 7: a count then that many frames, u8, or u16 after a 2-byte alignment when
//   the frame count passes 255 (frames 0 and the frame count are implied); then, 4-aligned, the values in code order:
//   code 5 one f32, code 6 one f32 a key, code 7 an (f32 value, f32 slope) pair a key; codes 0, 2, 3, 4 hold constants.
// Alignments are counted from the motion's start, which must lie 4-aligned in its pack.

#include "Bytes.h"

namespace remake
{

// The motion played `repeats` times in a row as one motion of repeats x its frame count: every key frame list
// repeated, a key at each seam taking the period's last key. For a slot that plays its motion once on a clock of
// its own (the title's Primal Groudon slot, inferred) rather than in a loop.
Bytes LoopGfMotion(const Bytes& motion, int repeats);

// The motion's frame count
int GfMotionFrames(const Bytes& motion);

}

#endif // REMAKE_GF_MOTION_H
