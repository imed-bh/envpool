#!/bin/bash
# Manual build script for C++20 modules test
# Works around CMake module scanning limitations with GCC 13.3

set -e  # Exit on error

echo "=== Manual C++20 Module Build & Test ==="
echo

# Create output directory
mkdir -p manual_build
cd manual_build

# Compiler flags
CXX="g++"
CXXFLAGS="-std=c++23 -fmodules-ts -O2"
MODULE_DIR="../envpool/modules"

echo "Step 1: Building core.types module..."
$CXX $CXXFLAGS -c -x c++ $MODULE_DIR/core/types.cppm -o types.o

echo "Step 2: Building core.errors module..."
$CXX $CXXFLAGS -c -x c++ $MODULE_DIR/core/errors.cppm -o errors.o

echo "Step 3: Building classic_control.cartpole module..."
$CXX $CXXFLAGS -c -x c++ $MODULE_DIR/env/classic_control/cartpole.cppm -o cartpole.o

echo "Step 4: Building toy_text.blackjack module..."
$CXX $CXXFLAGS -c -x c++ $MODULE_DIR/env/toy_text/blackjack.cppm -o blackjack.o

echo "Step 5: Compiling CartPole test..."
$CXX $CXXFLAGS -c ../tests/cpp/test_cartpole.cpp -o test_cartpole.o

echo "Step 6: Linking CartPole test..."
$CXX $CXXFLAGS test_cartpole.o cartpole.o types.o errors.o -o test_cartpole

echo "Step 7: Compiling Blackjack test..."
$CXX $CXXFLAGS -c ../tests/cpp/test_blackjack.cpp -o test_blackjack.o

echo "Step 8: Linking Blackjack test..."
$CXX $CXXFLAGS test_blackjack.o blackjack.o types.o errors.o -o test_blackjack

echo
echo "✓ Build complete!"
echo
echo "Running tests..."
echo

echo "--- CartPole Test ---"
./test_cartpole
echo

echo "--- Blackjack Test ---"
./test_blackjack
echo

echo "🎉 All tests passed!"
