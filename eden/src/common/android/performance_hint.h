// SPDX-FileCopyrightText: Copyright 2026 Pomegrade
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace Common::Android::PerformanceHint {

/// Adds the calling thread to the hint session (the guest cores and the GPU thread join it)
void AddCurrentThread();

/// Reports how long the last guest frame took, from one presented frame to the next
void ReportFrame(std::int64_t duration_ns);

} // namespace Common::Android::PerformanceHint
