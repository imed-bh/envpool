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
 * @module envpool.core.errors
 * @brief Error types and utilities for EnvPool
 *
 * This module provides:
 * - Error enums for all components
 * - std::expected type aliases
 * - Error handling utilities
 */

module;

export module envpool.core.errors;

import std;

namespace envpool::core {

//==============================================================================
// Error Types
//==============================================================================

/**
 * @enum BufferError
 * @brief Errors that can occur in CircularBuffer operations
 */
export enum class BufferError {
  Empty,       ///< Buffer is empty (Get failed)
  Full,        ///< Buffer is full (Put failed)
  Timeout,     ///< Operation timed out
  Closed,      ///< Buffer has been closed
};

/**
 * @enum QueueError
 * @brief Errors that can occur in queue operations
 */
export enum class QueueError {
  Empty,       ///< Queue is empty
  Full,        ///< Queue is full
  Timeout,     ///< Operation timed out
  Shutdown,    ///< Queue has been shut down
  InvalidId,   ///< Invalid environment ID
};

/**
 * @enum StateBufferError
 * @brief Errors that can occur in StateBuffer operations
 */
export enum class StateBufferError {
  OutOfStorage,         ///< No storage available
  InvalidPlayerCount,   ///< Invalid number of players
  AllocationFailed,     ///< Allocation failed
  AlreadyCompleted,     ///< Operation already completed
};

/**
 * @enum EnvPoolError
 * @brief Errors that can occur in AsyncEnvPool operations
 */
export enum class EnvPoolError {
  SendFailed,           ///< Failed to send action
  RecvFailed,           ///< Failed to receive state
  InvalidAction,        ///< Invalid action
  InvalidEnvId,         ///< Invalid environment ID
  NotInitialized,       ///< Pool not initialized
  AlreadyShutdown,      ///< Pool already shut down
};

//==============================================================================
// Error Formatting
//==============================================================================

/**
 * @brief Convert BufferError to string
 */
export [[nodiscard]] constexpr std::string_view toString(BufferError error) noexcept {
  switch (error) {
    case BufferError::Empty: return "Buffer is empty";
    case BufferError::Full: return "Buffer is full";
    case BufferError::Timeout: return "Operation timed out";
    case BufferError::Closed: return "Buffer is closed";
  }
  return "Unknown buffer error";
}

/**
 * @brief Convert QueueError to string
 */
export [[nodiscard]] constexpr std::string_view toString(QueueError error) noexcept {
  switch (error) {
    case QueueError::Empty: return "Queue is empty";
    case QueueError::Full: return "Queue is full";
    case QueueError::Timeout: return "Operation timed out";
    case QueueError::Shutdown: return "Queue is shutdown";
    case QueueError::InvalidId: return "Invalid environment ID";
  }
  return "Unknown queue error";
}

/**
 * @brief Convert StateBufferError to string
 */
export [[nodiscard]] constexpr std::string_view toString(StateBufferError error) noexcept {
  switch (error) {
    case StateBufferError::OutOfStorage: return "Out of storage";
    case StateBufferError::InvalidPlayerCount: return "Invalid player count";
    case StateBufferError::AllocationFailed: return "Allocation failed";
    case StateBufferError::AlreadyCompleted: return "Already completed";
  }
  return "Unknown state buffer error";
}

/**
 * @brief Convert EnvPoolError to string
 */
export [[nodiscard]] constexpr std::string_view toString(EnvPoolError error) noexcept {
  switch (error) {
    case EnvPoolError::SendFailed: return "Send failed";
    case EnvPoolError::RecvFailed: return "Receive failed";
    case EnvPoolError::InvalidAction: return "Invalid action";
    case EnvPoolError::InvalidEnvId: return "Invalid environment ID";
    case EnvPoolError::NotInitialized: return "Not initialized";
    case EnvPoolError::AlreadyShutdown: return "Already shutdown";
  }
  return "Unknown envpool error";
}

//==============================================================================
// Expected Type Aliases
//==============================================================================

/**
 * @brief Result type for operations that return T or BufferError
 */
export template <typename T>
using BufferResult = std::expected<T, BufferError>;

/**
 * @brief Result type for void operations that can fail with BufferError
 */
export using VoidBufferResult = std::expected<void, BufferError>;

/**
 * @brief Result type for operations that return T or QueueError
 */
export template <typename T>
using QueueResult = std::expected<T, QueueError>;

/**
 * @brief Result type for void operations that can fail with QueueError
 */
export using VoidQueueResult = std::expected<void, QueueError>;

/**
 * @brief Result type for operations that return T or StateBufferError
 */
export template <typename T>
using StateBufferResult = std::expected<T, StateBufferError>;

/**
 * @brief Result type for void operations that can fail with StateBufferError
 */
export using VoidStateBufferResult = std::expected<void, StateBufferError>;

/**
 * @brief Result type for operations that return T or EnvPoolError
 */
export template <typename T>
using EnvPoolResult = std::expected<T, EnvPoolError>;

/**
 * @brief Result type for void operations that can fail with EnvPoolError
 */
export using VoidEnvPoolResult = std::expected<void, EnvPoolError>;

//==============================================================================
// Utility Functions
//==============================================================================

/**
 * @brief Create a success result
 */
export template <typename T>
[[nodiscard]] constexpr auto makeSuccess(T&& value) {
  return std::expected<std::remove_cvref_t<T>, void>{std::forward<T>(value)};
}

/**
 * @brief Create an error result
 */
export template <typename E>
[[nodiscard]] constexpr auto makeError(E error) {
  return std::unexpected{error};
}

/**
 * @brief Check if result is success and execute function
 * @example
 *   auto result = doSomething();
 *   return onSuccess(result, [](auto& value) {
 *     // Process value
 *     return processedValue;
 *   });
 */
export template <typename T, typename E, typename F>
[[nodiscard]] auto onSuccess(std::expected<T, E>& result, F&& func) {
  if (result.has_value()) {
    return std::invoke(std::forward<F>(func), *result);
  }
  return result.error();
}

/**
 * @brief Check if result is error and execute function
 * @example
 *   auto result = doSomething();
 *   onError(result, [](auto error) {
 *     // Handle error
 *     logError(error);
 *   });
 */
export template <typename T, typename E, typename F>
void onError(const std::expected<T, E>& result, F&& func) {
  if (!result.has_value()) {
    std::invoke(std::forward<F>(func), result.error());
  }
}

}  // namespace envpool::core
