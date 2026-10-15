// Copyright 2026 Pomegrade
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

namespace AndroidPerformanceHint {

/// Adds the calling thread to the hint session (the emulation thread and the Vulkan worker join it)
void AddCurrentThread();

/// Reports how long the last emulated frame took, from one presented frame to the next
void ReportFrame(std::int64_t duration_ns);

} // namespace AndroidPerformanceHint
