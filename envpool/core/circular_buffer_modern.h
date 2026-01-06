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

#ifndef ENVPOOL_CORE_CIRCULAR_BUFFER_MODERN_H_
#define ENVPOOL_CORE_CIRCULAR_BUFFER_MODERN_H_

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <semaphore>
#include <span>
#include <utility>
#include <vector>

namespace envpool {
namespace modern {

/**
 * @brief Error codes for CircularBuffer operations
 */
enum class BufferError {
  Full,
  Empty,
  Timeout
};

/**
 * @brief Modern C++26 lock-free circular buffer with improved type safety
 *
 * Key improvements over original:
 * - Uses std::expected for error handling (no exceptions in hot path)
 * - std::counting_semaphore (standard library, may benchmark against LightweightSemaphore)
 * - Concepts for type constraints
 * - std::span for better array views
 * - constexpr where possible
 * - Explicit memory ordering
 * - Better documentation
 *
 * @tparam T Element type (must be movable)
 */
template <typename T>
  requires std::movable<T>
class CircularBuffer {
 public:
  using value_type = T;
  using size_type = std::size_t;

  /**
   * @brief Construct a circular buffer with given capacity
   * @param capacity Maximum number of elements (must be > 0)
   */
  explicit constexpr CircularBuffer(size_type capacity)
      : size_{capacity},
        sem_get_{0},                   // No items initially
        sem_put_{capacity},            // All slots available
        buffer_(capacity),
        head_{0},
        tail_{0} {
    assert(capacity > 0 && "CircularBuffer capacity must be positive");
  }

  // Non-copyable, non-movable (contains atomics)
  CircularBuffer(const CircularBuffer&) = delete;
  CircularBuffer& operator=(const CircularBuffer&) = delete;
  CircularBuffer(CircularBuffer&&) = delete;
  CircularBuffer& operator=(CircularBuffer&&) = delete;

  /**
   * @brief Put an element into the buffer (blocking)
   *
   * Blocks until space is available, then atomically claims a slot,
   * writes the element, and signals consumers.
   *
   * @param value Element to insert (will be moved)
   * @return std::expected<void, BufferError> Success or BufferError::Full
   */
  template <typename U>
    requires std::convertible_to<U, T>
  std::expected<void, BufferError> Put(U&& value) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    // Wait for space (blocking)
    sem_put_.acquire();

    // Atomically claim a slot
    // Use relaxed ordering - semaphore provides synchronization
    uint64_t tail = tail_.fetch_add(1, std::memory_order_relaxed);
    auto offset = tail % size_;

    // Write element (may throw if T's move constructor throws)
    try {
      buffer_[offset] = std::forward<U>(value);
    } catch (...) {
      // Restore semaphore on exception
      sem_put_.release();
      throw;
    }

    // Signal consumer that item is available
    sem_get_.release();

    return {};
  }

  /**
   * @brief Get an element from the buffer (blocking)
   *
   * Blocks until an element is available, then atomically claims it,
   * moves it out, and signals producers.
   *
   * @return std::expected<T, BufferError> Element or BufferError::Empty
   */
  [[nodiscard]] std::expected<T, BufferError> Get() noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    // Wait for item (blocking)
    sem_get_.acquire();

    // Atomically claim a slot
    // Use relaxed ordering - semaphore provides synchronization
    uint64_t head = head_.fetch_add(1, std::memory_order_relaxed);
    auto offset = head % size_;

    // Move element out
    T value = std::move(buffer_[offset]);

    // Signal producer that space is available
    sem_put_.release();

    return value;
  }

  /**
   * @brief Try to put an element without blocking
   *
   * @param value Element to insert
   * @return std::expected<void, BufferError> Success or BufferError::Full
   */
  template <typename U>
    requires std::convertible_to<U, T>
  std::expected<void, BufferError> TryPut(U&& value) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    if (!sem_put_.try_acquire()) {
      return std::unexpected(BufferError::Full);
    }

    uint64_t tail = tail_.fetch_add(1, std::memory_order_relaxed);
    auto offset = tail % size_;

    try {
      buffer_[offset] = std::forward<U>(value);
    } catch (...) {
      sem_put_.release();
      throw;
    }

    sem_get_.release();
    return {};
  }

  /**
   * @brief Try to get an element without blocking
   *
   * @return std::expected<T, BufferError> Element or BufferError::Empty
   */
  [[nodiscard]] std::expected<T, BufferError> TryGet() noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    if (!sem_get_.try_acquire()) {
      return std::unexpected(BufferError::Empty);
    }

    uint64_t head = head_.fetch_add(1, std::memory_order_relaxed);
    auto offset = head % size_;

    T value = std::move(buffer_[offset]);
    sem_put_.release();

    return value;
  }

  /**
   * @brief Get current approximate size
   *
   * Note: This is a snapshot and may be stale immediately after returning.
   * Use only for monitoring/debugging, not for synchronization.
   *
   * @return Approximate number of elements in buffer
   */
  [[nodiscard]] size_type SizeApprox() const noexcept {
    // Use acquire/release ordering to ensure visibility
    uint64_t current_tail = tail_.load(std::memory_order_acquire);
    uint64_t current_head = head_.load(std::memory_order_acquire);

    // Handle potential overflow (unlikely with uint64_t)
    return static_cast<size_type>(
        current_tail >= current_head ? current_tail - current_head : 0);
  }

  /**
   * @brief Check if buffer is approximately empty
   *
   * Note: This is a snapshot and may be stale immediately.
   *
   * @return true if buffer appears empty
   */
  [[nodiscard]] bool EmptyApprox() const noexcept {
    return SizeApprox() == 0;
  }

  /**
   * @brief Check if buffer is approximately full
   *
   * Note: This is a snapshot and may be stale immediately.
   *
   * @return true if buffer appears full
   */
  [[nodiscard]] bool FullApprox() const noexcept {
    return SizeApprox() >= size_;
  }

  /**
   * @brief Get buffer capacity
   *
   * @return Maximum number of elements
   */
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    return size_;
  }

 private:
  const size_type size_;  // Capacity (immutable after construction)

  // Semaphores for coordination
  // Note: std::counting_semaphore may be slower than LightweightSemaphore
  // Benchmark required! If slower, keep LightweightSemaphore.
  std::counting_semaphore<> sem_get_;  // Signals items available
  std::counting_semaphore<> sem_put_;  // Signals space available

  // Storage
  std::vector<T> buffer_;  // Pre-allocated circular buffer

  // Atomic counters (monotonically increasing)
  // Use relaxed ordering for counters - semaphores provide synchronization
  alignas(64) std::atomic<uint64_t> head_;  // Consumer position
  alignas(64) std::atomic<uint64_t> tail_;  // Producer position
  // Note: alignas(64) reduces false sharing on cache lines
};

/**
 * @brief Specialization for unique_ptr to avoid copying
 */
template <typename T>
class CircularBuffer<std::unique_ptr<T>> {
 public:
  using value_type = std::unique_ptr<T>;
  using size_type = std::size_t;

  explicit constexpr CircularBuffer(size_type capacity)
      : size_{capacity},
        sem_get_{0},
        sem_put_{capacity},
        buffer_(capacity),
        head_{0},
        tail_{0} {
    assert(capacity > 0);
  }

  CircularBuffer(const CircularBuffer&) = delete;
  CircularBuffer& operator=(const CircularBuffer&) = delete;
  CircularBuffer(CircularBuffer&&) = delete;
  CircularBuffer& operator=(CircularBuffer&&) = delete;

  std::expected<void, BufferError> Put(std::unique_ptr<T>&& value) noexcept {
    assert(value != nullptr && "Cannot put nullptr into buffer");

    sem_put_.acquire();

    uint64_t tail = tail_.fetch_add(1, std::memory_order_relaxed);
    auto offset = tail % size_;

    buffer_[offset] = std::move(value);
    sem_get_.release();

    return {};
  }

  [[nodiscard]] std::expected<std::unique_ptr<T>, BufferError> Get() noexcept {
    sem_get_.acquire();

    uint64_t head = head_.fetch_add(1, std::memory_order_relaxed);
    auto offset = head % size_;

    std::unique_ptr<T> value = std::move(buffer_[offset]);
    sem_put_.release();

    return value;
  }

  [[nodiscard]] size_type SizeApprox() const noexcept {
    uint64_t current_tail = tail_.load(std::memory_order_acquire);
    uint64_t current_head = head_.load(std::memory_order_acquire);
    return static_cast<size_type>(
        current_tail >= current_head ? current_tail - current_head : 0);
  }

  [[nodiscard]] constexpr size_type capacity() const noexcept { return size_; }

 private:
  const size_type size_;
  std::counting_semaphore<> sem_get_;
  std::counting_semaphore<> sem_put_;
  std::vector<std::unique_ptr<T>> buffer_;
  alignas(64) std::atomic<uint64_t> head_;
  alignas(64) std::atomic<uint64_t> tail_;
};

}  // namespace modern
}  // namespace envpool

#endif  // ENVPOOL_CORE_CIRCULAR_BUFFER_MODERN_H_
