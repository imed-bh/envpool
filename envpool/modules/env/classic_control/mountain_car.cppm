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
 * @module envpool.env.classic_control.mountain_car
 * @brief Mountain Car environment with discrete actions
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/classic_control/mountain_car.py
 *
 * A car is on a one-dimensional track, positioned between two mountains.
 * The goal is to drive up the mountain on the right; however, the car's engine
 * is not strong enough to scale the mountain in a single pass. Therefore, the
 * only way to succeed is to drive back and forth to build up momentum.
 */

module;

export module envpool.env.classic_control.mountain_car;

import std;
import envpool.core.types;

export namespace envpool::classic_control {

using namespace envpool::core;

//==============================================================================
// MountainCar Environment
//==============================================================================

/**
 * @struct MountainCarState
 * @brief Observation state for MountainCar environment
 */
struct MountainCarState {
  float position;  // Car position
  float velocity;  // Car velocity

  std::array<float, 2> toArray() const {
    return {position, velocity};
  }
};

/**
 * @struct MountainCarStepResult
 * @brief Result of a step in the environment
 */
struct MountainCarStepResult {
  MountainCarState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class MountainCarEnv
 * @brief Mountain Car environment with discrete actions
 *
 * The goal is to reach the flag on top of the mountain on the right side.
 *
 * Observation Space:
 *   - position: Position of the car along the x-axis (-1.2 to 0.6)
 *   - velocity: Velocity of the car (-0.07 to 0.07)
 *
 * Action Space (discrete):
 *   - 0: Accelerate to the left
 *   - 1: Don't accelerate
 *   - 2: Accelerate to the right
 *
 * Reward:
 *   - -1.0 for every step taken
 *
 * Episode Termination:
 *   - Position >= 0.5 (reaching the goal)
 *   - Episode length >= max_episode_steps (default: 200)
 */
class MountainCarEnv {
 private:
  static constexpr double kMinPos = -1.2;
  static constexpr double kMaxPos = 0.6;
  static constexpr double kMaxSpeed = 0.07;
  static constexpr double kForce = 0.001;
  static constexpr double kGoalPos = 0.5;
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
   * @brief Construct MountainCar environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 200)
   */
  explicit MountainCarEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 200)
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
  MountainCarState reset() {
    pos_ = posDist_(gen_);
    vel_ = 0.0;
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action Discrete action: 0 (left), 1 (nothing), 2 (right)
   * @return Step result containing observation, reward, done, truncated
   */
  MountainCarStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Convert action to force (-1, 0, 1)
    double force = static_cast<double>(action) - 1.0;

    // Update velocity based on action and gravity
    vel_ += force * kForce - std::cos(3.0 * pos_) * kGravity;

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
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    MountainCarStepResult result;
    result.observation = getCurrentState();
    result.reward = -1.0f;  // Penalty for every step
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
   * @brief Get current state as MountainCarState
   */
  MountainCarState getCurrentState() const {
    return MountainCarState{
      static_cast<float>(pos_),
      static_cast<float>(vel_)
    };
  }
};

}  // namespace envpool::classic_control
