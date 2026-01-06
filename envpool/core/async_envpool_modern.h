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

#ifndef ENVPOOL_CORE_ASYNC_ENVPOOL_MODERN_H_
#define ENVPOOL_CORE_ASYNC_ENVPOOL_MODERN_H_

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <memory>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "envpool/core/action_buffer_queue_modern.h"
#include "envpool/core/array.h"
#include "envpool/core/envpool.h"
#include "envpool/core/spec.h"
#include "envpool/core/state_buffer_queue.h"

namespace envpool {
namespace modern {

/**
 * @brief Error codes for AsyncEnvPool operations
 */
enum class EnvPoolError {
  InvalidAction,
  QueueFull,
  Timeout,
  Shutdown,
  ConfigError
};

/**
 * @brief Environment concept
 *
 * Defines requirements for environment types used with AsyncEnvPool.
 */
template <typename E>
concept Environment = requires(E env) {
  typename E::Spec;
  typename E::Action;
  typename E::State;
  { env.IsDone() } -> std::same_as<bool>;
};

/**
 * @brief Modern C++26 Async Environment Pool
 *
 * Key improvements over original:
 * - std::jthread for RAII thread management
 * - std::stop_token for cooperative cancellation
 * - std::expected for error handling
 * - Modern ActionBufferQueue integration
 * - Explicit memory ordering
 * - Better const-correctness
 * - Comprehensive error handling
 *
 * Architecture:
 * ```
 * User Thread: Send(actions) → ActionBufferQueue
 *                                      ↓
 * Worker Threads (N): Dequeue → Execute Env → StateBufferQueue
 *                                      ↓
 * User Thread: Recv() ← StateBufferQueue ← Results
 * ```
 *
 * Thread Safety:
 * - Send(): Thread-safe (internally synchronized)
 * - Recv(): Single-threaded (must be called from same thread)
 * - Reset(): Thread-safe
 *
 * @tparam Env Environment type (must satisfy Environment concept)
 */
template <Environment Env>
class AsyncEnvPool : public EnvPool<typename Env::Spec> {
 public:
  using Spec = typename Env::Spec;
  using Action = typename Env::Action;
  using State = typename Env::State;
  using ActionSlice = typename ActionBufferQueue::ActionSlice;

 private:
  // Configuration
  const std::size_t num_envs_;
  const std::size_t batch_size_;
  const std::size_t max_num_players_;
  const std::size_t num_threads_;
  const bool is_sync_;

  // Environment instances
  std::vector<std::unique_ptr<Env>> envs_;

  // Communication queues
  std::unique_ptr<ActionBufferQueue> action_queue_;
  std::unique_ptr<StateBufferQueue> state_queue_;

  // Worker threads (RAII with std::jthread)
  std::vector<std::jthread> workers_;

  // Synchronization for sync mode
  std::atomic<std::size_t> stepping_env_num_{0};

  // Performance metrics (optional)
  std::atomic<uint64_t> total_steps_{0};
  std::chrono::steady_clock::time_point start_time_;

 public:
  /**
   * @brief Construct AsyncEnvPool
   *
   * Initializes all environments in parallel and spawns worker threads.
   *
   * @param spec Environment specification
   * @throws std::runtime_error if configuration is invalid
   */
  explicit AsyncEnvPool(const Spec& spec)
      : EnvPool<Spec>(spec),
        num_envs_(spec.config["num_envs"_]),
        batch_size_(spec.config["batch_size"_] <= 0
                        ? num_envs_
                        : spec.config["batch_size"_]),
        max_num_players_(spec.config["max_num_players"_]),
        num_threads_(spec.config["num_threads"_] == 0
                        ? std::min(batch_size_,
                                  std::thread::hardware_concurrency())
                        : spec.config["num_threads"_]),
        is_sync_(batch_size_ == num_envs_ && max_num_players_ == 1),
        envs_(num_envs_),
        action_queue_(std::make_unique<ActionBufferQueue>(num_envs_)),
        state_queue_(std::make_unique<StateBufferQueue>(
            batch_size_, num_envs_, max_num_players_,
            spec.state_spec.template AllValues<ShapeSpec>())),
        start_time_(std::chrono::steady_clock::now()) {

    // Validate configuration
    if (num_envs_ == 0) {
      throw std::runtime_error("num_envs must be positive");
    }
    if (batch_size_ > num_envs_) {
      throw std::runtime_error("batch_size cannot exceed num_envs");
    }
    if (max_num_players_ == 0) {
      throw std::runtime_error("max_num_players must be positive");
    }

    // Initialize environments in parallel
    InitializeEnvironments(spec);

    // Spawn worker threads
    SpawnWorkers(spec);

    LOG(INFO) << "AsyncEnvPool initialized: " << num_envs_ << " envs, "
              << num_threads_ << " threads, batch_size=" << batch_size_
              << ", sync=" << is_sync_;
  }

  /**
   * @brief Destructor - RAII cleanup
   *
   * std::jthread automatically:
   * 1. Requests stop via stop_token
   * 2. Joins all threads
   *
   * No manual cleanup needed!
   */
  ~AsyncEnvPool() override {
    // Signal shutdown to action queue
    action_queue_->Shutdown();

    // std::jthread destructor will:
    // - Request stop on all threads (via stop_token)
    // - Join all threads automatically
    //
    // Workers will exit their loops when stop is requested

    LOG(INFO) << "AsyncEnvPool destroyed. Total steps: " << total_steps_.load()
              << ", Runtime: " << GetRuntimeSeconds() << "s";
  }

  // Non-copyable, non-movable
  AsyncEnvPool(const AsyncEnvPool&) = delete;
  AsyncEnvPool& operator=(const AsyncEnvPool&) = delete;
  AsyncEnvPool(AsyncEnvPool&&) = delete;
  AsyncEnvPool& operator=(AsyncEnvPool&&) = delete;

  /**
   * @brief Send actions to environments
   *
   * Actions are enqueued to the action buffer queue and will be
   * processed asynchronously by worker threads.
   *
   * @param action Action dictionary containing env_ids and action data
   * @return std::expected<void, EnvPoolError> Success or error
   */
  [[nodiscard]] std::expected<void, EnvPoolError> Send(
      const Action& action) noexcept {
    return SendImpl(action.template AllValues<Array>());
  }

  /**
   * @brief Send actions (array vector overload)
   */
  [[nodiscard]] std::expected<void, EnvPoolError> Send(
      const std::vector<Array>& action) noexcept {
    return SendImpl(action);
  }

  /**
   * @brief Send actions (move semantics)
   */
  [[nodiscard]] std::expected<void, EnvPoolError> Send(
      std::vector<Array>&& action) noexcept {
    return SendImpl(std::move(action));
  }

  /**
   * @brief Receive batch of states (blocking)
   *
   * Blocks until batch_size environments have completed.
   * In sync mode, ensures deterministic ordering.
   *
   * @return std::expected<std::vector<Array>, EnvPoolError> States or error
   */
  [[nodiscard]] std::expected<std::vector<Array>, EnvPoolError> Recv() noexcept {
    int additional_wait = 0;

    if (is_sync_) {
      auto current = stepping_env_num_.load(std::memory_order_acquire);
      if (current < batch_size_) {
        additional_wait = batch_size_ - current;
      }
    }

    auto result = state_queue_->Wait(additional_wait);

    if (is_sync_) {
      stepping_env_num_.fetch_sub(result[0].Shape(0), std::memory_order_release);
    }

    total_steps_.fetch_add(result[0].Shape(0), std::memory_order_relaxed);

    return result;
  }

  /**
   * @brief Try to receive states with timeout
   *
   * @param timeout Maximum time to wait
   * @return std::expected<std::vector<Array>, EnvPoolError> States or timeout error
   */
  template <typename Rep, typename Period>
  [[nodiscard]] std::expected<std::vector<Array>, EnvPoolError> TryRecvFor(
      const std::chrono::duration<Rep, Period>& timeout) noexcept {
    // TODO: Implement timeout support in StateBufferQueue
    // For now, just call regular Recv
    return Recv();
  }

  /**
   * @brief Force reset specific environments
   *
   * @param env_ids Array of environment IDs to reset
   * @return std::expected<void, EnvPoolError> Success or error
   */
  [[nodiscard]] std::expected<void, EnvPoolError> Reset(
      const Array& env_ids) noexcept {
    TArray<int> tenv_ids(env_ids);
    int count = tenv_ids.Shape(0);

    std::vector<ActionSlice> actions(count);
    for (int i = 0; i < count; ++i) {
      actions[i] = ActionSlice{
          .env_id = tenv_ids[i],
          .order = is_sync_ ? i : -1,
          .force_reset = true};
    }

    if (is_sync_) {
      stepping_env_num_.fetch_add(count, std::memory_order_release);
    }

    if (auto result = action_queue_->EnqueueBulk(actions); !result) {
      return std::unexpected(EnvPoolError::QueueFull);
    }

    return {};
  }

  /**
   * @brief Get performance statistics
   */
  struct Stats {
    std::size_t num_envs;
    std::size_t num_threads;
    std::size_t batch_size;
    uint64_t total_steps;
    double runtime_seconds;
    double steps_per_second;
  };

  [[nodiscard]] Stats GetStats() const noexcept {
    auto total = total_steps_.load(std::memory_order_relaxed);
    auto runtime = GetRuntimeSeconds();
    return Stats{
        .num_envs = num_envs_,
        .num_threads = num_threads_,
        .batch_size = batch_size_,
        .total_steps = total,
        .runtime_seconds = runtime,
        .steps_per_second = runtime > 0 ? total / runtime : 0.0};
  }

 private:
  /**
   * @brief Initialize all environment instances in parallel
   */
  void InitializeEnvironments(const Spec& spec) {
    std::size_t init_threads = std::min(
        std::thread::hardware_concurrency(), num_envs_);

    std::vector<std::jthread> init_workers;
    init_workers.reserve(init_threads);

    std::atomic<std::size_t> next_env{0};

    // Spawn initialization threads
    for (std::size_t i = 0; i < init_threads; ++i) {
      init_workers.emplace_back([this, &spec, &next_env](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
          std::size_t env_id = next_env.fetch_add(1, std::memory_order_relaxed);
          if (env_id >= num_envs_) break;

          envs_[env_id] = std::make_unique<Env>(spec, env_id);
        }
      });
    }

    // std::jthread destructor automatically joins
    // No manual join needed!
  }

  /**
   * @brief Spawn worker threads with std::jthread
   */
  void SpawnWorkers(const Spec& spec) {
    workers_.reserve(num_threads_);

    for (std::size_t i = 0; i < num_threads_; ++i) {
      workers_.emplace_back([this, i](std::stop_token stoken) {
        WorkerLoop(i, stoken);
      });

      // Set thread affinity if configured
      if (spec.config["thread_affinity_offset"_] >= 0) {
        SetThreadAffinity(workers_.back(), i,
                         spec.config["thread_affinity_offset"_]);
      }
    }
  }

  /**
   * @brief Main worker loop
   *
   * Uses std::stop_token for cooperative cancellation.
   */
  void WorkerLoop(std::size_t worker_id, std::stop_token stoken) noexcept {
    using namespace std::chrono_literals;

    while (!stoken.stop_requested()) {
      // Try to dequeue with timeout to check stop_token periodically
      auto action_result = action_queue_->TryDequeueFor(100ms);

      if (!action_result) {
        if (action_result.error() == QueueError::Shutdown) {
          break;  // Graceful shutdown
        }
        if (action_result.error() == QueueError::Timeout) {
          continue;  // Check stop_token and retry
        }
        // Other errors: log and continue
        DLOG(WARNING) << "Worker " << worker_id << " dequeue error";
        continue;
      }

      ActionSlice action = *action_result;

      // Execute environment step
      int env_id = action.env_id;
      int order = action.order;
      bool reset = action.force_reset || envs_[env_id]->IsDone();

      try {
        envs_[env_id]->EnvStep(state_queue_.get(), order, reset);
      } catch (const std::exception& e) {
        LOG(ERROR) << "Worker " << worker_id << " exception in env "
                   << env_id << ": " << e.what();
      }
    }

    DLOG(INFO) << "Worker " << worker_id << " exiting gracefully";
  }

  /**
   * @brief Send implementation (template to handle different arg types)
   */
  template <typename V>
  [[nodiscard]] std::expected<void, EnvPoolError> SendImpl(V&& action) noexcept {
    // Extract env_ids
    int* env_id = static_cast<int*>(action[0].Data());
    int count = action[0].Shape(0);

    if (count == 0) {
      return {};  // No-op
    }

    // Create action slices
    std::vector<ActionSlice> actions;
    actions.reserve(count);

    auto action_batch = std::make_shared<std::vector<Array>>(
        std::forward<V>(action));

    for (int i = 0; i < count; ++i) {
      int eid = env_id[i];

      // Validate env_id
      if (eid < 0 || static_cast<std::size_t>(eid) >= num_envs_) {
        return std::unexpected(EnvPoolError::InvalidAction);
      }

      envs_[eid]->SetAction(action_batch, i);
      actions.emplace_back(ActionSlice{
          .env_id = eid,
          .order = is_sync_ ? i : -1,
          .force_reset = false});
    }

    if (is_sync_) {
      stepping_env_num_.fetch_add(count, std::memory_order_release);
    }

    // Enqueue bulk
    if (auto result = action_queue_->EnqueueBulk(actions); !result) {
      if (result.error() == QueueError::Shutdown) {
        return std::unexpected(EnvPoolError::Shutdown);
      }
      return std::unexpected(EnvPoolError::QueueFull);
    }

    return {};
  }

  /**
   * @brief Set CPU affinity for thread
   */
  void SetThreadAffinity(std::jthread& thread, std::size_t thread_id,
                        int offset) noexcept {
#if defined(__linux__)
    std::size_t num_cpus = std::thread::hardware_concurrency();
    std::size_t cpu_id = (offset + thread_id) % num_cpus;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
    DLOG(INFO) << "Thread " << thread_id << " pinned to CPU " << cpu_id;
#else
    // Thread affinity not supported on this platform
    (void)thread;
    (void)thread_id;
    (void)offset;
#endif
  }

  /**
   * @brief Get runtime in seconds
   */
  [[nodiscard]] double GetRuntimeSeconds() const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_);
    return duration.count() / 1000.0;
  }
};

}  // namespace modern
}  // namespace envpool

#endif  // ENVPOOL_CORE_ASYNC_ENVPOOL_MODERN_H_
