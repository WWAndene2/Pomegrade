# Desktop tests for the Pokemon remake tooling (tools/remake), on data built
# in the test to the formats' layout (no game data).
#   cmake -S tests/remake -B build-remake-tests -G Ninja && ninja -C build-remake-tests
#   ./build-remake-tests/remake_formats_test
cmake_minimum_required(VERSION 3.16)
project(remake_tests CXX)

set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tools/remake remake EXCLUDE_FROM_ALL)

add_executable(remake_formats_test remake_formats_test.cpp)
target_link_libraries(remake_formats_test PRIVATE remake)
