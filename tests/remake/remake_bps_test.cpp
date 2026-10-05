// The remake tooling's BPS patches (Bps.h): what Azahar applies from a mod's
// romfs_ext folder. A patch written by hand to the beat format's layout is
// read, and patches made here rebuild their targets: one change inside a
// large file, data shifted by an insertion, a file grown, unrelated data.
#include "Bps.h"

#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void Push32(Bytes& b, uint32_t v)
{
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i)));
}

static Bytes Noise(size_t n, uint32_t seed)
{
    Bytes b;
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245 + 12345; b.push_back((uint8_t)(seed >> 16)); }
    return b;
}

// size: the patch's (set before the caller formats its message)
static bool RoundTrip(const Bytes& source, const Bytes& target, size_t* size = nullptr)
{
    const Bytes p = BpsCreate(source, target);
    if (size) *size = p.size();
    return BpsApply(source, p) == target;
}

int main()
{
    check(Crc32(Bytes{'1', '2', '3', '4', '5', '6', '7', '8', '9'}) == 0xCBF43926u, "CRC32: the standard check value");

    // by hand: source "ABCDEFGH", target "ABCxyEFGHEFG": SourceRead 3, TargetRead "xy",
    // SourceCopy 4 from 4 (+4), TargetCopy 3 from 5 (+5)
    const Bytes src = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'}, tgt = {'A', 'B', 'C', 'x', 'y', 'E', 'F', 'G', 'H', 'E', 'F', 'G'};
    Bytes hand = {'B', 'P', 'S', '1', 0x80 | 8, 0x80 | 12, 0x80 | 0,
                  0x80 | (2 << 2 | 0), 0x80 | (1 << 2 | 1), 'x', 'y', 0x80 | (3 << 2 | 2), 0x80 | (4 << 1), 0x80 | (2 << 2 | 3), 0x80 | (5 << 1)};
    Push32(hand, Crc32(src)); Push32(hand, Crc32(tgt)); Push32(hand, Crc32(hand));
    check(BpsApply(src, hand) == tgt, "a patch written to the format by hand: the four commands");
    {
        // 255 is 0x7F then 0x80: 127 + (0 + 1) * 128, the continuation's +1 included
        Bytes s255(255, 'q');
        Bytes p = {'B', 'P', 'S', '1', 0x7F, 0x80, 0x7F, 0x80, 0x80};
        // SourceRead 255: (254 << 2) = 1016 = 0x78 + (7 + 1) * 128 -> 0x78, 0x80 | 6
        p.push_back(0x78); p.push_back(0x80 | 6);
        Push32(p, Crc32(s255)); Push32(p, Crc32(s255)); Push32(p, Crc32(p));
        check(BpsApply(s255, p) == s255, "numbers over two bytes, read with the continuation's +1");
        check(BpsCreate(s255, s255).size() < 30, "an unchanged file: a few bytes of patch");
    }

    // a large file with one region rewritten
    const Bytes base = Noise(1 << 20, 7);
    Bytes changed = base;
    for (size_t i = 300000; i < 300100; i++) changed[i] ^= 0x5A;
    size_t size = 0;
    bool rebuilt = RoundTrip(base, changed, &size);
    check(rebuilt && size < 400, "1 MB, 100 bytes changed: rebuilt, patch " + std::to_string(size) + " bytes");

    // an insertion shifting everything after it (as a grown entry in an archive)
    Bytes inserted(base.begin(), base.begin() + 500000);
    const Bytes extra = Noise(777, 99);
    inserted.insert(inserted.end(), extra.begin(), extra.end());
    inserted.insert(inserted.end(), base.begin() + 500000, base.end());
    rebuilt = RoundTrip(base, inserted, &size);
    check(rebuilt && size < 1200, "777 bytes inserted mid-file: the rest copied shifted, patch " + std::to_string(size) + " bytes");

    // a removal, a moved block, unrelated data, empty files
    Bytes removed(base.begin(), base.begin() + 1000);
    removed.insert(removed.end(), base.begin() + 5000, base.end());
    rebuilt = RoundTrip(base, removed, &size);
    check(rebuilt && size < 200, "bytes removed: patch " + std::to_string(size) + " bytes");
    Bytes moved(base.begin() + 600000, base.end());
    moved.insert(moved.end(), base.begin(), base.begin() + 600000);
    rebuilt = RoundTrip(base, moved, &size);
    check(rebuilt && size < 200, "halves swapped: patch " + std::to_string(size) + " bytes");
    check(RoundTrip(base, Noise(5000, 3)) && RoundTrip(Bytes{}, Noise(100, 4)) && RoundTrip(Noise(100, 4), Bytes{}) && RoundTrip(Bytes{1, 2, 3}, Bytes{1, 2, 3, 4}),
          "unrelated data, empty source, empty target, short files");

    // refusals
    const Bytes p = BpsCreate(base, changed);
    bool refused = false;
    try { BpsApply(changed, p); } catch (const FormatError&) { refused = true; }
    check(refused, "applied to another file: refused (source checksum)");
    refused = false;
    try { Bytes bad = p; bad[bad.size() / 2] ^= 1; BpsApply(base, bad); } catch (const FormatError&) { refused = true; }
    check(refused, "a damaged patch: refused");
    refused = false;
    try { BpsApply(base, Bytes{'I', 'P', 'S', '1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}); } catch (const FormatError&) { refused = true; }
    check(refused, "not a BPS1 patch: refused");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
