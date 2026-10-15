// Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

// Pomegrade: Android's performance hints (ADPF) for the threads that make a frame, as Punica's Switch and 3DS
// cores do. An app may not raise its own threads' CPU frequency floor (uclamp is refused without CAP_SYS_NICE,
// read in the Xiaomi 13T's kernel source, kernel/sched/core.c); Android does it for a hint session, from the
// frame durations reported against a target. The NDK functions exist from Android 13 (API 33): found with dlsym,
// so Android 10 to 12 start as before and nothing happens there.

#include "Common/AndroidPerformanceHint.h"

#include <algorithm>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

namespace Common::AndroidPerformanceHint
{
namespace
{
struct Manager;
struct Session;
using GetManager = Manager* (*)();
using CreateSession = Session* (*)(Manager*, const int32_t*, size_t, int64_t);
using ReportActual = int (*)(Session*, int64_t);
using CloseSession = void (*)(Session*);

// Target of one frame: 60 per second, the NTSC rate of most GameCube and Wii games (PAL games, at 50, report
// 20 ms at full speed and ask a little more than they need); a slower frame means the emulation is behind and
// asks for higher clocks
constexpr int64_t TARGET_FRAME_NS = 16'666'667;

struct Hints
{
  GetManager get_manager{};
  CreateSession create_session{};
  ReportActual report_actual{};
  CloseSession close_session{};
  Manager* manager{};
  Session* session{};
  std::vector<int32_t> threads;
  std::mutex mutex;

  Hints()
  {
    void* library = dlopen("libandroid.so", RTLD_NOW);
    if (!library)
      return;
    get_manager = reinterpret_cast<GetManager>(dlsym(library, "APerformanceHint_getManager"));
    create_session =
        reinterpret_cast<CreateSession>(dlsym(library, "APerformanceHint_createSession"));
    report_actual = reinterpret_cast<ReportActual>(
        dlsym(library, "APerformanceHint_reportActualWorkDuration"));
    close_session = reinterpret_cast<CloseSession>(dlsym(library, "APerformanceHint_closeSession"));
    if (get_manager && create_session && report_actual && close_session)
      manager = get_manager();
  }
};

Hints& Get()
{
  static Hints hints;
  return hints;
}
}  // namespace

void AddCurrentThread()
{
  Hints& hints = Get();
  if (!hints.manager)
    return;
  std::lock_guard lock{hints.mutex};
  // the threads of an earlier game (stopped, then another started) are gone: a session with them is refused
  std::erase_if(hints.threads, [](int32_t tid) {
    return access(("/proc/self/task/" + std::to_string(tid)).c_str(), F_OK) != 0;
  });
  hints.threads.push_back(static_cast<int32_t>(gettid()));
  // a session's threads are fixed (setThreads is Android 14+): it is made again with the new one
  if (hints.session)
    hints.close_session(hints.session);
  hints.session =
      hints.create_session(hints.manager, hints.threads.data(), hints.threads.size(), TARGET_FRAME_NS);
}

void ReportFrame(std::int64_t duration_ns)
{
  Hints& hints = Get();
  if (!hints.manager || duration_ns <= 0)
    return;
  std::lock_guard lock{hints.mutex};
  if (hints.session)
    hints.report_actual(hints.session, duration_ns);
}
}  // namespace Common::AndroidPerformanceHint
