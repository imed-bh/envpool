// Copyright 2021 Garena Online Private Limited
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

/**
 * Performance benchmarks for AsyncEnvPool implementations.
 *
 * Measures:
 * - Throughput (steps/second)
 * - Latency (µs/batch)
 * - Scalability (varying envs/threads)
 * - Original vs Modern implementation comparison
 *
 * Usage:
 *   # Run all benchmarks
 *   ./async_envpool_benchmark
 *
 *   # Run specific benchmark
 *   ./async_envpool_benchmark --benchmark_filter=BM_AsyncEnvPool_Modern.*
 *
 *   # Output to JSON
 *   ./async_envpool_benchmark --benchmark_out=results.json \
 *       --benchmark_out_format=json
 *
 *   # Compare implementations
 *   ./async_envpool_benchmark --benchmark_filter="BM_AsyncEnvPool_(Original|Modern)/4/2"
 */

#include <benchmark/benchmark.h>

#include "envpool/core/async_envpool.h"
#include "envpool/dummy/dummy_envpool.h"

#ifdef ENVPOOL_USE_MODERN_IMPL
#include "envpool/core/async_envpool_modern.h"
#endif

namespace envpool {
namespace benchmark_suite {

/**
 * Helper: Create config for benchmarking.
 */
auto MakeBenchmarkConfig(int num_envs, int num_threads) {
  return MakeDict(
      "num_envs"_.Bind(num_envs),
      "batch_size"_.Bind(num_envs),
      "num_threads"_.Bind(num_threads),
      "seed"_.Bind(42),
      "max_num_players"_.Bind(1),
      "state_num"_.Bind(10),
      "action_num"_.Bind(6));
}

/**
 * Helper: Create action for all envs.
 */
auto MakeAction(int num_envs) {
  std::vector<int> env_ids;
  env_ids.reserve(num_envs);
  for (int i = 0; i < num_envs; ++i) {
    env_ids.push_back(i);
  }

  std::vector<double> list_action(6, 1.0);

  return MakeDict("env_id"_.Bind(std::move(env_ids)),
                  "list_action"_.Bind(std::move(list_action)));
}

/**
 * Benchmark 1: Original AsyncEnvPool - Basic throughput.
 *
 * Measures: Send/Recv throughput with original implementation.
 */
static void BM_AsyncEnvPool_Original_Throughput(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  // Reset and get initial state
  pool->Reset();
  pool->Recv();

  // Benchmark loop
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    pool->Send(action);
    pool->Recv();
  }

  // Report metrics
  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Original_Throughput)
    ->Args({4, 2})
    ->Args({8, 4})
    ->Args({16, 8})
    ->Args({32, 16})
    ->Args({64, 32})
    ->Unit(benchmark::kMicrosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 2: Modern AsyncEnvPool - Basic throughput.
 *
 * Measures: Send/Recv throughput with modern C++26 implementation.
 */
static void BM_AsyncEnvPool_Modern_Throughput(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  // Reset and get initial state
  pool->Reset();
  auto recv_result = pool->Recv();
  if (!recv_result) {
    state.SkipWithError("Failed to receive initial state");
    return;
  }

  // Benchmark loop
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    auto send_result = pool->Send(action);
    if (!send_result) {
      state.SkipWithError("Send failed");
      return;
    }

    recv_result = pool->Recv();
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  // Report metrics
  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Modern_Throughput)
    ->Args({4, 2})
    ->Args({8, 4})
    ->Args({16, 8})
    ->Args({32, 16})
    ->Args({64, 32})
    ->Unit(benchmark::kMicrosecond);
#endif

/**
 * Benchmark 3: Original AsyncEnvPool - Send latency.
 *
 * Measures: Latency of Send operation only.
 */
static void BM_AsyncEnvPool_Original_SendLatency(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  pool->Recv();

  // Benchmark Send only
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    pool->Send(action);
  }

  // Drain queue
  for (int i = 0; i < state.iterations(); ++i) {
    pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Original_SendLatency)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kNanosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 4: Modern AsyncEnvPool - Send latency.
 */
static void BM_AsyncEnvPool_Modern_SendLatency(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  auto recv_result = pool->Recv();

  // Benchmark Send only
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    auto send_result = pool->Send(action);
    if (!send_result) {
      state.SkipWithError("Send failed");
      return;
    }
  }

  // Drain queue
  for (int i = 0; i < state.iterations(); ++i) {
    recv_result = pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Modern_SendLatency)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kNanosecond);
#endif

/**
 * Benchmark 5: Original AsyncEnvPool - Recv latency.
 *
 * Measures: Latency of Recv operation only.
 */
static void BM_AsyncEnvPool_Original_RecvLatency(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  pool->Recv();

  // Send all actions first
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    pool->Send(action);
  }

  // Benchmark Recv only
  for (auto _ : state) {
    pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Original_RecvLatency)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kNanosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 6: Modern AsyncEnvPool - Recv latency.
 */
static void BM_AsyncEnvPool_Modern_RecvLatency(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  auto recv_result = pool->Recv();

  // Send all actions first
  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    auto send_result = pool->Send(action);
  }

  // Benchmark Recv only
  for (auto _ : state) {
    recv_result = pool->Recv();
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Modern_RecvLatency)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kNanosecond);
#endif

/**
 * Benchmark 7: Original AsyncEnvPool - Scalability (fixed threads).
 *
 * Measures: How throughput scales with number of environments (fixed threads).
 */
static void BM_AsyncEnvPool_Original_ScaleEnvs(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = 8;  // Fixed

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  pool->Recv();

  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    pool->Send(action);
    pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Original_ScaleEnvs)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 8: Modern AsyncEnvPool - Scalability (fixed threads).
 */
static void BM_AsyncEnvPool_Modern_ScaleEnvs(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = 8;  // Fixed

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  auto recv_result = pool->Recv();

  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    auto send_result = pool->Send(action);
    if (!send_result) {
      state.SkipWithError("Send failed");
      return;
    }

    recv_result = pool->Recv();
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Modern_ScaleEnvs)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);
#endif

/**
 * Benchmark 9: Original AsyncEnvPool - Scalability (fixed envs).
 *
 * Measures: How throughput scales with number of threads (fixed envs).
 */
static void BM_AsyncEnvPool_Original_ScaleThreads(benchmark::State& state) {
  int num_envs = 64;  // Fixed
  int num_threads = state.range(0);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  pool->Recv();

  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    pool->Send(action);
    pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Original_ScaleThreads)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Unit(benchmark::kMicrosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 10: Modern AsyncEnvPool - Scalability (fixed envs).
 */
static void BM_AsyncEnvPool_Modern_ScaleThreads(benchmark::State& state) {
  int num_envs = 64;  // Fixed
  int num_threads = state.range(0);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  auto recv_result = pool->Recv();

  for (auto _ : state) {
    auto action = MakeAction(num_envs);
    auto send_result = pool->Send(action);
    if (!send_result) {
      state.SkipWithError("Send failed");
      return;
    }

    recv_result = pool->Recv();
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations() * num_envs);
  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * num_envs,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Modern_ScaleThreads)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Unit(benchmark::kMicrosecond);
#endif

/**
 * Benchmark 11: Original AsyncEnvPool - Variable batch sizes.
 *
 * Measures: Impact of batch size on throughput.
 */
static void BM_AsyncEnvPool_Original_VariableBatch(benchmark::State& state) {
  int num_envs = 64;
  int batch_size = state.range(0);
  int num_threads = 8;

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  pool->Recv();

  for (auto _ : state) {
    // Send actions for batch_size envs
    std::vector<int> env_ids;
    for (int i = 0; i < batch_size; ++i) {
      env_ids.push_back(i % num_envs);
    }

    std::vector<double> list_action(6, 1.0);

    auto action = MakeDict("env_id"_.Bind(std::move(env_ids)),
                           "list_action"_.Bind(std::move(list_action)));

    pool->Send(action);
    pool->Recv();
  }

  state.SetItemsProcessed(state.iterations() * batch_size);
  state.counters["batch_size"] = batch_size;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * batch_size,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Original_VariableBatch)
    ->Arg(1)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 12: Modern AsyncEnvPool - Variable batch sizes.
 */
static void BM_AsyncEnvPool_Modern_VariableBatch(benchmark::State& state) {
  int num_envs = 64;
  int batch_size = state.range(0);
  int num_threads = 8;

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  pool->Reset();
  auto recv_result = pool->Recv();

  for (auto _ : state) {
    // Send actions for batch_size envs
    std::vector<int> env_ids;
    for (int i = 0; i < batch_size; ++i) {
      env_ids.push_back(i % num_envs);
    }

    std::vector<double> list_action(6, 1.0);

    auto action = MakeDict("env_id"_.Bind(std::move(env_ids)),
                           "list_action"_.Bind(std::move(list_action)));

    auto send_result = pool->Send(action);
    if (!send_result) {
      state.SkipWithError("Send failed");
      return;
    }

    recv_result = pool->Recv();
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  state.SetItemsProcessed(state.iterations() * batch_size);
  state.counters["batch_size"] = batch_size;
  state.counters["steps_per_sec"] =
      benchmark::Counter(state.iterations() * batch_size,
                         benchmark::Counter::kIsRate);
}

BENCHMARK(BM_AsyncEnvPool_Modern_VariableBatch)
    ->Arg(1)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);
#endif

/**
 * Benchmark 13: Original AsyncEnvPool - Reset performance.
 *
 * Measures: Cost of resetting all environments.
 */
static void BM_AsyncEnvPool_Original_Reset(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  for (auto _ : state) {
    pool->Reset();
    pool->Recv();  // Clear the reset states
  }

  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Original_Reset)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kMicrosecond);

#ifdef ENVPOOL_USE_MODERN_IMPL
/**
 * Benchmark 14: Modern AsyncEnvPool - Reset performance.
 */
static void BM_AsyncEnvPool_Modern_Reset(benchmark::State& state) {
  int num_envs = state.range(0);
  int num_threads = state.range(1);

  auto config = MakeBenchmarkConfig(num_envs, num_threads);
  auto pool = std::make_unique<modern::AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(config));

  for (auto _ : state) {
    pool->Reset();
    auto recv_result = pool->Recv();  // Clear the reset states
    if (!recv_result) {
      state.SkipWithError("Recv failed");
      return;
    }
  }

  state.counters["envs"] = num_envs;
  state.counters["threads"] = num_threads;
}

BENCHMARK(BM_AsyncEnvPool_Modern_Reset)
    ->Args({4, 2})
    ->Args({16, 8})
    ->Args({64, 32})
    ->Unit(benchmark::kMicrosecond);
#endif

}  // namespace benchmark_suite
}  // namespace envpool

BENCHMARK_MAIN();
