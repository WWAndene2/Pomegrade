// The compact skeleton motions (GfMotion.h), looped for the title's Primal slot: a motion written here to the layout
// SPICA reads (GF1Motion) is repeated, with u8 key frames (30 frames) and u16 ones past 255 frames (the title's own
// motions, 3080 and 3500 frames), and the result read back to the same layout.
#include "GfMotion.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void PushF(Bytes& b, float v) { uint32_t u; std::memcpy(&u, &v, 4); for (int i = 0; i < 4; i++) b.push_back((uint8_t)(u >> (8 * i))); }
static float F(const Bytes& b, size_t at) { uint32_t u = U32(b, at); float v; std::memcpy(&v, &u, 4); return v; }

// codes 0, 0, then 7 (Hermite: keys 4), 5 (a constant), 6 (linear: keys 3, 7), 1 (three skipped); 10 frames
static Bytes Motion()
{
    const int codes[6] = {0, 0, 7, 5, 6, 1};
    uint32_t word = 0;
    for (int k = 0; k < 6; k++) word |= (uint32_t)codes[k] << (3 * k);
    Bytes m = {6, 0, 10, 0, (uint8_t)word, (uint8_t)(word >> 8), (uint8_t)(word >> 16)};
    for (uint8_t b : {1, 4, 2, 3, 7}) m.push_back(b);
    for (float v : {1.0f, 0.1f, 2.0f, 0.2f, 3.0f, 0.3f}) PushF(m, v); // Hermite pairs at 0, 4, 10
    PushF(m, 9.0f);                                                      // the constant
    for (float v : {5.0f, 6.0f, 7.0f, 8.0f}) PushF(m, v);                // linear at 0, 3, 7, 10
    return m;
}

int main()
{
    const Bytes m = Motion();
    check(GfMotionFrames(m) == 10, "the motion reads 10 frames");

    const Bytes once = LoopGfMotion(m, 1);
    check(once == m, "looped once, the motion is unchanged");

    const Bytes three = LoopGfMotion(m, 3);
    check(GfMotionFrames(three) == 30, "looped 3 times: 30 frames");
    // lists: count 3*1+2 = 5: 4, 10, 14, 20, 24; count 3*2+2 = 8: 3, 7, 10, 13, 17, 20, 23, 27
    const Bytes lists = {5, 4, 10, 14, 20, 24, 8, 3, 7, 10, 13, 17, 20, 23, 27};
    check(Bytes(three.begin() + 7, three.begin() + 22) == lists, "u8 key lists repeated with a key at each seam");
    check(three.size() == 24 + 7 * 8 + 4 + 10 * 4, "values: 7 Hermite pairs, the constant, 10 linear values");
    check(F(three, 24) == 1.0f && F(three, 32) == 2.0f && F(three, 40) == 3.0f && F(three, 44) == 0.3f && F(three, 48) == 2.0f,
          "the seam takes the period's last pair, then the period repeats");
    check(F(three, 24 + 7 * 8 - 8) == 3.0f, "the last key is the period's end");
    check(F(three, 80) == 9.0f, "the constant kept");
    check(F(three, 84) == 5.0f && F(three, 88) == 6.0f && F(three, 96) == 8.0f && F(three, 100) == 6.0f && F(three, 120) == 8.0f,
          "linear values repeated the same way");

    const Bytes wide = LoopGfMotion(m, 30);
    check(GfMotionFrames(wide) == 300, "looped 30 times: 300 frames");
    check(U16(wide, 8) == 30 + 29 && U16(wide, 10) == 4 && U16(wide, 12) == 10 && U16(wide, 14) == 14,
          "past 255 frames the lists are u16, 2-aligned after the codes");
    check(LoopGfMotion(LoopGfMotion(m, 2), 1) == LoopGfMotion(m, 2), "a looped motion reads back as the layout");

    bool threw = false;
    try { LoopGfMotion(m, 7000); } catch (const FormatError&) { threw = true; }
    check(threw, "over 65535 frames refused");

    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
