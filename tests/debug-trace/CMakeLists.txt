# Desktop test of the DS debug trace (app/src/main/cpp/DebugTrace, no Android dependency).
#   cmake -S tests/debug-trace -B build-dt -G Ninja && ninja -C build-dt
#   cd build-dt && ./debug_trace_test
cmake_minimum_required(VERSION 3.16)
project(debug_trace_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(APP_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../melonDS-android/app/src/main/cpp)
add_executable(debug_trace_test debug_trace_test.cpp ${APP_CPP_DIR}/DebugTrace.cpp)
target_include_directories(debug_trace_test PRIVATE ${APP_CPP_DIR})
find_package(Threads REQUIRED)
target_link_libraries(debug_trace_test PRIVATE Threads::Threads)
