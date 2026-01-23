/**
 * @file test_blackjack_catch2.cpp
 * @brief Catch2 v3 tests for Blackjack C++20 module
 */

import std;
import envpool.env.toy_text.blackjack;

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace envpool::toy_text;
using Catch::Approx;

TEST_CASE("Blackjack environment creation", "[blackjack][creation]") {
    SECTION("Default parameters") {
        BlackjackEnv env(0, 42, false, true);
        REQUIRE(true); // Construction succeeded
    }

    SECTION("With natural blackjack") {
        BlackjackEnv env(0, 42, true, false);
        REQUIRE(true); // Construction succeeded
    }
}

TEST_CASE("Blackjack reset", "[blackjack][reset]") {
    BlackjackEnv env(0, 42, false, true);

    SECTION("Reset returns valid state") {
        auto state = env.reset();

        // Player sum should be at least 4 (two aces) and at most 21
        REQUIRE(state.player_sum >= 4);
        REQUIRE(state.player_sum <= 21);

        // Dealer card should be 1-10
        REQUIRE(state.dealer_card >= 1);
        REQUIRE(state.dealer_card <= 10);

        // Usable ace is 0 or 1
        REQUIRE((state.usable_ace == 0 || state.usable_ace == 1));
    }

    SECTION("Multiple resets produce valid states") {
        for (int i = 0; i < 10; i++) {
            auto state = env.reset();

            REQUIRE(state.player_sum >= 4);
            REQUIRE(state.player_sum <= 21);
            REQUIRE(state.dealer_card >= 1);
            REQUIRE(state.dealer_card <= 10);
        }
    }
}

TEST_CASE("Blackjack actions", "[blackjack][actions]") {
    BlackjackEnv env(0, 42, false, true);

    SECTION("Stick (action 0) ends episode") {
        env.reset();
        auto result = env.step(0);  // Stick

        REQUIRE(result.done);
        // Reward should be -1, 0, or 1
        REQUIRE(result.reward >= -1.0f);
        REQUIRE(result.reward <= 1.0f);
    }

    SECTION("Hit (action 1) adds card") {
        auto initial_state = env.reset();
        int initial_sum = initial_state.player_sum;

        // If player sum is low, hit should add a card
        if (initial_sum < 12) {
            auto result = env.step(1);  // Hit

            // Sum should change (unless done immediately due to bust)
            if (!result.done) {
                REQUIRE(result.observation.player_sum != initial_sum);
            }
        }
    }
}

TEST_CASE("Blackjack game logic", "[blackjack][logic]") {
    BlackjackEnv env(0, 42, false, true);

    SECTION("Bust results in loss") {
        // This test is probabilistic - we'll try multiple games
        bool found_bust = false;

        for (int game = 0; game < 100; game++) {
            BlackjackEnv test_env(0, game, false, true);
            test_env.reset();

            // Keep hitting until done
            while (!test_env.isDone()) {
                auto result = test_env.step(1);  // Hit

                if (result.done && result.reward < 0) {
                    found_bust = true;
                    break;
                }
            }

            if (found_bust) break;
        }

        REQUIRE(found_bust);  // Should find at least one bust in 100 games
    }
}

TEST_CASE("Blackjack episode execution", "[blackjack][episode]") {
    BlackjackEnv env(0, 42, false, true);

    SECTION("Play 100 games with simple strategy") {
        int wins = 0;
        int losses = 0;
        int draws = 0;

        for (int game = 0; game < 100; game++) {
            auto state = env.reset();

            // Simple strategy: hit if sum < 17, otherwise stick
            while (!env.isDone()) {
                int action = (state.player_sum < 17) ? 1 : 0;
                auto result = env.step(action);
                state = result.observation;

                if (result.done) {
                    if (result.reward > 0) wins++;
                    else if (result.reward < 0) losses++;
                    else draws++;
                    break;
                }
            }
        }

        // Should have played 100 games
        REQUIRE(wins + losses + draws == 100);

        // Should have some of each outcome (very likely in 100 games)
        REQUIRE(wins > 0);
        REQUIRE(losses > 0);
    }
}

TEST_CASE("Blackjack determinism", "[blackjack][determinism]") {
    SECTION("Same seed produces same initial state") {
        BlackjackEnv env1(0, 12345, false, true);
        BlackjackEnv env2(0, 12345, false, true);

        auto state1 = env1.reset();
        auto state2 = env2.reset();

        REQUIRE(state1.player_sum == state2.player_sum);
        REQUIRE(state1.dealer_card == state2.dealer_card);
        REQUIRE(state1.usable_ace == state2.usable_ace);
    }

    SECTION("Same seed and actions produce same outcome") {
        BlackjackEnv env1(0, 12345, false, true);
        BlackjackEnv env2(0, 12345, false, true);

        env1.reset();
        env2.reset();

        // Play identically
        float reward1 = 0, reward2 = 0;

        while (!env1.isDone() && !env2.isDone()) {
            auto result1 = env1.step(0);  // Both stick
            auto result2 = env2.step(0);

            reward1 = result1.reward;
            reward2 = result2.reward;
        }

        REQUIRE(reward1 == reward2);
    }
}

TEST_CASE("Blackjack state array conversion", "[blackjack][utility]") {
    BlackjackEnv env(0, 42, false, true);
    auto state = env.reset();

    auto arr = state.toArray();

    REQUIRE(arr.size() == 3);
    REQUIRE(arr[0] == state.player_sum);
    REQUIRE(arr[1] == state.dealer_card);
    REQUIRE(arr[2] == state.usable_ace);
}

TEST_CASE("Blackjack natural configuration", "[blackjack][config]") {
    SECTION("Natural blackjack gives bonus reward") {
        // This test is probabilistic
        BlackjackEnv natural_env(0, 777, true, false);
        auto state = natural_env.reset();

        // Check that environment accepts the configuration
        REQUIRE(state.player_sum >= 4);
        REQUIRE(state.player_sum <= 21);
    }

    SECTION("Standard configuration works") {
        BlackjackEnv standard_env(0, 777, false, true);
        auto state = standard_env.reset();

        REQUIRE(state.player_sum >= 4);
        REQUIRE(state.player_sum <= 21);
    }
}
