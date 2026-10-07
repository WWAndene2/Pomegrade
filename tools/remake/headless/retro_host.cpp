// A headless host for Azahar's libretro core (azahar/src/citra_libretro, built with the software renderer): runs Omega Ruby
// with a mod and a save, presses A through the title screen and the Continue menu, and reports whether the field came up
// (the core's log shows the module DllFieldEventPlayer loading, as on the phone when the map shows: ORAS_LITTLEROOT.md 11).
// When it does not, it calls pomegrade_report (pomegrade_report.inc, added to the core by the workflow only) to print where
// every thread of the game stands: the lead to the code that bounds a map piece (ORAS_LITTLEROOT.md 10, item 5).
//
//   retro_host <core.so> <game.3ds> <work dir> [seconds of game time, default 120] [script]
//
// With a script, the game is driven by its commands instead, one per line (# starts a comment), to play it turn by turn
// from the Remake mod workflow (headless input, +script; ORAS_LITTLEROOT.md section 0):
//   wait N                 N frames (60 a second) with nothing pressed
//   hold KEYS N            KEYS held for N frames: a b x y l r start select up down left right, joined with +
//   press KEYS             held 6 frames, then 6 released
//   mash KEYS N            pressed 4 frames every second, for N seconds (through the title and menus)
//   field [N]              A mashed until the field is up (DllFieldEventPlayer loads), at most N seconds (default 120)
//   screen                 the screen as text: 96 x 36 characters, darker to brighter " .:-=+*#%@"
//   report                 every thread of the game: state, PC, LR, SP, code addresses on its stack
//   save NAME / load NAME  a save state, <work dir>/NAME.state (the same mod must be loaded to use it)
// Whatever the commands, a screen that stays the same for 10 seconds is reported once (a freeze).
//
// <work dir>/Azahar is the core's user folder: put the mod in Azahar/load/mods/000400000011C400/ and the save in
// Azahar/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001/.
// Exit code 0: field reached; 1: not reached; 2: the core or game did not load.

#include "libretro.h"

#include <dlfcn.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::string workDir;
static bool fieldUp = false;
static unsigned long frame = 0;
static double luminance = 0;
static unsigned long logLines = 0;

static void Log(enum retro_log_level level, const char* format, ...)
{
    char text[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (strstr(text, "CRO \"DllFieldEventPlayer\" loaded")) fieldUp = true;
    // the modules and the file system tell where the game is; the rest is printed up to a bound
    if (level >= RETRO_LOG_WARN || strstr(text, "CRO \"") || strstr(text, "LayeredFS") || logLines < 400)
        fprintf(stdout, "[frame %lu] %s%s", frame, text, text[0] && text[strlen(text) - 1] == '\n' ? "" : "\n");
    logLines++;
}

static bool Environment(unsigned cmd, void* data)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = Log;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = workDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *static_cast<const retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        // the core's defaults, but the software renderer (no GPU on the runner) and the JIT
        auto* var = static_cast<retro_variable*>(data);
        var->value = nullptr;
        if (!strcmp(var->key, "citra_graphics_api")) var->value = "Software";
        // the interpreter instead of the JIT, for code coverage ("trace on"): slower, but every block passes its dispatch
        // (and not the FastInterp interpreter, which bypasses it too: the classic one, DynCom)
        if ((!strcmp(var->key, "citra_use_cpu_jit") || !strcmp(var->key, "citra_use_fastinterp")) && getenv("POMEGRADE_INTERPRETER")) var->value = "disabled";
        return var->value != nullptr;
    }
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(data) = 0; // the core then gives its options as SET_VARIABLES
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
    case RETRO_ENVIRONMENT_SET_MESSAGE:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        return true;
    default:
        return false; // no hardware rendering, rumble, microphone, ...
    }
}

static std::vector<uint32_t> lastFrame;
static unsigned lastWidth = 0, lastHeight = 0;
static uint64_t frameHash = 0;

static void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
    if (!data || !width || !height) return;
    lastWidth = width; lastHeight = height;
    lastFrame.resize((size_t)width * height);
    uint64_t hash = 1469598103934665603ull;
    for (unsigned y = 0; y < height; y++)
    {
        std::memcpy(&lastFrame[(size_t)y * width], static_cast<const uint8_t*>(data) + y * pitch, width * 4);
        for (unsigned x = 0; x < width; x += 7) hash = (hash ^ lastFrame[(size_t)y * width + x]) * 1099511628211ull;
    }
    frameHash = hash;
    // mean brightness of the last frame: a black screen (the hang seen on the phone) against a drawn map
    double sum = 0;
    for (unsigned y = 0; y < height; y += 4)
        for (unsigned x = 0; x < width; x += 4)
        {
            const uint32_t p = static_cast<const uint32_t*>(static_cast<const void*>(static_cast<const uint8_t*>(data) + y * pitch))[x];
            sum += ((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF);
        }
    luminance = sum / ((width / 4.0) * (height / 4.0) * 3.0);
}

static void AudioSample(int16_t, int16_t) {}
static size_t AudioBatch(const int16_t*, size_t frames) { return frames; }
static void InputPoll() {}

// without a script: A pressed for 4 frames every second, once the system has had 5 seconds to boot; with one: the keys
// its command holds (a bit per RETRO_DEVICE_ID_JOYPAD_* id)
static bool scripted = false;
static unsigned held = 0;
static int16_t InputState(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    if (scripted) return (held >> id) & 1;
    if (id == RETRO_DEVICE_ID_JOYPAD_A) return frame > 300 && frame % 60 < 4;
    return 0;
}

static unsigned Keys(const std::string& names)
{
    static const struct { const char* Name; unsigned Id; } keys[] = {
        {"a", RETRO_DEVICE_ID_JOYPAD_A}, {"b", RETRO_DEVICE_ID_JOYPAD_B}, {"x", RETRO_DEVICE_ID_JOYPAD_X}, {"y", RETRO_DEVICE_ID_JOYPAD_Y},
        {"l", RETRO_DEVICE_ID_JOYPAD_L}, {"r", RETRO_DEVICE_ID_JOYPAD_R}, {"start", RETRO_DEVICE_ID_JOYPAD_START},
        {"select", RETRO_DEVICE_ID_JOYPAD_SELECT}, {"up", RETRO_DEVICE_ID_JOYPAD_UP}, {"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
        {"left", RETRO_DEVICE_ID_JOYPAD_LEFT}, {"right", RETRO_DEVICE_ID_JOYPAD_RIGHT}};
    unsigned mask = 0;
    std::stringstream in(names);
    std::string name;
    while (std::getline(in, name, '+'))
    {
        bool known = false;
        for (const auto& k : keys) if (name == k.Name) { mask |= 1u << k.Id; known = true; }
        if (!known) printf("unknown key %s\n", name.c_str());
    }
    return mask;
}

static void Screen()
{
    if (lastFrame.empty()) { printf("(no frame yet)\n"); return; }
    static const char ramp[] = " .:-=+*#%@";
    const unsigned cols = 96, rows = 36;
    printf("screen %u x %u at frame %lu:\n", lastWidth, lastHeight, frame);
    for (unsigned r = 0; r < rows; r++)
    {
        std::string line;
        for (unsigned c = 0; c < cols; c++)
        {
            double sum = 0;
            unsigned n = 0;
            for (unsigned y = r * lastHeight / rows; y < (r + 1) * lastHeight / rows; y += 2)
                for (unsigned x = c * lastWidth / cols; x < (c + 1) * lastWidth / cols; x += 2, n++)
                {
                    const uint32_t p = lastFrame[(size_t)y * lastWidth + x];
                    sum += ((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF);
                }
            line += ramp[n ? std::min<int>(9, (int)(sum / n / 3 / 25.6)) : 0];
        }
        printf("|%s|\n", line.c_str());
    }
}

// "shot NAME": the last frame written to NAME.ppm in the work folder (binary PPM, 8-bit RGB), to look at a screen the text
// rendering of "screen" cannot tell apart (an error dialog, a white screen)
static void Shot(const std::string& name)
{
    if (lastFrame.empty()) { printf("(no frame yet)\n"); return; }
    const std::string path = workDir + "/" + name + ".ppm";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { printf("shot: cannot write %s\n", path.c_str()); return; }
    fprintf(f, "P6\n%u %u\n255\n", lastWidth, lastHeight);
    for (uint32_t p : lastFrame) { const unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p}; fwrite(rgb, 1, 3, f); }
    fclose(f);
    printf("shot %s at frame %lu\n", path.c_str(), frame);
}

template <typename T> static T Symbol(void* core, const char* name)
{
    void* p = dlsym(core, name);
    if (!p) { fprintf(stderr, "the core has no %s\n", name); exit(2); }
    return reinterpret_cast<T>(p);
}

static void Report(const char* line) { printf("%s\n", line); }

static void ThreadReport(void* core)
{
    auto report = reinterpret_cast<void (*)(void (*)(const char*))>(dlsym(core, "pomegrade_report"));
    if (report) report(Report);
    else printf("(no pomegrade_report in this core)\n");
}

// "mem ADDRESS LENGTH": the game's memory (pomegrade_peek), to read the objects a fatal-error report's registers point at
static void MemoryDump(void* core, const std::string& address, const std::string& length)
{
    auto peek = reinterpret_cast<void (*)(uint32_t, uint32_t, void (*)(const char*))>(dlsym(core, "pomegrade_peek"));
    if (peek) peek(strtoul(address.c_str(), nullptr, 0), strtoul(length.c_str(), nullptr, 0), Report);
    else printf("(no pomegrade_peek in this core)\n");
}

// "watch FRAMES ADDRESS...": runs FRAMES frames holding the keys of the last hold (none), printing each address's word
// whenever one changes, with the frame: the order in which the game writes a loader's fields (a race between threads)
static void Step();
static void Watch(void* core, unsigned long frames, const std::vector<uint32_t>& addresses, unsigned keys)
{
    auto peek = reinterpret_cast<void (*)(uint32_t, uint32_t, void (*)(const char*))>(dlsym(core, "pomegrade_peek"));
    if (!peek) { printf("(no pomegrade_peek in this core)\n"); return; }
    static std::string last;
    static void (*keep)(const char*) = [](const char* line) { last += line + 6; }; // "  mem " dropped
    std::string before;
    for (unsigned long k = 0; k < frames; k++)
    {
        held = keys;
        Step();
        last.clear();
        for (uint32_t a : addresses) peek(a, 4, keep);
        if (last != before) { printf("[frame %lu] watch%s\n", frame, last.c_str()); before = last; }
    }
    held = 0;
}

static void (*runFrame)() = nullptr;
static uint64_t sameSince = 0, sameHash = 0;
static bool freezeShown = false;

// one frame, watching for a screen that no longer changes
static void Step()
{
    runFrame();
    frame++;
    if (frameHash != sameHash) { sameHash = frameHash; sameSince = frame; freezeShown = false; }
    else if (!freezeShown && frame - sameSince >= 600)
    {
        printf("[frame %lu] the screen has not changed for 10 s (since frame %llu)\n", frame, (unsigned long long)sameSince);
        freezeShown = true;
    }
}

static bool StateFile(void* core, const std::string& name, bool save)
{
    const std::string path = workDir + "/" + name + ".state";
    if (save)
    {
        const size_t size = Symbol<size_t (*)()>(core, "retro_serialize_size")();
        std::vector<char> data(size);
        if (!size || !Symbol<bool (*)(void*, size_t)>(core, "retro_serialize")(data.data(), size)) { printf("save %s: the core made no state\n", name.c_str()); return false; }
        std::ofstream(path, std::ios::binary).write(data.data(), (std::streamsize)size);
        printf("[frame %lu] state %s saved, %zu bytes\n", frame, name.c_str(), size);
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty() || !Symbol<bool (*)(const void*, size_t)>(core, "retro_unserialize")(data.data(), data.size())) { printf("load %s: no state or the core refused it\n", name.c_str()); return false; }
    printf("[frame %lu] state %s loaded\n", frame, name.c_str());
    return true;
}

static int RunScript(void* core, const char* path)
{
    std::ifstream in(path);
    if (!in) { fprintf(stderr, "no script %s\n", path); return 2; }
    scripted = true;
    std::string line;
    while (std::getline(in, line))
    {
        if (line.find('#') != std::string::npos) line.erase(line.find('#'));
        std::stringstream words(line);
        std::string cmd, arg;
        if (!(words >> cmd)) continue;
        words >> arg;
        unsigned long n = 0;
        printf("> %s\n", line.c_str());
        if (cmd == "wait") { held = 0; n = strtoul(arg.c_str(), nullptr, 10); for (unsigned long k = 0; k < n; k++) Step(); }
        else if (cmd == "hold") { std::string count; words >> count; held = Keys(arg); n = strtoul(count.c_str(), nullptr, 10); for (unsigned long k = 0; k < n; k++) Step(); held = 0; }
        else if (cmd == "press") { held = Keys(arg); for (int k = 0; k < 6; k++) Step(); held = 0; for (int k = 0; k < 6; k++) Step(); }
        else if (cmd == "mash")
        {
            std::string count; words >> count;
            const unsigned keys = Keys(arg);
            n = 60 * strtoul(count.c_str(), nullptr, 10);
            for (unsigned long k = 0; k < n; k++) { held = k % 60 < 4 ? keys : 0; Step(); }
            held = 0;
        }
        else if (cmd == "field")
        {
            n = 60 * (arg.empty() ? 120 : strtoul(arg.c_str(), nullptr, 10));
            for (unsigned long k = 0; k < n && !fieldUp; k++) { held = k % 60 < 4 ? Keys("a") : 0; Step(); }
            held = 0;
            printf("[frame %lu] field %s\n", frame, fieldUp ? "up" : "not up");
        }
        else if (cmd == "screen") Screen();
        else if (cmd == "shot") Shot(arg);
        else if (cmd == "report") ThreadReport(core);
        else if (cmd == "watch")
        {
            // watch FRAMES [KEYS] ADDRESS...: KEYS held meanwhile when the word after FRAMES is not a number
            std::vector<uint32_t> addresses;
            std::string word;
            unsigned keys = 0;
            while (words >> word)
                if (isdigit((unsigned char)word[0])) addresses.push_back(strtoul(word.c_str(), nullptr, 0));
                else keys = Keys(word);
            Watch(core, strtoul(arg.c_str(), nullptr, 10), addresses, keys);
        }
        else if (cmd == "dump")
        {
            // dump ADDRESS LENGTH FILE: the game's memory written raw to FILE in the work folder (pomegrade_dump)
            std::string length, file; words >> length >> file;
            auto dump = reinterpret_cast<uint32_t (*)(uint32_t, uint32_t, const char*)>(dlsym(core, "pomegrade_dump"));
            const std::string path = workDir + "/" + file;
            if (!dump) printf("(no pomegrade_dump in this core)\n");
            else printf("dump %s: %u bytes mapped of %s\n", path.c_str(), dump(strtoul(arg.c_str(), nullptr, 0), strtoul(length.c_str(), nullptr, 0), path.c_str()), length.c_str());
        }
        else if (cmd == "trace")
        {
            // trace on | trace off FILE: the blocks of code the game runs in between (pomegrade_trace), FILE in the work folder
            auto trace = reinterpret_cast<uint32_t (*)(int, const char*)>(dlsym(core, "pomegrade_trace"));
            std::string file; words >> file;
            const std::string path = workDir + "/" + file;
            if (!trace) printf("(no pomegrade_trace in this core)\n");
            else if (arg == "on") { trace(1, nullptr); printf("[frame %lu] trace on\n", frame); }
            else printf("[frame %lu] trace off: %u blocks in %s\n", frame, trace(0, path.c_str()), path.c_str());
        }
        else if (cmd == "gdb")
        {
            // gdb PORT: the core's GDB stub listens there; the game waits for gdb-multiarch to connect and continue
            auto gdb = reinterpret_cast<bool (*)(uint16_t)>(dlsym(core, "pomegrade_gdb"));
            printf("gdb stub on port %s: %s\n", arg.c_str(), gdb && gdb((uint16_t)strtoul(arg.c_str(), nullptr, 10)) ? "open" : "not in this core");
        }
        else if (cmd == "mem") { std::string length; words >> length; MemoryDump(core, arg, length); }
        else if (cmd == "save" || cmd == "load") StateFile(core, arg, cmd == "save");
        else printf("unknown command %s\n", cmd.c_str());
        fflush(stdout);
    }
    printf("RESULT: script done at %lu s of game time, field %s, screen brightness %.1f\n", frame / 60, fieldUp ? "up" : "not up", luminance);
    fflush(stdout);
    std::_Exit(0);
}

int main(int argc, char** argv)
{
    if (argc < 4) { fprintf(stderr, "usage: retro_host <core.so> <game.3ds> <work dir> [seconds]\n"); return 2; }
    workDir = argv[3];
    const unsigned long frames = 60UL * (argc > 4 ? strtoul(argv[4], nullptr, 10) : 120);
    void* core = dlopen(argv[1], RTLD_NOW);
    if (!core) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }

    Symbol<void (*)(retro_environment_t)>(core, "retro_set_environment")(Environment);
    Symbol<void (*)(retro_video_refresh_t)>(core, "retro_set_video_refresh")(VideoRefresh);
    Symbol<void (*)(retro_audio_sample_t)>(core, "retro_set_audio_sample")(AudioSample);
    Symbol<void (*)(retro_audio_sample_batch_t)>(core, "retro_set_audio_sample_batch")(AudioBatch);
    Symbol<void (*)(retro_input_poll_t)>(core, "retro_set_input_poll")(InputPoll);
    Symbol<void (*)(retro_input_state_t)>(core, "retro_set_input_state")(InputState);
    Symbol<void (*)()>(core, "retro_init")();

    retro_game_info game{};
    game.path = argv[2];
    if (!Symbol<bool (*)(const retro_game_info*)>(core, "retro_load_game")(&game)) { fprintf(stderr, "the game did not load\n"); return 2; }
    auto run = Symbol<void (*)()>(core, "retro_run");
    runFrame = run;
    if (argc > 5) return RunScript(core, argv[5]);
    unsigned long fieldFrame = 0;
    for (frame = 0; frame < frames; frame++)
    {
        run();
        if (frame % 600 == 0) printf("[frame %lu] %lu s of game time, screen brightness %.1f\n", frame, frame / 60, luminance);
        // once the field is up, 5 more seconds to see it drawn
        if (fieldUp && !fieldFrame) fieldFrame = frame;
        if (fieldFrame && frame > fieldFrame + 300) break;
    }
    printf("RESULT: %s after %lu s of game time, screen brightness %.1f\n", fieldUp ? "FIELD" : "NO FIELD", frame / 60, luminance);
    if (!fieldUp)
    {
        ThreadReport(core);
    }
    fflush(stdout);
    std::_Exit(fieldUp ? 0 : 1); // the core's threads are not shut down: exit at once
}
