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
 * @module envpool.env.toy_text.frozen_lake
 * @brief Frozen Lake gridworld environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/toy_text/frozen_lake.py
 *
 * Navigate a frozen lake from start to goal while avoiding holes.
 */

module;

export module envpool.env.toy_text.frozen_lake;

import std;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// FrozenLake Environment
//==============================================================================

/**
 * @struct FrozenLakeState
 * @brief Observation state for FrozenLake environment
 */
struct FrozenLakeState {
  int position;  // Encoded position (x * size + y)

  int toInt() const {
    return position;
  }
};

/**
 * @struct FrozenLakeStepResult
 * @brief Result of a step in the environment
 */
struct FrozenLakeStepResult {
  FrozenLakeState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class FrozenLakeEnv
 * @brief Frozen Lake gridworld environment
 *
 * The agent must navigate from start (S) to goal (G) without falling in holes (H).
 * The ice is slippery, so actions may not behave as expected.
 *
 * Observation Space:
 *   - Single integer encoding position: x * size + y
 *
 * Action Space (discrete):
 *   - 0: Move left
 *   - 1: Move down
 *   - 2: Move right
 *   - 3: Move up
 *
 * Reward:
 *   - +1.0 for reaching goal
 *   - 0.0 otherwise
 *
 * The environment is stochastic: actions are perturbed by adjacent directions.
 */
class FrozenLakeEnv {
 private:
  int envId_;
  int x_;      // Row position
  int y_;      // Column position
  int size_;   // Grid size (4 or 8)
  int maxEpisodeSteps_;
  int elapsedStep_;
  bool done_;
  std::vector<std::string> map_;

  std::mt19937 gen_;
  std::uniform_int_distribution<int> perturbDist_;

 public:
  /**
   * @brief Construct FrozenLake environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param size Grid size (4 or 8, default: 4)
   * @param maxEpisodeSteps Maximum steps per episode (default: 100)
   */
  explicit FrozenLakeEnv(int envId = 0, int seed = 42, int size = 4, int maxEpisodeSteps = 100)
      : envId_{envId},
        x_{0},
        y_{0},
        size_{size},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        perturbDist_{-1, 1} {
    // Initialize map
    if (size_ != 8) {
      map_ = {"SFFF", "FHFH", "FFFH", "HFFG"};
    } else {
      map_ = {"SFFFFFFF", "FFFFFFFF", "FFFHFFFF",
              "FFFFFHFF", "FFFHFFFF", "FHHFFFHF",
              "FHFFHFHF", "FFFHFFFG"};
    }
  }

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  FrozenLakeState reset() {
    x_ = 0;
    y_ = 0;
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (left), 1 (down), 2 (right), 3 (up)
   * @return Step result containing observation, reward, done, truncated
   */
  FrozenLakeStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Perturb action due to slippery ice
    int perturbedAction = (action + perturbDist_(gen_) + 4) % 4;

    // Apply action
    if (perturbedAction == 0) {      // Left
      --y_;
    } else if (perturbedAction == 1) {  // Down
      ++x_;
    } else if (perturbedAction == 2) {  // Right
      ++y_;
    } else {                         // Up
      --x_;
    }

    // Clip to grid boundaries
    x_ = std::clamp(x_, 0, size_ - 1);
    y_ = std::clamp(y_, 0, size_ - 1);

    float reward = 0.0f;

    // Check cell type
    char cell = map_[x_][y_];
    if (cell == 'H' || cell == 'G') {
      done_ = true;
      reward = (cell == 'G') ? 1.0f : 0.0f;
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    FrozenLakeStepResult result;
    result.observation = getCurrentState();
    result.reward = reward;
    result.done = done_;
    result.truncated = done_ && maxStepsReached && cell != 'G' && cell != 'H';

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
   * @brief Get current state as FrozenLakeState
   */
  FrozenLakeState getCurrentState() const {
    return FrozenLakeState{x_ * size_ + y_};
  }
};

}  // namespace envpool::toy_text
