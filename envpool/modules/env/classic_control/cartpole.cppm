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
 * @module envpool.env.classic_control.cartpole
 * @brief CartPole balancing environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/classic_control/cartpole.py
 *
 * A pole is attached by an un-actuated joint to a cart, which moves along a frictionless track.
 * The pendulum starts upright, and the goal is to prevent it from falling over by increasing
 * and reducing the cart's velocity.
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

export module envpool.env.classic_control.cartpole;
import envpool.core.types;

export namespace envpool::classic_control {

using namespace envpool::core;

//==============================================================================
// CartPole Environment
//==============================================================================

/**
 * @struct CartPoleState
 * @brief Observation state for CartPole environment
 */
struct CartPoleState {
  float x;          // Cart position
  float x_dot;      // Cart velocity
  float theta;      // Pole angle (radians)
  float theta_dot;  // Pole angular velocity

  std::array<float, 4> toArray() const {
    return {x, x_dot, theta, theta_dot};
  }
};

/**
 * @struct CartPoleStepResult
 * @brief Result of a step in the environment
 */
struct CartPoleStepResult {
  CartPoleState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class CartPoleEnv
 * @brief CartPole balancing environment
 *
 * The system is controlled by applying a force of +10 or -10 to the cart.
 * The pendulum starts upright, and the goal is to prevent it from falling over.
 *
 * Observation Space:
 *   - x: Cart position (-2.4 to 2.4)
 *   - x_dot: Cart velocity (-inf to inf)
 *   - theta: Pole angle in radians (-0.209 to 0.209, ~12 degrees)
 *   - theta_dot: Pole angular velocity (-inf to inf)
 *
 * Action Space:
 *   - 0: Push cart to the left
 *   - 1: Push cart to the right
 *
 * Reward:
 *   - 1.0 for every step taken, including the termination step
 *
 * Episode Termination:
 *   - Pole angle is more than 12 degrees from vertical
 *   - Cart position is more than 2.4 units from center
 *   - Episode length is greater than max_episode_steps (default: 500)
 */
class CartPoleEnv {
 private:
  // Physics constants
  static constexpr double kGravity = 9.8;
  static constexpr double kMassCart = 1.0;
  static constexpr double kMassPole = 0.1;
  static constexpr double kMassTotal = kMassCart + kMassPole;
  static constexpr double kLength = 0.5;  // Half pole length
  static constexpr double kMassPoleLength = kMassPole * kLength;
  static constexpr double kForceMag = 10.0;
  static constexpr double kTau = 0.02;  // Time step (seconds)
  static constexpr double kThetaThresholdRadians = 12.0 * 2.0 * std::numbers::pi / 360.0;
  static constexpr double kXThreshold = 2.4;
  static constexpr double kInitRange = 0.05;

  // Environment state
  int envId_;
  int maxEpisodeSteps_;
  int elapsedStep_;
  double x_;
  double xDot_;
  double theta_;
  double thetaDot_;
  bool done_;

  // Random number generator
  std::mt19937 gen_;
  std::uniform_real_distribution<double> dist_;

 public:
  /**
   * @brief Construct CartPole environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 500)
   */
  explicit CartPoleEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 500)
      : envId_{envId},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        x_{0.0},
        xDot_{0.0},
        theta_{0.0},
        thetaDot_{0.0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        dist_{-kInitRange, kInitRange} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  CartPoleState reset() {
    x_ = dist_(gen_);
    xDot_ = dist_(gen_);
    theta_ = dist_(gen_);
    thetaDot_ = dist_(gen_);
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (left) or 1 (right)
   * @return Step result containing observation, reward, done, truncated
   */
  CartPoleStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Apply physics
    double force = (action == 1) ? kForceMag : -kForceMag;
    double costheta = std::cos(theta_);
    double sintheta = std::sin(theta_);

    double temp = (force + kMassPoleLength * thetaDot_ * thetaDot_ * sintheta) / kMassTotal;
    double thetaAcc = (kGravity * sintheta - costheta * temp) /
                      (kLength * (4.0 / 3.0 - kMassPole * costheta * costheta / kMassTotal));
    double xAcc = temp - kMassPoleLength * thetaAcc * costheta / kMassTotal;

    // Update state using Euler integration
    x_ += kTau * xDot_;
    xDot_ += kTau * xAcc;
    theta_ += kTau * thetaDot_;
    thetaDot_ += kTau * thetaAcc;

    // Check termination conditions
    bool outOfBounds = (x_ < -kXThreshold || x_ > kXThreshold ||
                        theta_ < -kThetaThresholdRadians || theta_ > kThetaThresholdRadians);
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);

    done_ = outOfBounds || maxStepsReached;

    CartPoleStepResult result;
    result.observation = getCurrentState();
    result.reward = 1.0f;  // Reward is always 1.0 per step
    result.done = done_;
    result.truncated = done_ && maxStepsReached && !outOfBounds;

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
   * @brief Get current step count
   */
  [[nodiscard]] int elapsedStep() const noexcept {
    return elapsedStep_;
  }

  /**
   * @brief Get maximum episode steps
   */
  [[nodiscard]] int maxEpisodeSteps() const noexcept {
    return maxEpisodeSteps_;
  }

  /**
   * @brief Set random seed
   */
  void setSeed(int seed) {
    gen_.seed(static_cast<unsigned int>(seed));
  }

 private:
  /**
   * @brief Get current state as CartPoleState
   */
  CartPoleState getCurrentState() const {
    return CartPoleState{
      static_cast<float>(x_),
      static_cast<float>(xDot_),
      static_cast<float>(theta_),
      static_cast<float>(thetaDot_)
    };
  }
};

}  // namespace envpool::classic_control
