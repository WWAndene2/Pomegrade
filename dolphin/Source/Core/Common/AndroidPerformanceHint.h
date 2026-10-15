// Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace Common::AndroidPerformanceHint
{
// Adds the calling thread to the hint session (the CPU thread and the video thread join it)
void AddCurrentThread();

// Reports how long the last frame took, from one presented frame to the next
void ReportFrame(std::int64_t duration_ns);
}  // namespace Common::AndroidPerformanceHint
