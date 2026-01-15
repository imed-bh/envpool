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
// See the  License for the specific language governing permissions and
// limitations under the License.

/**
 * @module envpool.async.buffer
 * @brief Lock-free bounded circular buffer with semaphore synchronization
 *
 * This module provides a thread-safe circular buffer implementation using:
 * - std::counting_semaphore for blocking operations
 * - Atomic operations for lock-free access
 * - Cache-line alignment to prevent false sharing
 *
 * Clean code principles applied:
 * - Small, focused functions (<15 lines each)
 * - Single Responsibility Principle
 * - Clear naming (no abbreviations)
 * - std::expected for error handling
 */

module;

export module envpool.async.buffer;

import std;
import envpool.core.types;
import envpool.core.errors;

namespace envpool::async {

using namespace envpool::core;

//==============================================================================
// CircularBuffer
//==============================================================================

/**
 * @class CircularBuffer
 * @brief Thread-safe bounded circular buffer
 *
 * Features:
 * - Lock-free put/get operations
 * - Blocking and non-blocking variants
 * - Timeout support
 * - Move-only semantics for efficiency
 *
 * @tparam T Element type (must be movable)
 */
export template <Movable T>
class CircularBuffer {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct buffer with given capacity
   * @param capacity Maximum number of elements
   */
  explicit CircularBuffer(size_type capacity)
      : capacity_{capacity},
        buffer_(capacity),
        availableForGet_{0},
        availableForPut_{capacity} {}

  // Non-copyable
  CircularBuffer(const CircularBuffer&) = delete;
  CircularBuffer& operator=(const CircularBuffer&) = delete;

  // Movable
  CircularBuffer(CircularBuffer&&) noexcept = default;
  CircularBuffer& operator=(CircularBuffer&&) noexcept = default;

  //============================================================================
  // Capacity
  //============================================================================

  /**
   * @brief Get buffer capacity
   */
  [[nodiscard]] size_type capacity() const noexcept {
    return capacity_;
  }

  /**
   * @brief Get approximate size (may be stale)
   */
  [[nodiscard]] size_type sizeApproximate() const noexcept {
    return computeSize();
  }

  /**
   * @brief Check if approximately empty (may be stale)
   */
  [[nodiscard]] bool isEmptyApproximate() const noexcept {
    return computeSize() == 0;
  }

  /**
   * @brief Check if approximately full (may be stale)
   */
  [[nodiscard]] bool isFullApproximate() const noexcept {
    return computeSize() == capacity_;
  }

  //============================================================================
  // Put Operations (Producers)
  //============================================================================

  /**
   * @brief Put element (blocking)
   * @param element Element to put
   * @return Success or error
   */
  [[nodiscard]] VoidBufferResult put(T element) noexcept {
    return putImpl(move(element), [this] {
      waitForSpace();
    });
  }

  /**
   * @brief Try to put element (non-blocking)
   * @param element Element to put
   * @return Success or BufferError::Full
   */
  [[nodiscard]] VoidBufferResult tryPut(T element) noexcept {
    return putImpl(move(element), [this]() -> bool {
      return tryAcquireSpace();
    });
  }

  /**
   * @brief Try to put element with timeout
   * @param element Element to put
   * @param timeout Maximum wait time
   * @return Success, BufferError::Full, or BufferError::Timeout
   */
  template <Duration D>
  [[nodiscard]] VoidBufferResult tryPutFor(T element, D timeout) noexcept {
    return putImpl(move(element), [this, timeout]() -> bool {
      return tryAcquireSpaceFor(timeout);
    });
  }

  //============================================================================
  // Get Operations (Consumers)
  //============================================================================

  /**
   * @brief Get element (blocking)
   * @return Element or error
   */
  [[nodiscard]] BufferResult<T> get() noexcept {
    return getImpl([this] {
      waitForElement();
    });
  }

  /**
   * @brief Try to get element (non-blocking)
   * @return Element or BufferError::Empty
   */
  [[nodiscard]] BufferResult<T> tryGet() noexcept {
    return getImpl([this]() -> bool {
      return tryAcquireElement();
    });
  }

  /**
   * @brief Try to get element with timeout
   * @param timeout Maximum wait time
   * @return Element, BufferError::Empty, or BufferError::Timeout
   */
  template <Duration D>
  [[nodiscard]] BufferResult<T> tryGetFor(D timeout) noexcept {
    return getImpl([this, timeout]() -> bool {
      return tryAcquireElementFor(timeout);
    });
  }

 private:
  //============================================================================
  // Implementation Helpers
  //============================================================================

  /**
   * @brief Compute current size (approximate due to memory ordering)
   */
  [[nodiscard]] size_type computeSize() const noexcept {
    const auto head = headIndex_.load(std::memory_order_relaxed);
    const auto tail = tailIndex_.load(std::memory_order_relaxed);
    return (tail >= head) ? (tail - head) : (capacity_ + tail - head);
  }

  /**
   * @brief Wait for space to become available
   */
  void waitForSpace() noexcept {
    availableForPut_.acquire();
  }

  /**
   * @brief Try to acquire space without blocking
   */
  [[nodiscard]] bool tryAcquireSpace() noexcept {
    return availableForPut_.try_acquire();
  }

  /**
   * @brief Try to acquire space with timeout
   */
  template <Duration D>
  [[nodiscard]] bool tryAcquireSpaceFor(D timeout) noexcept {
    return availableForPut_.try_acquire_for(timeout);
  }

  /**
   * @brief Signal that space is available
   */
  void signalSpaceAvailable() noexcept {
    availableForPut_.release();
  }

  /**
   * @brief Wait for element to become available
   */
  void waitForElement() noexcept {
    availableForGet_.acquire();
  }

  /**
   * @brief Try to acquire element without blocking
   */
  [[nodiscard]] bool tryAcquireElement() noexcept {
    return availableForGet_.try_acquire();
  }

  /**
   * @brief Try to acquire element with timeout
   */
  template <Duration D>
  [[nodiscard]] bool tryAcquireElementFor(D timeout) noexcept {
    return availableForGet_.try_acquire_for(timeout);
  }

  /**
   * @brief Signal that element is available
   */
  void signalElementAvailable() noexcept {
    availableForGet_.release();
  }

  /**
   * @brief Get next tail index
   */
  [[nodiscard]] size_type getAndIncrementTail() noexcept {
    const auto index = tailIndex_.fetch_add(1, std::memory_order_relaxed);
    return index % capacity_;
  }

  /**
   * @brief Get next head index
   */
  [[nodiscard]] size_type getAndIncrementHead() noexcept {
    const auto index = headIndex_.fetch_add(1, std::memory_order_relaxed);
    return index % capacity_;
  }

  /**
   * @brief Put implementation with custom acquisition strategy
   */
  template <Callable F>
  [[nodiscard]] VoidBufferResult putImpl(T element, F acquire) noexcept {
    // Acquire space
    if constexpr (std::same_as<decltype(acquire()), void>) {
      acquire();  // Blocking acquire
    } else {
      if (!acquire()) {
        return makeError(BufferError::Full);
      }
    }

    // Store element
    const auto index = getAndIncrementTail();
    buffer_[index] = move(element);

    // Signal element available
    signalElementAvailable();

    return {};
  }

  /**
   * @brief Get implementation with custom acquisition strategy
   */
  template <Callable F>
  [[nodiscard]] BufferResult<T> getImpl(F acquire) noexcept {
    // Acquire element
    if constexpr (std::same_as<decltype(acquire()), void>) {
      acquire();  // Blocking acquire
    } else {
      if (!acquire()) {
        return makeError(BufferError::Empty);
      }
    }

    // Retrieve element
    const auto index = getAndIncrementHead();
    T element = move(buffer_[index]);

    // Signal space available
    signalSpaceAvailable();

    return element;
  }

  //============================================================================
  // Member Variables
  //============================================================================

  // Immutable
  size_type capacity_;
  std::vector<T> buffer_;

  // Synchronization (cache-line aligned to prevent false sharing)
  alignas(CacheLineSize) std::counting_semaphore<> availableForGet_;
  alignas(CacheLineSize) std::counting_semaphore<> availableForPut_;

  // Indices (cache-line aligned to prevent false sharing)
  alignas(CacheLineSize) std::atomic<size_type> headIndex_{0};
  alignas(CacheLineSize) std::atomic<size_type> tailIndex_{0};
};

}  // namespace envpool::async
