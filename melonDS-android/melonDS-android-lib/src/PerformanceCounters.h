#ifndef PERFORMANCECOUNTERS_H
#define PERFORMANCECOUNTERS_H

// Performance counters (Pomegrade): where the time of a DS frame goes, shown
// in the app's performance details. CPU sections are timed on the emulation
// thread; GPU sections by timer queries (GPU_OpenGL_Timer). Off by default:
// when off, nothing is timed (one relaxed atomic load per section).

#include <atomic>
#include <chrono>

namespace melonDS::PerformanceCounters
{

enum class Section
{
    // CPU, emulation thread
    EmulationCpu,       // a whole frame: emulated CPUs, geometry, the renderer's CPU work
    PolygonMultiplierCpu,
    TexturesCpu,        // HD texture lookups and native upscaling (MMPX)
    // GPU
    GpuScene,           // the 3D scene, apart from what is below
    GpuShadows,         // the real-time shadow map
    GpuLighting,        // ambient occlusion, light bounce and reflections (one pass)
    GpuCompositor,      // the two screens composed, scene colour effects
    GpuFrameGeneration, // everything an in-between image draws
    Count
};

constexpr int SectionCount = (int)Section::Count;

struct Snapshot
{
    // milliseconds per DS frame, averaged over the last second; the GPU
    // sections are negative when nothing timed the GPU in that second (software
    // renderer, or no GL_EXT_disjoint_timer_query)
    float Ms[SectionCount] = {};
    int Frames = 0; // DS frames the averages cover (0: nothing measured yet)
};

extern std::atomic<bool> EnabledFlag;
inline bool Enabled() { return EnabledFlag.load(std::memory_order_relaxed); }

void SetEnabled(bool enabled);
void Add(Section section, double ms);
// the GPU was timed in the current window (GLTimer, emulation thread)
void MarkGpuTimed();
// once per emulated frame, emulation thread: publishes the averages every second
void EndFrame();
// any thread
Snapshot Get();

// times a CPU section from construction to destruction, when enabled. Two
// clock reads each time: the polygon multiplier's time includes them for each
// polygon it subdivides (their cost on a phone is not measured)
class CpuScope
{
public:
    explicit CpuScope(Section section) : Sect(section), Active(Enabled())
    {
        if (Active) Start = std::chrono::steady_clock::now();
    }
    ~CpuScope()
    {
        if (Active)
            Add(Sect, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count());
    }
    CpuScope(const CpuScope&) = delete;
    CpuScope& operator=(const CpuScope&) = delete;

private:
    Section Sect;
    bool Active;
    std::chrono::steady_clock::time_point Start;
};

}

#endif // PERFORMANCECOUNTERS_H
