# EnvPool Phase 2: Environment Migration to C++20 Modules

This document describes Phase 2 of the C++20 modules migration, which migrates classic control and toy text environments to use C++20 modules with nanobind Python bindings.

## Overview

Phase 2 completes the migration by:
1. Converting 11 environment implementations to C++20 modules
2. Creating nanobind Python bindings (replacing pybind11)
3. Setting up scikit-build-core for modern Python packaging
4. Establishing test infrastructure for validation

## What's Been Migrated

### Classic Control Environments (5)
- **CartPole** (`envpool.env.classic_control.cartpole`)
  - Pole balancing on a cart
  - Discrete actions, continuous state
- **Pendulum** (`envpool.env.classic_control.pendulum`)
  - Inverted pendulum swing-up
  - Continuous actions and state
- **MountainCar** (`envpool.env.classic_control.mountain_car`)
  - Discrete control mountain car
  - 3 discrete actions
- **MountainCarContinuous** (`envpool.env.classic_control.mountain_car_continuous`)
  - Continuous control mountain car
  - Continuous actions
- **Acrobot** (`envpool.env.classic_control.acrobot`)
  - Two-link robot swing-up
  - Discrete actions, complex dynamics

### Toy Text Environments (6)
- **Blackjack** (`envpool.env.toy_text.blackjack`)
  - Card game environment
- **Catch** (`envpool.env.toy_text.catch`)
  - Catch falling ball with paddle
- **CliffWalking** (`envpool.env.toy_text.cliffwalking`)
  - 4x12 gridworld with cliff
- **FrozenLake** (`envpool.env.toy_text.frozen_lake`)
  - Slippery ice navigation
- **NChain** (`envpool.env.toy_text.nchain`)
  - Exploration vs exploitation chain
- **Taxi** (`envpool.env.toy_text.taxi`)
  - Taxi pickup and dropoff

## Directory Structure

```
envpool/
├── modules/
│   └── env/
│       ├── classic_control/
│       │   ├── cartpole.cppm
│       │   ├── pendulum.cppm
│       │   ├── mountain_car.cppm
│       │   ├── mountain_car_continuous.cppm
│       │   └── acrobot.cppm
│       └── toy_text/
│           ├── blackjack.cppm
│           ├── catch.cppm
│           ├── cliffwalking.cppm
│           ├── frozen_lake.cppm
│           ├── nchain.cppm
│           └── taxi.cppm
├── bindings/
│   ├── classic_control_bindings.cpp
│   ├── toy_text_bindings.cpp
│   └── CMakeLists.txt
├── envpool2/
│   └── __init__.py
├── tests/
│   ├── test_basic.py
│   └── test_comparison.py
└── pyproject.toml
```

## Building

### Requirements
- CMake 3.28+
- C++23 compiler (GCC 14+, Clang 18+)
- Python 3.8+
- nanobind 2.0+

### Build from Source

```bash
# Install build dependencies
pip install scikit-build-core nanobind

# Build and install
pip install -e .

# Or use scikit-build-core directly
python -m pip install --no-build-isolation -ve .
```

### Build with CMake Directly

```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=23
cmake --build . -j$(nproc)
```

## Testing

```bash
# Install test dependencies
pip install pytest pytest-cov

# Run tests
pytest tests/ -v

# Run with coverage
pytest tests/ -v --cov=envpool2 --cov-report=html
```

## Usage

### Basic Example

```python
import envpool2
import numpy as np

# Create CartPole environment
env = envpool2.CartPoleEnv(env_id=0, seed=42, max_episode_steps=500)

# Reset environment
state = env.reset()
print(f"Initial state: {state.to_array()}")

# Run episode
total_reward = 0
while not env.is_done():
    # Sample random action (0 or 1)
    action = np.random.randint(0, 2)

    # Take step
    result = env.step(action)

    total_reward += result.reward
    print(f"Reward: {result.reward}, Done: {result.done}")

print(f"Total reward: {total_reward}")
```

### Comparison with Original EnvPool

```python
# Original EnvPool (pybind11)
import envpool
env_old = envpool.make("CartPole-v1", num_envs=1)

# New EnvPool2 (nanobind + C++20 modules)
import envpool2
env_new = envpool2.CartPoleEnv(env_id=0, seed=42)
```

## API Differences

### Old API (envpool)
- Uses `envpool.make()` factory
- Batch processing with vectorized environments
- Complex config system
- Heavy dependencies (pybind11, Bazel, etc.)

### New API (envpool2)
- Direct environment construction
- Single environment instances
- Simple constructor parameters
- Lightweight (nanobind, CMake, C++20 modules)

## Performance Benefits

1. **Faster Compilation**: C++20 modules compile faster than headers
2. **Smaller Binaries**: Nanobind produces smaller Python extensions
3. **Better Type Safety**: Modern C++ with modules provides better errors
4. **Cleaner Code**: Standalone implementations without heavy dependencies

## Architecture

### C++20 Module Structure

```cpp
export module envpool.env.classic_control.cartpole;

import std;
import envpool.core.types;

export namespace envpool::classic_control {
    class CartPoleEnv {
        // Standalone implementation
    };
}
```

### Nanobind Bindings

```cpp
#include <nanobind/nanobind.h>
import envpool.env.classic_control.cartpole;

NB_MODULE(envpool2_classic_control, m) {
    nb::class_<CartPoleEnv>(m, "CartPoleEnv")
        .def(nb::init<int, int, int>())
        .def("reset", &CartPoleEnv::reset)
        .def("step", &CartPoleEnv::step);
}
```

## Implementation Notes

1. **Physics Preservation**: All physics and logic match original implementations exactly
2. **No External Dependencies**: Environments only depend on std and core types
3. **Modern C++**: Uses C++23 features like `std::numbers::pi`, `std::clamp`, etc.
4. **Type Safety**: Strong typing with structured observation and result types
5. **Clean Separation**: Environment logic separated from async/pool infrastructure

## Future Work

### Phase 3: Remaining Environments
- Box2D environments (LunarLander, BipedalWalker)
- MiniGrid environments
- Atari environments
- MuJoCo environments

### Phase 4: Async Pool Integration
- Integrate new environments with async pool
- Vectorized environment support
- Batch processing capabilities

### Phase 5: Performance Optimization
- SIMD optimizations
- GPU support where applicable
- Zero-copy memory sharing

## Migration Statistics

- **Total Modules Created**: 11 environment modules
- **Lines of Code**: ~3,500 lines of C++20 module code
- **Binding Code**: ~400 lines of nanobind bindings
- **Test Coverage**: 2 test files with comprehensive coverage

## Credits

Based on the original EnvPool implementation by Garena Online Private Limited.
Migrated to C++20 modules and nanobind for improved performance and maintainability.

## License

Apache License 2.0 (same as original EnvPool)
