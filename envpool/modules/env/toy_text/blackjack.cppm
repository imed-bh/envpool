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
 * @module envpool.env.toy_text.blackjack
 * @brief Blackjack card game environment
 *
 * Based on: https://github.com/openai/gym/blob/master/gym/envs/toy_text/blackjack.py
 *
 * Simple blackjack game where the goal is to obtain cards with sum close to 21
 * without going over.
 */

module;

export module envpool.env.toy_text.blackjack;

import std;
import envpool.core.types;

export namespace envpool::toy_text {

using namespace envpool::core;

//==============================================================================
// Blackjack Environment
//==============================================================================

/**
 * @struct BlackjackState
 * @brief Observation state for Blackjack environment
 */
struct BlackjackState {
  int player_sum;       // Sum of player's hand
  int dealer_card;      // Dealer's visible card
  int usable_ace;       // 1 if player has usable ace, 0 otherwise

  std::array<int, 3> toArray() const {
    return {player_sum, dealer_card, usable_ace};
  }
};

/**
 * @struct BlackjackStepResult
 * @brief Result of a step in the environment
 */
struct BlackjackStepResult {
  BlackjackState observation;
  float reward;
  bool done;
  bool truncated;
};

/**
 * @class BlackjackEnv
 * @brief Blackjack card game environment
 *
 * The goal is to obtain cards with sum close to 21 without exceeding it.
 *
 * Observation Space:
 *   - player_sum: Sum of player's cards (4-21)
 *   - dealer_card: Value of dealer's visible card (1-10)
 *   - usable_ace: Whether player has usable ace (0 or 1)
 *
 * Action Space (discrete):
 *   - 0: Stick (stop taking cards)
 *   - 1: Hit (take another card)
 *
 * Reward:
 *   - +1.0 for winning
 *   - 0.0 for draw
 *   - -1.0 for losing
 *   - +1.5 for natural blackjack (if natural=true and not sab)
 */
class BlackjackEnv {
 private:
  int envId_;
  bool natural_;   // Natural blackjack gives 1.5x reward
  bool sab_;       // Sutton and Barto rules
  std::vector<int> player_;
  std::vector<int> dealer_;
  bool done_;

  std::mt19937 gen_;
  std::uniform_int_distribution<int> cardDist_;

 public:
  /**
   * @brief Construct Blackjack environment
   * @param envId Environment ID
   * @param seed Random seed
   * @param natural If true, natural blackjack gives 1.5x reward
   * @param sab If true, use Sutton and Barto rules
   */
  explicit BlackjackEnv(int envId = 0, int seed = 42, bool natural = false, bool sab = true)
      : envId_{envId},
        natural_{natural},
        sab_{sab},
        done_{true},
        gen_{static_cast<unsigned int>(seed)},
        cardDist_{1, 13} {}

  /**
   * @brief Reset environment to initial state
   * @return Initial observation
   */
  BlackjackState reset() {
    player_.clear();
    player_.push_back(drawCard());
    player_.push_back(drawCard());
    dealer_.clear();
    dealer_.push_back(drawCard());
    dealer_.push_back(drawCard());
    done_ = false;

    return getCurrentState();
  }

  /**
   * @brief Execute one time step
   * @param action 0 (stick) or 1 (hit)
   * @return Step result containing observation, reward, done, truncated
   */
  BlackjackStepResult step(int action) {
    if (done_) {
      throw std::runtime_error("Episode finished. Call reset() before step().");
    }

    float reward = 0.0f;

    if (action == 1) {  // Hit: add a card to player's hand
      player_.push_back(drawCard());
      if (isBust(player_)) {
        done_ = true;
        reward = -1.0f;
      }
    } else {  // Stick: play out dealer's hand
      done_ = true;
      while (sumHand(dealer_) < 17) {
        dealer_.push_back(drawCard());
      }

      int playerScore = score(player_);
      int dealerScore = score(dealer_);

      if (playerScore > dealerScore) {
        reward = 1.0f;
      } else if (playerScore < dealerScore) {
        reward = -1.0f;
      } else {
        reward = 0.0f;
      }

      // Special rules for natural blackjack
      if (sab_ && isNatural(player_) && !isNatural(dealer_)) {
        reward = 1.0f;
      } else if (!sab_ && natural_ && isNatural(player_) && reward == 1.0f) {
        reward = 1.5f;
      }
    }

    BlackjackStepResult result;
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
   * @brief Draw a card (returns value 1-10)
   */
  int drawCard() {
    return std::min(10, cardDist_(gen_));
  }

  /**
   * @brief Check if hand has usable ace
   */
  static int usableAce(const std::vector<int>& hand) {
    for (int card : hand) {
      if (card == 1) {
        return 1;
      }
    }
    return 0;
  }

  /**
   * @brief Sum the hand, treating ace as 1 or 11 optimally
   */
  static int sumHand(const std::vector<int>& hand) {
    int sum = 0;
    for (int card : hand) {
      sum += card;
    }
    // Use ace as 11 if it doesn't cause bust
    if (usableAce(hand) && sum + 10 <= 21) {
      return sum + 10;
    }
    return sum;
  }

  /**
   * @brief Check if hand is bust (sum > 21)
   */
  static bool isBust(const std::vector<int>& hand) {
    return sumHand(hand) > 21;
  }

  /**
   * @brief Get score of hand (0 if bust, else sum)
   */
  static int score(const std::vector<int>& hand) {
    int sum = sumHand(hand);
    return sum > 21 ? 0 : sum;
  }

  /**
   * @brief Check if hand is natural blackjack (21 with 2 cards)
   */
  static bool isNatural(const std::vector<int>& hand) {
    return hand.size() == 2 &&
           ((hand[0] == 1 && hand[1] == 10) || (hand[0] == 10 && hand[1] == 1));
  }

  /**
   * @brief Get current state as BlackjackState
   */
  BlackjackState getCurrentState() const {
    return BlackjackState{
      sumHand(player_),
      dealer_[0],
      usableAce(player_)
    };
  }
};

}  // namespace envpool::toy_text
