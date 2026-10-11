// The GBA core (mGBA) for GbaNative: one game at a time, run frame by frame from the emulation thread.
// The frame goes to a direct buffer the screen copies into a bitmap, the sound to a short array.
#include <jni.h>
#include <android/log.h>
#include <fcntl.h>
#include <cstring>
#include <string>

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>

namespace {

constexpr unsigned WIDTH = 240;
constexpr unsigned HEIGHT = 160;
// mGBA's 32-bit colour is R, G, B, then a byte its renderer leaves unset: made opaque for the bitmap
constexpr uint32_t OPAQUE = 0xFF000000;

mCore* core = nullptr;
color_t* frame = nullptr;
int sampleRate = 0;

void log(const char* message, const char* detail) {
    __android_log_print(ANDROID_LOG_WARN, "PomegradeGba", "%s: %s", message, detail);
}

void unload() {
    if (!core) return;
    core->unloadROM(core);
    core->deinit(core);
    core = nullptr;
    frame = nullptr;
}

std::string text(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out(chars);
    env->ReleaseStringUTFChars(s, chars);
    return out;
}

}

extern "C" {

// The game at romPath, its battery save at savePath (created when the game first saves), the BIOS at
// biosPath if one is given (mGBA's own otherwise). The frame is written into frameBuffer (240 x 160
// RGBA pixels, a direct buffer the caller keeps), the sound at rate Hz.
JNIEXPORT jboolean JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_load(JNIEnv* env, jobject, jstring romPath, jstring savePath,
                                                     jstring biosPath, jobject frameBuffer, jint rate) {
    unload();
    const std::string rom = text(env, romPath);
    core = mCoreFind(rom.c_str());
    if (!core) {
        log("not a game mGBA runs", rom.c_str());
        return JNI_FALSE;
    }
    mCoreInitConfig(core, nullptr);
    if (!core->init(core)) {
        log("the core did not start", rom.c_str());
        core = nullptr;
        return JNI_FALSE;
    }
    frame = static_cast<color_t*>(env->GetDirectBufferAddress(frameBuffer));
    core->setVideoBuffer(core, frame, WIDTH);

    sampleRate = rate;
    // about two frames of sound in the core's buffer, read after every frame
    const size_t perFrame = static_cast<size_t>(static_cast<double>(rate) * core->frameCycles(core) / core->frequency(core));
    core->setAudioBufferSize(core, perFrame * 2);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), rate);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), rate);

    if (!mCoreLoadFile(core, rom.c_str())) {
        log("the game did not load", rom.c_str());
        unload();
        return JNI_FALSE;
    }
    const std::string bios = text(env, biosPath);
    if (!bios.empty()) {
        if (VFile* vf = VFileOpen(bios.c_str(), O_RDONLY)) {
            core->loadBIOS(core, vf, 0);
        }
    }
    const std::string save = text(env, savePath);
    if (VFile* vf = VFileOpen(save.c_str(), O_CREAT | O_RDWR)) {
        // the core keeps the file and writes the game's saves into it
        core->loadSave(core, vf);
    }
    core->reset(core);
    return JNI_TRUE;
}

// One frame with the keys held (GBA key bits: A 0, B 1, Select 2, Start 3, Right 4, Left 5, Up 6,
// Down 7, R 8, L 9). Returns the number of stereo samples written into audio.
JNIEXPORT jint JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_runFrame(JNIEnv* env, jobject, jint keys, jshortArray audio) {
    if (!core) return 0;
    core->setKeys(core, static_cast<uint32_t>(keys));
    core->runFrame(core);
    for (unsigned i = 0; i < WIDTH * HEIGHT; i++) frame[i] |= OPAQUE;

    blip_t* left = core->getAudioChannel(core, 0);
    blip_t* right = core->getAudioChannel(core, 1);
    const jsize capacity = env->GetArrayLength(audio) / 2;
    int count = blip_samples_avail(left);
    if (count > capacity) count = capacity;
    if (count <= 0) return 0;
    jshort* out = env->GetShortArrayElements(audio, nullptr);
    blip_read_samples(left, out, count, 1);
    blip_read_samples(right, out + 1, count, 1);
    env->ReleaseShortArrayElements(audio, out, 0);
    return count;
}

JNIEXPORT jboolean JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_saveState(JNIEnv* env, jobject, jstring path) {
    if (!core) return JNI_FALSE;
    const std::string file = text(env, path);
    VFile* vf = VFileOpen(file.c_str(), O_CREAT | O_TRUNC | O_RDWR);
    if (!vf) return JNI_FALSE;
    const bool saved = mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
    vf->close(vf);
    return saved ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_loadState(JNIEnv* env, jobject, jstring path) {
    if (!core) return JNI_FALSE;
    const std::string file = text(env, path);
    VFile* vf = VFileOpen(file.c_str(), O_RDONLY);
    if (!vf) return JNI_FALSE;
    const bool loaded = mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC);
    vf->close(vf);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_reset(JNIEnv*, jobject) {
    if (core) core->reset(core);
}

JNIEXPORT void JNICALL
Java_io_github_wwandene2_pomegrade_gba_GbaNative_unload(JNIEnv*, jobject) {
    unload();
}

}
