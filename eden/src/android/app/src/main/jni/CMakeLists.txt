# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(yuzu-android SHARED
    emu_window/emu_window.cpp
    emu_window/emu_window.h
    native.cpp
    native.h
    native_config.cpp
    native_freedreno.cpp
    android_settings.cpp
    game_metadata.cpp
    native_log.cpp
    android_config.cpp
    android_config.h
    native_input.cpp
)

set_property(TARGET yuzu-android PROPERTY IMPORTED_LOCATION ${FFmpeg_LIBRARY_DIR})

target_link_libraries(yuzu-android PRIVATE audio_core common core input_common frontend_common video_core)
target_link_libraries(yuzu-android PRIVATE android camera2ndk EGL glad jnigraphics log)
if (ARCHITECTURE_arm64)
    target_link_libraries(yuzu-android PRIVATE adrenotools)
endif()

# Pomegrade: TEMPORARY, Arm's libGPUCounters for the Mali hardware counters of the diagnostic
# (common/pomegrade_diagnostics.h); deleted with it
if (ARCHITECTURE_arm64)
    AddPackage(
        NAME hwcpipe
        REPO ARM-software/libGPUCounters
        TAG 2.5.0
        HASH 80c345cdf5a7a255db32329be007d4920b177c7915e7c3515c4a049871292eae5276b8cd45fa2c6bf7445dcd5434c96afb57dc4244e2bfbee6813c5f2954b4df
        FORCE_BUNDLED_PACKAGE ON
        OPTIONS
            "HWCPIPE_WERROR OFF"
            "HWCPIPE_WALL OFF"
            "HWCPIPE_ENABLE_LTO OFF")
    target_link_libraries(yuzu-android PRIVATE hwcpipe)
    target_compile_definitions(yuzu-android PRIVATE POMEGRADE_HWCPIPE)
endif()

target_link_libraries(yuzu-android PRIVATE OpenSSL::SSL cpp-jwt::cpp-jwt)
if (ENABLE_UPDATE_CHECKER)
    target_compile_definitions(yuzu-android PUBLIC ENABLE_UPDATE_CHECKER)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} yuzu-android)
