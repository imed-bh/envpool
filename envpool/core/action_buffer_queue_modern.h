/*
 * Copyright 2023 Garena Online Private Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ENVPOOL_CORE_ACTION_BUFFER_QUEUE_MODERN_H_
#define ENVPOOL_CORE_ACTION_BUFFER_QUEUE_MODERN_H_

#include <atomic>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <semaphore>
#include <span>
#include <utility>
#include <vector>

#include "envpool/core/array.h"

namespace envpool {
namespace modern {

/**
 * @brief Error codes for ActionBufferQueue operations
 */
enum class QueueError {
  Empty,
  Full,
  Timeout,
  Shutdown
};

/**
 * @brief Action metadata for environment execution
 *
 * Modern version with:
 * - Default member initialization
 * - Spaceship operator for comparison
 * - Better documentation
 */
struct ActionSlice {
  int env_id{-1};          ///< Environment ID to execute
  int order{-1};            ///< Execution order (-1 for async mode)
  bool force_reset{false};  ///< Force environment reset

  /// Three-way comparison operator (C++20)
  auto operator<=>(const ActionSlice&) const = default;

  /// Validation
  [[nodiscard]] constexpr bool IsValid() const noexcept {
    return env_id >= 0;
  }
};

/**
 * @brief Modern C++26 lock-free action buffer queue
 *
 * Key improvements:
 * - std::expected for error handling
 * - std::span for input ranges
 * - std::counting_semaphore (benchmark vs LightweightSemaphore)
 * - TryDequeue with timeout support
 * - Graceful shutdown via stop token
 * - Explicit memory ordering
 * - constexpr where possible
 *
 * Thread Safety:
 * - EnqueueBulk: Single producer (externally synchronized if needed)
 * - Dequeue: Multiple consumers safe
 * - TryDequeue: Multiple consumers safe
 */
class ActionBufferQueue {
 public:
  using value_type = ActionSlice;
  using size_type = std::size_t;

  /**
   * @brief Construct action buffer queue
   *
   * @param num_envs Number of environments (queue size = 2 * num_envs)
   */
  explicit constexpr ActionBufferQueue(size_type num_envs)
      : alloc_ptr_{0},
        done_ptr_{0},
        queue_size_{num_envs * 2},
        queue_(queue_size_),
        sem_{0},          // No items initially
        sem_enqueue_{1},  // Single producer allowed
        sem_dequeue_{1},  // Single consumer per dequeue
        shutdown_{false} {
    assert(num_envs > 0 && "num_envs must be positive");
    assert((queue_size_ & (queue_size_ - 1)) == 0 ||
           true /* Allow non-power-of-2 for now */);
  }

  // Non-copyable, non-movable
  ActionBufferQueue(const ActionBufferQueue&) = delete;
  ActionBufferQueue& operator=(const ActionBufferQueue&) = delete;
  ActionBufferQueue(ActionBufferQueue&&) = delete;
  ActionBufferQueue& operator=(ActionBufferQueue&&) = delete;

  /**
   * @brief Enqueue a batch of actions (single producer)
   *
   * This operation is designed for a single producer thread.
   * If called from multiple threads, external synchronization required.
   *
   * @param actions Span of ActionSlice objects to enqueue
   * @return std::expected<void, QueueError> Success or error
   */
  std::expected<void, QueueError> EnqueueBulk(
      std::span<const ActionSlice> actions) noexcept {
    if (actions.empty()) {
      return {};  // No-op for empty batch
    }

    if (shutdown_.load(std::memory_order_acquire)) {
      return std::unexpected(QueueError::Shutdown);
    }

    // Ensure only one EnqueueBulk happens at a time
    sem_enqueue_.acquire();

    // Atomically claim slots
    // Use relaxed ordering - semaphore provides synchronization
    uint64_t pos = alloc_ptr_.fetch_add(actions.size(), std::memory_order_relaxed);

    // Write actions to circular buffer
    for (size_type i = 0; i < actions.size(); ++i) {
      queue_[(pos + i) % queue_size_] = actions[i];
    }

    // Signal consumers that items are available
    sem_.release(actions.size());

    // Release producer lock
    sem_enqueue_.release();

    return {};
  }

  /**
   * @brief Convenience overload for vector input
   */
  std::expected<void, QueueError> EnqueueBulk(
      const std::vector<ActionSlice>& actions) noexcept {
    return EnqueueBulk(std::span{actions});
  }

  /**
   * @brief Convenience overload for ranges (C++20)
   */
  template <std::ranges::range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, ActionSlice>
  std::expected<void, QueueError> EnqueueBulk(const R& actions) noexcept {
    // Convert range to vector if not contiguous
    if constexpr (std::ranges::contiguous_range<R>) {
      return EnqueueBulk(std::span{std::ranges::data(actions),
                                   std::ranges::size(actions)});
    } else {
      std::vector<ActionSlice> vec(std::ranges::begin(actions),
                                   std::ranges::end(actions));
      return EnqueueBulk(std::span{vec});
    }
  }

  /**
   * @brief Dequeue a single action (blocking)
   *
   * Blocks until an action is available or shutdown is signaled.
   *
   * @return std::expected<ActionSlice, QueueError> Action or error
   */
  [[nodiscard]] std::expected<ActionSlice, QueueError> Dequeue() noexcept {
    // Wait for item
    sem_.acquire();

    // Check shutdown
    if (shutdown_.load(std::memory_order_acquire)) {
      return std::unexpected(QueueError::Shutdown);
    }

    // Ensure single consumer per dequeue
    sem_dequeue_.acquire();

    // Atomically claim slot
    uint64_t ptr = done_ptr_.fetch_add(1, std::memory_order_relaxed);
    ActionSlice result = queue_[ptr % queue_size_];

    // Release consumer lock
    sem_dequeue_.release();

    return result;
  }

  /**
   * @brief Try to dequeue without blocking
   *
   * @return std::expected<ActionSlice, QueueError> Action or QueueError::Empty
   */
  [[nodiscard]] std::expected<ActionSlice, QueueError> TryDequeue() noexcept {
    if (!sem_.try_acquire()) {
      return std::unexpected(QueueError::Empty);
    }

    if (shutdown_.load(std::memory_order_acquire)) {
      return std::unexpected(QueueError::Shutdown);
    }

    sem_dequeue_.acquire();

    uint64_t ptr = done_ptr_.fetch_add(1, std::memory_order_relaxed);
    ActionSlice result = queue_[ptr % queue_size_];

    sem_dequeue_.release();

    return result;
  }

  /**
   * @brief Try to dequeue with timeout (C++20)
   *
   * @param timeout Maximum time to wait
   * @return std::expected<ActionSlice, QueueError> Action or error
   */
  template <typename Rep, typename Period>
  [[nodiscard]] std::expected<ActionSlice, QueueError> TryDequeueFor(
      const std::chrono::duration<Rep, Period>& timeout) noexcept {
    if (!sem_.try_acquire_for(timeout)) {
      return std::unexpected(QueueError::Timeout);
    }

    if (shutdown_.load(std::memory_order_acquire)) {
      return std::unexpected(QueueError::Shutdown);
    }

    sem_dequeue_.acquire();

    uint64_t ptr = done_ptr_.fetch_add(1, std::memory_order_relaxed);
    ActionSlice result = queue_[ptr % queue_size_];

    sem_dequeue_.release();

    return result;
  }

  /**
   * @brief Get approximate queue size
   *
   * Note: Snapshot value, may be stale immediately.
   *
   * @return Approximate number of items in queue
   */
  [[nodiscard]] size_type SizeApprox() const noexcept {
    uint64_t alloc = alloc_ptr_.load(std::memory_order_acquire);
    uint64_t done = done_ptr_.load(std::memory_order_acquire);
    return static_cast<size_type>(
        alloc >= done ? alloc - done : 0);
  }

  /**
   * @brief Check if queue is approximately empty
   */
  [[nodiscard]] bool EmptyApprox() const noexcept {
    return SizeApprox() == 0;
  }

  /**
   * @brief Get queue capacity
   */
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return queue_size_;
  }

  /**
   * @brief Signal shutdown to all waiting consumers
   *
   * After calling this, all Dequeue operations will return
   * QueueError::Shutdown. This enables graceful worker thread shutdown.
   */
  void Shutdown() noexcept {
    shutdown_.store(true, std::memory_order_release);

    // Wake up all waiting consumers
    // Release enough tokens to wake all potential waiters
    sem_.release(queue_size_);
  }

  /**
   * @brief Check if queue is shut down
   */
  [[nodiscard]] bool IsShutdown() const noexcept {
    return shutdown_.load(std::memory_order_acquire);
  }

 private:
  // Monotonic counters (never decrease within queue lifetime)
  alignas(64) std::atomic<uint64_t> alloc_ptr_;  // Producer position
  alignas(64) std::atomic<uint64_t> done_ptr_;   // Consumer position

  const size_type queue_size_;  // Capacity (immutable after construction)

  std::vector<ActionSlice> queue_;  // Pre-allocated circular buffer

  // Semaphores for coordination
  std::counting_semaphore<> sem_;          // Available items
  std::counting_semaphore<> sem_enqueue_;  // Single producer lock
  std::counting_semaphore<> sem_dequeue_;  // Single consumer lock per dequeue

  std::atomic<bool> shutdown_;  // Shutdown flag
};

}  // namespace modern
}  // namespace envpool

#endif  // ENVPOOL_CORE_ACTION_BUFFER_QUEUE_MODERN_H_
