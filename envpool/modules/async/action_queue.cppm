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
 * @module envpool.async.action
 * @brief Action queue for distributing actions to worker threads
 *
 * Clean code principles:
 * - Small functions (<12 lines each)
 * - Clear naming (no abbreviations)
 * - Single responsibility per function
 * - Extracted validation logic
 * - std::expected for errors
 */

module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <print>
#include <random>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <print>
#include <random>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

export module envpool.async.action;
import envpool.core.types;
import envpool.core.errors;
import envpool.async.buffer;

namespace envpool::async {

using namespace envpool::core;

//==============================================================================
// ActionSlice - Represents an action for a specific environment
//==============================================================================

/**
 * @struct ActionSlice
 * @brief Action metadata for environment execution
 */
export struct ActionSlice {
  int environmentId{-1};
  int executionOrder{-1};
  bool forceReset{false};

  // Spaceship operator for comparison
  auto operator<=>(const ActionSlice&) const = default;
};

//==============================================================================
// ActionBufferQueue - Thread-safe action distribution queue
//==============================================================================

/**
 * @class ActionBufferQueue
 * @brief Distributes actions to worker threads via circular buffer
 *
 * Features:
 * - Bulk enqueue operations
 * - Timeout support for dequeue
 * - Graceful shutdown mechanism
 * - Thread-safe validation
 */
export class ActionBufferQueue {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct queue for given number of environments
   * @param numEnvironments Total environments in pool
   * @param queueCapacity Maximum pending actions (default: numEnvironments * 2)
   */
  explicit ActionBufferQueue(
      size_type numEnvironments,
      size_type queueCapacity = 0)
      : numEnvironments_{numEnvironments},
        queueCapacity_{queueCapacity > 0 ? queueCapacity : numEnvironments * 2},
        buffer_{queueCapacity_} {}

  // Non-copyable, movable
  ActionBufferQueue(const ActionBufferQueue&) = delete;
  ActionBufferQueue& operator=(const ActionBufferQueue&) = delete;
  ActionBufferQueue(ActionBufferQueue&&) noexcept = default;
  ActionBufferQueue& operator=(ActionBufferQueue&&) noexcept = default;

  //============================================================================
  // Queue Operations
  //============================================================================

  /**
   * @brief Enqueue single action
   * @param action Action to enqueue
   * @return Success or error
   */
  [[nodiscard]] VoidQueueResult enqueue(ActionSlice action) noexcept {
    if (auto error = validateAction(action)) {
      return std::unexpected(*error);
    }

    if (isShutdown()) {
      return std::unexpected(QueueError::Shutdown);
    }

    return enqueueValidated(move(action));
  }

  /**
   * @brief Enqueue multiple actions
   * @param actions Span of actions to enqueue
   * @return Success or error
   */
  [[nodiscard]] VoidQueueResult enqueueBulk(
      Span<const ActionSlice> actions) noexcept {

    if (isShutdown()) {
      return std::unexpected(QueueError::Shutdown);
    }

    for (const auto& action : actions) {
      if (auto error = validateAction(action)) {
        return std::unexpected(*error);
      }
    }

    return enqueueBulkValidated(actions);
  }

  /**
   * @brief Dequeue action (blocking)
   * @return Action or error
   */
  [[nodiscard]] QueueResult<ActionSlice> dequeue() noexcept {
    auto result = buffer_.get();
    if (!result) {
      return std::unexpected(toQueueError(result.error()));
    }
    return *result;
  }

  /**
   * @brief Try to dequeue action (non-blocking)
   * @return Action or QueueError::Empty
   */
  [[nodiscard]] QueueResult<ActionSlice> tryDequeue() noexcept {
    auto result = buffer_.tryGet();
    if (!result) {
      return std::unexpected(toQueueError(result.error()));
    }
    return *result;
  }

  /**
   * @brief Try to dequeue with timeout
   * @param timeout Maximum wait duration
   * @return Action, QueueError::Empty, or QueueError::Timeout
   */
  template <Duration D>
  [[nodiscard]] QueueResult<ActionSlice> tryDequeueFor(D timeout) noexcept {
    auto result = buffer_.tryGetFor(timeout);
    if (!result) {
      return std::unexpected(toQueueError(result.error()));
    }
    return *result;
  }

  //============================================================================
  // Control Operations
  //============================================================================

  /**
   * @brief Initiate graceful shutdown
   *
   * After shutdown:
   * - All enqueue operations return QueueError::Shutdown
   * - Pending actions remain in queue
   * - Dequeue operations continue until queue empty
   */
  void shutdown() noexcept {
    shutdownFlag_.store(true, std::memory_order_release);
  }

  /**
   * @brief Check if queue is shutdown
   */
  [[nodiscard]] bool isShutdown() const noexcept {
    return shutdownFlag_.load(std::memory_order_acquire);
  }

  //============================================================================
  // Status Queries
  //============================================================================

  /**
   * @brief Get queue capacity
   */
  [[nodiscard]] size_type capacity() const noexcept {
    return queueCapacity_;
  }

  /**
   * @brief Get number of environments
   */
  [[nodiscard]] size_type numEnvironments() const noexcept {
    return numEnvironments_;
  }

  /**
   * @brief Get approximate queue size (may be stale)
   */
  [[nodiscard]] size_type sizeApproximate() const noexcept {
    return buffer_.sizeApproximate();
  }

 private:
  //============================================================================
  // Validation Helpers
  //============================================================================

  /**
   * @brief Validate action metadata
   * @return Error if invalid, nullopt if valid
   */
  [[nodiscard]] Optional<QueueError> validateAction(
      const ActionSlice& action) const noexcept {

    if (!isValidEnvironmentId(action.environmentId)) {
      return QueueError::InvalidId;
    }

    return std::nullopt;
  }

  /**
   * @brief Check if environment ID is valid
   */
  [[nodiscard]] bool isValidEnvironmentId(int envId) const noexcept {
    return envId >= 0 &&
           static_cast<size_type>(envId) < numEnvironments_;
  }

  //============================================================================
  // Enqueue Helpers
  //============================================================================

  /**
   * @brief Enqueue pre-validated action
   */
  [[nodiscard]] VoidQueueResult enqueueValidated(ActionSlice action) noexcept {
    auto result = buffer_.put(move(action));
    if (!result) {
      return std::unexpected(toQueueError(result.error()));
    }
    return {};
  }

  /**
   * @brief Enqueue pre-validated actions in bulk
   */
  [[nodiscard]] VoidQueueResult enqueueBulkValidated(
      Span<const ActionSlice> actions) noexcept {

    for (const auto& action : actions) {
      auto result = buffer_.put(action);
      if (!result) {
        return std::unexpected(toQueueError(result.error()));
      }
    }

    return {};
  }

  //============================================================================
  // Error Conversion
  //============================================================================

  /**
   * @brief Convert BufferError to QueueError
   */
  [[nodiscard]] static QueueError toQueueError(BufferError error) noexcept {
    switch (error) {
      case BufferError::Empty: return QueueError::Empty;
      case BufferError::Full: return QueueError::Full;
      case BufferError::Timeout: return QueueError::Timeout;
      case BufferError::Closed: return QueueError::Shutdown;
    }
    return QueueError::Empty;  // Unreachable
  }

  //============================================================================
  // Member Variables
  //============================================================================

  // Configuration (immutable)
  size_type numEnvironments_;
  size_type queueCapacity_;

  // State
  std::atomic<bool> shutdownFlag_{false};

  // Storage
  CircularBuffer<ActionSlice> buffer_;
};

}  // namespace envpool::async
