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
 * @module envpool.env.classic_control.pendulum
 * @brief Inverted pendulum swing-up environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/classic_control/pendulum.py
 *
 * The inverted pendulum swingup problem is based on the classic problem in control theory.
 * The goal is to swing up and stabilize the pendulum in the upright position.
 */

module;

export module envpool.env.classic_control.pendulum;

import std;
import envpool.core.types;

export namespace envpool::classic_control {

using namespace envpool::core;

//==============================================================================
// Pendulum Environment
//==============================================================================

/**
 * @struct PendulumState
 * @brief Observation state for Pendulum environment
 */
struct PendulumState {
  float cos_theta;  // Cosine of angle
  float sin_theta;  // Sine of angle
  float theta_dot;  // Angular velocity

  std::array<float, 3> toArray() const {
    return {cos_theta, sin_theta, theta_dot};
  }
};

/**
 * @struct PendulumStepResult
 * @brief Result of a step in the environment
 */
struct PendulumStepResult {
  PendulumState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class PendulumEnv
 * @brief Inverted pendulum swing-up environment
 *
 * The goal is to swing up and keep the pendulum standing upright.
 *
 * Observation Space:
 *   - cos(theta): Cosine of angle from vertical (-1.0 to 1.0)
 *   - sin(theta): Sine of angle from vertical (-1.0 to 1.0)
 *   - theta_dot: Angular velocity (-8.0 to 8.0)
 *
 * Action Space:
 *   - Continuous action in range [-2.0, 2.0] representing torque
 *
 * Reward:
 *   - Reward is penalized based on: -(theta^2 + 0.1*theta_dot^2 + 0.001*action^2)
 *
 * Episode Termination:
 *   - Episode length is greater than max_episode_steps (default: 200)
 */
class PendulumEnv {
 private:
  static constexpr double kMaxSpeed = 8.0;
  static constexpr double kMaxTorque = 2.0;
  static constexpr double kDt = 0.05;
  static constexpr double kGravity = 10.0;

  int envId_;
  int maxEpisodeSteps_;
  int elapsedStep_;
  int version_;
  double theta_;
  double thetaDot_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> thetaDist_;
  std::uniform_real_distribution<double> thetaDotDist_;

 public:
  /**
   * @brief Construct Pendulum environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 200)
   * @param version Version (0 or 1, affects integration order)
   */
  explicit PendulumEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 200, int version = 0)
      : envId_{envId},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        version_{version},
        theta_{0.0},
        thetaDot_{0.0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        thetaDist_{-std::numbers::pi, std::numbers::pi},
        thetaDotDist_{-1.0, 1.0} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  PendulumState reset() {
    theta_ = thetaDist_(gen_);
    thetaDot_ = thetaDotDist_(gen_);
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action Continuous torque value (will be clipped to [-2, 2])
   * @return Step result containing observation, reward, done, truncated
   */
  PendulumStepResult step(float action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Clip action to valid range
    double u = std::clamp(static_cast<double>(action), -kMaxTorque, kMaxTorque);

    // Compute cost (negative reward)
    double cost = theta_ * theta_ + 0.1 * thetaDot_ * thetaDot_ + 0.001 * u * u;

    // Update angular velocity
    double newThetaDot = thetaDot_ + 3.0 * (kGravity / 2.0 * std::sin(theta_) + u) * kDt;

    // Version 0: update theta before clipping theta_dot
    if (version_ == 0) {
      theta_ += newThetaDot * kDt;
    }

    // Clip angular velocity
    thetaDot_ = std::clamp(newThetaDot, -kMaxSpeed, kMaxSpeed);

    // Version 1: update theta after clipping theta_dot
    if (version_ == 1) {
      theta_ += thetaDot_ * kDt;
    }

    // Normalize angle to [-pi, pi]
    while (theta_ < -std::numbers::pi) {
      theta_ += 2.0 * std::numbers::pi;
    }
    while (theta_ >= std::numbers::pi) {
      theta_ -= 2.0 * std::numbers::pi;
    }

    done_ = (elapsedStep_ >= maxEpisodeSteps_);

    PendulumStepResult result;
    result.observation = getCurrentState();
    result.reward = static_cast<float>(-cost);
    result.done = done_;
    result.truncated = done_;

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
   * @brief Get current state as PendulumState
   */
  PendulumState getCurrentState() const {
    return PendulumState{
      static_cast<float>(std::cos(theta_)),
      static_cast<float>(std::sin(theta_)),
      static_cast<float>(thetaDot_)
    };
  }
};

}  // namespace envpool::classic_control
