# Desktop tests for frame generation (intermediate frames) in the OpenGL renderer.
# Needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/frame-generation -B build-fg -G Ninja && ninja -C build-fg
#   cd build-fg && ./frame_generation_test
cmake_minimum_required(VERSION 3.16)
project(frame_generation_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(frame_generation_test frame_generation_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(frame_generation_test PRIVATE core EGL GLESv2)
