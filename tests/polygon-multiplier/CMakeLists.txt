# Desktop tests for the polygon multiplier.
# The render test needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/polygon-multiplier -B build-pm -G Ninja && ninja -C build-pm
#   cd build-pm && ./polygon_multiplier_test && ./multiplier_render_test
cmake_minimum_required(VERSION 3.16)
project(polygon_multiplier_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

add_executable(polygon_multiplier_test polygon_multiplier_test.cpp ${LIB_DIR}/src/GPU3D_PolygonMultiplier.cpp)
target_include_directories(polygon_multiplier_test PRIVATE ${LIB_DIR}/src)

# platform stubs shared with the HD texture tests
add_executable(multiplier_render_test multiplier_render_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(multiplier_render_test PRIVATE core EGL GLESv2)
