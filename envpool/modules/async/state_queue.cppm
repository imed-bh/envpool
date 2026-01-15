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
 * @module envpool.async.state.queue
 * @brief State buffer queue with background buffer creation
 *
 * Clean code principles:
 * - std::jthread for automatic thread management
 * - Small focused functions
 * - Clear buffer lifecycle
 * - Explicit error handling
 */

module;

export module envpool.async.state.queue;

import std;
import envpool.core.types;
import envpool.core.errors;
import envpool.async.buffer;
import envpool.async.state.buffer;

namespace envpool::async {

using namespace envpool::core;

//==============================================================================
// StateBufferQueue - Manages pool of state buffers
//==============================================================================

/**
 * @class StateBufferQueue
 * @brief Queue of state buffers with background creation threads
 *
 * Features:
 * - Background buffer creation with std::jthread
 * - Automatic buffer recycling
 * - Lock-free buffer exchange
 * - RAII cleanup (no manual thread management!)
 */
export class StateBufferQueue {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct state buffer queue
   * @param queueSize Number of buffers in circulation
   * @param batchSize States per buffer
   * @param maxPlayersPerEnv Maximum players per environment
   * @param numCreationThreads Background buffer creation threads
   */
  StateBufferQueue(
      size_type queueSize,
      size_type batchSize,
      size_type maxPlayersPerEnv,
      size_type numCreationThreads = 1)
      : queueSize_{queueSize},
        batchSize_{batchSize},
        maxPlayersPerEnv_{maxPlayersPerEnv},
        stockBuffer_{queueSize},
        activeBuffers_(queueSize) {

    initializeBuffers();
    spawnCreationThreads(numCreationThreads);
  }

  // Non-copyable, movable
  StateBufferQueue(const StateBufferQueue&) = delete;
  StateBufferQueue& operator=(const StateBufferQueue&) = delete;
  StateBufferQueue(StateBufferQueue&&) noexcept = default;
  StateBufferQueue& operator=(StateBufferQueue&&) noexcept = default;

  // Destructor automatically stops and joins all threads (std::jthread RAII!)
  ~StateBufferQueue() = default;

  //============================================================================
  // Buffer Operations
  //============================================================================

  /**
   * @brief Get current active buffer for writing
   * @return Reference to active buffer
   */
  [[nodiscard]] StateBuffer& currentBuffer() noexcept {
    const size_type index = currentBufferIndex();
    return *activeBuffers_[index];
  }

  /**
   * @brief Wait for current buffer to complete and rotate
   * @param additionalStates Additional states needed
   * @return Completed buffer ready for reading
   */
  [[nodiscard]] UniquePtr<StateBuffer> waitAndRotate(
      size_type additionalStates = 0) {

    auto& current = currentBuffer();
    current.waitForBatch(additionalStates);

    return exchangeBuffer();
  }

  //============================================================================
  // Status
  //============================================================================

  /**
   * @brief Get queue size
   */
  [[nodiscard]] size_type queueSize() const noexcept {
    return queueSize_;
  }

  /**
   * @brief Get batch size
   */
  [[nodiscard]] size_type batchSize() const noexcept {
    return batchSize_;
  }

  /**
   * @brief Get number of creation threads
   */
  [[nodiscard]] size_type numCreationThreads() const noexcept {
    return creationThreads_.size();
  }

 private:
  //============================================================================
  // Initialization
  //============================================================================

  /**
   * @brief Initialize buffer pool
   */
  void initializeBuffers() {
    for (size_type i = 0; i < queueSize_; ++i) {
      activeBuffers_[i] = createBuffer();
    }
  }

  /**
   * @brief Spawn background buffer creation threads
   * @param numThreads Number of threads to spawn
   */
  void spawnCreationThreads(size_type numThreads) {
    creationThreads_.reserve(numThreads);

    for (size_type i = 0; i < numThreads; ++i) {
      creationThreads_.emplace_back([this](std::stop_token stopToken) {
        runCreationLoop(stopToken);
      });
    }
  }

  //============================================================================
  // Buffer Creation
  //============================================================================

  /**
   * @brief Create new state buffer
   */
  [[nodiscard]] UniquePtr<StateBuffer> createBuffer() const {
    return std::make_unique<StateBuffer>(batchSize_, maxPlayersPerEnv_);
  }

  /**
   * @brief Background buffer creation loop
   *
   * Continuously creates buffers and puts them in stock until stopped.
   * std::stop_token provides cooperative cancellation.
   */
  void runCreationLoop(std::stop_token stopToken) noexcept {
    while (!stopToken.stop_requested()) {
      auto buffer = createBuffer();
      auto result = stockBuffer_.put(move(buffer));

      if (!result) {
        // Buffer full or closed, yield and retry
        std::this_thread::yield();
      }
    }
  }

  //============================================================================
  // Buffer Exchange
  //============================================================================

  /**
   * @brief Get current buffer index
   */
  [[nodiscard]] size_type currentBufferIndex() const noexcept {
    return bufferIndex_.load(std::memory_order_acquire) % queueSize_;
  }

  /**
   * @brief Advance to next buffer
   */
  void advanceBufferIndex() noexcept {
    bufferIndex_.fetch_add(1, std::memory_order_release);
  }

  /**
   * @brief Exchange current buffer with fresh one from stock
   */
  [[nodiscard]] UniquePtr<StateBuffer> exchangeBuffer() {
    // Get fresh buffer from stock
    auto freshBuffer = getBufferFromStock();

    // Swap with current active buffer
    const size_type index = currentBufferIndex();
    auto completedBuffer = move(activeBuffers_[index]);
    activeBuffers_[index] = move(freshBuffer);

    // Advance to next buffer slot
    advanceBufferIndex();

    return completedBuffer;
  }

  /**
   * @brief Get buffer from stock (blocking if empty)
   */
  [[nodiscard]] UniquePtr<StateBuffer> getBufferFromStock() {
    auto result = stockBuffer_.get();

    if (!result) {
      // This shouldn't happen with background creation,
      // but handle gracefully
      return createBuffer();
    }

    return move(*result);
  }

  //============================================================================
  // Member Variables
  //============================================================================

  // Configuration (immutable)
  size_type queueSize_;
  size_type batchSize_;
  size_type maxPlayersPerEnv_;

  // Buffer management
  CircularBuffer<UniquePtr<StateBuffer>> stockBuffer_;
  std::vector<UniquePtr<StateBuffer>> activeBuffers_;
  std::atomic<size_type> bufferIndex_{0};

  // Background threads (RAII: automatic stop and join!)
  std::vector<std::jthread> creationThreads_;
};

}  // namespace envpool::async
