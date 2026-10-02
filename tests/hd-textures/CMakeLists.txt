# Desktop tests for HD texture replacement.
# Needs EGL + GLES 3.2 (Mesa works without a GPU: libegl-dev libgles-dev libegl-mesa0).
#   cmake -S tests/hd-textures -B build-tests -G Ninja && ninja -C build-tests
#   cd build-tests && ./replacement_test && ./opengl_renderer_test && ./texture_upscale_test
cmake_minimum_required(VERSION 3.16)
project(hd_textures_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

add_executable(replacement_test replacement_test.cpp
    ${LIB_DIR}/src/GPU3D_TextureReplacement.cpp ${LIB_DIR}/src/xxhash/xxhash.c)
target_include_directories(replacement_test PRIVATE ${LIB_DIR}/src ${LIB_DIR})

add_executable(opengl_renderer_test opengl_renderer_test.cpp platform_stubs.cpp)
target_link_libraries(opengl_renderer_test PRIVATE core EGL GLESv2)

add_executable(texture_upscale_test texture_upscale_test.cpp platform_stubs.cpp)
target_link_libraries(texture_upscale_test PRIVATE core EGL GLESv2)
