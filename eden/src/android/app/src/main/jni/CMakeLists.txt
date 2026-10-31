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
    native_post_processing.cpp
)

set_property(TARGET yuzu-android PROPERTY IMPORTED_LOCATION ${FFmpeg_LIBRARY_DIR})

target_link_libraries(yuzu-android PRIVATE audio_core common core input_common frontend_common video_core)
target_link_libraries(yuzu-android PRIVATE android camera2ndk EGL glad jnigraphics log GLESv2)
if (ARCHITECTURE_arm64)
    target_link_libraries(yuzu-android PRIVATE adrenotools)
endif()

# Pomegrade: TEMPORARY, Arm's libGPUCounters for the Mali hardware counters of the diagnostic
# (common/pomegrade_diagnostics.h); deleted with it
if (ARCHITECTURE_arm64)
    AddPackage(
        NAME hwcpipe
        REPO ARM-software/libGPUCounters
        VERSION 2.5.0 # Eden's CPMUtil since its master of 26 October: VERSION (archive/2.5.0.tar.gz), no TAG
        HASH 9dc6ab57dc751b1e2f7ffc85c9c09cfabcdcdc6c182fdc37a28fd9ea03377d4b2eb482e0f229121e30e85422eac965689fc13aee70ac36570b4256eaf8275d37
        FORCE_BUNDLED_PACKAGE ON
        OPTIONS
            "HWCPIPE_WERROR OFF"
            "HWCPIPE_WALL OFF"
            "HWCPIPE_ENABLE_LTO OFF")
    # Arm's code under Eden's warnings-as-errors (-Werror=shadow and others, src/CMakeLists.txt) does not build
    # (CI run 305): its own warnings are not ours to fix
    target_compile_options(device PRIVATE -w)
    target_compile_options(hwcpipe PRIVATE -w)
    target_link_libraries(yuzu-android PRIVATE hwcpipe)
    target_compile_definitions(yuzu-android PRIVATE POMEGRADE_HWCPIPE)
endif()

target_link_libraries(yuzu-android PRIVATE ${FFmpeg_LIBRARIES} OpenSSL::SSL cpp-jwt::cpp-jwt)
if (ENABLE_UPDATE_CHECKER)
    target_compile_definitions(yuzu-android PUBLIC ENABLE_UPDATE_CHECKER)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} yuzu-android)

target_link_options(yuzu-android PRIVATE "-Wl,-Bsymbolic")
