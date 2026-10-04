# Desktop test for relief by material and volumetric grass in the OpenGL renderer.
# Needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/volumetric-grass -B build-vg -G Ninja && ninja -C build-vg
#   cd build-vg && ./volumetric_grass_test
cmake_minimum_required(VERSION 3.16)
project(volumetric_grass_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(volumetric_grass_test volumetric_grass_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(volumetric_grass_test PRIVATE core EGL GLESv2)
