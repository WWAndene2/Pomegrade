// Frame rate modes (app/src/main/cpp/renderer/FrameRatePacer): which DS frames
// are shown and how many images are generated after each, by mode, screen
// refresh rate, load and heat.
#include "FrameRatePacer.h"
#include <cstdio>
#include <initializer_list>

static bool ok = true;
static void check(bool cond, const char* what)
{
    printf("%s: %s\n", what, cond ? "yes" : "NO");
    ok = ok && cond;
}

// shown frames and generated images over n DS frames
static void Run(FrameRatePacer& p, int n, int& shown, int& generated, double workMs = 4, double intervalMs = 1000.0 / 60)
{
    shown = generated = 0;
    for (int i = 0; i < n; i++)
    {
        FrameRatePacer::Plan plan = p.NextFrame();
        shown += plan.Show;
        generated += plan.Show ? plan.Generated : 0;
        p.FrameDone(workMs, intervalMs);
    }
}

int main()
{
    int shown, generated;

    // fixed modes on a 240 Hz screen: images per second = (shown + generated) per 60 frames
    for (int mode : {30, 60, 120, 240})
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetMode(mode);
        Run(p, 60, shown, generated);
        printf("mode %d on 240 Hz: %d images a second\n", mode, shown + generated);
        check(shown + generated == mode, "fixed mode: its images a second");
    }

    // the screen's rate caps the generated images (no judder from dropped ones)
    struct { int mode; float hz; int expected; } caps[] = {
        {120, 60, 60}, {120, 90, 60}, {120, 119.88f, 120}, {120, 144, 120},
        {240, 60, 60}, {240, 144, 120}, {240, 165, 120}, {240, 239.76f, 240},
        {30, 144, 30}, {60, 240, 60},
    };
    for (auto& c : caps)
    {
        FrameRatePacer p;
        p.SetDisplayRate(c.hz);
        p.SetMode(c.mode);
        Run(p, 60, shown, generated);
        printf("mode %d on %.2f Hz: %d images a second (expected %d)\n", c.mode, c.hz, shown + generated, c.expected);
        check(shown + generated == c.expected, "capped by the screen");
    }

    // 30: every other DS frame, nothing generated
    {
        FrameRatePacer p;
        p.SetMode(30);
        bool alternate = true;
        for (int i = 0; i < 10; i++)
        {
            auto plan = p.NextFrame();
            alternate = alternate && plan.Show == (i % 2 == 0) && plan.NextShow == !plan.Show && plan.Generated == 0;
        }
        check(alternate, "30: one DS frame in two shown (and the 3D rendered during a shown one is hidden)");
    }

    // a screen that changes rate while a fixed mode runs
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetMode(240);
        Run(p, 10, shown, generated);
        p.SetDisplayRate(60);
        Run(p, 60, shown, generated);
        check(shown + generated == 60, "screen slowed to 60 Hz: no images generated");
        p.SetDisplayRate(240);
        Run(p, 60, shown, generated);
        check(shown + generated == 240, "back to 240 Hz: 240 again");
    }

    // adaptive: starts at the screen's top, lowered a step each busy second,
    // raised again after three calm seconds
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetMode(FrameRatePacer::Adaptive);
        check(p.NextFrame().Generated == 3, "adaptive on 240 Hz: starts at 240");
        p.FrameDone(4, 1000.0 / 60);
        Run(p, 59, shown, generated, 15); // busy: 15 ms of a 16.7 ms frame
        check(p.CurrentRate() == 120, "adaptive, one busy second: 120");
        Run(p, 60, shown, generated, 15);
        check(p.CurrentRate() == 60, "adaptive, two busy seconds: 60");
        Run(p, 60, shown, generated, 15);
        check(p.CurrentRate() == 30, "adaptive, three busy seconds: 30");
        Run(p, 60, shown, generated, 15);
        check(p.CurrentRate() == 30, "adaptive: never below 30");
        Run(p, 120, shown, generated, 5); // calm: 5 ms
        check(p.CurrentRate() == 30, "adaptive, two calm seconds: unchanged");
        Run(p, 60, shown, generated, 5);
        check(p.CurrentRate() == 60, "adaptive, three calm seconds: raised a step");
        Run(p, 180 * 3, shown, generated, 5);
        check(p.CurrentRate() == 240, "adaptive, calm long enough: back to the top");
        Run(p, 60, shown, generated, 10); // between the two shares
        check(p.CurrentRate() == 240, "adaptive, neither busy nor calm: unchanged");
        Run(p, 60, shown, generated, 5, 20.0); // light work but frames late
        check(p.CurrentRate() == 120, "adaptive, frames late: lowered");
    }

    // adaptive is capped by the screen, and by heat
    {
        FrameRatePacer p;
        p.SetDisplayRate(144);
        p.SetMode(FrameRatePacer::Adaptive);
        Run(p, 600, shown, generated, 2);
        check(p.CurrentRate() == 120, "adaptive on 144 Hz: 120 at most");
        p.SetThermalLimit(true);
        Run(p, 1, shown, generated, 2);
        check(p.CurrentRate() == 60, "phone hot: adaptive lowered to 60 at once");
        Run(p, 600, shown, generated, 2);
        check(p.CurrentRate() == 60, "phone hot: stays at 60 however calm");
        p.SetThermalLimit(false);
        Run(p, 180, shown, generated, 2);
        check(p.CurrentRate() == 120, "cooled down: back up");
    }

    // no image generation (software or Compute renderer): 60 at most, 30 still works
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetGenerationAvailable(false);
        for (int mode : {30, 60, 120, 240, FrameRatePacer::Adaptive})
        {
            p.SetMode(mode);
            Run(p, 60, shown, generated, 2);
            printf("no generation, mode %d: %d images a second\n", mode, shown + generated);
            check(shown + generated == (mode == 30 ? 30 : 60), "no generation: 60 at most");
        }
        check(p.CurrentRate() == 60, "no generation: adaptive reports 60");
    }

    // stalls (a pause, shader compilation) don't lower the adaptive rate
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetMode(FrameRatePacer::Adaptive);
        Run(p, 1, shown, generated, 4);
        p.NextFrame();
        p.FrameDone(4, 5000); // resumed after five seconds
        p.NextFrame();
        p.FrameDone(900, 900); // shaders compiled
        Run(p, 59, shown, generated, 4);
        check(p.CurrentRate() == 240, "stalls ignored by adaptive");
    }

    // fixed modes ignore load
    {
        FrameRatePacer p;
        p.SetDisplayRate(240);
        p.SetMode(240);
        Run(p, 600, shown, generated, 30, 30);
        check(p.CurrentRate() == 240, "fixed 240 stays at 240 under load");
    }

    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
