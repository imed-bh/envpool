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

#ifndef ENVPOOL_CORE_STATE_BUFFER_QUEUE_MODERN_H_
#define ENVPOOL_CORE_STATE_BUFFER_QUEUE_MODERN_H_

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "envpool/core/array.h"
#include "envpool/core/circular_buffer_modern.h"
#include "envpool/core/spec.h"
#include "envpool/core/state_buffer_modern.h"

namespace envpool {
namespace modern {

/**
 * @brief Error codes for StateBufferQueue operations
 */
enum class StateQueueError {
  AllocationFailed,
  Timeout,
  Shutdown,
  InvalidConfig
};

/**
 * @brief Modern C++26 State Buffer Queue
 *
 * Manages a circular queue of StateBuffer objects for collecting
 * environment results from multiple worker threads.
 *
 * Key improvements:
 * - std::expected for error handling
 * - std::jthread for background buffer creation
 * - Explicit memory ordering
 * - Better RAII and lifetime management
 * - Enhanced error handling
 *
 * Architecture:
 * ```
 * Worker Threads → Allocate() → StateBuffer[i] → Done()
 *                                    ↓
 * User Thread ← Wait() ← Completed StateBuffer → Recycled
 *                                    ↓
 * Background Thread → Create new StateBuffer → stock_buffer_
 * ```
 *
 * Thread Safety:
 * - Allocate(): Safe from multiple threads (workers)
 * - Wait(): MUST be called from single thread only!
 * - Done(): Safe from multiple threads
 *
 * @note This is a critical performance component. Buffer recycling
 *       and lock-free allocation are essential for throughput.
 */
class StateBufferQueue {
 public:
  using size_type = std::size_t;
  using WritableSlice = StateBuffer::WritableSlice;

 private:
  const size_type batch_;
  const size_type max_num_players_;
  const std::vector<bool> is_player_state_;
  const std::vector<ShapeSpec> specs_;
  const size_type queue_size_;

  // Circular queue of state buffers
  std::vector<std::unique_ptr<StateBuffer>> queue_;

  // Atomic counters
  alignas(64) std::atomic<uint64_t> alloc_count_{0};
  alignas(64) std::atomic<uint64_t> done_ptr_{0};

  // Buffer recycling system
  CircularBuffer<std::unique_ptr<StateBuffer>> stock_buffer_;

  // Background threads for creating buffers (std::jthread for RAII)
  std::vector<std::jthread> create_buffer_threads_;

 public:
  /**
   * @brief Construct StateBufferQueue
   *
   * @param batch_env Batch size (number of envs per batch)
   * @param num_envs Total number of environments
   * @param max_num_players Maximum players per environment
   * @param specs Array specifications
   */
  StateBufferQueue(size_type batch_env, size_type num_envs,
                   size_type max_num_players,
                   const std::vector<ShapeSpec>& specs)
      : batch_(batch_env),
        max_num_players_(max_num_players),
        is_player_state_(Transform(specs,
                                   [](const ShapeSpec& s) {
                                     return (!s.shape.empty() &&
                                             s.shape[0] == -1);
                                   })),
        specs_(Transform(specs,
                        [=](ShapeSpec s) {
                          if (!s.shape.empty() && s.shape[0] == -1) {
                            s.shape[0] = batch_ * max_num_players_;
                            return s;
                          }
                          return s.Batch(batch_);
                        })),
        queue_size_((num_envs / batch_env + 2) * 2),
        queue_(queue_size_),
        stock_buffer_(queue_size_) {

    if (batch_env == 0) {
      throw std::invalid_argument("batch_env must be positive");
    }
    if (num_envs == 0) {
      throw std::invalid_argument("num_envs must be positive");
    }
    if (max_num_players == 0) {
      throw std::invalid_argument("max_num_players must be positive");
    }

    // Initialize all buffers
    for (auto& q : queue_) {
      q = std::make_unique<StateBuffer>(batch_, max_num_players_, specs_,
                                        is_player_state_);
    }

    // Spawn background threads to create stock buffers
    SpawnBufferCreationThreads();

    DLOG(INFO) << "StateBufferQueue initialized: batch=" << batch_
               << ", queue_size=" << queue_size_;
  }

  /**
   * @brief Destructor - RAII cleanup
   *
   * std::jthread automatically requests stop and joins.
   * No manual cleanup needed!
   */
  ~StateBufferQueue() {
    // std::jthread destructor will:
    // 1. Request stop on all threads (via stop_token)
    // 2. Join all threads automatically
    //
    // Background threads will exit their loops when stop is requested

    DLOG(INFO) << "StateBufferQueue destroyed";
  }

  // Non-copyable, non-movable
  StateBufferQueue(const StateBufferQueue&) = delete;
  StateBufferQueue& operator=(const StateBufferQueue&) = delete;
  StateBufferQueue(StateBufferQueue&&) = delete;
  StateBufferQueue& operator=(StateBufferQueue&&) = delete;

  /**
   * @brief Allocate slice for writing state (from worker thread)
   *
   * Thread-safe: Multiple workers can call this concurrently.
   *
   * @param num_players Number of players for this environment
   * @param order Execution order (-1 for async, ≥0 for sync)
   * @return std::expected<WritableSlice, StateQueueError> Slice or error
   */
  [[nodiscard]] std::expected<WritableSlice, StateQueueError> Allocate(
      size_type num_players, int order = -1) noexcept {
    // Atomically claim allocation slot
    size_type pos = alloc_count_.fetch_add(1, std::memory_order_acquire);
    size_type offset = (pos / batch_) % queue_size_;

    // Try to allocate from the buffer at this offset
    auto result = queue_[offset]->Allocate(num_players, order);

    if (!result) {
      // Allocation failed (buffer full or invalid params)
      // In production, this indicates a serious bug!
      return std::unexpected(StateQueueError::AllocationFailed);
    }

    return result;
  }

  /**
   * @brief Wait for batch to complete (from user thread)
   *
   * CRITICAL: This MUST be called from a single thread!
   * Concurrent Wait() calls will cause undefined behavior.
   *
   * @param additional_done_count Virtual completions (for partial batches)
   * @return std::expected<std::vector<Array>, StateQueueError> Arrays or error
   */
  [[nodiscard]] std::expected<std::vector<Array>, StateQueueError> Wait(
      size_type additional_done_count = 0) noexcept {
    // Get fresh buffer from stock (blocks if none available)
    auto newbuf_result = stock_buffer_.Get();
    if (!newbuf_result) {
      return std::unexpected(StateQueueError::Timeout);
    }

    std::unique_ptr<StateBuffer> newbuf = std::move(*newbuf_result);

    // Atomically claim next completed buffer
    size_type pos = done_ptr_.fetch_add(1, std::memory_order_acquire);
    size_type offset = pos % queue_size_;

    // Wait for this buffer to complete
    auto arr_result = queue_[offset]->Wait(additional_done_count);
    if (!arr_result) {
      // Wait failed (should not happen in normal operation)
      return std::unexpected(StateQueueError::AllocationFailed);
    }

    if (additional_done_count > 0) {
      // Move allocation pointer forward to skip virtual completions
      alloc_count_.fetch_add(additional_done_count, std::memory_order_release);
    }

    // Swap completed buffer with fresh one (recycling)
    std::swap(queue_[offset], newbuf);

    return std::move(*arr_result);
  }

  /**
   * @brief Try to wait with timeout
   *
   * @param timeout Maximum time to wait
   * @return std::expected<std::vector<Array>, StateQueueError> Arrays or timeout
   */
  template <typename Rep, typename Period>
  [[nodiscard]] std::expected<std::vector<Array>, StateQueueError> TryWaitFor(
      const std::chrono::duration<Rep, Period>& timeout) noexcept {
    // TODO: Implement timeout in StateBuffer::Wait
    // For now, just call regular Wait
    return Wait();
  }

  /**
   * @brief Get queue statistics (for debugging/monitoring)
   */
  struct Stats {
    size_type batch;
    size_type queue_size;
    uint64_t total_allocations;
    uint64_t total_completions;
  };

  [[nodiscard]] Stats GetStats() const noexcept {
    return Stats{
        .batch = batch_,
        .queue_size = queue_size_,
        .total_allocations = alloc_count_.load(std::memory_order_relaxed),
        .total_completions = done_ptr_.load(std::memory_order_relaxed)};
  }

 private:
  /**
   * @brief Spawn background threads to create stock buffers
   */
  void SpawnBufferCreationThreads() {
    size_type num_threads = std::max(1UL,
                                    std::thread::hardware_concurrency() / 64);

    create_buffer_threads_.reserve(num_threads);

    for (size_type i = 0; i < num_threads; ++i) {
      create_buffer_threads_.emplace_back([this](std::stop_token stoken) {
        BufferCreationLoop(stoken);
      });
    }

    DLOG(INFO) << "Spawned " << num_threads << " buffer creation threads";
  }

  /**
   * @brief Background loop for creating state buffers
   *
   * Continuously creates new StateBuffer objects and puts them in
   * the stock buffer for recycling.
   */
  void BufferCreationLoop(std::stop_token stoken) noexcept {
    while (!stoken.stop_requested()) {
      // Create new buffer
      auto new_buffer = std::make_unique<StateBuffer>(
          batch_, max_num_players_, specs_, is_player_state_);

      // Put in stock (blocks if stock is full)
      auto result = stock_buffer_.Put(std::move(new_buffer));

      if (!result) {
        // Put failed (should not happen unless shutting down)
        break;
      }

      // Check stop_token periodically
      if (stoken.stop_requested()) {
        break;
      }
    }

    DLOG(INFO) << "Buffer creation thread exiting";
  }

  /**
   * @brief Transform helper (from original code)
   */
  template <typename Container, typename Func>
  static auto Transform(const Container& c, Func f) {
    using ResultType = decltype(f(std::declval<typename Container::value_type>()));
    std::vector<ResultType> result;
    result.reserve(c.size());
    std::ranges::transform(c, std::back_inserter(result), f);
    return result;
  }
};

}  // namespace modern
}  // namespace envpool

#endif  // ENVPOOL_CORE_STATE_BUFFER_QUEUE_MODERN_H_
