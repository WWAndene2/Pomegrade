#include "Bps.h"

#include <cstring>
#include <unordered_map>

namespace remake
{

uint32_t Crc32(const Bytes& data)
{
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
            table[i] = c;
        }
    uint32_t crc = 0xFFFFFFFFu;
    for (uint8_t b : data) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

namespace
{

enum Command { SourceRead = 0, TargetRead = 1, SourceCopy = 2, TargetCopy = 3 };
constexpr size_t Block = 32; // shortest run copied from the source; shorter ones are cheaper as new bytes
constexpr uint64_t HashBase = 0x100000001B3ull;

// beat's variable-length number: 7 bits a byte, the last one flagged, each continuation adding one
void PushNumber(Bytes& out, uint64_t v)
{
    for (;;)
    {
        const uint8_t x = v & 0x7F;
        v >>= 7;
        if (!v) { out.push_back(0x80 | x); return; }
        out.push_back(x);
        v--;
    }
}

uint64_t ReadNumber(const Bytes& p, size_t& at, size_t end)
{
    uint64_t v = 0, shift = 1;
    for (;;)
    {
        if (at >= end || shift > (1ull << 56)) throw FormatError("BPS: number past the commands");
        const uint8_t x = p[at++];
        v += (x & 0x7F) * shift;
        if (x & 0x80) return v;
        shift <<= 7;
        v += shift;
    }
}

void Push32(Bytes& out, uint32_t v)
{
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)(v >> (8 * i)));
}

uint64_t Hash(const uint8_t* p)
{
    uint64_t h = 0;
    for (size_t i = 0; i < Block; i++) h = h * HashBase + p[i];
    return h;
}

}

Bytes BpsCreate(const Bytes& source, const Bytes& target)
{
    Bytes out = {'B', 'P', 'S', '1'};
    PushNumber(out, source.size());
    PushNumber(out, target.size());
    PushNumber(out, 0); // no metadata

    // the source's aligned blocks, first occurrence of each
    std::unordered_map<uint64_t, size_t> blocks;
    blocks.reserve(source.size() / Block + 1);
    for (size_t s = 0; s + Block <= source.size(); s += Block) blocks.emplace(Hash(&source[s]), s);
    uint64_t topPower = 1; // HashBase^(Block-1), to roll the hash
    for (size_t i = 1; i < Block; i++) topPower *= HashBase;

    size_t literal = 0, sourceRelative = 0, continueAt = 0;
    auto command = [&](Command c, size_t length) { PushNumber(out, ((uint64_t)(length - 1) << 2) | c); };
    auto flushLiteral = [&](size_t end) {
        if (end == literal) return;
        command(TargetRead, end - literal);
        out.insert(out.end(), target.begin() + (long)literal, target.begin() + (long)end);
    };
    auto runAt = [&](size_t s, size_t t) -> size_t { // matching bytes forward, 0 under a block
        if (s + Block > source.size() || memcmp(&source[s], &target[t], Block)) return 0;
        size_t n = Block;
        while (s + n < source.size() && t + n < target.size() && source[s + n] == target[t + n]) n++;
        return n;
    };

    size_t t = 0;
    uint64_t h = target.size() >= Block ? Hash(&target[0]) : 0;
    while (t + Block <= target.size())
    {
        size_t bestS = 0, bestN = 0;
        auto consider = [&](size_t s) { const size_t n = runAt(s, t); if (n > bestN) { bestN = n; bestS = s; } };
        consider(t);          // unchanged in place
        consider(continueAt); // the source continuing where the last run ended (shifted data)
        const auto found = blocks.find(h);
        if (found != blocks.end()) consider(found->second);
        if (!bestN)
        {
            if (t + Block < target.size()) h = (h - target[t] * topPower) * HashBase + target[t + Block];
            t++;
            continue;
        }
        // take back the new bytes the run also matches
        while (t > literal && bestS > 0 && source[bestS - 1] == target[t - 1]) { t--; bestS--; bestN++; }
        flushLiteral(t);
        if (bestS == t)
            command(SourceRead, bestN);
        else
        {
            command(SourceCopy, bestN);
            const long long delta = (long long)bestS - (long long)sourceRelative;
            PushNumber(out, ((uint64_t)(delta < 0 ? -delta : delta) << 1) | (delta < 0));
            sourceRelative = bestS + bestN;
        }
        t += bestN;
        literal = t;
        continueAt = bestS + bestN;
        if (t + Block <= target.size()) h = Hash(&target[t]);
    }
    flushLiteral(target.size());

    Push32(out, Crc32(source));
    Push32(out, Crc32(target));
    Push32(out, Crc32(out));
    return out;
}

Bytes BpsApply(const Bytes& source, const Bytes& patch)
{
    if (patch.size() < 4 + 3 + 12 || memcmp(patch.data(), "BPS1", 4)) throw FormatError("BPS: not a BPS1 patch");
    const size_t end = patch.size() - 12;
    auto footer = [&](size_t i) { return U32(patch, end + 4 * i); };
    if (Crc32(Bytes(patch.begin(), patch.end() - 4)) != footer(2)) throw FormatError("BPS: patch checksum mismatch");
    size_t at = 4;
    const uint64_t sourceSize = ReadNumber(patch, at, end), targetSize = ReadNumber(patch, at, end), metadata = ReadNumber(patch, at, end);
    if (sourceSize != source.size() || Crc32(source) != footer(0)) throw FormatError("BPS: made for another source file");
    if (metadata > end - at) throw FormatError("BPS: metadata past the commands");
    at += metadata;
    Bytes target;
    target.reserve(targetSize);
    size_t sourceRelative = 0, targetRelative = 0;
    while (at < end)
    {
        const uint64_t data = ReadNumber(patch, at, end);
        const uint64_t length = (data >> 2) + 1;
        if (length > targetSize - target.size()) throw FormatError("BPS: command past the target's size");
        switch (data & 3)
        {
        case SourceRead:
            if (target.size() + length > source.size()) throw FormatError("BPS: source read past the source");
            target.insert(target.end(), source.begin() + (long)target.size(), source.begin() + (long)(target.size() + length));
            break;
        case TargetRead:
            if (length > end - at) throw FormatError("BPS: new bytes past the commands");
            target.insert(target.end(), patch.begin() + (long)at, patch.begin() + (long)(at + length));
            at += length;
            break;
        case SourceCopy:
        case TargetCopy:
        {
            const uint64_t off = ReadNumber(patch, at, end);
            size_t& rel = (data & 3) == SourceCopy ? sourceRelative : targetRelative;
            rel = (off & 1) ? rel - (size_t)(off >> 1) : rel + (size_t)(off >> 1);
            if ((data & 3) == SourceCopy)
            {
                if (rel > source.size() || length > source.size() - rel) throw FormatError("BPS: source copy past the source");
                target.insert(target.end(), source.begin() + (long)rel, source.begin() + (long)(rel + length));
            }
            else
            {
                if (rel >= target.size()) throw FormatError("BPS: target copy from beyond what is written");
                for (uint64_t i = 0; i < length; i++) target.push_back(target[rel + i]); // may overlap what it writes
            }
            rel += length;
            break;
        }
        }
    }
    if (target.size() != targetSize || Crc32(target) != footer(1)) throw FormatError("BPS: result does not match the target checksum");
    return target;
}

}
