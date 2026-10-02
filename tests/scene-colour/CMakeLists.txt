# Desktop tests for scene-adaptive colour (OLED deep blacks, adaptive colours).
# The compositor test needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/scene-colour -B build-sc -G Ninja && ninja -C build-sc
#   cd build-sc && ./scene_colour_test && ./scene_colour_compositor_test
cmake_minimum_required(VERSION 3.16)
project(scene_colour_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

add_executable(scene_colour_test scene_colour_test.cpp ${LIB_DIR}/src/GPU_SceneColour.cpp)
target_include_directories(scene_colour_test PRIVATE ${LIB_DIR}/src)

# platform stubs shared with the HD texture tests
add_executable(scene_colour_compositor_test scene_colour_compositor_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(scene_colour_compositor_test PRIVATE core EGL GLESv2)
