#ifndef DEBUGTRACE_H
#define DEBUGTRACE_H

#include <cstdarg>
#include <string>

// DS debug trace (Pomegrade): a record of what the DS emulator does while a game
// starts and runs, made to find where it stops (a black screen with no error).
// Turned on in Pomegrade's settings (DS > Record a debug trace). Written to
// <folder>/<date>/trace.txt as it happens, one line at a time, flushed at once,
// so a game that freezes and is killed keeps everything up to that point:
//   - the device and the settings (given by the app);
//   - every message of the emulator's log (melonDS, OpenGL errors, shader
//     compilations and their time);
//   - the stages the emulator goes through (setup, ROM load, OpenGL context and
//     renderer, the first frames: run, rendered, presented);
//   - every second, from a watchdog thread: the current stage and for how long,
//     frames emulated and frames presented. A stage that lasts is where it hangs.
// Off: one test of a flag per call. Any thread.
namespace DebugTrace
{

// on: a new record in a sub-folder of folder named after the date, starting
// with header (several lines); off: the record ends
void Configure(bool enabled, const std::string& folder, const std::string& header);
bool Enabled();

// a line of the record
void Note(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void NoteV(const char* prefix, const char* fmt, va_list args);

// where the emulator is now (a string literal, kept as is); written to the
// record when it changes, until the first frames have been presented
void Stage(const char* stage);

// counters for the watchdog's lines
void FrameEmulated();
void FramePresented(bool valid);

}

#endif // DEBUGTRACE_H
