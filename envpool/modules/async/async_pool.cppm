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
 * @module envpool.async.pool
 * @brief Main async environment pool orchestrating worker threads
 *
 * Clean code principles applied throughout:
 * - Small focused functions (<15 lines)
 * - std::jthread for automatic thread management
 * - std::stop_token for cooperative cancellation
 * - Clear separation of concerns
 * - RAII everywhere - zero manual cleanup!
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

export module envpool.async.pool;
import envpool.core.types;
import envpool.core.errors;
import envpool.async.action;
import envpool.async.state.queue;

namespace envpool::async {

using namespace envpool::core;

//==============================================================================
// Environment Concept
//==============================================================================

/**
 * @concept Environment
 * @brief Requirements for environment types
 */
export template <typename Env>
concept Environment = requires(Env env) {
  { env.reset() } -> std::same_as<void>;
  { env.step() } -> std::same_as<void>;
  { env.id() } -> std::convertible_to<int>;
};

//==============================================================================
// AsyncEnvPool - Main orchestration class
//==============================================================================

/**
 * @class AsyncEnvPool
 * @brief Async environment pool with worker threads
 *
 * Key features:
 * - std::jthread workers (automatic join on destruction!)
 * - std::stop_token for graceful shutdown
 * - Lock-free action/state queues
 * - Zero manual thread management
 * - Explicit error handling with std::expected
 *
 * @tparam Env Environment type (must satisfy Environment concept)
 */
export template <Environment Env>
class AsyncEnvPool {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct async environment pool
   * @param numEnvironments Number of environment instances
   * @param numWorkerThreads Number of worker threads
   * @param batchSize States per batch
   * @param maxPlayersPerEnv Maximum players per environment
   */
  AsyncEnvPool(
      size_type numEnvironments,
      size_type numWorkerThreads,
      size_type batchSize,
      size_type maxPlayersPerEnv = 1)
      : numEnvironments_{numEnvironments},
        numWorkerThreads_{numWorkerThreads},
        actionQueue_{numEnvironments},
        stateQueue_{batchSize, batchSize, maxPlayersPerEnv} {

    initializeEnvironments();
    spawnWorkerThreads();
  }

  // Non-copyable, movable
  AsyncEnvPool(const AsyncEnvPool&) = delete;
  AsyncEnvPool& operator=(const AsyncEnvPool&) = delete;
  AsyncEnvPool(AsyncEnvPool&&) noexcept = default;
  AsyncEnvPool& operator=(AsyncEnvPool&&) noexcept = default;

  /**
   * @brief Destructor - automatic cleanup via RAII!
   *
   * Shutdown sequence:
   * 1. Shutdown action queue (no new actions)
   * 2. std::jthread requests stop on all workers
   * 3. std::jthread automatically joins all workers
   * 4. All resources cleaned up automatically
   *
   * No manual cleanup needed!
   */
  ~AsyncEnvPool() {
    shutdown();
  }

  //============================================================================
  // Main Operations
  //============================================================================

  /**
   * @brief Send action to environment
   * @param action Action to execute
   * @return Success or error
   */
  [[nodiscard]] VoidEnvPoolResult send(const ActionSlice& action) noexcept {
    auto result = actionQueue_.enqueue(action);
    if (!result) {
      return std::unexpected(toEnvPoolError(result.error()));
    }
    return {};
  }

  /**
   * @brief Send multiple actions
   * @param actions Span of actions
   * @return Success or error
   */
  [[nodiscard]] VoidEnvPoolResult sendBulk(
      Span<const ActionSlice> actions) noexcept {

    auto result = actionQueue_.enqueueBulk(actions);
    if (!result) {
      return std::unexpected(toEnvPoolError(result.error()));
    }
    return {};
  }

  /**
   * @brief Receive completed states
   * @param additionalStates Additional states to wait for
   * @return Completed state buffer or error
   */
  [[nodiscard]] EnvPoolResult<UniquePtr<StateBuffer>> receive(
      size_type additionalStates = 0) noexcept {

    try {
      auto buffer = stateQueue_.waitAndRotate(additionalStates);
      return buffer;
    } catch (...) {
      return std::unexpected(EnvPoolError::RecvFailed);
    }
  }

  /**
   * @brief Reset all environments
   */
  void reset() noexcept {
    for (auto& env : environments_) {
      env.reset();
    }
  }

  //============================================================================
  // Control
  //============================================================================

  /**
   * @brief Initiate graceful shutdown
   *
   * After shutdown:
   * - Action queue stops accepting new actions
   * - Workers finish current actions then stop
   * - std::jthread automatically joins on destruction
   */
  void shutdown() noexcept {
    actionQueue_.shutdown();
    // std::jthread will handle the rest automatically!
  }

  /**
   * @brief Check if pool is shutdown
   */
  [[nodiscard]] bool isShutdown() const noexcept {
    return actionQueue_.isShutdown();
  }

  //============================================================================
  // Status
  //============================================================================

  /**
   * @brief Get number of environments
   */
  [[nodiscard]] size_type numEnvironments() const noexcept {
    return numEnvironments_;
  }

  /**
   * @brief Get number of worker threads
   */
  [[nodiscard]] size_type numWorkers() const noexcept {
    return workers_.size();
  }

 private:
  //============================================================================
  // Initialization
  //============================================================================

  /**
   * @brief Initialize all environment instances
   */
  void initializeEnvironments() {
    environments_.reserve(numEnvironments_);
    for (size_type i = 0; i < numEnvironments_; ++i) {
      environments_.emplace_back(static_cast<int>(i));
    }
  }

  /**
   * @brief Spawn all worker threads
   */
  void spawnWorkerThreads() {
    workers_.reserve(numWorkerThreads_);
    for (size_type i = 0; i < numWorkerThreads_; ++i) {
      workers_.emplace_back([this, id = i](std::stop_token stopToken) {
        runWorkerLoop(id, stopToken);
      });
    }
  }

  //============================================================================
  // Worker Thread Logic
  //============================================================================

  /**
   * @brief Main worker thread loop
   * @param workerId Worker thread ID
   * @param stopToken Cooperative cancellation token
   *
   * Worker loop:
   * 1. Try to dequeue action (with timeout)
   * 2. Check stop_token (cooperative cancellation)
   * 3. Execute action on environment
   * 4. Repeat until stopped
   */
  void runWorkerLoop(size_type workerId, std::stop_token stopToken) noexcept {
    using namespace std::chrono_literals;

    while (!stopToken.stop_requested()) {
      auto actionResult = tryDequeueAction(100ms);

      if (!actionResult) {
        handleDequeueError(actionResult.error(), stopToken);
        continue;
      }

      executeAction(*actionResult);
    }
  }

  /**
   * @brief Try to dequeue action with timeout
   */
  [[nodiscard]] QueueResult<ActionSlice> tryDequeueAction(
      std::chrono::milliseconds timeout) noexcept {

    return actionQueue_.tryDequeueFor(timeout);
  }

  /**
   * @brief Handle dequeue errors
   */
  void handleDequeueError(
      QueueError error,
      const std::stop_token& stopToken) noexcept {

    if (error == QueueError::Shutdown) {
      return;  // Normal shutdown
    }

    if (error == QueueError::Timeout) {
      // Check stop token and continue
      if (stopToken.stop_requested()) {
        return;
      }
    }

    // For other errors, yield and retry
    std::this_thread::yield();
  }

  /**
   * @brief Execute action on environment
   */
  void executeAction(const ActionSlice& action) noexcept {
    if (!isValidEnvironmentId(action.environmentId)) {
      return;  // Invalid ID, skip
    }

    auto& env = getEnvironment(action.environmentId);

    if (action.forceReset) {
      env.reset();
    } else {
      env.step();
    }
  }

  //============================================================================
  // Environment Access
  //============================================================================

  /**
   * @brief Check if environment ID is valid
   */
  [[nodiscard]] bool isValidEnvironmentId(int envId) const noexcept {
    return envId >= 0 &&
           static_cast<size_type>(envId) < numEnvironments_;
  }

  /**
   * @brief Get environment by ID
   */
  [[nodiscard]] Env& getEnvironment(int envId) noexcept {
    return environments_[static_cast<size_type>(envId)];
  }

  //============================================================================
  // Error Conversion
  //============================================================================

  /**
   * @brief Convert QueueError to EnvPoolError
   */
  [[nodiscard]] static EnvPoolError toEnvPoolError(QueueError error) noexcept {
    switch (error) {
      case QueueError::Empty: return EnvPoolError::RecvFailed;
      case QueueError::Full: return EnvPoolError::SendFailed;
      case QueueError::Timeout: return EnvPoolError::RecvFailed;
      case QueueError::Shutdown: return EnvPoolError::AlreadyShutdown;
      case QueueError::InvalidId: return EnvPoolError::InvalidEnvId;
    }
    return EnvPoolError::SendFailed;  // Unreachable
  }

  //============================================================================
  // Member Variables
  //============================================================================

  // Configuration (immutable)
  size_type numEnvironments_;
  size_type numWorkerThreads_;

  // Environment instances
  std::vector<Env> environments_;

  // Queues
  ActionBufferQueue actionQueue_;
  StateBufferQueue stateQueue_;

  // Worker threads (RAII: automatic stop and join!)
  std::vector<std::jthread> workers_;
};

}  // namespace envpool::async
