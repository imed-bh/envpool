"""
EnvPool2 - High-performance parallel RL environments with C++20 modules and nanobind

This is the next-generation EnvPool implementation using:
- C++20 modules for better compilation times and modularity
- Nanobind for fast, modern Python bindings
- Standalone environment implementations without heavy dependencies
"""

__version__ = "0.1.0"

# Import environment modules
try:
    from .envpool2_classic_control import (
        CartPoleEnv,
        PendulumEnv,
        MountainCarEnv,
        MountainCarContinuousEnv,
        AcrobotEnv,
    )
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import classic control environments: {e}")

try:
    from .envpool2_toy_text import (
        BlackjackEnv,
        CatchEnv,
        CliffWalkingEnv,
        FrozenLakeEnv,
        NChainEnv,
        TaxiEnv,
    )
except ImportError as e:
    import warnings
    warnings.warn(f"Failed to import toy text environments: {e}")

__all__ = [
    # Classic Control
    "CartPoleEnv",
    "PendulumEnv",
    "MountainCarEnv",
    "MountainCarContinuousEnv",
    "AcrobotEnv",
    # Toy Text
    "BlackjackEnv",
    "CatchEnv",
    "CliffWalkingEnv",
    "FrozenLakeEnv",
    "NChainEnv",
    "TaxiEnv",
]
