# Desktop test for the inspector (call sites, display lists, cartridge file names),
# run on real ARM code, with the interpreter and with the x86-64 JIT.
#   cmake -S tests/inspector -B build-inspector -G Ninja && ninja -C build-inspector
#   ./build-inspector/inspector_test
cmake_minimum_required(VERSION 3.16)
project(inspector_tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
# the JIT is what changes most: tested here with its x86-64 back end
set(ENABLE_JIT ON CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)

# platform stubs shared with the HD texture tests
add_executable(inspector_test inspector_test.cpp ../hd-textures/platform_stubs.cpp)
target_link_libraries(inspector_test PRIVATE core EGL GLESv2)
