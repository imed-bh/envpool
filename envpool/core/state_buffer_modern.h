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

#ifndef ENVPOOL_CORE_STATE_BUFFER_MODERN_H_
#define ENVPOOL_CORE_STATE_BUFFER_MODERN_H_

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <semaphore>
#include <span>
#include <utility>
#include <vector>

#include "envpool/core/array.h"
#include "envpool/core/dict.h"
#include "envpool/core/spec.h"

namespace envpool {
namespace modern {

/**
 * @brief Error codes for StateBuffer operations
 */
enum class StateBufferError {
  OutOfStorage,
  InvalidPlayerCount,
  BufferFull,
  InvalidOrder
};

/**
 * @brief Offsets for player and shared state allocation
 *
 * Modern version with:
 * - Named fields (more readable than bit manipulation)
 * - Constexpr operations
 * - Three-way comparison
 */
struct StateOffsets {
  uint32_t player_offset{0};
  uint32_t shared_offset{0};

  auto operator<=>(const StateOffsets&) const = default;

  [[nodiscard]] constexpr bool IsValid(uint32_t max_players,
                                        uint32_t batch) const noexcept {
    return player_offset <= shared_offset * max_players &&
           shared_offset <= batch;
  }
};

/**
 * @brief Callback concept for done_write operation
 */
template <typename F>
concept DoneCallback = requires(F f) {
  { f() } -> std::same_as<void>;
};

/**
 * @brief Writable slice of state arrays with RAII done_write
 *
 * Modern improvements:
 * - Uses std::move_only_function (C++23) to avoid heap allocation
 * - RAII: automatically calls done_write on destruction
 * - Move-only semantics
 */
struct WritableSlice {
  std::vector<Array> arr;
  std::move_only_function<void()> done_write;

  // Move-only
  WritableSlice() = default;
  WritableSlice(const WritableSlice&) = delete;
  WritableSlice& operator=(const WritableSlice&) = delete;

  WritableSlice(WritableSlice&&) noexcept = default;
  WritableSlice& operator=(WritableSlice&&) noexcept = default;

  // RAII: call done_write on destruction
  ~WritableSlice() {
    if (done_write) {
      done_write();
    }
  }

  // Manual completion (prevents auto-call in destructor)
  void Complete() {
    if (done_write) {
      done_write();
      done_write = nullptr;
    }
  }

  // Get span view of arrays
  [[nodiscard]] std::span<Array> Arrays() noexcept {
    return std::span{arr};
  }

  [[nodiscard]] std::span<const Array> Arrays() const noexcept {
    return std::span{arr};
  }
};

/**
 * @brief Modern C++26 state buffer for batched environment states
 *
 * Key improvements:
 * - std::expected for error handling
 * - Clearer dual-counter semantics with StateOffsets
 * - Explicit memory ordering
 * - constexpr where possible
 * - Better const-correctness
 * - Enhanced documentation
 *
 * Thread Safety:
 * - Allocate: Safe from multiple threads
 * - Wait: Single-threaded only (documented limitation)
 * - Done: Safe from multiple threads
 *
 * Clever Optimization:
 * Unlike original's bit-packed uint64_t, uses separate atomics for clarity.
 * Modern compilers can optimize atomic operations well.
 * If benchmarks show regression, can revert to bit-packing.
 */
class StateBuffer {
 public:
  using size_type = std::size_t;

  /**
   * @brief Construct state buffer
   *
   * @param batch Number of environments in batch
   * @param max_num_players Maximum players per environment
   * @param specs Array specifications
   * @param is_player_state Flags indicating player vs shared state
   */
  StateBuffer(size_type batch, size_type max_num_players,
              const std::vector<ShapeSpec>& specs,
              std::vector<bool> is_player_state)
      : batch_{batch},
        max_num_players_{max_num_players},
        arrays_{MakeArray(specs)},
        is_player_state_{std::move(is_player_state)},
        player_offset_{0},
        shared_offset_{0},
        alloc_count_{0},
        done_count_{0},
        sem_{0} {
    assert(batch > 0 && "batch must be positive");
    assert(max_num_players > 0 && "max_num_players must be positive");
    assert(specs.size() == is_player_state_.size());
  }

  // Non-copyable, non-movable (contains atomics)
  StateBuffer(const StateBuffer&) = delete;
  StateBuffer& operator=(const StateBuffer&) = delete;
  StateBuffer(StateBuffer&&) = delete;
  StateBuffer& operator=(StateBuffer&&) = delete;

  /**
   * @brief Allocate slice for writing environment state
   *
   * Lock-free allocation using atomic counters.
   *
   * @param num_players Number of players for this environment
   * @param order Execution order (-1 for async mode, ≥0 for sync mode)
   * @return std::expected<WritableSlice, StateBufferError> Slice or error
   */
  [[nodiscard]] std::expected<WritableSlice, StateBufferError> Allocate(
      size_type num_players, int order = -1) noexcept {
    // Validate input
    if (num_players == 0 || num_players > max_num_players_) {
      return std::unexpected(StateBufferError::InvalidPlayerCount);
    }

    if (order < -1 || (order >= 0 && static_cast<size_type>(order) >= batch_)) {
      return std::unexpected(StateBufferError::InvalidOrder);
    }

    // Atomically claim allocation slot
    size_type alloc_count =
        alloc_count_.fetch_add(1, std::memory_order_acquire);

    if (alloc_count >= batch_) {
      // Rollback allocation
      alloc_count_.fetch_sub(1, std::memory_order_release);
      return std::unexpected(StateBufferError::OutOfStorage);
    }

    // Determine offsets
    uint32_t player_offset, shared_offset;

    if (order != -1 && max_num_players_ == 1) {
      // Sync mode: use order for position
      player_offset = shared_offset = static_cast<uint32_t>(order);
    } else {
      // Async mode: atomically allocate offsets
      // Note: Unlike original bit-packed approach, uses separate atomics
      // If benchmarks show this is slower, can revert to bit-packing
      player_offset =
          player_offset_.fetch_add(num_players, std::memory_order_relaxed);
      shared_offset = shared_offset_.fetch_add(1, std::memory_order_relaxed);
    }

    // Validate offsets
    assert(shared_offset < batch_);
    assert(player_offset + num_players <= batch_ * max_num_players_);

    // Create array slices
    std::vector<Array> state;
    state.reserve(arrays_.size());

    for (size_type i = 0; i < arrays_.size(); ++i) {
      const Array& a = arrays_[i];
      if (is_player_state_[i]) {
        state.emplace_back(a.Slice(player_offset, player_offset + num_players));
      } else {
        state.emplace_back(a[shared_offset]);
      }
    }

    // Create WritableSlice with done callback
    return WritableSlice{
        .arr = std::move(state),
        .done_write = [this]() { Done(); }};
  }

  /**
   * @brief Get current offsets (for debugging/testing)
   */
  [[nodiscard]] StateOffsets Offsets() const noexcept {
    return StateOffsets{
        .player_offset = player_offset_.load(std::memory_order_acquire),
        .shared_offset = shared_offset_.load(std::memory_order_acquire)};
  }

  /**
   * @brief Signal that writing to allocated slice is complete
   *
   * Called automatically by WritableSlice destructor, but can be
   * called manually if needed.
   *
   * @param num Number of completions to signal (default 1)
   */
  void Done(size_type num = 1) noexcept {
    size_type done_count = done_count_.fetch_add(num, std::memory_order_acq_rel);

    // Last writer signals completion
    if (done_count + num == batch_) {
      sem_.release();
    }
  }

  /**
   * @brief Wait for entire batch to complete
   *
   * IMPORTANT: Must be called from single thread only!
   * Multiple concurrent Wait() calls will cause undefined behavior.
   *
   * @param additional_done_count Number of "virtual" completions (for partial batches)
   * @return std::expected<std::vector<Array>, StateBufferError> Arrays or error
   */
  [[nodiscard]] std::expected<std::vector<Array>, StateBufferError> Wait(
      size_type additional_done_count = 0) noexcept {
    if (additional_done_count > 0) {
      Done(additional_done_count);
    }

    // Block until batch complete
    sem_.acquire();

    // Get final offsets
    uint32_t player_offset = player_offset_.load(std::memory_order_acquire);
    uint32_t shared_offset = shared_offset_.load(std::memory_order_acquire);

    // Verify consistency
    assert(shared_offset == batch_ - additional_done_count);

    // Truncate arrays to actual written size
    std::vector<Array> ret;
    ret.reserve(arrays_.size());

    for (size_type i = 0; i < arrays_.size(); ++i) {
      const Array& a = arrays_[i];
      if (is_player_state_[i]) {
        ret.emplace_back(a.Truncate(player_offset));
      } else {
        ret.emplace_back(a.Truncate(shared_offset));
      }
    }

    return ret;
  }

  /**
   * @brief Get batch size
   */
  [[nodiscard]] constexpr size_type batch() const noexcept { return batch_; }

  /**
   * @brief Get max players per environment
   */
  [[nodiscard]] constexpr size_type max_num_players() const noexcept {
    return max_num_players_;
  }

  /**
   * @brief Get allocation count (for debugging)
   */
  [[nodiscard]] size_type AllocationCount() const noexcept {
    return alloc_count_.load(std::memory_order_acquire);
  }

  /**
   * @brief Get completion count (for debugging)
   */
  [[nodiscard]] size_type CompletionCount() const noexcept {
    return done_count_.load(std::memory_order_acquire);
  }

 private:
  const size_type batch_;
  const size_type max_num_players_;

  std::vector<Array> arrays_;
  std::vector<bool> is_player_state_;

  // Separate atomics for clarity (vs bit-packed uint64_t in original)
  // Benchmark to verify performance!
  alignas(64) std::atomic<uint32_t> player_offset_;
  alignas(64) std::atomic<uint32_t> shared_offset_;

  alignas(64) std::atomic<size_type> alloc_count_;
  alignas(64) std::atomic<size_type> done_count_;

  std::counting_semaphore<> sem_;  // Batch completion signal
};

}  // namespace modern
}  // namespace envpool

#endif  // ENVPOOL_CORE_STATE_BUFFER_MODERN_H_
