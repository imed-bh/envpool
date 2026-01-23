# Building EnvPool2 - Phase 2 Migration

This guide covers building the new C++20 module-based environments with nanobind bindings.

## Quick Start

```bash
# Install dependencies
pip install scikit-build-core nanobind numpy gymnasium

# Build and install in development mode
pip install -e .

# Run tests
pytest tests/ -v
```

## Requirements

### System Requirements
- **CMake**: 3.28 or later
- **Compiler**:
  - GCC 14+ (recommended)
  - Clang 18+ (with libc++)
  - MSVC 19.35+ (Visual Studio 2022)
- **Python**: 3.8 or later

### Python Dependencies
```bash
pip install -r requirements-build.txt
```

Contents of `requirements-build.txt`:
```
scikit-build-core>=0.8.0
nanobind>=2.0.0
numpy>=1.19.0
gymnasium>=0.28.0
pytest>=7.0.0
```

## Detailed Build Instructions

### Option 1: Using pip (Recommended)

This is the easiest method and handles all the CMake configuration automatically:

```bash
# Clean build
pip install --no-build-isolation -e .

# With verbose output
pip install --no-build-isolation -ve .

# For release build
pip install .
```

### Option 2: Using scikit-build-core directly

```bash
python -m pip install --no-build-isolation -ve .
```

### Option 3: Manual CMake Build

For development or debugging:

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake .. \
    -DCMAKE_CXX_STANDARD=23 \
    -DCMAKE_BUILD_TYPE=Release \
    -DENVPOOL_BUILD_TESTS=OFF \
    -DENVPOOL_BUILD_PYTHON=OFF

# Build
cmake --build . -j$(nproc)

# The bindings will be in build/envpool/bindings/
```

## Build Configuration

### CMake Options

The build can be customized with these CMake options:

```bash
cmake .. \
    -DCMAKE_CXX_STANDARD=23 \              # Use C++23
    -DCMAKE_BUILD_TYPE=Release \            # Release or Debug
    -DENVPOOL_BUILD_TESTS=OFF \             # Don't build old tests
    -DENVPOOL_BUILD_PYTHON=OFF              # Don't build old Python bindings
```

### Environment Variables

You can also set environment variables:

```bash
export CMAKE_BUILD_TYPE=Release
export CMAKE_CXX_STANDARD=23
pip install -e .
```

## Troubleshooting

### Issue: "C++20 modules not supported"

**Solution**: Ensure you have a recent compiler:
```bash
# Check GCC version (need 14+)
g++ --version

# Check Clang version (need 18+)
clang++ --version

# On Ubuntu, install GCC 14
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-14 g++-14

# Set as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
```

### Issue: "nanobind not found"

**Solution**: Install nanobind first:
```bash
pip install nanobind
```

Or let CMake fetch it automatically (configured in `envpool/bindings/CMakeLists.txt`).

### Issue: "Module std not found"

**Solution**: Make sure your compiler supports C++23 standard library modules:
```bash
# GCC 14+ required
# Clang 18+ required with libc++
```

### Issue: Import error when running Python

**Solution**: Make sure the build completed successfully and the module is in the Python path:
```bash
# Check if modules were built
ls build/python/envpool2/

# Should see:
# envpool2_classic_control.*.so
# envpool2_toy_text.*.so

# If not there, rebuild
pip install -e . --force-reinstall
```

### Issue: CMake can't find Python

**Solution**: Specify Python explicitly:
```bash
cmake .. -DPython_EXECUTABLE=$(which python3)
```

## Building on Different Platforms

### Linux (Ubuntu 22.04+)

```bash
# Install dependencies
sudo apt update
sudo apt install cmake ninja-build gcc-14 g++-14 python3-dev

# Build
pip install -e .
```

### macOS (with Homebrew)

```bash
# Install dependencies
brew install cmake ninja gcc@14 python@3.11

# Build with GCC
export CC=gcc-14
export CXX=g++-14
pip install -e .
```

### Windows (with Visual Studio 2022)

```powershell
# Open Visual Studio 2022 Developer Command Prompt

# Build
pip install -e .
```

## Verifying the Build

After building, verify everything works:

```bash
# Test imports
python -c "import envpool2; print(envpool2.__version__)"

# List available environments
python -c "import envpool2; print(envpool2.__all__)"

# Quick test
python -c "
import envpool2
env = envpool2.CartPoleEnv(seed=42)
state = env.reset()
print('CartPole initial state:', state.to_array())
"

# Run full test suite
pytest tests/ -v
```

## Development Build

For faster iteration during development:

```bash
# Build with debug symbols
export CMAKE_BUILD_TYPE=Debug
pip install -e . --no-build-isolation

# Or use ccache for faster rebuilds
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
pip install -e . --no-build-isolation
```

## Building Documentation

```bash
# Install doc dependencies
pip install sphinx sphinx-rtd-theme

# Build docs
cd docs
make html

# View in browser
open _build/html/index.html
```

## Performance Tips

1. **Use Release builds**: Always use `-DCMAKE_BUILD_TYPE=Release` for production
2. **Enable LTO**: Add `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`
3. **Native optimization**: Add `-DCMAKE_CXX_FLAGS="-march=native"`
4. **Parallel build**: Use `-j$(nproc)` with CMake

Example:
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_CXX_FLAGS="-march=native"
cmake --build . -j$(nproc)
```

## Clean Build

If you encounter issues, try a clean build:

```bash
# Remove build artifacts
pip uninstall envpool2
rm -rf build dist *.egg-info

# Clean build
pip install -e . --no-build-isolation
```

## Next Steps

After building successfully:
1. Run the test suite: `pytest tests/ -v`
2. Try the examples in `MIGRATION_PHASE2.md`
3. Compare performance with original envpool
4. Report any issues on GitHub

## Support

For build issues:
1. Check this guide first
2. Check the GitHub issues
3. Open a new issue with:
   - Your OS and version
   - Compiler version
   - Full error message
   - Build command used
