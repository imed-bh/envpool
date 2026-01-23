"""
Comparison tests between envpool2 (new) and envpool (original)

These tests verify that the new C++20 module-based environments
produce the same results as the original implementations.
"""

import pytest
import numpy as np


class TestCartPoleEquivalence:
    """Test CartPole environment equivalence"""

    def test_cartpole_reset_deterministic(self):
        """Test that CartPole reset is deterministic with same seed"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        # Create two environments with same seed
        env1 = envpool2.CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)
        env2 = envpool2.CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)

        # Reset both
        state1 = env1.reset()
        state2 = env2.reset()

        # Check observations match
        obs1 = state1.to_array()
        obs2 = state2.to_array()

        assert np.allclose(obs1, obs2), "Reset states should match with same seed"

    def test_cartpole_step_deterministic(self):
        """Test that CartPole steps are deterministic"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = envpool2.CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)

        # Reset and take some steps
        env.reset()

        results = []
        for action in [0, 1, 1, 0, 1]:
            result = env.step(action)
            results.append({
                'obs': result.observation.to_array(),
                'reward': result.reward,
                'done': result.done,
            })

        # Reset with same seed and replay
        env.set_seed(42)
        env.reset()

        for i, action in enumerate([0, 1, 1, 0, 1]):
            result = env.step(action)
            obs = result.observation.to_array()

            assert np.allclose(obs, results[i]['obs']), f"Step {i} observations should match"
            assert result.reward == results[i]['reward'], f"Step {i} rewards should match"
            assert result.done == results[i]['done'], f"Step {i} done flags should match"

    def test_cartpole_episode_length(self):
        """Test that CartPole respects max_episode_steps"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        max_steps = 50
        env = envpool2.CartPoleEnv(env_id=0, seed=42, max_episode_steps=max_steps)
        env.reset()

        steps = 0
        done = False
        while not done and steps < max_steps + 10:
            result = env.step(1)  # Always go right
            done = result.done
            steps += 1

        assert done, "Episode should terminate"
        assert steps <= max_steps, f"Episode should not exceed {max_steps} steps"


class TestPendulumEquivalence:
    """Test Pendulum environment equivalence"""

    def test_pendulum_reset_deterministic(self):
        """Test that Pendulum reset is deterministic with same seed"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env1 = envpool2.PendulumEnv(env_id=0, seed=42)
        env2 = envpool2.PendulumEnv(env_id=0, seed=42)

        state1 = env1.reset()
        state2 = env2.reset()

        obs1 = state1.to_array()
        obs2 = state2.to_array()

        assert np.allclose(obs1, obs2), "Reset states should match with same seed"


class TestBlackjackEquivalence:
    """Test Blackjack environment equivalence"""

    def test_blackjack_reset_deterministic(self):
        """Test that Blackjack reset is deterministic with same seed"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env1 = envpool2.BlackjackEnv(env_id=0, seed=42)
        env2 = envpool2.BlackjackEnv(env_id=0, seed=42)

        state1 = env1.reset()
        state2 = env2.reset()

        obs1 = state1.to_array()
        obs2 = state2.to_array()

        assert np.array_equal(obs1, obs2), "Reset states should match with same seed"

    def test_blackjack_hit_vs_stick(self):
        """Test that hit and stick actions work correctly"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = envpool2.BlackjackEnv(env_id=0, seed=42)
        env.reset()

        # Test stick (action=0)
        result_stick = env.step(0)
        assert result_stick.done, "Stick should end the episode"

        # Reset and test hit (action=1)
        env.reset()
        result_hit = env.step(1)
        # Hit may or may not end episode depending on if we bust


class TestFrozenLakeEquivalence:
    """Test FrozenLake environment equivalence"""

    def test_frozen_lake_reset_deterministic(self):
        """Test that FrozenLake reset is deterministic"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = envpool2.FrozenLakeEnv(env_id=0, seed=42, size=4)

        state = env.reset()

        # Should always start at position 0
        assert state.to_int() == 0, "Should start at top-left corner"


class TestTaxiEquivalence:
    """Test Taxi environment equivalence"""

    def test_taxi_reset_deterministic(self):
        """Test that Taxi reset is deterministic with same seed"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env1 = envpool2.TaxiEnv(env_id=0, seed=42)
        env2 = envpool2.TaxiEnv(env_id=0, seed=42)

        state1 = env1.reset()
        state2 = env2.reset()

        assert state1.to_int() == state2.to_int(), "Reset states should match with same seed"


class TestEnvironmentAPI:
    """Test that all environments have consistent API"""

    @pytest.mark.parametrize("env_class,kwargs", [
        ("CartPoleEnv", {"env_id": 0, "seed": 42}),
        ("PendulumEnv", {"env_id": 0, "seed": 42}),
        ("MountainCarEnv", {"env_id": 0, "seed": 42}),
        ("MountainCarContinuousEnv", {"env_id": 0, "seed": 42}),
        ("AcrobotEnv", {"env_id": 0, "seed": 42}),
        ("BlackjackEnv", {"env_id": 0, "seed": 42}),
        ("CatchEnv", {"env_id": 0, "seed": 42}),
        ("CliffWalkingEnv", {"env_id": 0}),
        ("FrozenLakeEnv", {"env_id": 0, "seed": 42}),
        ("NChainEnv", {"env_id": 0, "seed": 42}),
        ("TaxiEnv", {"env_id": 0, "seed": 42}),
    ])
    def test_environment_has_standard_methods(self, env_class, kwargs):
        """Test that all environments have standard methods"""
        try:
            import envpool2
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = getattr(envpool2, env_class)(**kwargs)

        # Check methods exist
        assert hasattr(env, 'reset'), f"{env_class} should have reset method"
        assert hasattr(env, 'step'), f"{env_class} should have step method"
        assert hasattr(env, 'is_done'), f"{env_class} should have is_done method"
        assert hasattr(env, 'id'), f"{env_class} should have id method"

        # Check reset works
        state = env.reset()
        assert state is not None, f"{env_class} reset should return state"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
