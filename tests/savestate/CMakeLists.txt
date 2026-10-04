# Desktop test for loading save states (shared WRAM mapping restored).
#   cmake -S tests/savestate -B build-savestate -G Ninja && ninja -C build-savestate
#   ./build-savestate/savestate_test
cmake_minimum_required(VERSION 3.16)
project(savestate_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(savestate_test savestate_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(savestate_test PRIVATE core EGL GLESv2)
