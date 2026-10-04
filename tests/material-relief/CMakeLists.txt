# Desktop test for relief by material in the OpenGL renderer: volumetric grass, no relief on sky, clothes.
# Needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/material-relief -B build-mr -G Ninja && ninja -C build-mr
#   cd build-mr && ./material_relief_test
cmake_minimum_required(VERSION 3.16)
project(material_relief_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(material_relief_test material_relief_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(material_relief_test PRIVATE core EGL GLESv2)
