# EnvPool CMake + Conan Build Guide

This guide explains how to build EnvPool using the modern CMake + Conan build system.

## Prerequisites

### Required

- **CMake** 3.25 or later
- **Conan** 2.0 or later
- **C++ Compiler** with C++23/26 support:
  - GCC 13+ (recommended: GCC 14 for full C++26)
  - Clang 17+ (recommended: Clang 18 for full C++26)
  - MSVC 19.38+ (Visual Studio 2022 17.8+)
- **Ninja** (recommended) or Make
- **Python** 3.8+ (for Python bindings)
- **Git**

### Optional

- **CUDA** 11.0+ (for GPU acceleration)
- **Qt5** (for ViZDoom visualization)

---

## Quick Start

### 1. Install Dependencies

#### Ubuntu/Debian

```bash
# Install CMake, Ninja, and compilers
sudo apt update
sudo apt install -y cmake ninja-build g++-14 python3-pip git

# Install Conan
pip3 install conan>=2.0

# Configure Conan profile
conan profile detect --force
```

#### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja gcc@14 python3

# Install Conan
pip3 install conan>=2.0

# Configure Conan profile
conan profile detect --force
```

#### Windows

```powershell
# Install using Chocolatey
choco install cmake ninja python git visualstudio2022buildtools

# Install Conan
pip install conan>=2.0

# Configure Conan profile
conan profile detect --force
```

---

### 2. Clone Repository

```bash
git clone https://github.com/sail-sg/envpool.git
cd envpool
```

---

### 3. Install Conan Dependencies

```bash
# Install all dependencies for Release build
conan install . --output-folder=build/release --build=missing \
    --settings=build_type=Release

# For Debug build with sanitizers
conan install . --output-folder=build/debug --build=missing \
    --settings=build_type=Debug
```

**Options**: You can customize the build with Conan options:

```bash
# Minimal build (no Atari, MuJoCo, etc.)
conan install . --output-folder=build/minimal --build=missing \
    -o build_atari=False \
    -o build_mujoco=False \
    -o build_vizdoom=False \
    -o build_procgen=False

# Modern C++26 implementation
conan install . --output-folder=build/modern --build=missing \
    -o use_modern_impl=True
```

---

### 4. Configure with CMake

#### Using CMake Presets (Recommended)

```bash
# List available presets
cmake --list-presets

# Configure for release
cmake --preset=release

# Configure for debug
cmake --preset=debug

# Configure for modern C++26
cmake --preset=modern
```

#### Manual Configuration

```bash
# Release build
cmake -B build/release -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=build/release/generators/conan_toolchain.cmake \
    -G Ninja

# Debug build
cmake -B build/debug -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=build/debug/generators/conan_toolchain.cmake \
    -G Ninja
```

---

### 5. Build

```bash
# Build release
cmake --build --preset=release

# Or manually
cmake --build build/release --parallel

# Build debug
cmake --build --preset=debug
```

---

### 6. Run Tests

```bash
# Run all tests
ctest --preset=default

# Or manually
cd build/release && ctest --output-on-failure

# Run specific test
./build/release/envpool/core/action_buffer_queue_test
```

---

### 7. Install

```bash
# Install to system (requires sudo on Linux/macOS)
sudo cmake --install build/release

# Install to custom prefix
cmake --install build/release --prefix=/path/to/install
```

---

## Build Options

Configure these options with `-D` flag or in CMake presets:

| Option | Default | Description |
|--------|---------|-------------|
| `ENVPOOL_BUILD_TESTS` | ON | Build unit tests |
| `ENVPOOL_BUILD_BENCHMARKS` | ON | Build performance benchmarks |
| `ENVPOOL_BUILD_PYTHON` | ON | Build Python bindings |
| `ENVPOOL_BUILD_ATARI` | ON | Build Atari environments |
| `ENVPOOL_BUILD_MUJOCO` | ON | Build MuJoCo environments |
| `ENVPOOL_BUILD_VIZDOOM` | OFF | Build ViZDoom environments |
| `ENVPOOL_BUILD_PROCGEN` | OFF | Build Procgen environments |
| `ENVPOOL_ENABLE_CUDA` | OFF | Enable CUDA support |
| `ENVPOOL_USE_MODERN_IMPL` | ON | Use modern C++26 refactored code |
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug/Release/RelWithDebInfo) |

**Example**:

```bash
cmake --preset=release \
    -DENVPOOL_BUILD_VIZDOOM=ON \
    -DENVPOOL_ENABLE_CUDA=ON
```

---

## CMake Presets

Pre-configured presets for common scenarios:

| Preset | Description |
|--------|-------------|
| `debug` | Debug build with Address/UB sanitizers |
| `release` | Optimized release build |
| `relwithdebinfo` | Release with debug symbols (for profiling) |
| `minimal` | Minimal build (core only, no extra envs) |
| `modern` | Modern C++26 implementation |
| `cuda` | CUDA-enabled build |

**Workflow presets** (configure + build + test):

```bash
# Full debug workflow
cmake --workflow --preset=debug

# Full release workflow
cmake --workflow --preset=release
```

---

## Development Workflow

### 1. Code Changes

```bash
# Make your changes
vim envpool/core/circular_buffer_modern.h

# Reconfigure (if CMakeLists.txt changed)
cmake --preset=debug

# Rebuild (incremental)
cmake --build --preset=debug

# Run specific test
./build/debug/envpool/core/circular_buffer_modern_test
```

### 2. Running Benchmarks

```bash
# Build benchmarks
cmake --build build/release --target circular_buffer_benchmark

# Run benchmark
./build/release/envpool/core/circular_buffer_benchmark

# Run with custom options
./build/release/envpool/core/circular_buffer_benchmark \
    --benchmark_filter=BM_CircularBuffer_Modern.* \
    --benchmark_repetitions=10 \
    --benchmark_out=results.json \
    --benchmark_out_format=json
```

### 3. Code Coverage (Debug build)

```bash
# Configure with coverage
cmake --preset=debug -DCMAKE_CXX_FLAGS="--coverage"

# Build and run tests
cmake --build --preset=debug
ctest --preset=default

# Generate coverage report
lcov --capture --directory build/debug --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# Open report
xdg-open coverage_report/index.html
```

### 4. Sanitizers

Sanitizers are automatically enabled in Debug builds:

```bash
# Address Sanitizer + Undefined Behavior Sanitizer
cmake --preset=debug
cmake --build --preset=debug
./build/debug/envpool/core/action_buffer_queue_test

# Thread Sanitizer (requires separate build)
cmake -B build/tsan -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
    -DCMAKE_TOOLCHAIN_FILE=build/debug/generators/conan_toolchain.cmake

cmake --build build/tsan
./build/tsan/envpool/core/action_buffer_queue_test
```

---

## Python Bindings

Build and install Python bindings:

```bash
# Build with Python support
cmake --preset=release -DENVPOOL_BUILD_PYTHON=ON
cmake --build --preset=release

# Install Python package (editable mode for development)
pip install -e .

# Or build wheel
python setup.py bdist_wheel
pip install dist/envpool-*.whl
```

---

## Troubleshooting

### Conan Dependency Resolution

If Conan fails to resolve dependencies:

```bash
# Update Conan remotes
conan remote list
conan remote add conancenter https://center.conan.io

# Clear cache and reinstall
conan remove "*" --confirm
conan install . --output-folder=build/release --build=missing
```

### Compiler Not Found

Specify compiler explicitly:

```bash
# Using GCC
export CC=gcc-14
export CXX=g++-14

# Using Clang
export CC=clang-18
export CXX=clang++-18

# Then configure
cmake --preset=release
```

### CMake Can't Find Conan Toolchain

Ensure toolchain file path is correct:

```bash
# Check file exists
ls build/release/generators/conan_toolchain.cmake

# If missing, run conan install again
conan install . --output-folder=build/release --build=missing
```

### C++23/26 Features Not Available

Check compiler version:

```bash
g++ --version  # Should be 13+ (14+ for C++26)
clang++ --version  # Should be 17+ (18+ for C++26)
```

If too old, install newer compiler and update Conan profile:

```bash
conan profile detect --force
conan profile show default

# Edit profile if needed
conan profile path default
```

---

## Performance Tips

### 1. Use Release Build

```bash
cmake --preset=release  # Always for benchmarks!
```

### 2. Enable Link-Time Optimization (LTO)

```bash
cmake --preset=release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### 3. Native Architecture Optimizations

Already enabled via `-march=native`. To target specific CPU:

```bash
cmake --preset=release -DCMAKE_CXX_FLAGS="-march=skylake"
```

### 4. Profile-Guided Optimization (PGO)

```bash
# 1. Build instrumented binary
cmake -B build/pgo-gen -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-fprofile-generate" \
    -DCMAKE_TOOLCHAIN_FILE=build/release/generators/conan_toolchain.cmake

cmake --build build/pgo-gen

# 2. Run workload to generate profile data
./build/pgo-gen/envpool/core/circular_buffer_benchmark

# 3. Rebuild with profile data
cmake -B build/pgo-use -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-fprofile-use" \
    -DCMAKE_TOOLCHAIN_FILE=build/release/generators/conan_toolchain.cmake

cmake --build build/pgo-use
```

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y cmake ninja-build g++-14
          pip install conan>=2.0

      - name: Configure Conan
        run: conan profile detect --force

      - name: Install Conan dependencies
        run: conan install . --output-folder=build --build=missing

      - name: Configure CMake
        run: cmake --preset=release

      - name: Build
        run: cmake --build --preset=release

      - name: Test
        run: ctest --preset=release

      - name: Benchmark
        run: ./build/release/envpool/core/circular_buffer_benchmark
```

---

## Comparison: Bazel vs CMake

| Feature | Bazel | CMake + Conan |
|---------|-------|---------------|
| **Configuration** | BUILD files | CMakeLists.txt |
| **Dependencies** | http_archive | Conan Center |
| **Learning Curve** | Steep | Moderate |
| **IDE Support** | Limited | Excellent |
| **Build Speed** | Fast (caching) | Fast (ccache) |
| **Ecosystem** | Google-centric | Universal |
| **Windows Support** | Good | Excellent |

### Migration Benefits

1. **Better IDE Integration** - CLion, VSCode, Visual Studio all work great
2. **Standard Tooling** - CMake is industry standard
3. **Easier Dependencies** - Conan Center has 1000+ packages
4. **Simpler Workflow** - No custom Bazel rules
5. **Better Debugging** - Standard tools (GDB, LLDB, MSVC debugger)

---

## Next Steps

1. Read [ASYNC_ARCHITECTURE.md](./ASYNC_ARCHITECTURE.md) to understand the core design
2. Check [CPP26_REFACTORING_PLAN.md](./CPP26_REFACTORING_PLAN.md) for modernization strategy
3. Run benchmarks to verify performance
4. Contribute to the modern C++26 refactoring!

---

## Support

- **Issues**: https://github.com/sail-sg/envpool/issues
- **Discussions**: https://github.com/sail-sg/envpool/discussions
- **Documentation**: https://envpool.readthedocs.io/

---

**Happy Building!** 🚀
