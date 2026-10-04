#include "GPU2D_SpriteLimitInversion.h"

#include <algorithm>
#include <cstdlib>

namespace melonDS
{

// sprite size in pixels by shape (attribute 0 bits 14-15) and size
// (attribute 1 bits 14-15)
static const u8 SpriteWidth[4][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}, {8, 8, 8, 8}};
static const u8 SpriteHeight[4][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}, {8, 8, 8, 8}};

bool SpriteLimitInversion::IsDrawn(const u16* attr)
{
    const bool affine = attr[0] & 0x0100;
    if (!affine && (attr[0] & 0x0200)) return false; // disabled
    if ((attr[0] >> 14) == 3) return false;          // shape 3: prohibited

    int w = SpriteWidth[attr[0] >> 14][attr[1] >> 14];
    int h = SpriteHeight[attr[0] >> 14][attr[1] >> 14];
    if (affine && (attr[0] & 0x0200)) { w *= 2; h *= 2; } // double size
    int x = attr[1] & 0x1FF; if (x >= 256) x -= 512;
    int y = attr[0] & 0xFF; if (y + h > 256) y -= 256;
    return x + w > 0 && x < 256 && y + h > 0 && y < 192;
}

u32 SpriteLimitInversion::LookOf(const u16* attr)
{
    // tile (10 bits), palette (4), shape and size (4), 256 colours, affine,
    // mode (normal, translucent, window, bitmap)
    return (attr[2] & 0x3FF) | ((attr[2] >> 12) << 10) | ((attr[0] >> 14) << 14) | ((attr[1] >> 14) << 16)
         | (((attr[0] >> 13) & 1) << 18) | (((attr[0] >> 8) & 1) << 19) | (((attr[0] >> 10) & 3) << 20);
}

int SpriteLimitInversion::CyclePeriod(u32 history)
{
    const u32 mask = (1u << Window) - 1;
    const u32 h = history & mask;
    if (h == 0 || h == mask) return 0;
    for (int p = 2; p <= 4; p++)
    {
        bool same = true;
        for (int i = 0; i + p < Window && same; i++)
            same = ((h >> i) & 1) == ((h >> (i + p)) & 1);
        if (same) return p;
    }
    return 0;
}

void SpriteLimitInversion::AddFrame(const u16* oam)
{
    std::vector<bool> matched(Tracks.size(), false);
    for (Track& t : Tracks) { t.History <<= 1; t.Frames++; }

    for (int i = 0; oam && i < 128; i++)
    {
        const u16* attr = &oam[i * 4];
        if (!IsDrawn(attr)) continue;

        Sprite s;
        for (int a = 0; a < 3; a++) s.Attr[a] = attr[a];
        s.X = attr[1] & 0x1FF; if (s.X >= 256) s.X -= 512;
        s.Y = attr[0] & 0xFF; if (s.Y >= 192) s.Y -= 256;
        const u32 look = LookOf(attr);

        // the nearest sprite of the same look not matched yet
        int best = -1, bestDist = MaxMove + 1;
        for (size_t t = 0; t < Tracks.size(); t++)
        {
            if (matched[t] || Tracks[t].Look != look) continue;
            const int d = std::max(std::abs(Tracks[t].Last.X - s.X), std::abs(Tracks[t].Last.Y - s.Y));
            if (d < bestDist) { best = (int)t; bestDist = d; }
        }
        if (best < 0)
        {
            Track t;
            t.Id = NextId++;
            t.Look = look;
            t.Frames = 1;
            Tracks.push_back(t);
            matched.push_back(false);
            best = (int)Tracks.size() - 1;
        }
        Track& t = Tracks[best];
        matched[best] = true;
        t.Last = s;
        t.History |= 1;
        t.Shown++;
    }

    // forget sprites not drawn for a whole window
    std::vector<Track> kept;
    kept.reserve(Tracks.size());
    for (Track& t : Tracks)
    {
        if (!(t.History & ((1u << Window) - 1))) continue;
        t.Period = t.Frames >= (u32)Window ? CyclePeriod(t.History) : 0;
        kept.push_back(t);
    }
    Tracks.swap(kept);
}

std::vector<SpriteLimitInversion::Sprite> SpriteLimitInversion::Restored() const
{
    std::vector<Sprite> out;
    for (const Track& t : Tracks)
        if (t.Period && !(t.History & 1)) out.push_back(t.Last);
    return out;
}

u32 SpriteLimitInversion::Flickering() const
{
    u32 n = 0;
    for (const Track& t : Tracks) if (t.Period) n++;
    return n;
}

}
