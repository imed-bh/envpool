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
 * @module envpool.async.state.buffer
 * @brief Batched state storage with RAII automatic completion
 *
 * Clean code principles:
 * - RAII WritableSlice (automatic done_write)
 * - Small helper functions
 * - Clear separation of concerns
 * - Explicit validation
 */

module;

export module envpool.async.state.buffer;

import std;
import envpool.core.types;
import envpool.core.errors;

namespace envpool::async {

using namespace envpool::core;

//==============================================================================
// WritableSlice - RAII wrapper for state data
//==============================================================================

/**
 * @class WritableSlice
 * @brief RAII wrapper that automatically calls completion callback
 *
 * Key feature: Destructor calls done_write automatically
 * This prevents forgotten callbacks and ensures proper synchronization
 */
export class WritableSlice {
 public:
  /**
   * @brief Construct writable slice
   * @param completionCallback Called on destruction or explicit complete()
   */
  explicit WritableSlice(MoveOnlyFunction<void()> completionCallback) noexcept
      : completionCallback_{move(completionCallback)} {}

  // Non-copyable
  WritableSlice(const WritableSlice&) = delete;
  WritableSlice& operator=(const WritableSlice&) = delete;

  // Movable
  WritableSlice(WritableSlice&& other) noexcept
      : completionCallback_{move(other.completionCallback_)},
        completed_{exchange(other.completed_, true)} {}

  WritableSlice& operator=(WritableSlice&& other) noexcept {
    if (this != &other) {
      completeIfNeeded();
      completionCallback_ = move(other.completionCallback_);
      completed_ = exchange(other.completed_, true);
    }
    return *this;
  }

  /**
   * @brief Destructor - automatically calls completion
   */
  ~WritableSlice() {
    completeIfNeeded();
  }

  /**
   * @brief Explicitly mark as complete (optional)
   *
   * Useful when you want to signal completion before destruction.
   * Safe to call multiple times.
   */
  void complete() noexcept {
    completeIfNeeded();
  }

  /**
   * @brief Check if already completed
   */
  [[nodiscard]] bool isCompleted() const noexcept {
    return completed_;
  }

 private:
  /**
   * @brief Call completion callback if not yet called
   */
  void completeIfNeeded() noexcept {
    if (!completed_ && completionCallback_) {
      completionCallback_();
      completed_ = true;
    }
  }

  MoveOnlyFunction<void()> completionCallback_;
  bool completed_{false};
};

//==============================================================================
// StateBuffer - Batched state storage
//==============================================================================

/**
 * @class StateBuffer
 * @brief Collects states from multiple players into a batch
 *
 * Features:
 * - Lock-free allocation
 * - Batching for efficiency
 * - RAII WritableSlice
 * - Explicit validation
 */
export class StateBuffer {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct state buffer
   * @param batchSize Number of states per batch
   * @param maxPlayersPerEnv Maximum players per environment
   */
  StateBuffer(size_type batchSize, size_type maxPlayersPerEnv)
      : batchSize_{batchSize},
        maxPlayersPerEnv_{maxPlayersPerEnv},
        totalCapacity_{batchSize * maxPlayersPerEnv} {

    ensureValidConfiguration();
  }

  // Non-copyable, movable
  StateBuffer(const StateBuffer&) = delete;
  StateBuffer& operator=(const StateBuffer&) = delete;
  StateBuffer(StateBuffer&&) noexcept = default;
  StateBuffer& operator=(StateBuffer&&) noexcept = default;

  //============================================================================
  // Allocation
  //============================================================================

  /**
   * @brief Allocate space for state
   * @param numPlayers Number of players in this state
   * @param order Execution order (-1 for unordered)
   * @return WritableSlice with RAII completion or error
   */
  [[nodiscard]] StateBufferResult<WritableSlice> allocate(
      size_type numPlayers,
      int order = -1) noexcept {

    if (auto error = validateAllocation(numPlayers)) {
      return std::unexpected(*error);
    }

    return allocateValidated(numPlayers, order);
  }

  //============================================================================
  // Batch Completion
  //============================================================================

  /**
   * @brief Wait for batch to complete
   * @param additionalStates Additional states needed beyond batch size
   * @return Completed when batch ready
   */
  void waitForBatch(size_type additionalStates = 0) noexcept {
    const size_type targetCount = batchSize_ + additionalStates;
    waitUntilCount(targetCount);
  }

  /**
   * @brief Check if batch is complete (approximate)
   */
  [[nodiscard]] bool isBatchComplete() const noexcept {
    return completedCount() >= batchSize_;
  }

  //============================================================================
  // Status Queries
  //============================================================================

  /**
   * @brief Get batch size
   */
  [[nodiscard]] size_type batchSize() const noexcept {
    return batchSize_;
  }

  /**
   * @brief Get maximum players per environment
   */
  [[nodiscard]] size_type maxPlayersPerEnv() const noexcept {
    return maxPlayersPerEnv_;
  }

  /**
   * @brief Get current allocation count (approximate)
   */
  [[nodiscard]] size_type allocationCount() const noexcept {
    return allocationCount_.load(std::memory_order_relaxed);
  }

  /**
   * @brief Get current completion count (approximate)
   */
  [[nodiscard]] size_type completedCount() const noexcept {
    return completedCount_.load(std::memory_order_relaxed);
  }

 private:
  //============================================================================
  // Validation
  //============================================================================

  /**
   * @brief Ensure configuration is valid
   */
  void ensureValidConfiguration() const {
    if (batchSize_ == 0) {
      throw std::invalid_argument("Batch size must be > 0");
    }
    if (maxPlayersPerEnv_ == 0) {
      throw std::invalid_argument("Max players must be > 0");
    }
  }

  /**
   * @brief Validate allocation request
   */
  [[nodiscard]] Optional<StateBufferError> validateAllocation(
      size_type numPlayers) const noexcept {

    if (numPlayers == 0 || numPlayers > maxPlayersPerEnv_) {
      return StateBufferError::InvalidPlayerCount;
    }

    if (isOutOfStorage()) {
      return StateBufferError::OutOfStorage;
    }

    return std::nullopt;
  }

  /**
   * @brief Check if out of storage
   */
  [[nodiscard]] bool isOutOfStorage() const noexcept {
    return allocationCount() >= batchSize_;
  }

  //============================================================================
  // Allocation Implementation
  //============================================================================

  /**
   * @brief Allocate space (pre-validated)
   */
  [[nodiscard]] StateBufferResult<WritableSlice> allocateValidated(
      size_type numPlayers,
      int order) noexcept {

    const size_type slotIndex = acquireSlot();
    if (slotIndex >= batchSize_) {
      releaseSlot();
      return std::unexpected(StateBufferError::OutOfStorage);
    }

    return createWritableSlice(slotIndex, numPlayers, order);
  }

  /**
   * @brief Acquire allocation slot
   */
  [[nodiscard]] size_type acquireSlot() noexcept {
    return allocationCount_.fetch_add(1, std::memory_order_acquire);
  }

  /**
   * @brief Release allocation slot
   */
  void releaseSlot() noexcept {
    allocationCount_.fetch_sub(1, std::memory_order_release);
  }

  /**
   * @brief Create writable slice with RAII completion
   */
  [[nodiscard]] WritableSlice createWritableSlice(
      size_type slotIndex,
      size_type numPlayers,
      int order) noexcept {

    // Create completion callback that increments completed count
    auto completionCallback = [this]() noexcept {
      markSlotCompleted();
    };

    return WritableSlice{move(completionCallback)};
  }

  /**
   * @brief Mark slot as completed
   */
  void markSlotCompleted() noexcept {
    completedCount_.fetch_add(1, std::memory_order_release);
  }

  //============================================================================
  // Waiting
  //============================================================================

  /**
   * @brief Wait until specified number of states completed
   */
  void waitUntilCount(size_type targetCount) noexcept {
    while (completedCount() < targetCount) {
      std::this_thread::yield();
    }
  }

  //============================================================================
  // Member Variables
  //============================================================================

  // Configuration (immutable)
  size_type batchSize_;
  size_type maxPlayersPerEnv_;
  size_type totalCapacity_;

  // Allocation tracking (cache-line aligned)
  alignas(CacheLineSize) std::atomic<size_type> allocationCount_{0};
  alignas(CacheLineSize) std::atomic<size_type> completedCount_{0};
};

}  // namespace envpool::async
