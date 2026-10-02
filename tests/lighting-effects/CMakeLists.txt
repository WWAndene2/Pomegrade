# Desktop tests for the lighting effects (pseudo ray tracing) of the OpenGL renderer.
# Needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/lighting-effects -B build-le -G Ninja && ninja -C build-le
#   cd build-le && ./lighting_effects_test
cmake_minimum_required(VERSION 3.16)
project(lighting_effects_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(lighting_effects_test lighting_effects_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(lighting_effects_test PRIVATE core EGL GLESv2)
