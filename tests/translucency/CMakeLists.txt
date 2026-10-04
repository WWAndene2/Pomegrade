# Desktop test for translucent polygons (DS blending rules) in the OpenGL renderer, against the software one.
# Real 3D commands from ARM code; no JIT needed.
#   cmake -S tests/decals -B build-translucency -G Ninja && ninja -C build-translucency
#   ./build-translucency/translucency_test
cmake_minimum_required(VERSION 3.16)
project(translucency_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(translucency_test translucency_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(translucency_test PRIVATE core EGL GLESv2)
