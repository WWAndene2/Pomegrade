#pragma once
// scratch: Azahar's log macros printed to stdout
#include <fmt/format.h>
#include "common/logging/formatter.h"
#define LOG_PRINT(level, ...) (fmt::print("[{}] ", level), fmt::print(__VA_ARGS__), fmt::print("\n"))
#define LOG_TRACE(c, ...) ((void)0)
#define LOG_DEBUG(c, ...) ((void)0)
#define LOG_INFO(c, ...) LOG_PRINT("info", __VA_ARGS__)
#define LOG_WARNING(c, ...) LOG_PRINT("warning", __VA_ARGS__)
#define LOG_ERROR(c, ...) LOG_PRINT("error", __VA_ARGS__)
#define LOG_CRITICAL(c, ...) LOG_PRINT("critical", __VA_ARGS__)
