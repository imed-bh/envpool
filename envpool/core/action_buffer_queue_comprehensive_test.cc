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

#include "envpool/core/action_buffer_queue.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

using ActionSlice = typename ActionBufferQueue::ActionSlice;

// Test multi-producer scenario
TEST(ActionBufferQueueComprehensiveTest, MultiProducer) {
  std::size_t num_envs = 1000;
  ActionBufferQueue queue(num_envs);

  std::size_t num_producers = 8;
  std::size_t actions_per_producer = 1000;
  std::atomic<std::size_t> total_enqueued{0};

  std::vector<std::thread> producers;
  for (std::size_t p = 0; p < num_producers; ++p) {
    producers.emplace_back([&, p]() {
      std::mt19937 gen(p);
      std::uniform_int_distribution<> dist(1, 100);

      for (std::size_t i = 0; i < actions_per_producer; ++i) {
        std::size_t batch_size = dist(gen);
        std::vector<ActionSlice> actions;
        for (std::size_t j = 0; j < batch_size; ++j) {
          actions.push_back(ActionSlice{
            .env_id = static_cast<int>((p * actions_per_producer + i + j) % num_envs),
            .order = -1,
            .force_reset = false
          });
        }
        queue.EnqueueBulk(actions);
        total_enqueued += batch_size;
      }
    });
  }

  // Consumer thread
  std::atomic<std::size_t> total_dequeued{0};
  std::thread consumer([&]() {
    while (total_dequeued < num_producers * actions_per_producer * 50) {
      if (queue.SizeApprox() > 0) {
        queue.Dequeue();
        total_dequeued++;
      } else {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    }
  });

  for (auto& p : producers) {
    p.join();
  }
  consumer.join();

  EXPECT_GT(total_enqueued, 0);
  EXPECT_EQ(total_dequeued, total_enqueued);
}

// Test wraparound behavior
TEST(ActionBufferQueueComprehensiveTest, Wraparound) {
  std::size_t num_envs = 100;
  ActionBufferQueue queue(num_envs);  // queue_size = 200

  // Fill and drain multiple times to test wraparound
  for (std::size_t round = 0; round < 1000; ++round) {
    std::vector<ActionSlice> actions;
    for (std::size_t i = 0; i < num_envs; ++i) {
      actions.push_back(ActionSlice{
        .env_id = static_cast<int>(i),
        .order = static_cast<int>(round),
        .force_reset = false
      });
    }
    queue.EnqueueBulk(actions);

    for (std::size_t i = 0; i < num_envs; ++i) {
      ActionSlice slice = queue.Dequeue();
      EXPECT_EQ(slice.env_id, static_cast<int>(i));
      EXPECT_EQ(slice.order, static_cast<int>(round));
    }
  }
}

// Stress test with high concurrency
TEST(ActionBufferQueueComprehensiveTest, HighConcurrencyStress) {
  std::size_t num_envs = 10000;
  ActionBufferQueue queue(num_envs);

  std::size_t num_producers = 16;
  std::size_t num_consumers = 16;
  std::size_t operations = 10000;

  std::atomic<bool> stop{false};
  std::atomic<std::size_t> total_produced{0};
  std::atomic<std::size_t> total_consumed{0};

  // Producer threads
  std::vector<std::thread> producers;
  for (std::size_t p = 0; p < num_producers; ++p) {
    producers.emplace_back([&, p]() {
      std::mt19937 gen(p);
      std::uniform_int_distribution<> batch_dist(1, 50);

      for (std::size_t i = 0; i < operations; ++i) {
        std::vector<ActionSlice> actions;
        std::size_t batch_size = batch_dist(gen);
        for (std::size_t j = 0; j < batch_size; ++j) {
          actions.push_back(ActionSlice{
            .env_id = static_cast<int>((p * 1000 + i + j) % num_envs),
            .order = -1,
            .force_reset = (j % 10 == 0)
          });
        }
        queue.EnqueueBulk(actions);
        total_produced += batch_size;
      }
    });
  }

  // Consumer threads
  std::vector<std::thread> consumers;
  for (std::size_t c = 0; c < num_consumers; ++c) {
    consumers.emplace_back([&]() {
      while (!stop) {
        if (queue.SizeApprox() > 0) {
          queue.Dequeue();
          total_consumed++;
        } else {
          std::this_thread::yield();
        }
      }
    });
  }

  // Wait for all producers
  for (auto& p : producers) {
    p.join();
  }

  // Wait for consumers to catch up
  while (total_consumed < total_produced) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  stop = true;
  for (auto& c : consumers) {
    c.join();
  }

  EXPECT_EQ(total_consumed, total_produced);
}

// Test memory ordering guarantees
TEST(ActionBufferQueueComprehensiveTest, MemoryOrdering) {
  std::size_t num_envs = 1000;
  ActionBufferQueue queue(num_envs);

  struct SharedData {
    std::atomic<int> value{0};
  };
  SharedData shared;

  std::thread producer([&]() {
    for (int i = 0; i < 10000; ++i) {
      shared.value.store(i, std::memory_order_release);

      std::vector<ActionSlice> actions;
      actions.push_back(ActionSlice{
        .env_id = i % num_envs,
        .order = i,
        .force_reset = false
      });
      queue.EnqueueBulk(actions);
    }
  });

  std::thread consumer([&]() {
    int last_value = -1;
    for (int i = 0; i < 10000; ++i) {
      ActionSlice slice = queue.Dequeue();
      int current_value = shared.value.load(std::memory_order_acquire);

      // Due to queue synchronization, current_value should be >= slice.order
      // (may be greater due to producer running ahead)
      EXPECT_GE(current_value, slice.order);
      EXPECT_GT(slice.order, last_value);
      last_value = slice.order;
    }
  });

  producer.join();
  consumer.join();
}

// Test FIFO ordering
TEST(ActionBufferQueueComprehensiveTest, FIFOOrdering) {
  std::size_t num_envs = 100;
  ActionBufferQueue queue(num_envs);

  // Enqueue actions with sequential orders
  std::vector<ActionSlice> actions;
  for (int i = 0; i < 1000; ++i) {
    actions.push_back(ActionSlice{
      .env_id = i % num_envs,
      .order = i,
      .force_reset = false
    });
  }
  queue.EnqueueBulk(actions);

  // Verify FIFO order
  for (int i = 0; i < 1000; ++i) {
    ActionSlice slice = queue.Dequeue();
    EXPECT_EQ(slice.order, i);
  }
}

// Test with variable timing
TEST(ActionBufferQueueComprehensiveTest, VariableTiming) {
  std::size_t num_envs = 500;
  ActionBufferQueue queue(num_envs);

  std::atomic<int> sequence{0};

  std::thread producer([&]() {
    std::mt19937 gen(42);
    std::uniform_int_distribution<> sleep_dist(0, 100);

    for (int i = 0; i < 1000; ++i) {
      std::vector<ActionSlice> actions;
      actions.push_back(ActionSlice{
        .env_id = i % num_envs,
        .order = sequence++,
        .force_reset = false
      });
      queue.EnqueueBulk(actions);

      // Random micro-sleeps
      std::this_thread::sleep_for(std::chrono::microseconds(sleep_dist(gen)));
    }
  });

  std::thread consumer([&]() {
    std::mt19937 gen(43);
    std::uniform_int_distribution<> sleep_dist(0, 100);

    int expected_order = 0;
    for (int i = 0; i < 1000; ++i) {
      ActionSlice slice = queue.Dequeue();
      EXPECT_EQ(slice.order, expected_order++);

      // Random micro-sleeps
      std::this_thread::sleep_for(std::chrono::microseconds(sleep_dist(gen)));
    }
  });

  producer.join();
  consumer.join();
}

// Test burst scenarios
TEST(ActionBufferQueueComprehensiveTest, BurstScenario) {
  std::size_t num_envs = 1000;
  ActionBufferQueue queue(num_envs);

  std::atomic<std::size_t> total_items{0};

  // Producer creates bursts
  std::thread producer([&]() {
    for (int burst = 0; burst < 100; ++burst) {
      // Create large burst
      std::vector<ActionSlice> actions;
      for (std::size_t i = 0; i < num_envs; ++i) {
        actions.push_back(ActionSlice{
          .env_id = static_cast<int>(i),
          .order = burst,
          .force_reset = false
        });
      }
      queue.EnqueueBulk(actions);
      total_items += num_envs;

      // Pause between bursts
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  // Consumer drains continuously
  std::thread consumer([&]() {
    std::size_t consumed = 0;
    while (consumed < 100 * num_envs) {
      if (queue.SizeApprox() > 0) {
        queue.Dequeue();
        consumed++;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();
}
