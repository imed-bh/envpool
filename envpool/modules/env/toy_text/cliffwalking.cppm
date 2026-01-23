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
 * @module envpool.env.toy_text.cliffwalking
 * @brief Cliff Walking gridworld environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/toy_text/cliffwalking.py
 *
 * Navigate a 4x12 gridworld from start to goal while avoiding the cliff.
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

export module envpool.env.toy_text.cliffwalking;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// CliffWalking Environment
//==============================================================================

/**
 * @struct CliffWalkingState
 * @brief Observation state for CliffWalking environment
 */
struct CliffWalkingState {
  int position;  // Encoded position (x * 12 + y)

  int toInt() const {
    return position;
  }
};

/**
 * @struct CliffWalkingStepResult
 * @brief Result of a step in the environment
 */
struct CliffWalkingStepResult {
  CliffWalkingState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class CliffWalkingEnv
 * @brief Cliff Walking gridworld environment
 *
 * This is a 4x12 gridworld with a cliff in the bottom row.
 * The agent starts at (3,0) and must reach (3,11) without falling off the cliff.
 *
 * Observation Space:
 *   - Single integer encoding position: x * 12 + y (0-47)
 *
 * Action Space (discrete):
 *   - 0: Move up
 *   - 1: Move right
 *   - 2: Move down
 *   - 3: Move left
 *
 * Reward:
 *   - -1 for each step
 *   - -100 for falling off the cliff (and agent is reset to start)
 */
class CliffWalkingEnv {
 private:
  int envId_;
  int x_;  // Row position (0-3)
  int y_;  // Column position (0-11)
  bool done_;

 public:
  /**
   * @brief Construct CliffWalking environment
   * @param envId Environment ID
   */
  explicit CliffWalkingEnv(int envId = 0)
      : envId_{envId},
        x_{3},
        y_{0},
        done_{true} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  CliffWalkingState reset() {
    x_ = 3;
    y_ = 0;
    done_ = false;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (up), 1 (right), 2 (down), 3 (left)
   * @return Step result containing observation, reward, done, truncated
   */
  CliffWalkingStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    float reward = -1.0f;

    // Apply action
    if (action == 0) {      // Up
      --x_;
    } else if (action == 1) {  // Right
      ++y_;
    } else if (action == 2) {  // Down
      ++x_;
    } else {                // Left
      --y_;
    }

    // Clip to grid boundaries
    x_ = std::clamp(x_, 0, 3);
    y_ = std::clamp(y_, 0, 11);

    // Check if fell off cliff (bottom row, not start or goal)
    if (x_ == 3 && y_ > 0 && y_ < 11) {
      reward = -100.0f;
      x_ = 3;
      y_ = 0;
    }

    // Check if reached goal
    if (x_ == 3 && y_ == 11) {
      done_ = true;
    }

    CliffWalkingStepResult result;
    result.observation = getCurrentState();
    result.reward = reward;
    result.done = done_;
    result.truncated = false;

    return result;
  }

  /**
   * @brief Check if episode is done
   */
  [[nodiscard]] bool isDone() const noexcept {
    return done_;
  }

  /**
   * @brief Get environment ID
   */
  [[nodiscard]] int id() const noexcept {
    return envId_;
  }

 private:
  /**
   * @brief Get current state as CliffWalkingState
   */
  CliffWalkingState getCurrentState() const {
    return CliffWalkingState{x_ * 12 + y_};
  }
};

}  // namespace envpool::toy_text
