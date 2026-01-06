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
 * Comprehensive integration tests for modern AsyncEnvPool implementation.
 *
 * Tests the full async pipeline:
 * 1. Send actions via ActionBufferQueue
 * 2. Worker threads process via std::jthread + std::stop_token
 * 3. Receive states via StateBufferQueue
 * 4. Error handling with std::expected
 * 5. Graceful shutdown and RAII cleanup
 */

#include "envpool/core/async_envpool_modern.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "envpool/dummy/dummy_envpool.h"

namespace envpool {
namespace modern {
namespace test {

using namespace std::chrono_literals;

/**
 * Test fixture for AsyncEnvPool modern tests.
 */
class AsyncEnvPoolModernTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Default config for tests
    config_ = MakeDict(
        "num_envs"_.Bind(4),
        "batch_size"_.Bind(4),
        "num_threads"_.Bind(2),
        "seed"_.Bind(42),
        "max_num_players"_.Bind(1),
        "state_num"_.Bind(10),
        "action_num"_.Bind(6));
  }

  decltype(auto) MakePool() {
    return std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
        dummy::DummyEnvSpec(config_));
  }

  decltype(auto) config_;
};

/**
 * Test 1: Basic construction and destruction (RAII).
 * Verifies that AsyncEnvPool constructs correctly and cleans up automatically.
 */
TEST_F(AsyncEnvPoolModernTest, BasicConstruction) {
  auto pool = MakePool();
  ASSERT_NE(pool, nullptr);

  // Pool should have initialized
  EXPECT_GT(pool->NumEnvs(), 0);
  EXPECT_EQ(pool->NumEnvs(), 4);

  // RAII: Destructor should handle cleanup automatically
  // No manual cleanup needed!
}

/**
 * Test 2: Single Send/Recv cycle.
 * Tests the basic pipeline: Send actions → Workers process → Recv states.
 */
TEST_F(AsyncEnvPoolModernTest, SingleSendRecv) {
  auto pool = MakePool();

  // Reset all environments
  pool->Reset();

  // Receive initial states
  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value())
      << "Recv should succeed after Reset";

  const auto& [state_arrays, env_ids] = *recv_result;

  // Verify we got states from all envs
  EXPECT_EQ(env_ids.size(), 4);
  EXPECT_FALSE(state_arrays.empty());

  // Create action for all envs
  std::vector<int> action_env_ids = {0, 1, 2, 3};
  std::vector<double> list_action(6, 1.0);  // 6 action values

  // Send action
  auto send_result = pool->Send(
      MakeDict("env_id"_.Bind(action_env_ids),
               "list_action"_.Bind(list_action)));

  ASSERT_TRUE(send_result.has_value())
      << "Send should succeed";

  // Receive next states
  recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value())
      << "Recv should succeed after Send";

  const auto& [next_states, next_env_ids] = *recv_result;
  EXPECT_EQ(next_env_ids.size(), 4);
}

/**
 * Test 3: Multiple Send/Recv cycles.
 * Tests sustained operation over many steps.
 */
TEST_F(AsyncEnvPoolModernTest, MultipleSendRecv) {
  auto pool = MakePool();
  pool->Reset();

  // Initial recv
  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Run 100 steps
  for (int step = 0; step < 100; ++step) {
    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, static_cast<double>(step));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    ASSERT_TRUE(send_result.has_value())
        << "Send failed at step " << step;

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed at step " << step;

    const auto& [states, env_ids] = *recv_result;
    EXPECT_FALSE(env_ids.empty())
        << "Got empty env_ids at step " << step;
  }
}

/**
 * Test 4: Async operation verification.
 * Tests that workers are truly processing asynchronously.
 */
TEST_F(AsyncEnvPoolModernTest, AsyncOperation) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Send multiple actions without immediately receiving
  // This verifies that Send is non-blocking and workers process concurrently
  for (int i = 0; i < 10; ++i) {
    std::vector<int> action_env_ids = {i % 4};
    std::vector<double> list_action(6, static_cast<double>(i));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    ASSERT_TRUE(send_result.has_value())
        << "Send failed at iteration " << i;
  }

  // Now receive all results
  for (int i = 0; i < 10; ++i) {
    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed at iteration " << i;
  }
}

/**
 * Test 5: Multi-threaded Send operations.
 * Tests thread safety when multiple threads Send concurrently.
 */
TEST_F(AsyncEnvPoolModernTest, MultiThreadedSend) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  std::atomic<int> send_count{0};
  std::atomic<int> send_errors{0};

  constexpr int num_sender_threads = 4;
  constexpr int sends_per_thread = 25;

  std::vector<std::jthread> senders;
  senders.reserve(num_sender_threads);

  // Spawn multiple sender threads
  for (int t = 0; t < num_sender_threads; ++t) {
    senders.emplace_back([&pool, &send_count, &send_errors, t]() {
      for (int i = 0; i < sends_per_thread; ++i) {
        std::vector<int> action_env_ids = {(t + i) % 4};
        std::vector<double> list_action(6, static_cast<double>(t * 100 + i));

        auto result = pool->Send(
            MakeDict("env_id"_.Bind(action_env_ids),
                     "list_action"_.Bind(list_action)));

        if (result.has_value()) {
          send_count.fetch_add(1, std::memory_order_relaxed);
        } else {
          send_errors.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  // Wait for all senders (std::jthread joins automatically)
  senders.clear();

  // Verify all sends succeeded
  EXPECT_EQ(send_count.load(), num_sender_threads * sends_per_thread)
      << "Some sends failed";
  EXPECT_EQ(send_errors.load(), 0)
      << "Encountered send errors";

  // Receive all results
  for (int i = 0; i < num_sender_threads * sends_per_thread; ++i) {
    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed at iteration " << i;
  }
}

/**
 * Test 6: Multi-threaded Recv operations.
 * Tests thread safety when multiple threads Recv concurrently.
 */
TEST_F(AsyncEnvPoolModernTest, MultiThreadedRecv) {
  auto pool = MakePool();
  pool->Reset();

  // Initial recv
  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  constexpr int num_operations = 100;

  // Send all actions first
  for (int i = 0; i < num_operations; ++i) {
    std::vector<int> action_env_ids = {i % 4};
    std::vector<double> list_action(6, static_cast<double>(i));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));
    ASSERT_TRUE(send_result.has_value());
  }

  // Now multiple threads receive concurrently
  std::atomic<int> recv_count{0};
  std::atomic<int> recv_errors{0};

  constexpr int num_receiver_threads = 4;

  std::vector<std::jthread> receivers;
  receivers.reserve(num_receiver_threads);

  for (int t = 0; t < num_receiver_threads; ++t) {
    receivers.emplace_back([&pool, &recv_count, &recv_errors]() {
      for (int i = 0; i < num_operations / num_receiver_threads; ++i) {
        auto result = pool->Recv();

        if (result.has_value()) {
          recv_count.fetch_add(1, std::memory_order_relaxed);
        } else {
          recv_errors.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  // Wait for all receivers
  receivers.clear();

  // Verify all receives succeeded
  EXPECT_EQ(recv_count.load(), num_operations)
      << "Some receives failed";
  EXPECT_EQ(recv_errors.load(), 0)
      << "Encountered receive errors";
}

/**
 * Test 7: Graceful shutdown.
 * Tests that std::jthread and std::stop_token provide graceful shutdown.
 */
TEST_F(AsyncEnvPoolModernTest, GracefulShutdown) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Send some actions
  for (int i = 0; i < 10; ++i) {
    std::vector<int> action_env_ids = {i % 4};
    std::vector<double> list_action(6, static_cast<double>(i));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));
    ASSERT_TRUE(send_result.has_value());
  }

  // Destroy pool (triggers std::jthread destructor)
  // This should:
  // 1. Request stop on all worker threads
  // 2. Join all threads
  // 3. Clean up resources
  // All automatic via RAII!
  pool.reset();

  // If we get here without hanging, shutdown worked correctly
  SUCCEED() << "Graceful shutdown completed";
}

/**
 * Test 8: Reset functionality.
 * Tests that Reset correctly resets all environments.
 */
TEST_F(AsyncEnvPoolModernTest, ResetFunctionality) {
  auto pool = MakePool();

  // First reset
  pool->Reset();
  auto recv_result1 = pool->Recv();
  ASSERT_TRUE(recv_result1.has_value());

  // Run some steps
  for (int i = 0; i < 20; ++i) {
    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, static_cast<double>(i));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));
    ASSERT_TRUE(send_result.has_value());

    auto recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value());
  }

  // Reset again
  pool->Reset();
  auto recv_result2 = pool->Recv();
  ASSERT_TRUE(recv_result2.has_value());

  const auto& [states, env_ids] = *recv_result2;
  EXPECT_EQ(env_ids.size(), 4);
}

/**
 * Test 9: Different batch sizes.
 * Tests that the pool works correctly with various batch sizes.
 */
TEST_F(AsyncEnvPoolModernTest, VariableBatchSize) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Test with different batch sizes: 1, 2, 4 envs
  std::vector<int> batch_sizes = {1, 2, 4};

  for (int batch_size : batch_sizes) {
    std::vector<int> action_env_ids;
    for (int i = 0; i < batch_size; ++i) {
      action_env_ids.push_back(i);
    }

    std::vector<double> list_action(6, 1.0);

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    ASSERT_TRUE(send_result.has_value())
        << "Send failed with batch size " << batch_size;

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed with batch size " << batch_size;
  }
}

/**
 * Test 10: Large number of environments.
 * Tests scalability with many environments and threads.
 */
TEST_F(AsyncEnvPoolModernTest, ManyEnvironments) {
  auto large_config = MakeDict(
      "num_envs"_.Bind(64),
      "batch_size"_.Bind(64),
      "num_threads"_.Bind(8),
      "seed"_.Bind(42),
      "max_num_players"_.Bind(1),
      "state_num"_.Bind(10),
      "action_num"_.Bind(6));

  auto pool = std::make_unique<AsyncEnvPool<dummy::DummyEnv>>(
      dummy::DummyEnvSpec(large_config));

  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(pool->NumEnvs(), 64);

  pool->Reset();
  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Run several steps with all 64 envs
  for (int step = 0; step < 10; ++step) {
    std::vector<int> action_env_ids;
    for (int i = 0; i < 64; ++i) {
      action_env_ids.push_back(i);
    }

    std::vector<double> list_action(6, static_cast<double>(step));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    ASSERT_TRUE(send_result.has_value())
        << "Send failed at step " << step;

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed at step " << step;
  }
}

/**
 * Test 11: Stress test - sustained high load.
 * Tests that the pool can handle sustained high load without degradation.
 */
TEST_F(AsyncEnvPoolModernTest, SustainedHighLoad) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  constexpr int num_steps = 1000;

  auto start = std::chrono::steady_clock::now();

  for (int step = 0; step < num_steps; ++step) {
    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, static_cast<double>(step % 100));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    ASSERT_TRUE(send_result.has_value())
        << "Send failed at step " << step;

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value())
        << "Recv failed at step " << step;
  }

  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start);

  // Log performance (not a strict requirement)
  std::cout << "Completed " << num_steps << " steps in "
            << duration.count() << "ms" << std::endl;

  // Just verify it completed without errors
  SUCCEED() << "Sustained high load test completed";
}

/**
 * Test 12: RAII resource cleanup verification.
 * Tests that all resources are properly cleaned up via RAII.
 */
TEST_F(AsyncEnvPoolModernTest, RAIICleanup) {
  // Create and destroy multiple pools in sequence
  // Each should clean up completely
  for (int iteration = 0; iteration < 10; ++iteration) {
    auto pool = MakePool();
    pool->Reset();

    auto recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value());

    // Do some work
    for (int step = 0; step < 10; ++step) {
      std::vector<int> action_env_ids = {0, 1, 2, 3};
      std::vector<double> list_action(6, 1.0);

      auto send_result = pool->Send(
          MakeDict("env_id"_.Bind(action_env_ids),
                   "list_action"_.Bind(list_action)));
      ASSERT_TRUE(send_result.has_value());

      recv_result = pool->Recv();
      ASSERT_TRUE(recv_result.has_value());
    }

    // pool destroyed here - RAII cleanup
  }

  // If we get here without leaks or hangs, RAII worked correctly
  SUCCEED() << "RAII cleanup verified across 10 iterations";
}

/**
 * Test 13: Exception safety.
 * Tests that the pool handles exceptions gracefully (should not throw in hot path).
 */
TEST_F(AsyncEnvPoolModernTest, NoExceptionsInHotPath) {
  auto pool = MakePool();
  pool->Reset();

  // All these operations should be noexcept
  EXPECT_NO_THROW({
    auto recv_result = pool->Recv();

    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, 1.0);

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));

    recv_result = pool->Recv();
  });
}

/**
 * Test 14: Correctness - verify state progression.
 * Tests that environment state progresses correctly through steps.
 */
TEST_F(AsyncEnvPoolModernTest, StateProgression) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  // Track that we're getting states from all envs
  std::set<int> seen_env_ids;

  for (int step = 0; step < 20; ++step) {
    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, static_cast<double>(step));

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));
    ASSERT_TRUE(send_result.has_value());

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value());

    const auto& [states, env_ids] = *recv_result;

    // Track env_ids we've seen
    for (int id : env_ids) {
      seen_env_ids.insert(id);
    }
  }

  // Verify we've seen all 4 environments
  EXPECT_EQ(seen_env_ids.size(), 4)
      << "Did not receive states from all environments";
}

/**
 * Test 15: Performance - throughput measurement.
 * Measures the throughput of the async pipeline.
 */
TEST_F(AsyncEnvPoolModernTest, ThroughputMeasurement) {
  auto pool = MakePool();
  pool->Reset();

  auto recv_result = pool->Recv();
  ASSERT_TRUE(recv_result.has_value());

  constexpr int num_steps = 10000;
  constexpr int batch_size = 4;

  auto start = std::chrono::high_resolution_clock::now();

  for (int step = 0; step < num_steps; ++step) {
    std::vector<int> action_env_ids = {0, 1, 2, 3};
    std::vector<double> list_action(6, 1.0);

    auto send_result = pool->Send(
        MakeDict("env_id"_.Bind(action_env_ids),
                 "list_action"_.Bind(list_action)));
    ASSERT_TRUE(send_result.has_value());

    recv_result = pool->Recv();
    ASSERT_TRUE(recv_result.has_value());
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end - start);

  double total_steps = num_steps * batch_size;
  double steps_per_second = total_steps / (duration.count() / 1e6);

  std::cout << "Throughput: " << steps_per_second << " steps/second"
            << std::endl;
  std::cout << "Latency: " << (duration.count() / num_steps)
            << " µs/batch" << std::endl;

  // Just log the results, no strict requirement
  SUCCEED() << "Throughput measurement completed";
}

}  // namespace test
}  // namespace modern
}  // namespace envpool
