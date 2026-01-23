"""
Basic tests for EnvPool2 environments

These tests verify basic functionality of each environment.
"""

import pytest
import numpy as np


class TestCartPole:
    """Test CartPole environment"""

    def test_import(self):
        """Test that CartPole can be imported"""
        try:
            from envpool2 import CartPoleEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

    def test_create_and_reset(self):
        """Test creating and resetting CartPole"""
        try:
            from envpool2 import CartPoleEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)
        state = env.reset()

        assert hasattr(state, 'x'), "State should have x attribute"
        assert hasattr(state, 'x_dot'), "State should have x_dot attribute"
        assert hasattr(state, 'theta'), "State should have theta attribute"
        assert hasattr(state, 'theta_dot'), "State should have theta_dot attribute"

        obs = state.to_array()
        assert len(obs) == 4, "Observation should have 4 elements"

    def test_step(self):
        """Test stepping CartPole"""
        try:
            from envpool2 import CartPoleEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)
        env.reset()

        result = env.step(1)  # Action: push right

        assert hasattr(result, 'observation'), "Result should have observation"
        assert hasattr(result, 'reward'), "Result should have reward"
        assert hasattr(result, 'done'), "Result should have done"
        assert hasattr(result, 'truncated'), "Result should have truncated"

        assert result.reward == 1.0, "Reward should be 1.0"


class TestPendulum:
    """Test Pendulum environment"""

    def test_create_and_reset(self):
        """Test creating and resetting Pendulum"""
        try:
            from envpool2 import PendulumEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = PendulumEnv(env_id=0, seed=42)
        state = env.reset()

        obs = state.to_array()
        assert len(obs) == 3, "Observation should have 3 elements"

    def test_step_continuous(self):
        """Test stepping Pendulum with continuous action"""
        try:
            from envpool2 import PendulumEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = PendulumEnv(env_id=0, seed=42)
        env.reset()

        result = env.step(0.5)  # Continuous action

        assert result.reward <= 0.0, "Pendulum reward should be negative (cost)"


class TestMountainCar:
    """Test MountainCar environment"""

    def test_create_and_reset(self):
        """Test creating and resetting MountainCar"""
        try:
            from envpool2 import MountainCarEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = MountainCarEnv(env_id=0, seed=42)
        state = env.reset()

        obs = state.to_array()
        assert len(obs) == 2, "Observation should have 2 elements"

    def test_discrete_actions(self):
        """Test MountainCar discrete actions"""
        try:
            from envpool2 import MountainCarEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = MountainCarEnv(env_id=0, seed=42)
        env.reset()

        # Test all three actions
        for action in [0, 1, 2]:
            result = env.step(action)
            assert result.reward == -1.0, "MountainCar reward should be -1.0 per step"


class TestBlackjack:
    """Test Blackjack environment"""

    def test_create_and_reset(self):
        """Test creating and resetting Blackjack"""
        try:
            from envpool2 import BlackjackEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = BlackjackEnv(env_id=0, seed=42)
        state = env.reset()

        obs = state.to_array()
        assert len(obs) == 3, "Observation should have 3 elements"
        assert obs[0] >= 4, "Player sum should be at least 4"
        assert 1 <= obs[1] <= 10, "Dealer card should be 1-10"
        assert obs[2] in [0, 1], "Usable ace should be 0 or 1"


class TestTaxi:
    """Test Taxi environment"""

    def test_create_and_reset(self):
        """Test creating and resetting Taxi"""
        try:
            from envpool2 import TaxiEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = TaxiEnv(env_id=0, seed=42)
        state = env.reset()

        encoded = state.to_int()
        assert 0 <= encoded < 500, "Taxi state should be 0-499"

    def test_actions(self):
        """Test Taxi actions"""
        try:
            from envpool2 import TaxiEnv
        except ImportError:
            pytest.skip("envpool2 not built yet")

        env = TaxiEnv(env_id=0, seed=42)
        env.reset()

        # Test movement actions (0-3)
        for action in range(4):
            result = env.step(action)
            assert result.reward == -1.0, "Movement should have -1 reward"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
