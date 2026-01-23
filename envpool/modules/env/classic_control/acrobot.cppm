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
 * @module envpool.env.classic_control.acrobot
 * @brief Two-link robot (Acrobot) swing-up environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/classic_control/acrobot.py
 *
 * The system consists of two links connected linearly to form a chain,
 * with one end of the chain fixed. The goal is to swing the free end above
 * a given height while starting from a hanging configuration.
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

export module envpool.env.classic_control.acrobot;
import envpool.core.types;

export namespace envpool::classic_control {

using namespace envpool::core;

//==============================================================================
// Acrobot Environment
//==============================================================================

/**
 * @struct AcrobotState
 * @brief Observation state for Acrobot environment
 */
struct AcrobotState {
  float cos_theta1;    // Cosine of first joint angle
  float sin_theta1;    // Sine of first joint angle
  float cos_theta2;    // Cosine of second joint angle
  float sin_theta2;    // Sine of second joint angle
  float theta1_dot;    // Angular velocity of first joint
  float theta2_dot;    // Angular velocity of second joint
  float theta1;        // First joint angle (for info)
  float theta2;        // Second joint angle (for info)

  std::array<float, 6> toArray() const {
    return {cos_theta1, sin_theta1, cos_theta2, sin_theta2, theta1_dot, theta2_dot};
  }
};

/**
 * @struct AcrobotStepResult
 * @brief Result of a step in the environment
 */
struct AcrobotStepResult {
  AcrobotState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class AcrobotEnv
 * @brief Two-link robot (Acrobot) swing-up environment
 *
 * The goal is to swing the end of the lower link up to a given height.
 *
 * Observation Space:
 *   - cos(theta1): Cosine of first joint angle
 *   - sin(theta1): Sine of first joint angle
 *   - cos(theta2): Cosine of second joint angle
 *   - sin(theta2): Sine of second joint angle
 *   - theta1_dot: Angular velocity of first joint (-4*pi to 4*pi)
 *   - theta2_dot: Angular velocity of second joint (-9*pi to 9*pi)
 *
 * Action Space (discrete):
 *   - 0: Apply -1 torque to the actuated joint
 *   - 1: Apply 0 torque to the actuated joint
 *   - 2: Apply +1 torque to the actuated joint
 *
 * Reward:
 *   - -1.0 for every step taken
 *   - 0.0 on the terminal step (when goal is reached)
 *
 * Episode Termination:
 *   - The free end reaches the target height
 *   - Episode length >= max_episode_steps (default: 500)
 */
class AcrobotEnv {
 private:
  // Physics constants
  static constexpr double kG = 9.8;        // Gravity
  static constexpr double kDt = 0.2;       // Time step
  static constexpr double kL = 1.0;        // Link length
  static constexpr double kM = 1.0;        // Link mass
  static constexpr double kLC = 0.5;       // Link center of mass position
  static constexpr double kI = 1.0;        // Link moment of inertia
  static constexpr double kMaxVel1 = 4.0 * std::numbers::pi;
  static constexpr double kMaxVel2 = 9.0 * std::numbers::pi;
  static constexpr double kInitRange = 0.1;

  int envId_;
  int maxEpisodeSteps_;
  int elapsedStep_;
  double theta1_;
  double theta2_;
  double theta1Dot_;
  double theta2Dot_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> dist_;

  /**
   * @brief Helper struct for 5D state vector used in RK4 integration
   */
  struct Vec5 {
    double s0{0}, s1{0}, s2{0}, s3{0}, s4{0};

    Vec5() = default;
    Vec5(double s0, double s1, double s2, double s3, double s4)
        : s0(s0), s1(s1), s2(s2), s3(s3), s4(s4) {}

    Vec5 operator+(const Vec5& v) const {
      return {s0 + v.s0, s1 + v.s1, s2 + v.s2, s3 + v.s3, s4 + v.s4};
    }

    Vec5 operator*(double v) const {
      return {s0 * v, s1 * v, s2 * v, s3 * v, s4 * v};
    }
  };

 public:
  /**
   * @brief Construct Acrobot environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 500)
   */
  explicit AcrobotEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 500)
      : envId_{envId},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        theta1_{0.0},
        theta2_{0.0},
        theta1Dot_{0.0},
        theta2Dot_{0.0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        dist_{-kInitRange, kInitRange} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  AcrobotState reset() {
    theta1_ = dist_(gen_);
    theta2_ = dist_(gen_);
    theta1Dot_ = dist_(gen_);
    theta2Dot_ = dist_(gen_);
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action Discrete action: 0 (-1 torque), 1 (0 torque), 2 (+1 torque)
   * @return Step result containing observation, reward, done, truncated
   */
  AcrobotStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Convert action to torque
    double torque = static_cast<double>(action) - 1.0;

    // Integrate using RK4
    Vec5 state{theta1_, theta2_, theta1Dot_, theta2Dot_, torque};
    state = rk4(state);

    theta1_ = state.s0;
    theta2_ = state.s1;
    theta1Dot_ = state.s2;
    theta2Dot_ = state.s3;

    // Normalize angles to [-pi, pi]
    while (theta1_ < -std::numbers::pi) {
      theta1_ += 2.0 * std::numbers::pi;
    }
    while (theta1_ >= std::numbers::pi) {
      theta1_ -= 2.0 * std::numbers::pi;
    }
    while (theta2_ < -std::numbers::pi) {
      theta2_ += 2.0 * std::numbers::pi;
    }
    while (theta2_ >= std::numbers::pi) {
      theta2_ -= 2.0 * std::numbers::pi;
    }

    // Clip velocities
    theta1Dot_ = std::clamp(theta1Dot_, -kMaxVel1, kMaxVel1);
    theta2Dot_ = std::clamp(theta2Dot_, -kMaxVel2, kMaxVel2);

    // Check if goal reached (free end above threshold)
    bool goalReached = (-std::cos(theta1_) - std::cos(theta1_ + theta2_) > 1.0);

    float reward = -1.0f;
    if (goalReached) {
      done_ = true;
      reward = 0.0f;
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    AcrobotStepResult result;
    result.observation = getCurrentState();
    result.reward = reward;
    result.done = done_;
    result.truncated = done_ && maxStepsReached && !goalReached;

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
   * @brief Get current state as AcrobotState
   */
  AcrobotState getCurrentState() const {
    return AcrobotState{
      static_cast<float>(std::cos(theta1_)),
      static_cast<float>(std::sin(theta1_)),
      static_cast<float>(std::cos(theta2_)),
      static_cast<float>(std::sin(theta2_)),
      static_cast<float>(theta1Dot_),
      static_cast<float>(theta2Dot_),
      static_cast<float>(theta1_),
      static_cast<float>(theta2_)
    };
  }

  /**
   * @brief RK4 integration
   */
  Vec5 rk4(const Vec5& y0) const {
    Vec5 k1 = derivs(y0, 0.0);
    Vec5 k2 = derivs(y0 + k1 * (kDt / 2.0), kDt / 2.0);
    Vec5 k3 = derivs(y0 + k2 * (kDt / 2.0), kDt / 2.0);
    Vec5 k4 = derivs(y0 + k3 * kDt, kDt);
    return y0 + (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (kDt / 6.0);
  }

  /**
   * @brief Compute derivatives for dynamics
   */
  Vec5 derivs(const Vec5& s, double t) const {
    double theta1 = s.s0;
    double theta2 = s.s1;
    double dtheta1 = s.s2;
    double dtheta2 = s.s3;
    double a = s.s4;  // torque

    double d1 = kM * kLC * kLC +
                kM * (kL * kL + kLC * kLC + 2.0 * kL * kLC * std::cos(theta2)) +
                kI * 2.0;
    double d2 = kM * (kLC * kLC + kL * kLC * std::cos(theta2)) + kI;
    double phi2 = kM * kLC * kG * std::cos(theta1 + theta2 - std::numbers::pi / 2.0);
    double phi1 =
        -(dtheta2 + 2.0 * dtheta1) * kM * kL * kLC * dtheta2 * std::sin(theta2) +
        kM * (kLC + kL) * kG * std::cos(theta1 - std::numbers::pi / 2.0) + phi2;
    double ddtheta2 =
        (a + d2 / d1 * phi1 -
         kM * kL * kLC * dtheta1 * dtheta1 * std::sin(theta2) - phi2) /
        (kM * kLC * kLC + kI - d2 * d2 / d1);
    double ddtheta1 = -(d2 * ddtheta2 + phi1) / d1;

    return {dtheta1, dtheta2, ddtheta1, ddtheta2, 0.0};
  }
};

}  // namespace envpool::classic_control
