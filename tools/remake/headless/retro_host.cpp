// A headless host for Azahar's libretro core (azahar/src/citra_libretro, built with the software renderer): runs Omega Ruby
// with a mod and a save, presses A through the title screen and the Continue menu, and reports whether the field came up
// (the core's log shows the module DllFieldEventPlayer loading, as on the phone when the map shows: ORAS_LITTLEROOT.md 11).
// When it does not, it calls pomegrade_report (pomegrade_report.inc, added to the core by the workflow only) to print where
// every thread of the game stands: the lead to the code that bounds a map piece (ORAS_LITTLEROOT.md 10, item 5).
//
//   retro_host <core.so> <game.3ds> <work dir> [seconds of game time, default 120]
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
#include <string>

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

static void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
    if (!data || !width || !height) return;
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

// A pressed for 4 frames every second, once the system has had 5 seconds to boot
static int16_t InputState(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_A) return frame > 300 && frame % 60 < 4;
    return 0;
}

template <typename T> static T Symbol(void* core, const char* name)
{
    void* p = dlsym(core, name);
    if (!p) { fprintf(stderr, "the core has no %s\n", name); exit(2); }
    return reinterpret_cast<T>(p);
}

static void Report(const char* line) { printf("%s\n", line); }

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
        auto report = reinterpret_cast<void (*)(void (*)(const char*))>(dlsym(core, "pomegrade_report"));
        if (report) report(Report);
        else printf("(no pomegrade_report in this core)\n");
    }
    fflush(stdout);
    std::_Exit(fieldUp ? 0 : 1); // the core's threads are not shut down: exit at once
}
