// Copyright 2023 Garena Online Private Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <benchmark/benchmark.h>

#include <memory>
#include <thread>
#include <vector>

#include "envpool/core/circular_buffer.h"
#include "envpool/core/circular_buffer_modern.h"

// Benchmark original CircularBuffer
static void BM_CircularBuffer_Original_SingleProducerConsumer(
    benchmark::State& state) {
  CircularBuffer<int> buffer(state.range(0));

  std::thread producer([&]() {
    for (auto _ : state) {
      for (int i = 0; i < state.range(1); ++i) {
        buffer.Put(i);
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < state.range(1); ++i) {
      benchmark::DoNotOptimize(buffer.Get());
    }
  }

  producer.join();

  state.SetItemsProcessed(state.iterations() * state.range(1));
  state.SetBytesProcessed(state.iterations() * state.range(1) * sizeof(int));
}

// Benchmark modern CircularBuffer
static void BM_CircularBuffer_Modern_SingleProducerConsumer(
    benchmark::State& state) {
  envpool::modern::CircularBuffer<int> buffer(state.range(0));

  std::thread producer([&]() {
    for (auto _ : state) {
      for (int i = 0; i < state.range(1); ++i) {
        auto result = buffer.Put(i);
        benchmark::DoNotOptimize(result);
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < state.range(1); ++i) {
      auto result = buffer.Get();
      benchmark::DoNotOptimize(result);
    }
  }

  producer.join();

  state.SetItemsProcessed(state.iterations() * state.range(1));
  state.SetBytesProcessed(state.iterations() * state.range(1) * sizeof(int));
}

// Original with unique_ptr
static void BM_CircularBuffer_Original_UniquePtr(benchmark::State& state) {
  CircularBuffer<std::unique_ptr<int>> buffer(state.range(0));

  std::thread producer([&]() {
    for (auto _ : state) {
      for (int i = 0; i < state.range(1); ++i) {
        buffer.Put(std::make_unique<int>(i));
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < state.range(1); ++i) {
      benchmark::DoNotOptimize(buffer.Get());
    }
  }

  producer.join();

  state.SetItemsProcessed(state.iterations() * state.range(1));
}

// Modern with unique_ptr
static void BM_CircularBuffer_Modern_UniquePtr(benchmark::State& state) {
  envpool::modern::CircularBuffer<std::unique_ptr<int>> buffer(state.range(0));

  std::thread producer([&]() {
    for (auto _ : state) {
      for (int i = 0; i < state.range(1); ++i) {
        auto result = buffer.Put(std::make_unique<int>(i));
        benchmark::DoNotOptimize(result);
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < state.range(1); ++i) {
      auto result = buffer.Get();
      benchmark::DoNotOptimize(result);
    }
  }

  producer.join();

  state.SetItemsProcessed(state.iterations() * state.range(1));
}

// Multi-producer/multi-consumer - Original
static void BM_CircularBuffer_Original_MultiThreaded(benchmark::State& state) {
  CircularBuffer<int> buffer(state.range(0));
  int num_threads = state.range(2);

  for (auto _ : state) {
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int t = 0; t < num_threads; ++t) {
      producers.emplace_back([&, t]() {
        for (int i = 0; i < state.range(1); ++i) {
          buffer.Put(t * 1000 + i);
        }
      });
    }

    for (int t = 0; t < num_threads; ++t) {
      consumers.emplace_back([&]() {
        for (int i = 0; i < state.range(1); ++i) {
          benchmark::DoNotOptimize(buffer.Get());
        }
      });
    }

    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
  }

  state.SetItemsProcessed(state.iterations() * state.range(1) * num_threads);
}

// Multi-producer/multi-consumer - Modern
static void BM_CircularBuffer_Modern_MultiThreaded(benchmark::State& state) {
  envpool::modern::CircularBuffer<int> buffer(state.range(0));
  int num_threads = state.range(2);

  for (auto _ : state) {
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int t = 0; t < num_threads; ++t) {
      producers.emplace_back([&, t]() {
        for (int i = 0; i < state.range(1); ++i) {
          auto result = buffer.Put(t * 1000 + i);
          benchmark::DoNotOptimize(result);
        }
      });
    }

    for (int t = 0; t < num_threads; ++t) {
      consumers.emplace_back([&]() {
        for (int i = 0; i < state.range(1); ++i) {
          auto result = buffer.Get();
          benchmark::DoNotOptimize(result);
        }
      });
    }

    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
  }

  state.SetItemsProcessed(state.iterations() * state.range(1) * num_threads);
}

// Register benchmarks
// Format: BufferSize, ItemsPerIteration
BENCHMARK(BM_CircularBuffer_Original_SingleProducerConsumer)
    ->Args({100, 100})
    ->Args({1000, 1000})
    ->Args({10000, 1000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CircularBuffer_Modern_SingleProducerConsumer)
    ->Args({100, 100})
    ->Args({1000, 1000})
    ->Args({10000, 1000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CircularBuffer_Original_UniquePtr)
    ->Args({100, 100})
    ->Args({1000, 1000})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CircularBuffer_Modern_UniquePtr)
    ->Args({100, 100})
    ->Args({1000, 1000})
    ->Unit(benchmark::kMicrosecond);

// Format: BufferSize, ItemsPerThread, NumThreads
BENCHMARK(BM_CircularBuffer_Original_MultiThreaded)
    ->Args({1000, 100, 2})
    ->Args({1000, 100, 4})
    ->Args({1000, 100, 8})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_CircularBuffer_Modern_MultiThreaded)
    ->Args({1000, 100, 2})
    ->Args({1000, 100, 4})
    ->Args({1000, 100, 8})
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
