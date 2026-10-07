#ifndef GPU_OPENGL_TIMER_H
#define GPU_OPENGL_TIMER_H

// GPU timing of the OpenGL renderer's passes (Pomegrade), for the performance
// counters. GL_EXT_disjoint_timer_query time-elapsed queries, one at a time
// (they can't nest): a scope ends the running query and starts its own, and
// restarts the enclosing section's when it ends. Results are read a few frames
// later, without waiting for the GPU. Used only on the thread owning the GL
// context, and only while the counters are enabled.

#include "PerformanceCounters.h"

namespace melonDS::GLTimer
{

// GPU time from construction to destruction counts towards section. Inside a
// frame generation scope, everything counts as frame generation.
class Scope
{
public:
    explicit Scope(PerformanceCounters::Section section);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    bool Active;
};

// once per emulated frame: adds the results the GPU has finished to the counters
void Collect();

// deletes the query objects (the GL context is about to go)
void Release();

}

#endif // GPU_OPENGL_TIMER_H
