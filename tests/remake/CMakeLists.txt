# Desktop tests for the Pokemon remake tooling (tools/remake), on data built
# in the test to the formats' layout (no game data).
#   cmake -S tests/remake -B build-remake-tests -G Ninja && ninja -C build-remake-tests
#   ./build-remake-tests/remake_formats_test
cmake_minimum_required(VERSION 3.16)
project(remake_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tools/remake remake EXCLUDE_FROM_ALL)

add_executable(remake_formats_test remake_formats_test.cpp)
target_link_libraries(remake_formats_test PRIVATE remake)

add_executable(remake_models_test remake_models_test.cpp)
target_link_libraries(remake_models_test PRIVATE remake)

add_executable(remake_world_test remake_world_test.cpp)
target_link_libraries(remake_world_test PRIVATE remake)

add_executable(remake_scale_test remake_scale_test.cpp)
target_link_libraries(remake_scale_test PRIVATE remake)

add_executable(remake_zones_test remake_zones_test.cpp)
target_link_libraries(remake_zones_test PRIVATE remake)

add_executable(remake_n3ds_test remake_n3ds_test.cpp)
target_link_libraries(remake_n3ds_test PRIVATE remake)

add_executable(remake_bch_test remake_bch_test.cpp)
target_link_libraries(remake_bch_test PRIVATE remake)

add_executable(remake_bps_test remake_bps_test.cpp)
target_link_libraries(remake_bps_test PRIVATE remake)

add_executable(remake_town_test remake_town_test.cpp)
target_link_libraries(remake_town_test PRIVATE remake)

add_executable(remake_shapes_test remake_shapes_test.cpp)
target_link_libraries(remake_shapes_test PRIVATE remake)

add_executable(remake_topview_test remake_topview_test.cpp)
target_link_libraries(remake_topview_test PRIVATE remake)

add_executable(remake_zone_test remake_zone_test.cpp)
target_link_libraries(remake_zone_test PRIVATE remake)

add_executable(remake_matrix_test remake_matrix_test.cpp)
target_link_libraries(remake_matrix_test PRIVATE remake)

add_executable(remake_textures_test remake_textures_test.cpp)
target_link_libraries(remake_textures_test PRIVATE remake)

# Checks the tool's texture names against the DS core's own decoder and hash;
# builds the core (as tests/hd-textures does), so it is skipped unless asked:
#   cmake -S tests/remake -B build-remake-tests -DREMAKE_CORE_CHECK=ON
option(REMAKE_CORE_CHECK "build emulator_texture_name_test against the DS core" OFF)
if(REMAKE_CORE_CHECK)
    enable_language(C)
    set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
    set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
    set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
    add_compile_definitions(MELONDS_GL_HEADER=<GLES3/gl32.h>)
    set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/melonDS-android-lib)
    add_subdirectory(${LIB_DIR} melonDS EXCLUDE_FROM_ALL)
    add_executable(emulator_texture_name_test emulator_texture_name_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../hd-textures/platform_stubs.cpp)
    target_link_libraries(emulator_texture_name_test PRIVATE remake core EGL GLESv2)
endif()

add_executable(remake_motion_test remake_motion_test.cpp)
target_link_libraries(remake_motion_test PRIVATE remake)

add_executable(remake_text_test remake_text_test.cpp)
target_link_libraries(remake_text_test PRIVATE remake)

add_executable(remake_script_test remake_script_test.cpp)
target_link_libraries(remake_script_test PRIVATE remake)
