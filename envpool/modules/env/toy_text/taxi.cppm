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
 * @module envpool.env.toy_text.taxi
 * @brief Taxi navigation and pickup/dropoff environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/toy_text/taxi.py
 *
 * Navigate a 5x5 grid to pick up and drop off passengers at designated locations.
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

export module envpool.env.toy_text.taxi;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// Taxi Environment
//==============================================================================

/**
 * @struct TaxiState
 * @brief Observation state for Taxi environment
 */
struct TaxiState {
  int encoded_state;  // Encoded state: ((x * 5 + y) * 5 + passenger_loc) * 4 + dest

  int toInt() const {
    return encoded_state;
  }
};

/**
 * @struct TaxiStepResult
 * @brief Result of a step in the environment
 */
struct TaxiStepResult {
  TaxiState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class TaxiEnv
 * @brief Taxi navigation and pickup/dropoff environment
 *
 * Navigate a 5x5 grid world to pick up and drop off passengers.
 * There are 4 designated locations: R, G, Y, B.
 *
 * Observation Space:
 *   - Single integer encoding: ((x * 5 + y) * 5 + passenger_loc) * 4 + dest (0-499)
 *     - x, y: taxi position (0-4 each)
 *     - passenger_loc: 0-3 for locations, 4 for in taxi
 *     - dest: destination location (0-3)
 *
 * Action Space (discrete):
 *   - 0: Move south
 *   - 1: Move north
 *   - 2: Move east
 *   - 3: Move west
 *   - 4: Pick up passenger
 *   - 5: Drop off passenger
 *
 * Reward:
 *   - +20 for successful dropoff
 *   - -1 for each step
 *   - -10 for illegal pickup/dropoff
 */
class TaxiEnv {
 private:
  int envId_;
  int x_;              // Taxi row (0-4)
  int y_;              // Taxi column (0-4)
  int passengerLoc_;   // Passenger location: 0-3 for locations, 4 for in taxi
  int destination_;    // Destination location (0-3)
  int maxEpisodeSteps_;
  int elapsedStep_;
  bool done_;

  // Location coordinates: R(0,0), G(0,4), Y(4,0), B(4,3)
  std::vector<std::array<int, 2>> locations_;

  // Map for movement (walls represented)
  std::vector<std::string> map_;
  std::vector<std::string> locMap_;

  std::mt19937 gen_;
  std::uniform_int_distribution<int> carDist_;
  std::uniform_int_distribution<int> locDist_;

 public:
  /**
   * @brief Construct Taxi environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param maxEpisodeSteps Maximum steps per episode (default: 200)
   */
  explicit TaxiEnv(int envId = 0, int seed = 42, int maxEpisodeSteps = 200)
      : envId_{envId},
        x_{0},
        y_{0},
        passengerLoc_{0},
        destination_{0},
        maxEpisodeSteps_{maxEpisodeSteps},
        elapsedStep_{0},
        done_{true},
        locations_{{{0, 0}, {0, 4}, {4, 0}, {4, 3}}},
        map_({"|:|::|", "|:|::|", "|::::|", "||:|:|", "||:|:|"}),
        locMap_({"0   1", "     ", "     ", "     ", "2  3 "}),
        gen_{static_cast<unsigned int>(seed)},
        carDist_{0, 3},
        locDist_{0, 4} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  TaxiState reset() {
    x_ = locDist_(gen_);
    y_ = locDist_(gen_);
    passengerLoc_ = carDist_(gen_);
    destination_ = carDist_(gen_);
    done_ = false;
    elapsedStep_ = 0;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0-5 (south, north, east, west, pickup, dropoff)
   * @return Step result containing observation, reward, done, truncated
   */
  TaxiStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    ++elapsedStep_;

    float reward = -1.0f;  // Default step penalty

    if (action == 0) {  // South
      if (x_ < 4) {
        ++x_;
      }
    } else if (action == 1) {  // North
      if (x_ > 0) {
        --x_;
      }
    } else if (action == 2) {  // East
      if (map_[x_][y_ + 1] == ':') {
        ++y_;
      }
    } else if (action == 3) {  // West
      if (map_[x_][y_] == ':') {
        --y_;
      }
    } else if (action == 4) {  // Pick up
      if (passengerLoc_ < 4 &&
          x_ == locations_[passengerLoc_][0] &&
          y_ == locations_[passengerLoc_][1]) {
        passengerLoc_ = 4;  // Passenger in taxi
      } else {
        reward = -10.0f;  // Illegal pickup
      }
    } else {  // Drop off
      if (passengerLoc_ == 4 &&
          x_ == locations_[destination_][0] &&
          y_ == locations_[destination_][1]) {
        passengerLoc_ = destination_;
        done_ = true;
        reward = 20.0f;  // Successful dropoff
      } else if (passengerLoc_ == 4 && locMap_[x_][y_] != ' ') {
        // Dropped off at wrong location
        passengerLoc_ = locMap_[x_][y_] - '0';
      } else {
        reward = -10.0f;  // Illegal dropoff
      }
    }

    // Check max steps
    bool maxStepsReached = (elapsedStep_ >= maxEpisodeSteps_);
    if (maxStepsReached) {
      done_ = true;
    }

    TaxiStepResult result;
    result.observation = getCurrentState();
    result.reward = reward;
    result.done = done_;
    result.truncated = done_ && maxStepsReached && reward != 20.0f;

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
   * @brief Get current state as TaxiState
   */
  TaxiState getCurrentState() const {
    int encoded = ((x_ * 5 + y_) * 5 + passengerLoc_) * 4 + destination_;
    return TaxiState{encoded};
  }
};

}  // namespace envpool::toy_text
