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
 * @module envpool.core.types
 * @brief Core type definitions and concepts for EnvPool
 *
 * This module provides:
 * - Fundamental concepts for type constraints
 * - Common type aliases
 * - Type utilities
 */

module;

export module envpool.core.types;

import std;


namespace envpool::core {

//==============================================================================
// Concepts
//==============================================================================

/**
 * @concept Movable
 * @brief Requires that a type is move-constructible and move-assignable
 */
export template <typename T>
concept Movable = std::movable<T>;

/**
 * @concept Copyable
 * @brief Requires that a type is copyable
 */
export template <typename T>
concept Copyable = std::copyable<T>;

/**
 * @concept Numeric
 * @brief Requires that a type is numeric (integral or floating point)
 */
export template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

/**
 * @concept Duration
 * @brief Requires that a type is a std::chrono::duration
 */
export template <typename T>
concept Duration = requires {
  typename T::rep;
  typename T::period;
} && requires(T d) {
  { d.count() } -> std::convertible_to<typename T::rep>;
};

/**
 * @concept Callable
 * @brief Requires that a type is callable with given arguments
 */
export template <typename F, typename... Args>
concept Callable = std::invocable<F, Args...>;

/**
 * @concept CallableReturning
 * @brief Requires that a callable returns a specific type
 */
export template <typename F, typename R, typename... Args>
concept CallableReturning =
    Callable<F, Args...> &&
    std::convertible_to<std::invoke_result_t<F, Args...>, R>;

//==============================================================================
// Type Aliases
//==============================================================================

/**
 * @brief Size type for containers and counts
 */
export using size_type = std::size_t;

/**
 * @brief Difference type for pointer arithmetic
 */
export using difference_type = std::ptrdiff_t;

/**
 * @brief Byte type for raw memory
 */
export using byte_type = std::byte;

/**
 * @brief Move-only function wrapper
 */
export template <typename Signature>
using MoveOnlyFunction = std::move_only_function<Signature>;

/**
 * @brief Unique pointer
 */
export template <typename T>
using UniquePtr = std::unique_ptr<T>;

/**
 * @brief Shared pointer
 */
export template <typename T>
using SharedPtr = std::shared_ptr<T>;

/**
 * @brief Span (non-owning view of contiguous sequence)
 */
export template <typename T, std::size_t Extent = std::dynamic_extent>
using Span = std::span<T, Extent>;

/**
 * @brief Optional value
 */
export template <typename T>
using Optional = std::optional<T>;

//==============================================================================
// Utility Functions
//==============================================================================

/**
 * @brief Forward a value (perfect forwarding)
 */
export template <typename T>
[[nodiscard]] constexpr decltype(auto) forward(T&& value) noexcept {
  return std::forward<T>(value);
}

/**
 * @brief Move a value
 */
export template <typename T>
[[nodiscard]] constexpr std::remove_reference_t<T>&& move(T&& value) noexcept {
  return std::move(value);
}

/**
 * @brief Exchange two values
 */
export template <typename T, typename U = T>
[[nodiscard]] constexpr T exchange(T& obj, U&& new_value) noexcept {
  return std::exchange(obj, std::forward<U>(new_value));
}

//==============================================================================
// Constants
//==============================================================================

/**
 * @brief Cache line size for alignment
 */
export inline constexpr size_type CacheLineSize = 64;

/**
 * @brief Dynamic extent for spans
 */
export inline constexpr size_type DynamicExtent = std::dynamic_extent;

}  // namespace envpool::core
