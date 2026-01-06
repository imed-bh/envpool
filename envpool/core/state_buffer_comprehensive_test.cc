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

#include "envpool/core/state_buffer.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "ThreadPool.h"
#include "envpool/core/spec.h"

// Test concurrent allocation from multiple threads
TEST(StateBufferComprehensiveTest, HighConcurrency Allocation) {
  int batch = 256;
  int max_num_players = 10;
  std::vector<ShapeSpec> specs{ShapeSpec(1, {batch * max_num_players, 10, 4}),
                               ShapeSpec(4, {batch, 5, 2})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({true, false}));

  std::size_t num_threads = 32;
  ThreadPool pool(num_threads);
  std::vector<std::future<void>> futures;

  std::atomic<int> allocation_count{0};
  std::atomic<int> done_count{0};

  for (std::size_t i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      std::size_t num_players = 1 + (i % max_num_players);

      auto slice = buffer.Allocate(num_players);
      allocation_count++;

      EXPECT_EQ(slice.arr.size(), specs.size());
      EXPECT_EQ(slice.arr[0].Shape(0), num_players);  // Player state
      EXPECT_EQ(slice.arr[1].Shape(0), 1);            // Shared state

      // Simulate work
      std::this_thread::sleep_for(std::chrono::microseconds(10 + (i % 100)));

      slice.done_write();
      done_count++;
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  auto result = buffer.Wait();
  EXPECT_EQ(allocation_count, batch);
  EXPECT_EQ(done_count, batch);
}

// Test dual-counter atomicity
TEST(StateBufferComprehensiveTest, DualCounterAtomicity) {
  int batch = 1000;
  int max_num_players = 5;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch * max_num_players, 2}),
                               ShapeSpec(4, {batch, 2})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({true, false}));

  // Concurrent allocations with different player counts
  ThreadPool pool(std::thread::hardware_concurrency());
  std::vector<std::future<std::pair<uint32_t, uint32_t>>> futures;

  for (int i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      std::size_t num_players = 1 + (i % max_num_players);
      auto slice = buffer.Allocate(num_players);

      // Get offsets immediately after allocation
      auto offsets = buffer.Offsets();

      slice.done_write();
      return offsets;
    }));
  }

  // Collect all offset pairs
  std::vector<std::pair<uint32_t, uint32_t>> all_offsets;
  for (auto& f : futures) {
    all_offsets.push_back(f.get());
  }

  auto result = buffer.Wait();

  // Verify invariants:
  // 1. shared_offset should be monotonically increasing
  // 2. player_offset should grow consistent with num_players
  uint32_t prev_shared = 0;
  uint32_t prev_player = 0;

  std::sort(all_offsets.begin(), all_offsets.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  for (const auto& [player_offset, shared_offset] : all_offsets) {
    EXPECT_GE(shared_offset, prev_shared);
    EXPECT_GE(player_offset, prev_player);
    prev_shared = shared_offset;
    prev_player = player_offset;
  }
}

// Test allocation failure handling
TEST(StateBufferComprehensiveTest, AllocationExhaustion) {
  int batch = 32;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch, 2})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  // Allocate exactly batch items
  std::vector<StateBuffer::WritableSlice> slices;
  for (int i = 0; i < batch; ++i) {
    slices.push_back(buffer.Allocate(1));
  }

  // Next allocation should throw
  EXPECT_THROW(buffer.Allocate(1), std::out_of_range);

  // Complete all allocations
  for (auto& slice : slices) {
    slice.done_write();
  }

  auto result = buffer.Wait();
  EXPECT_EQ(result[0].Shape(0), batch);
}

// Test memory visibility after done_write
TEST(StateBufferComprehensiveTest, MemoryVisibility) {
  int batch = 100;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch, 1})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  ThreadPool pool(batch);
  std::vector<std::future<void>> futures;

  for (int i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      auto slice = buffer.Allocate(1);

      // Write specific value
      auto* ptr = reinterpret_cast<int*>(slice.arr[0].Data());
      *ptr = i * 1000;

      // Ensure write completes before done_write
      std::atomic_thread_fence(std::memory_order_release);

      slice.done_write();
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  auto result = buffer.Wait();

  // Verify all writes are visible
  auto* ptr = reinterpret_cast<int*>(result[0].Data());
  std::vector<int> values(ptr, ptr + batch);

  std::sort(values.begin(), values.end());

  for (int i = 0; i < batch; ++i) {
    EXPECT_EQ(values[i], i * 1000);
  }
}

// Test ordered allocation (sync mode)
TEST(StateBufferComprehensiveTest, OrderedAllocationParallel) {
  int batch = 256;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  ThreadPool pool(32);
  std::vector<std::future<void>> futures;
  std::vector<int> order(batch);
  std::iota(order.begin(), order.end(), 0);

  // Shuffle to simulate out-of-order execution
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(order.begin(), order.end(), g);

  for (int i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      int my_order = order[i];
      auto slice = buffer.Allocate(1, my_order);

      // Write order value
      auto* ptr = reinterpret_cast<int*>(slice.arr[0].Data());
      *ptr = my_order;

      slice.done_write();
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  auto result = buffer.Wait();

  // Verify results are in order
  auto* ptr = reinterpret_cast<int*>(result[0].Data());
  for (int i = 0; i < batch; ++i) {
    EXPECT_EQ(ptr[i], i) << "Mismatch at index " << i;
  }
}

// Test edge case: batch=1
TEST(StateBufferComprehensiveTest, EdgeCaseBatchOne) {
  int batch = 1;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch, 10})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  auto slice = buffer.Allocate(1);
  auto* ptr = reinterpret_cast<int*>(slice.arr[0].Data());
  *ptr = 42;
  slice.done_write();

  auto result = buffer.Wait();
  auto* result_ptr = reinterpret_cast<int*>(result[0].Data());
  EXPECT_EQ(*result_ptr, 42);
}

// Test edge case: large batch
TEST(StateBufferComprehensiveTest, EdgeCaseLargeBatch) {
  int batch = 10000;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(1, {batch})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  ThreadPool pool(std::thread::hardware_concurrency());
  std::vector<std::future<void>> futures;

  for (int i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&]() {
      auto slice = buffer.Allocate(1);
      slice.done_write();
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  auto result = buffer.Wait();
  EXPECT_EQ(result[0].Shape(0), batch);
}

// Test partial batch with additional_done_count
TEST(StateBufferComprehensiveTest, PartialBatchAdditionalDone) {
  int batch = 100;
  int max_num_players = 1;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({false}));

  int actual_allocations = 60;

  ThreadPool pool(actual_allocations);
  std::vector<std::future<void>> futures;

  for (int i = 0; i < actual_allocations; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      auto slice = buffer.Allocate(1);
      auto* ptr = reinterpret_cast<int*>(slice.arr[0].Data());
      *ptr = i;
      slice.done_write();
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  // Wait with additional_done_count to account for missing allocations
  auto result = buffer.Wait(batch - actual_allocations);
  EXPECT_EQ(result[0].Shape(0), actual_allocations);

  // Verify values
  auto* ptr = reinterpret_cast<int*>(result[0].Data());
  std::vector<int> values(ptr, ptr + actual_allocations);
  std::sort(values.begin(), values.end());

  for (int i = 0; i < actual_allocations; ++i) {
    EXPECT_EQ(values[i], i);
  }
}

// Stress test: rapid allocation and completion
TEST(StateBufferComprehensiveTest, RapidAllocationCompletion) {
  int batch = 500;
  int max_num_players = 3;
  std::vector<ShapeSpec> specs{ShapeSpec(4, {batch * max_num_players, 2}),
                               ShapeSpec(8, {batch, 3})};

  StateBuffer buffer(batch, max_num_players, specs,
                     std::vector<bool>({true, false}));

  ThreadPool pool(100);
  std::vector<std::future<void>> futures;

  for (int i = 0; i < batch; ++i) {
    futures.push_back(pool.enqueue([&, i]() {
      std::size_t num_players = 1 + (i % max_num_players);
      auto slice = buffer.Allocate(num_players);

      // Immediate completion (no work)
      slice.done_write();
    }));
  }

  for (auto& f : futures) {
    f.wait();
  }

  auto result = buffer.Wait();
  EXPECT_GT(result[0].Shape(0), 0);
  EXPECT_EQ(result[1].Shape(0), batch);
}
