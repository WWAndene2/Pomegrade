#include "FrameRatePacer.h"

#include <algorithm>

namespace
{
constexpr double FrameBudgetMs = 1000.0 / 60.0;
// adaptive: a second of frames is judged at a time
constexpr int WindowLength = 60;
// lower the rate when the emulation thread is busy more than this share of a
// frame, or frames come late; raise it after a few calm seconds below the
// lower share (the gap keeps it from going back and forth)
constexpr double BusyShare = 0.8;
constexpr double CalmShare = 0.45;
constexpr double LateInterval = FrameBudgetMs * 1.08;
constexpr int CalmWindowsToRaise = 3;
// a frame taking longer is a stall, not load: ignored
constexpr double StallMs = 100.0;

int StepDown(int level)
{
    return level > 120 ? 120 : level > 60 ? 60 : 30;
}

int StepUp(int level)
{
    return level < 60 ? 60 : level < 120 ? 120 : 240;
}
}

void FrameRatePacer::SetMode(int mode)
{
    Mode.store(mode, std::memory_order_relaxed);
}

void FrameRatePacer::SetDisplayRate(float hz)
{
    DisplayRate.store(hz, std::memory_order_relaxed);
}

void FrameRatePacer::SetThermalLimit(bool limited)
{
    Thermal.store(limited, std::memory_order_relaxed);
}

void FrameRatePacer::SetGenerationAvailable(bool available)
{
    GenerationAvailable.store(available, std::memory_order_relaxed);
}

int FrameRatePacer::DisplayCap(float hz)
{
    // a little under the nominal rate: screens report 119.88, 239.76...
    return hz >= 239.0f ? 240 : hz >= 119.0f ? 120 : 60;
}

int FrameRatePacer::TargetFor(int mode) const
{
    const int cap = GenerationAvailable.load(std::memory_order_relaxed)
            ? DisplayCap(DisplayRate.load(std::memory_order_relaxed)) : 60;
    if (mode == 30 || mode == 60)
        return mode;
    if (mode == Adaptive)
        return Thermal.load(std::memory_order_relaxed) ? std::min(cap, 60) : cap;
    return std::min(mode, cap);
}

FrameRatePacer::Plan FrameRatePacer::NextFrame()
{
    const int mode = Mode.load(std::memory_order_relaxed);
    const int target = TargetFor(mode);
    int level = Level.load(std::memory_order_relaxed);
    if (mode != AppliedMode)
    {
        // adaptive starts at the top and comes down if the phone can't keep up
        AppliedMode = mode;
        level = target;
        WindowWork = WindowInterval = 0;
        WindowFrames = CalmWindows = 0;
    }
    else if (mode != Adaptive)
        level = target; // the screen's rate can change while the game runs
    else
        level = std::min(level, target); // a slower screen, or the phone got hot
    Level.store(level, std::memory_order_relaxed);

    Plan plan;
    if (level == 30)
    {
        plan.Show = (FrameCount & 1) == 0;
        plan.NextShow = !plan.Show;
    }
    else
        plan.Generated = level / 60 - 1;
    FrameCount++;
    return plan;
}

void FrameRatePacer::FrameDone(double workMs, double intervalMs)
{
    if (AppliedMode != Adaptive || workMs > StallMs || intervalMs > StallMs)
        return;
    WindowWork += workMs;
    WindowInterval += intervalMs;
    if (++WindowFrames < WindowLength)
        return;

    const double work = WindowWork / WindowFrames, interval = WindowInterval / WindowFrames;
    WindowWork = WindowInterval = 0;
    WindowFrames = 0;

    int level = Level.load(std::memory_order_relaxed);
    if (work > FrameBudgetMs * BusyShare || interval > LateInterval)
    {
        level = StepDown(level);
        CalmWindows = 0;
    }
    else if (work < FrameBudgetMs * CalmShare && interval <= LateInterval)
    {
        if (++CalmWindows >= CalmWindowsToRaise)
        {
            level = std::min(StepUp(level), TargetFor(Adaptive));
            CalmWindows = 0;
        }
    }
    else
        CalmWindows = 0;
    Level.store(level, std::memory_order_relaxed);
}
