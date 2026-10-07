#include "DebugTrace.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace DebugTrace
{

namespace
{
using Clock = std::chrono::steady_clock;

// stage lines are written for the first frames only (they change every frame)
constexpr unsigned long long StageFramesWritten = 10;

std::atomic<bool> On{false};
std::mutex FileMutex; // the file and the watchdog's lifetime
FILE* File = nullptr;
Clock::time_point Start;

std::atomic<const char*> CurrentStage{"none"};
std::atomic<long long> StageSinceNs{0};
std::atomic<unsigned long long> Emulated{0}, PresentedValid{0}, PresentedEmpty{0};

std::thread Watchdog;
std::mutex WatchdogMutex;
std::condition_variable WatchdogWake;
bool WatchdogStop = false;

long long NowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - Start).count();
}

// one line: seconds since the record started, the thread, the text
void WriteLine(const char* prefix, const char* fmt, va_list args)
{
    char text[2048];
    vsnprintf(text, sizeof(text), fmt, args);
    // log messages bring their own line end, notes don't
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
        text[--len] = 0;
    char thread[32] = "?";
    pthread_getname_np(pthread_self(), thread, sizeof(thread));
    std::lock_guard lock(FileMutex);
    if (!File) return;
    fprintf(File, "[%9.3f] [%s] %s%s\n", NowNs() / 1e9, thread, prefix, text);
    fflush(File);
}

void Line(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WriteLine("", fmt, args);
    va_end(args);
}

void WatchdogLoop()
{
    pthread_setname_np(pthread_self(), "TraceWatchdog");
    std::unique_lock lock(WatchdogMutex);
    while (!WatchdogWake.wait_for(lock, std::chrono::seconds(1), [] { return WatchdogStop; }))
    {
        const double stageSeconds = (NowNs() - StageSinceNs.load()) / 1e9;
        Line("watchdog: stage \"%s\" for %.1f s; frames emulated %llu, presented %llu (and %llu presentations with no new frame)",
             CurrentStage.load(), stageSeconds, Emulated.load(), PresentedValid.load(), PresentedEmpty.load());
    }
}

void StopRecord()
{
    {
        std::lock_guard lock(WatchdogMutex);
        WatchdogStop = true;
    }
    WatchdogWake.notify_all();
    if (Watchdog.joinable())
        Watchdog.join();
    std::lock_guard lock(FileMutex);
    if (File)
    {
        fprintf(File, "[%9.3f] record ended\n", NowNs() / 1e9);
        fclose(File);
        File = nullptr;
    }
}
}

void Configure(bool enabled, const std::string& folder, const std::string& header)
{
    if (On.exchange(false))
        StopRecord();
    if (!enabled || folder.empty())
        return;

    char date[32];
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    std::strftime(date, sizeof(date), "%Y-%m-%d_%H-%M-%S", &local);
    std::error_code error;
    const std::filesystem::path dir = std::filesystem::path(folder) / date;
    std::filesystem::create_directories(dir, error);
    {
        std::lock_guard lock(FileMutex);
        File = fopen((dir / "trace.txt").c_str(), "w");
        if (!File) return;
        Start = Clock::now();
        fprintf(File, "Pomegrade DS debug trace, %s\n%s\n\n", date, header.c_str());
        fflush(File);
    }
    CurrentStage = "configured";
    StageSinceNs = 0;
    Emulated = PresentedValid = PresentedEmpty = 0;
    WatchdogStop = false;
    On = true;
    Watchdog = std::thread(WatchdogLoop);
}

bool Enabled()
{
    return On.load(std::memory_order_relaxed);
}

void Note(const char* fmt, ...)
{
    if (!Enabled()) return;
    va_list args;
    va_start(args, fmt);
    WriteLine("", fmt, args);
    va_end(args);
}

void NoteV(const char* prefix, const char* fmt, va_list args)
{
    if (!Enabled()) return;
    WriteLine(prefix, fmt, args);
}

void Stage(const char* stage)
{
    if (!Enabled()) return;
    if (CurrentStage.exchange(stage) == stage) return;
    StageSinceNs = NowNs();
    if (Emulated.load() < StageFramesWritten)
        Line("stage: %s", stage);
}

void FrameEmulated()
{
    if (!Enabled()) return;
    const unsigned long long n = ++Emulated;
    if (n <= StageFramesWritten)
        Line("frame %llu emulated", n);
}

void FramePresented(bool valid)
{
    if (!Enabled()) return;
    if (valid)
    {
        const unsigned long long n = ++PresentedValid;
        if (n <= StageFramesWritten)
            Line("frame presented (%llu)", n);
    }
    else
        ++PresentedEmpty;
}

}
