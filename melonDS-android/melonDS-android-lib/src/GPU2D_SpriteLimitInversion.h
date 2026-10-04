#ifndef GPU2D_SPRITELIMITINVERSION_H
#define GPU2D_SPRITELIMITINVERSION_H

// Sprite-limit inversion (Pomegrade, DS_ENGINE_REMAKE.md 5.5, 5.12 step 12):
// the 2D engines draw at most 128 sprites and a limited number of sprite
// pixels per scanline, so a game with more sprites than that shows a
// different subset each frame (multiplexing), and they flicker. Fed one
// engine's OAM per frame, this follows each sprite from frame to frame and
// finds those drawn on a short regular cycle (shown, hidden, shown...) while
// staying in place: the sprites to draw every frame instead.
//
// A sprite is followed by what it draws (tile, palette, size, colour mode,
// affine or not), not by its OAM slot, since multiplexing games move sprites
// between slots, and is matched to the nearest sprite of the same look that
// moved at most MaxMove pixels. Only cycles of 2 to 4 frames count: a
// sprite blinking on purpose (a cursor, damage) blinks slower than that, the
// risk the design notes name. Restored sprites are found, not drawn yet.

#include "types.h"

#include <vector>

namespace melonDS
{

class SpriteLimitInversion
{
public:
    static constexpr int MaxMove = 16;   // pixels per frame
    static constexpr int Window = 8;     // frames the cycle is checked over

    struct Sprite
    {
        u16 Attr[3] = {}; // OAM attributes 0-2 as last drawn
        int X = 0, Y = 0; // screen position (signed, attributes 0-1)
    };
    struct Track
    {
        u32 Id = 0;
        u32 Look = 0;      // tile, palette, size, colour mode, affine
        Sprite Last;       // the last time it was drawn
        u32 History = 0;   // bit n: drawn n frames ago (bit 0: this frame)
        u32 Frames = 0;    // frames followed
        u32 Shown = 0;     // frames drawn
        int Period = 0;    // its drawing cycle in frames (0: none found)
    };

    // one engine's OAM (128 entries of four u16) at the end of a frame;
    // null: no sprites drawn (the engine's sprites are off)
    void AddFrame(const u16* oam);
    void Clear() { Tracks.clear(); NextId = 1; }

    [[nodiscard]] const std::vector<Track>& GetTracks() const { return Tracks; }
    // the sprites to draw this frame that the game hid: flickering sprites
    // not drawn this frame, at their last position
    [[nodiscard]] std::vector<Sprite> Restored() const;
    // sprites drawn on a regular cycle (the multiplexed ones)
    [[nodiscard]] u32 Flickering() const;

    // whether OAM entry attributes describe a drawn sprite
    static bool IsDrawn(const u16* attr);
    static u32 LookOf(const u16* attr);

private:
    std::vector<Track> Tracks;
    u32 NextId = 1;
    static int CyclePeriod(u32 history);
};

}

#endif // GPU2D_SPRITELIMITINVERSION_H
