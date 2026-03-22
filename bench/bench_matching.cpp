// Milestone 6 — matching engine benchmarks
#include <benchmark/benchmark.h>

#include "engine.hpp"

// Placeholder — real workloads to be added in M6.
static void BM_Placeholder(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(0);
    }
}
BENCHMARK(BM_Placeholder);
