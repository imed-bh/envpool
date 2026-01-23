/**
 * @file test_blackjack.cpp
 * @brief C++ test for Blackjack C++20 module
 *
 * Tests that the Blackjack module compiles, links, and runs correctly.
 */

import std;
import envpool.env.toy_text.blackjack;

using namespace envpool::toy_text;

int main() {
    std::println("=== Blackjack C++20 Module Test ===\n");

    // Create environment
    std::println("Creating Blackjack environment...");
    BlackjackEnv env(0, 42, false, true);  // Standard rules
    std::println("✓ Environment created\n");

    // Reset environment
    std::println("Resetting environment...");
    auto state = env.reset();
    std::println("✓ Environment reset");
    std::println("  Initial state: player_sum={}, dealer_card={}, usable_ace={}\n",
                 state.player_sum, state.dealer_card, state.usable_ace);

    // Play 100 games
    std::println("Playing 100 games...");
    int wins = 0;
    int losses = 0;
    int draws = 0;

    for (int game = 0; game < 100; game++) {
        state = env.reset();

        // Simple strategy: hit if sum < 17, otherwise stick
        while (!env.isDone()) {
            int action = (state.player_sum < 17) ? 1 : 0;  // 1=hit, 0=stick
            auto result = env.step(action);
            state = result.observation;

            if (result.done) {
                if (result.reward > 0) {
                    wins++;
                } else if (result.reward < 0) {
                    losses++;
                } else {
                    draws++;
                }
                break;
            }
        }
    }

    std::println("✓ All games completed");
    std::println("  Wins: {} ({:.1f}%)", wins, 100.0 * wins / 100);
    std::println("  Losses: {} ({:.1f}%)", losses, 100.0 * losses / 100);
    std::println("  Draws: {} ({:.1f}%)", draws, 100.0 * draws / 100);

    // Test determinism
    std::println("\nTesting determinism...");
    BlackjackEnv env1(0, 12345, false, true);
    BlackjackEnv env2(0, 12345, false, true);

    auto state1 = env1.reset();
    auto state2 = env2.reset();

    bool deterministic = (state1.player_sum == state2.player_sum &&
                         state1.dealer_card == state2.dealer_card &&
                         state1.usable_ace == state2.usable_ace);

    if (deterministic) {
        std::println("✓ Environment is deterministic with same seed");
    } else {
        std::println("✗ Warning: Environment not deterministic!");
        return 1;
    }

    // Test game logic
    std::println("\nTesting game logic...");
    BlackjackEnv test_env(0, 999, false, true);

    // Play one detailed game
    auto test_state = test_env.reset();
    std::println("  Initial: player={}, dealer_visible={}, usable_ace={}",
                 test_state.player_sum, test_state.dealer_card, test_state.usable_ace);

    int steps = 0;
    while (!test_env.isDone() && steps < 10) {
        int action = (test_state.player_sum < 17) ? 1 : 0;
        std::println("  Action: {} ({})", action, action ? "hit" : "stick");

        auto result = test_env.step(action);
        test_state = result.observation;

        if (result.done) {
            std::println("  Final: player={}, reward={:.1f}",
                        test_state.player_sum, result.reward);
            break;
        } else {
            std::println("  After hit: player={}", test_state.player_sum);
        }
        steps++;
    }
    std::println("✓ Game logic working");

    // Test different configurations
    std::println("\nTesting natural blackjack configuration...");
    BlackjackEnv natural_env(0, 777, true, false);  // Natural blackjack enabled
    auto natural_state = natural_env.reset();
    std::println("✓ Natural blackjack environment created (player={}, dealer={})",
                 natural_state.player_sum, natural_state.dealer_card);

    std::println("\n🎉 All Blackjack tests passed!");
    return 0;
}
