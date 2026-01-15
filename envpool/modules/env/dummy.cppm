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
 * @module envpool.env.dummy
 * @brief Simple test environment for validation
 *
 * Clean code principles:
 * - Small, focused methods
 * - Clear state management
 * - Simple step logic
 * - Easy to understand and test
 */

module;

export module envpool.env.dummy;

import std;
import envpool.core.types;

namespace envpool::env {

using namespace envpool::core;

//==============================================================================
// DummyEnvironment - Simple test environment
//==============================================================================

/**
 * @class DummyEnvironment
 * @brief Minimal environment for testing async pool
 *
 * Features:
 * - Simple integer state
 * - Increments on step
 * - Resets to zero
 * - No complex logic (perfect for testing!)
 */
export class DummyEnvironment {
 public:
  //============================================================================
  // Construction
  //============================================================================

  /**
   * @brief Construct dummy environment
   * @param envId Environment ID
   * @param maxSteps Maximum steps before done (default: 100)
   */
  explicit DummyEnvironment(int envId, int maxSteps = 100)
      : envId_{envId},
        maxSteps_{maxSteps},
        currentStep_{0} {}

  //============================================================================
  // Environment Interface
  //============================================================================

  /**
   * @brief Reset environment to initial state
   */
  void reset() noexcept {
    currentStep_ = 0;
  }

  /**
   * @brief Execute one step
   */
  void step() noexcept {
    if (!isDone()) {
      incrementStep();
    }
  }

  /**
   * @brief Check if episode is done
   */
  [[nodiscard]] bool isDone() const noexcept {
    return currentStep_ >= maxSteps_;
  }

  //============================================================================
  // Accessors
  //============================================================================

  /**
   * @brief Get environment ID
   */
  [[nodiscard]] int id() const noexcept {
    return envId_;
  }

  /**
   * @brief Get current step count
   */
  [[nodiscard]] int currentStep() const noexcept {
    return currentStep_;
  }

  /**
   * @brief Get maximum steps
   */
  [[nodiscard]] int maxSteps() const noexcept {
    return maxSteps_;
  }

  /**
   * @brief Get simple observation (current step)
   */
  [[nodiscard]] int observation() const noexcept {
    return currentStep_;
  }

  /**
   * @brief Get reward (always 1.0 for dummy env)
   */
  [[nodiscard]] double reward() const noexcept {
    return isDone() ? 0.0 : 1.0;
  }

 private:
  //============================================================================
  // Implementation
  //============================================================================

  /**
   * @brief Increment step counter
   */
  void incrementStep() noexcept {
    ++currentStep_;
  }

  //============================================================================
  // Member Variables
  //============================================================================

  int envId_;
  int maxSteps_;
  int currentStep_;
};

}  // namespace envpool::env
