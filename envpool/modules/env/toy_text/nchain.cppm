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
 * @module envpool.env.toy_text.nchain
 * @brief N-Chain environment testing exploration vs exploitation
 *
 * Based on: https://github.com/openai/gym/blob/v0.20.0/gym/envs/toy_text/nchain.py
 *
 * A simple chain environment where the agent must balance immediate small rewards
 * with larger delayed rewards.
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

export module envpool.env.toy_text.nchain;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// NChain Environment
//==============================================================================

/**
 * @struct NChainState
 * @brief Observation state for NChain environment
 */
struct NChainState {
  int state;  // Current state in chain (0-4)

  int toInt() const {
    return state;
  }
};

/**
 * @struct NChainStepResult
 * @brief Result of a step in the environment
 */
struct NChainStepResult {
  NChainState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class NChainEnv
 * @brief N-Chain environment for testing exploration
 *
 * A 5-state chain where the agent can:
 * - Forward: Move forward in chain, large reward at end (10)
 * - Backward: Go back to start, small immediate reward (2)
 *
 * This tests the agent's ability to defer immediate gratification for larger rewards.
 *
 * Observation Space:
 *   - Single integer representing state in chain (0-4)
 *
 * Action Space (discrete):
 *   - 0: Move forward in chain
 *   - 1: Go back to start and get small reward
 *
 * Reward:
 *   - 2 for backward action
 *   - 10 for forward action at end of chain
 *   - 0 otherwise
 *
 * Actions are stochastic with 20% chance of being flipped.
 */
class NChainEnv {
 private:
  static constexpr int kNumStates = 5;
  static constexpr double kSlipProb = 0.2;

  int envId_;
  int state_;
  int maxEpisodeSteps_;
  int elapsedStep_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> slipDist_;

 public:
  /**
   * @brief Construct NChain environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 1000)
   */
  explicit NChainEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 1000)
      : envId_{envId},
        state_{0},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        slipDist_{0.0, 1.0} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  NChainState reset() {
    state_ = 0;
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (forward) or 1 (backward)
   * @return Step result containing observation, reward, done, truncated
   */
  NChainStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    // Slip: randomly flip action with 20% probability
    int effectiveAction = action;
    if (slipDist_(gen_) < kSlipProb) {
      effectiveAction = 1 - action;
    }

    float reward = 0.0f;

    if (effectiveAction == 1) {
      // Backward: return to start and get small reward
      reward = 2.0f;
      state_ = 0;
    } else if (state_ < kNumStates - 1) {
      // Forward: advance in chain
      ++state_;
    } else {
      // At end of chain: get large reward
      reward = 10.0f;
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    NChainStepResult result;
    result.observation = getCurrentState();
    result.reward = reward;
    result.done = done_;
    result.truncated = done_ && maxStepsReached;

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
   * @brief Get current state as NChainState
   */
  NChainState getCurrentState() const {
    return NChainState{state_};
  }
};

}  // namespace envpool::toy_text
