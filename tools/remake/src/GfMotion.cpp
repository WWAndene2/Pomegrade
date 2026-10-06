#include "GfMotion.h"

namespace remake
{

int GfMotionFrames(const Bytes& motion) { return U16(motion, 2); }

Bytes LoopGfMotion(const Bytes& motion, int repeats)
{
    const int codesCount = U16(motion, 0), period = U16(motion, 2);
    const long frames = (long)period * repeats;
    if (repeats < 1 || period < 1 || frames > 0xFFFF) throw FormatError("a looped motion must keep its frame count within 1-65535");
    std::vector<int> codes(codesCount);
    size_t at = 4;
    for (int i = 0; i < codesCount; i += 8, at += 3)
    {
        const uint32_t word = U8(motion, at) | U8(motion, at + 1) << 8 | U8(motion, at + 2) << 16;
        for (int k = 0; k < 8 && i + k < codesCount; k++) codes[i + k] = (word >> (3 * k)) & 7;
    }
    Bytes out(motion.begin(), motion.begin() + at);
    out[2] = (uint8_t)frames; out[3] = (uint8_t)(frames >> 8);
    auto put16 = [&](int v) { out.push_back((uint8_t)v); out.push_back((uint8_t)(v >> 8)); };

    // the key lists: inner keys of the period, then the loop's
    const bool wide = period > 0xFF, outWide = frames > 0xFF;
    if (wide && (at & 1)) at++;
    if (outWide && (out.size() & 1)) out.push_back(0);
    std::vector<std::vector<int>> lists;
    // codes 0 and 1 are not elements (GF1Motion walks the elements from 2) but would still own a key list: none seen
    if (codesCount < 2 || codes[0] > 5 || codes[1] > 5) throw FormatError("a motion whose first two codes own key lists");
    for (int i = 2; i < codesCount; i++)
        if (codes[i] > 5)
        {
            const int count = wide ? U16(motion, at) : U8(motion, at);
            at += wide ? 2 : 1;
            std::vector<int> keys;
            for (int k = 0; k < count; k++, at += wide ? 2 : 1)
            {
                keys.push_back(wide ? U16(motion, at) : U8(motion, at));
                if (keys.back() <= 0 || keys.back() >= period || (k && keys.back() <= keys[k - 1]))
                    throw FormatError("a motion key frame lies outside its period or out of order");
            }
            lists.push_back(keys);
            const long loopCount = (long)count * repeats + repeats - 1;
            if (loopCount > (outWide ? 0xFFFF : 0xFF)) throw FormatError("a looped motion's key list is too long");
            if (outWide) put16((int)loopCount); else out.push_back((uint8_t)loopCount);
            for (int n = 0; n < repeats; n++)
            {
                if (n) { if (outWide) put16(n * period); else out.push_back((uint8_t)(n * period)); }
                for (int key : keys) { if (outWide) put16(n * period + key); else out.push_back((uint8_t)(n * period + key)); }
            }
        }
    at = (at + 3) & ~(size_t)3;
    while (out.size() & 3) out.push_back(0);

    // the values, in code order
    size_t list = 0;
    for (int i = 2; i < codesCount; i++)
    {
        if (codes[i] == 5) { const Bytes value = Slice(motion, at, 4); out.insert(out.end(), value.begin(), value.end()); at += 4; }
        if (codes[i] < 6) continue;
        const size_t stride = codes[i] == 7 ? 8 : 4, keys = lists.at(list++).size() + 2;
        const Bytes values = Slice(motion, at, stride * keys); // frame 0, the inner keys, the period's end
        at += stride * keys;
        out.insert(out.end(), values.begin(), values.begin() + stride);
        for (int n = 0; n < repeats; n++)
        {
            if (n) out.insert(out.end(), values.end() - stride, values.end()); // the seam takes the period's last key
            out.insert(out.end(), values.begin() + stride, values.end() - stride);
        }
        out.insert(out.end(), values.end() - stride, values.end());
    }
    // a slot may run on to the next one's alignment; anything more is not this layout
    if (motion.size() - at > 15) throw FormatError("a motion's values end well before the motion does");
    return out;
}

}
