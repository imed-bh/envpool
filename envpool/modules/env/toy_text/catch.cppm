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
 * @module envpool.env.toy_text.catch
 * @brief Catch falling object game environment
 *
 * Based on: https://github.com/deepmind/bsuite/blob/master/bsuite/environments/catch.py
 *
 * A simple game where a paddle must catch a falling ball.
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

export module envpool.env.toy_text.catch;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// Catch Environment
//==============================================================================

/**
 * @struct CatchState
 * @brief Observation state for Catch environment (grid representation)
 */
struct CatchState {
  std::vector<float> grid;  // Height x Width grid
  int height;
  int width;

  std::vector<float> toVector() const {
    return grid;
  }
};

/**
 * @struct CatchStepResult
 * @brief Result of a step in the environment
 */
struct CatchStepResult {
  CatchState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class CatchEnv
 * @brief Catch falling object game environment
 *
 * The goal is to move a paddle at the bottom to catch a falling ball.
 *
 * Observation Space:
 *   - 2D grid (height x width) with 1.0 marking ball and paddle positions
 *
 * Action Space (discrete):
 *   - 0: Move paddle left
 *   - 1: Stay in place
 *   - 2: Move paddle right
 *
 * Reward:
 *   - +1.0 for catching the ball
 *   - -1.0 for missing the ball
 *   - 0.0 otherwise
 */
class CatchEnv {
 private:
  int envId_;
  int ballX_;       // Ball row (vertical position)
  int ballY_;       // Ball column (horizontal position)
  int paddle_;      // Paddle column position
  int height_;
  int width_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_int_distribution<int> yDist_;

 public:
  /**
   * @brief Construct Catch environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param height Grid height (default: 10)
   * @param width Grid width (default: 5)
   */
  explicit CatchEnv(int envId = 0, int seed = 42, int height = 10, int width = 5)
      : envId_{envId},
        ballX_{0},
        ballY_{0},
        paddle_{0},
        height_{height},
        width_{width},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        yDist_{0, width - 1} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  CatchState reset() {
    ballX_ = 0;
    ballY_ = yDist_(gen_);
    paddle_ = width_ / 2;
    done_ = false;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (left), 1 (stay), 2 (right)
   * @return Step result containing observation, reward, done, truncated
   */
  CatchStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    // Move paddle
    paddle_ += (action - 1);
    paddle_ = std::clamp(paddle_, 0, width_ - 1);

    // Move ball down
    ++ballX_;

    float reward = 0.0f;

    // Check if ball reached bottom
    if (ballX_ == height_ - 1) {
      done_ = true;
      reward = (ballY_ == paddle_) ? 1.0f : -1.0f;
    }

    CatchStepResult result;
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

  /**
   * @brief Set random seed
   */
  void setSeed(int seed) {
    gen_.seed(static_cast<unsigned int>(seed));
  }

 private:
  /**
   * @brief Get current state as CatchState (2D grid)
   */
  CatchState getCurrentState() const {
    std::vector<float> grid(height_ * width_, 0.0f);

    // Mark ball position
    grid[ballX_ * width_ + ballY_] = 1.0f;

    // Mark paddle position
    grid[(height_ - 1) * width_ + paddle_] = 1.0f;

    return CatchState{grid, height_, width_};
  }
};

}  // namespace envpool::toy_text
