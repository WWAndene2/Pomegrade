#include "GPU_OpenGL_Timer.h"

#include "OpenGLSupport.h"

#include <cstring>
#include <deque>
#include <vector>

#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif
#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif

namespace melonDS::GLTimer
{

using PerformanceCounters::Section;

namespace
{
struct Query
{
    GLuint Id;
    Section Sect;
};

bool Checked = false;
bool Supported = false;

std::vector<GLuint> FreeQueries;
std::deque<Query> Pending;  // ended, result not read yet, oldest first
Query Running = {0, Section::GpuScene};

constexpr int MaxDepth = 8;
Section Stack[MaxDepth];
int Depth = 0;

// results the GPU never delivers (lost context...) don't pile up
constexpr size_t MaxPending = 256;

bool Init()
{
    if (!Checked)
    {
        Checked = true;
        Supported = false;
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; i++)
        {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (ext && !strcmp(ext, "GL_EXT_disjoint_timer_query"))
                Supported = true;
        }
    }
    return Supported;
}

void Recycle(GLuint id)
{
    FreeQueries.push_back(id);
}

void StopRunning()
{
    if (!Running.Id) return;
    glEndQuery(GL_TIME_ELAPSED_EXT);
    Pending.push_back(Running);
    Running.Id = 0;
}

void Start(Section section)
{
    StopRunning();
    GLuint id;
    if (FreeQueries.empty())
        glGenQueries(1, &id);
    else
    {
        id = FreeQueries.back();
        FreeQueries.pop_back();
    }
    glBeginQuery(GL_TIME_ELAPSED_EXT, id);
    Running = {id, section};
    PerformanceCounters::MarkGpuTimed();
}
}

Scope::Scope(Section section)
{
    Active = PerformanceCounters::Enabled() && Init() && Depth < MaxDepth;
    if (!Active) return;
    for (int i = 0; i < Depth; i++)
        if (Stack[i] == Section::GpuFrameGeneration)
            section = Section::GpuFrameGeneration;
    Stack[Depth++] = section;
    Start(section);
}

Scope::~Scope()
{
    if (!Active) return;
    Depth--;
    StopRunning();
    if (Depth > 0)
        Start(Stack[Depth - 1]);
}

void Collect()
{
    if (!Supported || Pending.empty())
        return;

    // reading the flag clears it: a disjoint event (frequency change, context
    // loss...) makes every result in flight meaningless
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint)
    {
        for (const Query& q : Pending) Recycle(q.Id);
        Pending.clear();
        return;
    }

    while (!Pending.empty())
    {
        const Query& q = Pending.front();
        GLuint available = 0;
        glGetQueryObjectuiv(q.Id, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) break;
        GLuint ns = 0;
        glGetQueryObjectuiv(q.Id, GL_QUERY_RESULT, &ns);
        PerformanceCounters::Add(q.Sect, ns / 1e6);
        Recycle(q.Id);
        Pending.pop_front();
    }
    while (Pending.size() > MaxPending)
    {
        Recycle(Pending.front().Id);
        Pending.pop_front();
    }
}

void Release()
{
    if (Running.Id)
    {
        glEndQuery(GL_TIME_ELAPSED_EXT);
        glDeleteQueries(1, &Running.Id);
        Running.Id = 0;
    }
    for (const Query& q : Pending) glDeleteQueries(1, &q.Id);
    Pending.clear();
    if (!FreeQueries.empty())
        glDeleteQueries((GLsizei)FreeQueries.size(), FreeQueries.data());
    FreeQueries.clear();
    Depth = 0;
    Checked = false;
}

}
