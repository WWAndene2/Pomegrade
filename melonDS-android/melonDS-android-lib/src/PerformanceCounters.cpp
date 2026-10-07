#include "PerformanceCounters.h"

#include <mutex>

namespace melonDS::PerformanceCounters
{

std::atomic<bool> EnabledFlag{false};

namespace
{
// emulation thread only
double Sums[SectionCount] = {};
int WindowFrames = 0;
bool WasEnabled = false;
std::chrono::steady_clock::time_point WindowStart;

bool GpuTimed = false;

// published, read from any thread
std::mutex PublishedMutex;
Snapshot Published;

constexpr bool IsGpuSection(int s) { return s >= (int)Section::GpuScene; }

void ResetWindow()
{
    for (double& s : Sums) s = 0;
    WindowFrames = 0;
    GpuTimed = false;
    WindowStart = std::chrono::steady_clock::now();
}
}

void SetEnabled(bool enabled)
{
    if (EnabledFlag.exchange(enabled, std::memory_order_relaxed) == enabled)
        return;
    if (!enabled)
    {
        // EndFrame publishes only while enabled, checked under the same lock:
        // nothing measured before this point is published after it
        std::lock_guard lock(PublishedMutex);
        Published = {};
    }
}

void Add(Section section, double ms)
{
    Sums[(int)section] += ms;
}

void MarkGpuTimed()
{
    GpuTimed = true;
}

void EndFrame()
{
    const bool enabled = Enabled();
    if (!enabled)
    {
        WasEnabled = false;
        return;
    }
    if (!WasEnabled)
    {
        // a new measurement starts clean (sections timed before the switch are dropped)
        WasEnabled = true;
        ResetWindow();
        return;
    }

    WindowFrames++;
    const auto now = std::chrono::steady_clock::now();
    if (now - WindowStart < std::chrono::seconds(1))
        return;

    Snapshot snapshot;
    snapshot.Frames = WindowFrames;
    for (int s = 0; s < SectionCount; s++)
        snapshot.Ms[s] = IsGpuSection(s) && !GpuTimed ? -1.0f : (float)(Sums[s] / WindowFrames);
    {
        std::lock_guard lock(PublishedMutex);
        if (Enabled())
            Published = snapshot;
    }
    ResetWindow();
}

Snapshot Get()
{
    std::lock_guard lock(PublishedMutex);
    return Published;
}

}
