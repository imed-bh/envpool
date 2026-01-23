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
 * @module envpool.env.classic_control.mountain_car_continuous
 * @brief Mountain Car environment with continuous actions
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/classic_control/continuous_mountain_car.py
 *
 * A car is on a one-dimensional track, positioned between two mountains.
 * The goal is to drive up the mountain on the right with continuous control.
 */

module;

export module envpool.env.classic_control.mountain_car_continuous;

import std;
import envpool.core.types;

export namespace envpool::classic_control {

using namespace envpool::core;

//==============================================================================
// MountainCarContinuous Environment
//==============================================================================

/**
 * @struct MountainCarContinuousState
 * @brief Observation state for MountainCarContinuous environment
 */
struct MountainCarContinuousState {
  float position;  // Car position
  float velocity;  // Car velocity

  std::array<float, 2> toArray() const {
    return {position, velocity};
  }
};

/**
 * @struct MountainCarContinuousStepResult
 * @brief Result of a step in the environment
 */
struct MountainCarContinuousStepResult {
  MountainCarContinuousState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class MountainCarContinuousEnv
 * @brief Mountain Car environment with continuous actions
 *
 * The goal is to reach the flag on top of the mountain on the right side
 * using continuous control.
 *
 * Observation Space:
 *   - position: Position of the car along the x-axis (-1.2 to 0.6)
 *   - velocity: Velocity of the car (-0.07 to 0.07)
 *
 * Action Space (continuous):
 *   - Continuous action in range [-1.0, 1.0] representing force
 *
 * Reward:
 *   - Penalty of -0.1 * action^2 for each step
 *   - Bonus of +100 when reaching the goal
 *
 * Episode Termination:
 *   - Position >= 0.45 (reaching the goal)
 *   - Episode length >= max_episode_steps (default: 999)
 */
class MountainCarContinuousEnv {
 private:
  static constexpr double kMinPos = -1.2;
  static constexpr double kMaxPos = 0.6;
  static constexpr double kMaxSpeed = 0.07;
  static constexpr double kPower = 0.0015;
  static constexpr double kGoalPos = 0.45;
  static constexpr double kGoalVel = 0.0;
  static constexpr double kGravity = 0.0025;

  int envId_;
  int maxEpisodeSteps_;
  int elapsedStep_;
  double pos_;
  double vel_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> posDist_;

 public:
  /**
   * @brief Construct MountainCarContinuous environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 999)
   */
  explicit MountainCarContinuousEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 999)
      : envId_{envId},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        pos_{0.0},
        vel_{0.0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        posDist_{-0.6, -0.4} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  MountainCarContinuousState reset() {
    pos_ = posDist_(gen_);
    vel_ = 0.0;
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action Continuous action (will be clipped to [-1, 1])
   * @return Step result containing observation, reward, done, truncated
   */
  MountainCarContinuousStepResult step(float action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Clip action and compute power penalty
    double act = std::clamp(static_cast<double>(action), -1.0, 1.0);
    double reward = -0.1 * act * act;

    // Update velocity based on action and gravity
    vel_ += act * kPower - std::cos(3.0 * pos_) * kGravity;

    // Clip velocity
    vel_ = std::clamp(vel_, -kMaxSpeed, kMaxSpeed);

    // Update position
    pos_ += vel_;

    // Clip position
    pos_ = std::clamp(pos_, kMinPos, kMaxPos);

    // If at left boundary and moving left, stop
    if (pos_ == kMinPos && vel_ < 0.0) {
      vel_ = 0.0;
    }

    // Check goal
    if (pos_ >= kGoalPos && vel_ >= kGoalVel) {
      done_ = true;
      reward += 100.0;  // Goal bonus
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    MountainCarContinuousStepResult result;
    result.observation = getCurrentState();
    result.reward = static_cast<float>(reward);
    result.done = done_;
    result.truncated = done_ && maxStepsReached && !(pos_ >= kGoalPos);

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
   * @brief Get current state as MountainCarContinuousState
   */
  MountainCarContinuousState getCurrentState() const {
    return MountainCarContinuousState{
      static_cast<float>(pos_),
      static_cast<float>(vel_)
    };
  }
};

}  // namespace envpool::classic_control
