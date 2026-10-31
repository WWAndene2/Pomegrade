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
//   audio NAME / audio off the game's sound from then on into <work dir>/NAME.wav (16-bit stereo at the core's rate), or stop
//   until COND MAX [CYCLE] frames run until COND holds, at most MAX, then "[frame N] until: met|not met after F frames";
//                          COND: log:TEXT (a core log line holding TEXT, '_' for a space: log:DllBattle), change (the
//                          screen differs from when it began), still:N (the screen unchanged N frames), bright>X /
//                          bright<X (mean brightness 0-255); CYCLE: keys held in turn, KEYS:N,KEYS:N (left:16,right:16:
//                          walking back and forth), none by default. Screen conditions need drawing: with drawing off,
//                          one frame in 15 is drawn meanwhile (23 October: one command instead of a run per guess)
//   burst NAME EVERY COUNT COUNT shots NAME_1..., one every EVERY frames (an intro, a transition)
// Whatever the commands, a screen that stays the same for 10 seconds is reported once (a freeze).
//
// <work dir>/Azahar is the core's user folder: put the mod in Azahar/load/mods/000400000011C400/ and the save in
// Azahar/sdmc/Nintendo 3DS/00000000000000000000000000000000/00000000000000000000000000000000/title/00040000/0011c400/data/00000001/.
// Exit code 0: field reached; 1: not reached; 2: the core or game did not load.

#include "libretro.h"

#include <dlfcn.h>
#include <sys/stat.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cerrno>
#include <chrono>

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

// "until log:TEXT": set when a core log line holds the text watched
static std::string logWatch;
static bool logSeen = false;

static void Log(enum retro_log_level level, const char* format, ...)
{
    char text[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (strstr(text, "CRO \"DllFieldEventPlayer\" loaded")) fieldUp = true;
    if (!logWatch.empty() && strstr(text, logWatch.c_str())) logSeen = true;
    // the modules and the file system tell where the game is; the rest is printed up to a bound
    if (level >= RETRO_LOG_WARN || strstr(text, "CRO \"") || strstr(text, "LayeredFS") || logLines < 400)
        fprintf(stdout, "[frame %lu] %s%s", frame, text, text[0] && text[strlen(text) - 1] == '\n' ? "" : "\n");
    logLines++;
}


// POMEGRADE_GL=1: the core's OpenGL renderer instead of the software one, in an EGL context with no screen (Mesa's
// llvmpipe on the runner): the core draws into a framebuffer of ours, read back for the screenshots and the checks.
static const bool glMode = getenv("POMEGRADE_GL") != nullptr;

static retro_hw_render_callback hwRender{};
static unsigned glFbo = 0;
static constexpr unsigned GlFboSize = 2048;
typedef void (*GlGen)(int, unsigned*);
typedef void (*GlBind)(unsigned, unsigned);
typedef void (*GlStorage)(unsigned, unsigned, int, int);
typedef void (*GlAttach)(unsigned, unsigned, unsigned, unsigned);
typedef void (*GlRead)(int, int, int, int, unsigned, unsigned, void*);
// EGL loaded only in this mode (libEGL.so.1), so the software runs need no EGL on the runner
static void* egl = nullptr;
template <typename F> static F Egl(const char* name) { return reinterpret_cast<F>(dlsym(egl, name)); }
static uintptr_t GlCurrentFramebuffer() { return glFbo; }
static retro_proc_address_t GlProc(const char* name)
{
    return reinterpret_cast<retro_proc_address_t>(Egl<void* (*)(const char*)>("eglGetProcAddress")(name));
}
static bool GlCreateContext()
{
    egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!egl) return false;
    // Mesa's surfaceless platform: no X or Wayland in the container (EGL_DEFAULT_DISPLAY needs one)
    const auto eglGetPlatformDisplay = Egl<EGLDisplay (*)(EGLenum, void*, const EGLAttrib*)>("eglGetPlatformDisplay");
    const auto eglInitialize = Egl<EGLBoolean (*)(EGLDisplay, EGLint*, EGLint*)>("eglInitialize");
    const auto eglBindAPI = Egl<EGLBoolean (*)(EGLenum)>("eglBindAPI");
    const auto eglCreateContext = Egl<EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*)>("eglCreateContext");
    const auto eglMakeCurrent = Egl<EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext)>("eglMakeCurrent");
    EGLDisplay display = eglGetPlatformDisplay(0x31DD, EGL_DEFAULT_DISPLAY, nullptr); // EGL_PLATFORM_SURFACELESS_MESA
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_API)) return false;
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) return false;
    unsigned colour = 0, depth = 0;
    reinterpret_cast<GlGen>(GlProc("glGenFramebuffers"))(1, &glFbo);
    reinterpret_cast<GlBind>(GlProc("glBindFramebuffer"))(0x8D40, glFbo); // GL_FRAMEBUFFER
    reinterpret_cast<GlGen>(GlProc("glGenRenderbuffers"))(1, &colour);
    reinterpret_cast<GlBind>(GlProc("glBindRenderbuffer"))(0x8D41, colour); // GL_RENDERBUFFER
    reinterpret_cast<GlStorage>(GlProc("glRenderbufferStorage"))(0x8D41, 0x8058, GlFboSize, GlFboSize); // GL_RGBA8
    reinterpret_cast<GlAttach>(GlProc("glFramebufferRenderbuffer"))(0x8D40, 0x8CE0, 0x8D41, colour); // COLOR_ATTACHMENT0
    reinterpret_cast<GlGen>(GlProc("glGenRenderbuffers"))(1, &depth);
    reinterpret_cast<GlBind>(GlProc("glBindRenderbuffer"))(0x8D41, depth);
    reinterpret_cast<GlStorage>(GlProc("glRenderbufferStorage"))(0x8D41, 0x88F0, GlFboSize, GlFboSize); // DEPTH24_STENCIL8
    reinterpret_cast<GlAttach>(GlProc("glFramebufferRenderbuffer"))(0x8D40, 0x821A, 0x8D41, depth); // DEPTH_STENCIL_ATTACHMENT
    return true;
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
        if (!strcmp(var->key, "citra_graphics_api")) var->value = glMode ? "OpenGL" : "Software";
        // POMEGRADE_SCALE: the internal resolution (1-10 times the 3DS's), with POMEGRADE_GL
        if (!strcmp(var->key, "citra_resolution_factor") && getenv("POMEGRADE_SCALE")) var->value = getenv("POMEGRADE_SCALE");
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
    case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
        // OpenGL only with POMEGRADE_GL
        auto* cb = static_cast<retro_hw_render_callback*>(data);
        if (!glMode || (cb->context_type != RETRO_HW_CONTEXT_OPENGL_CORE && cb->context_type != RETRO_HW_CONTEXT_OPENGL)) return false;
        if (!GlCreateContext()) { fprintf(stderr, "no OpenGL context (EGL)\n"); return false; }
        cb->get_current_framebuffer = GlCurrentFramebuffer;
        cb->get_proc_address = GlProc;
        hwRender = *cb;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
        if (!glMode) return false;
        *static_cast<unsigned*>(data) = RETRO_HW_CONTEXT_OPENGL_CORE;
        return true;
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
        return glMode;
    default:
        return false; // no rumble, microphone, ...
    }
}

static std::vector<uint32_t> lastFrame;
static unsigned lastWidth = 0, lastHeight = 0;
static uint64_t frameHash = 0;

static std::vector<uint32_t> glPixels;
static void VideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
    if (!data || !width || !height) return;
    if (data == RETRO_HW_FRAME_BUFFER_VALID)
    {
        // the frame the core drew into our framebuffer, bottom row first in OpenGL: read and turned upright, as XRGB
        std::vector<uint32_t> rows((size_t)width * height);
        reinterpret_cast<GlBind>(GlProc("glBindFramebuffer"))(0x8CA8, glFbo); // GL_READ_FRAMEBUFFER
        reinterpret_cast<GlRead>(GlProc("glReadPixels"))(0, 0, (int)width, (int)height, 0x80E1, 0x1401, rows.data()); // BGRA, UNSIGNED_BYTE
        glPixels.resize(rows.size());
        for (unsigned y = 0; y < height; y++) std::memcpy(&glPixels[(size_t)y * width], &rows[(size_t)(height - 1 - y) * width], width * 4);
        data = glPixels.data();
        pitch = width * 4;
    }
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

// "audio NAME": the core's samples written to a WAV file (its header's sizes filled in when it is closed), to check that a
// sound plays (a stream's level), not to listen to in a run that skips frames
static FILE* audioFile = nullptr;
static uint32_t audioFrames = 0, audioRate = 32728;
static void AudioClose()
{
    if (!audioFile) return;
    const uint32_t bytes = audioFrames * 4, riff = 36 + bytes;
    fseek(audioFile, 4, SEEK_SET); fwrite(&riff, 4, 1, audioFile);
    fseek(audioFile, 40, SEEK_SET); fwrite(&bytes, 4, 1, audioFile);
    fclose(audioFile);
    audioFile = nullptr;
}
static void AudioOpen(const std::string& path)
{
    AudioClose();
    audioFile = fopen(path.c_str(), "wb");
    if (!audioFile) return;
    audioFrames = 0;
    const uint32_t fmtSize = 16, rate = audioRate, byteRate = audioRate * 4, zero = 0;
    const uint16_t pcm = 1, channels = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, audioFile); fwrite(&zero, 4, 1, audioFile); fwrite("WAVEfmt ", 1, 8, audioFile);
    fwrite(&fmtSize, 4, 1, audioFile); fwrite(&pcm, 2, 1, audioFile); fwrite(&channels, 2, 1, audioFile);
    fwrite(&rate, 4, 1, audioFile); fwrite(&byteRate, 4, 1, audioFile); fwrite(&align, 2, 1, audioFile); fwrite(&bits, 2, 1, audioFile);
    fwrite("data", 1, 4, audioFile); fwrite(&zero, 4, 1, audioFile);
}
static void AudioSample(int16_t left, int16_t right)
{
    if (!audioFile) return;
    const int16_t both[2] = {left, right};
    fwrite(both, 2, 2, audioFile);
    audioFrames++;
}
static size_t AudioBatch(const int16_t* data, size_t frames)
{
    if (audioFile) { fwrite(data, 4, frames, audioFile); audioFrames += (uint32_t)frames; }
    return frames;
}
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
// "draw off": the core's software renderer skips its triangles (pomegrade_set_skip_draw, sw_rasterizer.cpp), the game's
// logic running on, much faster where no picture is wanted; a shot, a screen check and the final result draw a few frames
// first, then drawing stops again. The screen does not change while off: no freeze can be told, and "field" (which waits
// for the field's picture) needs drawing on
static void (*setSkipDraw)(bool) = nullptr;
static bool drawOff = false;
// "draw every N" (owner's idea, 20 October): drawing on, but only one frame in N drawn (the others skip their triangles
// as "draw off" does), the game's logic still run every frame. Measured on the idle field (20 October): every frame drawn
// 2.0 frames/s (run fps_on), one in 4 3.9 (fps_e4), one in 15 18-24 (fps_e15, e15b: its shot whole), none about 67
// (fps_off): a drawn frame costs far more than its share, so a large N pays where a picture is needed now and then
static unsigned long drawEvery = 1;

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

// one frame, watching for a screen that no longer changes; once a minute of real time, the speed: the other "[frame N]"
// lines come only with an event, so the last of them is not the frame the game is at (ORAS_ENGINE.md 0, rule 3)
static void Step()
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    static Clock::time_point lastSpeed = start;
    static unsigned long lastSpeedFrame = 0;
    if (drawEvery > 1 && !drawOff && setSkipDraw) setSkipDraw(frame % drawEvery != 0);
    runFrame();
    frame++;
    const Clock::time_point now = Clock::now();
    if (now - lastSpeed >= std::chrono::seconds(60))
    {
        const double minute = std::chrono::duration<double>(now - lastSpeed).count();
        const double total = std::chrono::duration<double>(now - start).count();
        printf("[frame %lu] speed: %.1f frames/s over the last minute, %.1f since the start (%.0f s)\n", frame,
               (frame - lastSpeedFrame) / minute, frame / total, total);
        fflush(stdout);
        lastSpeed = now; lastSpeedFrame = frame;
    }
    if (frameHash != sameHash) { sameHash = frameHash; sameSince = frame; freezeShown = false; }
    else if (!freezeShown && frame - sameSince >= 600)
    {
        printf("[frame %lu] the screen has not changed for 10 s (since frame %llu)\n", frame, (unsigned long long)sameSince);
        freezeShown = true;
    }
}


static void DrawFrames()
{
    if (!drawOff && drawEvery <= 1) return;
    setSkipDraw(false);
    // drawn whole: Step's one-in-N skip would leave these frames undrawn (a black shot, run fps_e15)
    const unsigned long every = drawEvery;
    drawEvery = 1;
    for (int k = 0; k < 3; k++) Step();
    drawEvery = every;
}

static void DrawStop() { if (drawOff) setSkipDraw(true); }

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
    // interactive FIFO: from then on the commands come from the named pipe FIFO, a batch per writer (headless/play.sh), each
    // batch followed by "[ready frame N]" in the log; "quit" ends the session as the script's end would
    std::string fifo;
    std::ifstream pipe;
    const auto next = [&](std::string& l) {
        if (fifo.empty()) return (bool)std::getline(in, l);
        while (true)
        {
            if (pipe.is_open() && std::getline(pipe, l)) return l != "quit";
            if (pipe.is_open()) { pipe.close(); printf("[ready frame %lu]\n", frame); fflush(stdout); }
            pipe.open(fifo); // blocks until a writer opens the pipe
            if (!pipe.is_open()) { printf("interactive: cannot open %s\n", fifo.c_str()); fflush(stdout); return false; }
        }
    };
    while (next(line))
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
        else if (cmd == "screen") { DrawFrames(); Screen(); DrawStop(); }
        else if (cmd == "shot") { DrawFrames(); Shot(arg); DrawStop(); }
        else if (cmd == "until")
        {
            std::string max, cycle; words >> max >> cycle;
            const unsigned long limit = strtoul(max.c_str(), nullptr, 10);
            std::vector<std::pair<unsigned, unsigned long>> steps; // keys, frames
            for (size_t at = 0; at < cycle.size();)
            {
                const size_t comma = cycle.find(',', at), colon = cycle.find(':', at);
                const std::string part = cycle.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                if (colon == std::string::npos || colon > at + part.size()) { printf("until: a cycle step is KEYS:FRAMES\n"); break; }
                steps.push_back({Keys(part.substr(0, colon - at)), strtoul(part.c_str() + (colon - at) + 1, nullptr, 10)});
                at = comma == std::string::npos ? cycle.size() : comma + 1;
            }
            const bool screen = arg.rfind("log:", 0) != 0;
            const unsigned long everyWas = drawEvery;
            const bool offWas = drawOff;
            if (screen && drawOff && setSkipDraw) { drawOff = false; drawEvery = 15; setSkipDraw(false); }
            logWatch.clear(); logSeen = false;
            if (!screen) { logWatch = arg.substr(4); for (char& c : logWatch) if (c == '_') c = ' '; }
            const uint64_t startHash = frameHash;
            const double threshold = arg.size() > 7 ? strtod(arg.c_str() + 7, nullptr) : 0;
            const unsigned long stillFor = arg.rfind("still:", 0) == 0 ? strtoul(arg.c_str() + 6, nullptr, 10) : 0;
            unsigned long k = 0, stepAt = 0;
            size_t step = 0;
            bool met = false;
            for (; k < limit && !met; k++)
            {
                if (!steps.empty())
                {
                    if (stepAt >= steps[step].second) { step = (step + 1) % steps.size(); stepAt = 0; }
                    held = steps[step].first;
                    stepAt++;
                }
                Step();
                if (!screen) met = logSeen;
                else if (arg == "change") met = frameHash != startHash;
                else if (stillFor) met = frame - sameSince >= stillFor;
                else if (arg.rfind("bright>", 0) == 0) met = luminance > threshold;
                else if (arg.rfind("bright<", 0) == 0) met = luminance < threshold;
                else { printf("until: unknown condition %s\n", arg.c_str()); break; }
            }
            held = 0;
            logWatch.clear();
            if (screen && setSkipDraw) { drawEvery = everyWas; drawOff = offWas; setSkipDraw(drawOff); }
            printf("[frame %lu] until %s: %s after %lu frames, screen brightness %.1f\n", frame, arg.c_str(), met ? "met" : "NOT met", k, luminance);
        }
        else if (cmd == "burst")
        {
            std::string count; words >> count;
            const unsigned long every = strtoul(count.c_str(), nullptr, 10);
            std::string number; words >> number;
            const unsigned long n = strtoul(number.c_str(), nullptr, 10);
            for (unsigned long k = 1; k <= n; k++)
            {
                for (unsigned long f = 0; f + 3 < every; f++) Step();
                DrawFrames(); Shot(arg + "_" + std::to_string(k)); DrawStop();
            }
        }
        else if (cmd == "frames")
        {
            // frames N NAME [KEYS]: N consecutive frames saved, NAME_000.ppm..., KEYS held meanwhile (test sequences for frame
            // generation: a 3DS game's real in-between frames to compare generated ones with)
            std::string name, keys; words >> name >> keys;
            held = keys.empty() ? 0 : Keys(keys);
            DrawFrames();
            const unsigned n = (unsigned)strtoul(arg.c_str(), nullptr, 10);
            for (unsigned k = 0; k < n; k++) { char b[32]; snprintf(b, sizeof b, "_%03u", k); Step(); Shot(name + b); }
            held = 0;
            DrawStop();
        }
        else if (cmd == "draw")
        {
            if (!setSkipDraw) setSkipDraw = reinterpret_cast<void (*)(bool)>(dlsym(core, "pomegrade_set_skip_draw"));
            if (!setSkipDraw) printf("draw: the core has no pomegrade_set_skip_draw (an older build)\n");
            else if (arg == "every")
            {
                words >> drawEvery;
                if (drawEvery < 1) drawEvery = 1;
                drawOff = false;
                setSkipDraw(false);
                printf("[frame %lu] drawing one frame in %lu\n", frame, drawEvery);
            }
            else { drawOff = arg == "off"; drawEvery = 1; setSkipDraw(drawOff); printf("[frame %lu] drawing %s\n", frame, drawOff ? "off" : "on"); }
        }
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
        else if (cmd == "writers" || cmd == "readers")
        {
            // writers|readers LO HI [PCLO PCHI] | writers|readers off FILE: the game's writes into (reads from) [LO, HI) in between,
            // with their code (pomegrade_writers / pomegrade_readers; with POMEGRADE_INTERPRETER=1, the JIT bypasses both). FILE is
            // relative to the run's folder ("writes.txt", not "runs/NAME/writes.txt")
            std::string second; words >> second;
            auto log = reinterpret_cast<uint32_t (*)(uint32_t, uint32_t, const char*)>(dlsym(core, ("pomegrade_" + cmd).c_str()));
            if (!log) printf("(no pomegrade_%s in this core)\n", cmd.c_str());
            else if (arg == "off") { const std::string path = workDir + "/" + second; printf("[frame %lu] %s off: %u %s in %s\n", frame, cmd.c_str(), log(0, 0, path.c_str()), cmd == "writers" ? "writes" : "reads", path.c_str()); }
            else
            {
                // PCLO PCHI, optional: only the accesses made by the code in [PCLO, PCHI) (pomegrade_writers_code / pomegrade_readers_code)
                std::string pclo, pchi; words >> pclo >> pchi;
                auto code = reinterpret_cast<void (*)(uint32_t, uint32_t)>(dlsym(core, ("pomegrade_" + cmd + "_code").c_str()));
                if (code) code(pclo.empty() ? 0 : strtoul(pclo.c_str(), nullptr, 0), pchi.empty() ? 0xFFFFFFFFu : strtoul(pchi.c_str(), nullptr, 0));
                log(strtoul(arg.c_str(), nullptr, 0), strtoul(second.c_str(), nullptr, 0), nullptr);
                printf("[frame %lu] %s %s %s %s %s\n", frame, cmd.c_str(), arg.c_str(), second.c_str(), pclo.c_str(), pchi.c_str());
            }
        }
        else if (cmd == "poke")
        {
            // poke ADDRESS VALUE: one word of the game's memory written (pomegrade_poke); a float is written as its bits (0x3F800000)
            std::string value; words >> value;
            auto poke = reinterpret_cast<bool (*)(uint32_t, uint32_t)>(dlsym(core, "pomegrade_poke"));
            if (!poke) printf("(no pomegrade_poke in this core)\n");
            else printf("[frame %lu] poke %s %s: %s\n", frame, arg.c_str(), value.c_str(), poke(strtoul(arg.c_str(), nullptr, 0), strtoul(value.c_str(), nullptr, 0)) ? "written" : "not mapped");
        }
        else if (cmd == "audio")
        {
            if (arg == "off") { AudioClose(); printf("[frame %lu] audio off: %u frames\n", frame, audioFrames); }
            else { AudioOpen(workDir + "/" + arg + ".wav"); printf("[frame %lu] audio into %s.wav\n", frame, arg.c_str()); }
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
        else if (cmd == "lights")
        {
            // lights on | lights off FILE: the light state and shader uniforms of each draw in between (pomegrade_lights)
            auto lights = reinterpret_cast<uint32_t (*)(int, const char*)>(dlsym(core, "pomegrade_lights"));
            std::string file; words >> file;
            const std::string path = workDir + "/" + file;
            if (!lights) printf("(no pomegrade_lights in this core)\n");
            else if (arg == "on") { lights(1, nullptr); printf("[frame %lu] lights on\n", frame); }
            else printf("[frame %lu] lights off: %u draws in %s\n", frame, lights(0, path.c_str()), path.c_str());
        }
        else if (cmd == "gdb")
        {
            // gdb PORT: the core's GDB stub listens there; the game waits for gdb-multiarch to connect and continue
            auto gdb = reinterpret_cast<bool (*)(uint16_t)>(dlsym(core, "pomegrade_gdb"));
            printf("gdb stub on port %s: %s\n", arg.c_str(), gdb && gdb((uint16_t)strtoul(arg.c_str(), nullptr, 10)) ? "open" : "not in this core");
        }
        else if (cmd == "mem") { std::string length; words >> length; MemoryDump(core, arg, length); }
        else if (cmd == "save") StateFile(core, arg, true);
        else if (cmd == "interactive")
        {
            fifo = arg;
            if (mkfifo(fifo.c_str(), 0600) != 0 && errno != EEXIST) printf("interactive: no pipe %s\n", fifo.c_str());
            printf("[ready frame %lu]\n", frame);
        }
        else if (cmd == "load")
        {
            // the core may refuse a state until it has run long enough (run cache2, 8 October: refused at frame 120): tried
            // again every 60 frames, 20 times
            for (int i = 0; i < 20 && !StateFile(core, arg, false); i++)
                for (int f = 0; f < 60; f++) Step();
        }
        else printf("unknown command %s\n", cmd.c_str());
        fflush(stdout);
    }
    DrawFrames();
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
    if (glMode && hwRender.context_reset) hwRender.context_reset(); // the frontend's part once the game is loaded
    {
        retro_system_av_info av{};
        Symbol<void (*)(retro_system_av_info*)>(core, "retro_get_system_av_info")(&av);
        if (av.timing.sample_rate > 0) audioRate = (uint32_t)(av.timing.sample_rate + 0.5);
    }
    auto run = Symbol<void (*)()>(core, "retro_run");
    runFrame = run;
    if (argc > 5) { const int result = RunScript(core, argv[5]); AudioClose(); return result; }
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
