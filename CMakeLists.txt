cmake_minimum_required(VERSION 3.20)
project(LowLatencyExchangeEngine LANGUAGES CXX)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Default build type ────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /wd4100)        # /wd4100 = unreferenced formal parameter (stubs)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ── Debug preprocessor define ─────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(EXCHANGE_DEBUG)
endif()

# ── External dependencies (FetchContent) ─────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
    GIT_SHALLOW    TRUE
)
set(BENCHMARK_ENABLE_TESTING       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS   OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS         OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(exchange_engine STATIC
    src/order_book.cpp
    src/engine.cpp
    src/validation.cpp
    src/snapshot.cpp
    src/invariants.cpp
    src/replay.cpp
)
target_include_directories(exchange_engine PUBLIC include)
target_compile_features(exchange_engine PUBLIC cxx_std_20)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(exchange_tests
    tests/test_types.cpp
    tests/test_price_levels.cpp
    tests/test_limit_matching.cpp
    tests/test_market_orders.cpp
    tests/test_cancel.cpp
    tests/test_modify.cpp
    tests/test_invariants.cpp
)
target_link_libraries(exchange_tests PRIVATE exchange_engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(exchange_tests)

# ── Benchmarks ────────────────────────────────────────────────────────────────
add_executable(exchange_bench
    bench/bench_matching.cpp
    bench/bench_workloads.cpp
)
target_link_libraries(exchange_bench PRIVATE exchange_engine benchmark::benchmark_main)
