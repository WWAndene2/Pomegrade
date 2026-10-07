// DS debug trace (app/src/main/cpp/DebugTrace): a record in a dated folder,
// lines written as they come, and a watchdog that says, every second, which
// stage the emulator is in and for how long (where a frozen start stops).
#include "DebugTrace.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

static bool ok = true;
static void check(bool cond, const char* what)
{
    printf("%s: %s\n", what, cond ? "yes" : "NO");
    ok = ok && cond;
}

static void Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DebugTrace::NoteV("ERROR: ", fmt, args);
    va_end(args);
}

static std::string ReadRecord(const std::filesystem::path& folder)
{
    for (auto& dir : std::filesystem::directory_iterator(folder))
    {
        std::ifstream in(dir.path() / "trace.txt");
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }
    return {};
}

static int Count(const std::string& text, const std::string& what)
{
    int n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) n++;
    return n;
}

int main()
{
    const auto folder = std::filesystem::temp_directory_path() / "pomegrade_debug_trace_test";
    std::filesystem::remove_all(folder);

    // off: nothing written, nothing created
    DebugTrace::Note("not written");
    DebugTrace::Stage("not written either");
    check(!DebugTrace::Enabled() && !std::filesystem::exists(folder), "off: no record");

    DebugTrace::Configure(true, folder.string(), "Device: test\nSettings: test");
    check(DebugTrace::Enabled(), "on");
    DebugTrace::Stage("loading the ROM");
    DebugTrace::Note("ROM loaded: result %d", 0);
    Log("failed to compile fragment shader %s\n", "RenderShader00");
    DebugTrace::Stage("creating the OpenGL renderer");
    // the record is readable while the game runs (a frozen game gets killed)
    std::this_thread::sleep_for(std::chrono::milliseconds(2300));
    const std::string during = ReadRecord(folder);
    for (int i = 0; i < 3; i++) { DebugTrace::FrameEmulated(); DebugTrace::FramePresented(true); }
    DebugTrace::FramePresented(false);
    DebugTrace::Configure(false, "", "");
    const std::string record = ReadRecord(folder);
    printf("--- record ---\n%s--------------\n", record.c_str());

    check(record.rfind("Pomegrade DS debug trace, ", 0) == 0 && record.find("Device: test\nSettings: test") != std::string::npos,
          "the header first");
    check(record.find("stage: loading the ROM") != std::string::npos && record.find("stage: creating the OpenGL renderer") != std::string::npos,
          "stages written as they change");
    check(record.find("ROM loaded: result 0") != std::string::npos, "notes written");
    check(record.find("ERROR: failed to compile fragment shader RenderShader00\n") != std::string::npos,
          "log messages written, one line each");
    check(Count(during, "watchdog: stage \"creating the OpenGL renderer\" for 2.") == 1 &&
          Count(during, "watchdog: stage \"creating the OpenGL renderer\"") >= 2,
          "watchdog: the stage it is stuck in, and for how long, every second, while it runs");
    check(Count(record, "frame 3 emulated") == 1 && Count(record, "frame presented (3)") == 1, "first frames written");
    check(record.find("record ended") != std::string::npos, "turned off: the record ends");
    check(record.find("not written") == std::string::npos, "nothing from before it was on");

    std::filesystem::remove_all(folder);
    puts(ok ? "ALL OK" : "FAILED");
    return ok ? 0 : 1;
}
