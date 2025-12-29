cmake_minimum_required (VERSION 3.10)

project (TimeseriesDB VERSION 1.0 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (Threads REQUIRED)
include_directories (include)

# load gen exe
add_executable (load_gen tools/load_gen.cpp)
target_link_libraries(load_gen PRIVATE Threads::Threads)

# DB exe
add_executable (tsdb_server src/tsdb_server.cpp)
target_link_libraries (tsdb_server PRIVATE Threads::Threads)

# Unit test exe
add_executable (unit_tests tests/unit_tests.cpp)

# Compiler optimizations for high-throughput testing
if (MSVC)
    target_compile_options (load_gen PRIVATE /W4 /O2)
else ()
    target_compile_options (load_gen PRIVATE -Wall -Wextra -O3)
endif ()