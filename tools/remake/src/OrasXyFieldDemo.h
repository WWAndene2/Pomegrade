#ifndef REMAKE_ORASXYFIELDDEMO_H
#define REMAKE_ORASXYFIELDDEMO_H

// The field's demos given the sequence heap DllFieldDemo gives them (3 November, Clemont's quiz picture): Omega Ruby's
// DllSequence plays a demo from a sequence object whose + 0x268 is a work heap, made by DllSequence 0x10712D34 (seq, 0)
// and freed by 0x10712DF0 (seq; nothing when + 0x26C is 0). Only DllFieldDemo, DllPorocCase, DllSangoIntro and
// DllSangoStaffroll call them; the field's own demo event (DllField 0x102CA520 makes the sequence, + 0x38 of the event;
// 0x102CA1DC ends it) does not, so CallFieldCaptureDemoSimplePoke (X/Y's quiz, its picture) allocated from an unset
// heap and wrote to 0xC (run b42, traced in b51). X's DllSequence plays the same demo from the field alone. Two hooks:
// after the sequence is made, the heap made; before it ends, the heap freed. DllSequence is loaded on demand, so the two
// functions are reached from DllField's import of its sequence maker (the stub's word at 0x102464C4, relocated by the
// loader): maker - 0x598 and maker - 0x4DC (their distances in DllSequence, linked 0x107132CC, 0x10712D34, 0x10712DF0)

#include "CodePatch.h"

#include <vector>

namespace remake
{

std::vector<CodeWord> XyFieldDemoHeapPatchAt(uint32_t cave);

}

#endif // REMAKE_ORASXYFIELDDEMO_H
