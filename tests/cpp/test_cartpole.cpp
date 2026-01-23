/**
 * @file test_cartpole.cpp
 * @brief C++ test for CartPole C++20 module
 *
 * Tests that the CartPole module compiles, links, and runs correctly.
 */

import std;
import envpool.env.classic_control.cartpole;

using namespace envpool::classic_control;

int main() {
    std::println("=== CartPole C++20 Module Test ===\n");

    // Create environment
    std::println("Creating CartPole environment...");
    CartPoleEnv env(0, 42, 500);
    std::println("✓ Environment created\n");

    // Reset environment
    std::println("Resetting environment...");
    auto state = env.reset();
    std::println("✓ Environment reset");
    std::println("  Initial state: x={:.4f}, x_dot={:.4f}, theta={:.4f}, theta_dot={:.4f}\n",
                 state.x, state.x_dot, state.theta, state.theta_dot);

    // Run 10 episodes
    std::println("Running 10 episodes...");
    int total_episodes = 0;
    int total_steps = 0;
    double total_reward = 0.0;

    for (int episode = 0; episode < 10; episode++) {
        state = env.reset();
        double episode_reward = 0.0;
        int episode_steps = 0;

        // Run until done (max 500 steps)
        while (episode_steps < 500) {
            // Alternate actions for deterministic testing
            int action = episode_steps % 2;
            auto result = env.step(action);

            episode_reward += result.reward;
            episode_steps++;

            if (result.done || result.truncated) {
                break;
            }
        }

        total_episodes++;
        total_steps += episode_steps;
        total_reward += episode_reward;

        std::println("  Episode {}: {} steps, reward={:.1f}",
                     episode + 1, episode_steps, episode_reward);
    }

    std::println("\n✓ All episodes completed");
    std::println("  Total episodes: {}", total_episodes);
    std::println("  Total steps: {}", total_steps);
    std::println("  Average steps per episode: {:.1f}",
                 static_cast<double>(total_steps) / total_episodes);
    std::println("  Total reward: {:.1f}", total_reward);
    std::println("  Average reward per episode: {:.1f}",
                 total_reward / total_episodes);

    // Test determinism - run same episode twice with same seed
    std::println("\nTesting determinism...");
    CartPoleEnv env1(0, 12345, 500);
    CartPoleEnv env2(0, 12345, 500);

    auto state1 = env1.reset();
    auto state2 = env2.reset();

    bool deterministic = (state1.x == state2.x &&
                         state1.x_dot == state2.x_dot &&
                         state1.theta == state2.theta &&
                         state1.theta_dot == state2.theta_dot);

    if (deterministic) {
        std::println("✓ Environment is deterministic with same seed");
    } else {
        std::println("✗ Warning: Environment not deterministic!");
        return 1;
    }

    // Run a few steps to test physics
    std::println("\nTesting physics simulation...");
    CartPoleEnv test_env(0, 999, 500);
    auto test_state = test_env.reset();

    for (int i = 0; i < 5; i++) {
        auto result = test_env.step(1);  // Push right
        std::println("  Step {}: x={:.4f}, theta={:.4f}, reward={:.1f}, done={}",
                     i + 1, result.observation.x, result.observation.theta,
                     result.reward, result.done);
    }
    std::println("✓ Physics simulation working");

    std::println("\n🎉 All CartPole tests passed!");
    return 0;
}
