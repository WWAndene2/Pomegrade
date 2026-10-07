# Desktop tests for the performance counters (CPU sections and GPU timer queries of the OpenGL renderer).
# Needs EGL + GLES 3.2 with GL_EXT_disjoint_timer_query (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/performance-counters -B build-pc -G Ninja && ninja -C build-pc
#   cd build-pc && ./performance_counters_test
cmake_minimum_required(VERSION 3.16)
project(performance_counters_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(performance_counters_test performance_counters_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(performance_counters_test PRIVATE core EGL GLESv2)
