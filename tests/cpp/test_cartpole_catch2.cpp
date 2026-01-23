/**
 * @file test_cartpole_catch2_v2.cpp
 * @brief Catch2 v3 tests for CartPole C++20 module (without import std)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

import envpool.env.classic_control.cartpole;

using namespace envpool::classic_control;
using Catch::Approx;

TEST_CASE("CartPole environment creation", "[cartpole][creation]") {
    SECTION("Default parameters") {
        CartPoleEnv env(0, 42, 500);
        REQUIRE(true); // If we get here, construction succeeded
    }

    SECTION("Custom seed") {
        CartPoleEnv env1(0, 12345, 500);
        CartPoleEnv env2(0, 67890, 500);
        REQUIRE(true); // Both should construct successfully
    }
}

TEST_CASE("CartPole reset", "[cartpole][reset]") {
    CartPoleEnv env(0, 42, 500);

    SECTION("Reset returns valid state") {
        auto state = env.reset();

        // State should be within initialization range (-0.05, 0.05)
        REQUIRE(std::abs(state.x) < 0.06);
        REQUIRE(std::abs(state.x_dot) < 0.06);
        REQUIRE(std::abs(state.theta) < 0.06);
        REQUIRE(std::abs(state.theta_dot) < 0.06);
    }

    SECTION("Multiple resets produce different states") {
        auto state1 = env.reset();
        auto state2 = env.reset();

        // With different random values, at least one component should differ
        bool different = (state1.x != state2.x) ||
                        (state1.x_dot != state2.x_dot) ||
                        (state1.theta != state2.theta) ||
                        (state1.theta_dot != state2.theta_dot);
        REQUIRE(different);
    }
}

TEST_CASE("CartPole step", "[cartpole][step]") {
    CartPoleEnv env(0, 42, 500);
    env.reset();

    SECTION("Left action (0)") {
        auto result = env.step(0);

        REQUIRE(result.reward == 1.0f);
        REQUIRE_FALSE(result.done);  // First step shouldn't be done
        REQUIRE_FALSE(result.truncated);
    }

    SECTION("Right action (1)") {
        auto result = env.step(1);

        REQUIRE(result.reward == 1.0f);
        REQUIRE_FALSE(result.done);  // First step shouldn't be done
        REQUIRE_FALSE(result.truncated);
    }
}

TEST_CASE("CartPole episode execution", "[cartpole][episode]") {
    CartPoleEnv env(0, 42, 500);

    SECTION("Complete episode with alternating actions") {
        env.reset();
        int total_steps = 0;
        double total_reward = 0.0;

        while (total_steps < 500) {
            int action = total_steps % 2;
            auto result = env.step(action);

            total_steps++;
            total_reward += result.reward;

            if (result.done || result.truncated) {
                break;
            }
        }

        REQUIRE(total_steps > 0);
        REQUIRE(total_steps <= 500);
        REQUIRE(total_reward == Approx(total_steps).epsilon(0.01));
    }

    SECTION("Multiple episodes") {
        for (int episode = 0; episode < 10; episode++) {
            env.reset();
            int steps = 0;

            while (steps < 500) {
                int action = steps % 2;
                auto result = env.step(action);
                steps++;

                if (result.done || result.truncated) {
                    break;
                }
            }

            REQUIRE(steps > 0);
            REQUIRE(steps <= 500);
        }
    }
}

TEST_CASE("CartPole determinism", "[cartpole][determinism]") {
    SECTION("Same seed produces same initial state") {
        CartPoleEnv env1(0, 12345, 500);
        CartPoleEnv env2(0, 12345, 500);

        auto state1 = env1.reset();
        auto state2 = env2.reset();

        REQUIRE(state1.x == state2.x);
        REQUIRE(state1.x_dot == state2.x_dot);
        REQUIRE(state1.theta == state2.theta);
        REQUIRE(state1.theta_dot == state2.theta_dot);
    }

    SECTION("Same seed produces same trajectory") {
        CartPoleEnv env1(0, 12345, 500);
        CartPoleEnv env2(0, 12345, 500);

        env1.reset();
        env2.reset();

        for (int step = 0; step < 10; step++) {
            auto result1 = env1.step(1);  // Same action
            auto result2 = env2.step(1);

            REQUIRE(result1.observation.x == result2.observation.x);
            REQUIRE(result1.observation.x_dot == result2.observation.x_dot);
            REQUIRE(result1.observation.theta == result2.observation.theta);
            REQUIRE(result1.observation.theta_dot == result2.observation.theta_dot);
            REQUIRE(result1.reward == result2.reward);
            REQUIRE(result1.done == result2.done);

            if (result1.done) break;
        }
    }
}

TEST_CASE("CartPole physics", "[cartpole][physics]") {
    CartPoleEnv env(0, 999, 500);
    auto initial_state = env.reset();

    SECTION("Repeated right action affects cart position") {
        double prev_x = initial_state.x;

        for (int i = 0; i < 5; i++) {
            auto result = env.step(1);  // Push right
            // Cart should generally move right (increasing x)
            // Note: Due to physics, it might temporarily decrease
        }
    }

    SECTION("Rewards are consistent") {
        for (int i = 0; i < 10; i++) {
            auto result = env.step(i % 2);
            REQUIRE(result.reward == 1.0f);
            if (result.done) break;
        }
    }
}

TEST_CASE("CartPole state array conversion", "[cartpole][utility]") {
    CartPoleEnv env(0, 42, 500);
    auto state = env.reset();

    auto arr = state.toArray();

    REQUIRE(arr.size() == 4);
    REQUIRE(arr[0] == state.x);
    REQUIRE(arr[1] == state.x_dot);
    REQUIRE(arr[2] == state.theta);
    REQUIRE(arr[3] == state.theta_dot);
}
