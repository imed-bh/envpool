#!/bin/bash
# Verification script for Phase 2 migration

echo "======================================"
echo "Phase 2 Migration Verification Script"
echo "======================================"
echo ""

success=0
failure=0

check_file() {
    if [ -f "$1" ]; then
        echo "✓ $1"
        ((success++))
    else
        echo "✗ $1 MISSING"
        ((failure++))
    fi
}

echo "Checking Classic Control Modules (5)..."
check_file "envpool/modules/env/classic_control/cartpole.cppm"
check_file "envpool/modules/env/classic_control/pendulum.cppm"
check_file "envpool/modules/env/classic_control/mountain_car.cppm"
check_file "envpool/modules/env/classic_control/mountain_car_continuous.cppm"
check_file "envpool/modules/env/classic_control/acrobot.cppm"
echo ""

echo "Checking Toy Text Modules (6)..."
check_file "envpool/modules/env/toy_text/blackjack.cppm"
check_file "envpool/modules/env/toy_text/catch.cppm"
check_file "envpool/modules/env/toy_text/cliffwalking.cppm"
check_file "envpool/modules/env/toy_text/frozen_lake.cppm"
check_file "envpool/modules/env/toy_text/nchain.cppm"
check_file "envpool/modules/env/toy_text/taxi.cppm"
echo ""

echo "Checking Nanobind Bindings..."
check_file "envpool/bindings/classic_control_bindings.cpp"
check_file "envpool/bindings/toy_text_bindings.cpp"
check_file "envpool/bindings/CMakeLists.txt"
echo ""

echo "Checking Build System..."
check_file "envpool/modules/CMakeLists.txt"
check_file "CMakeLists.txt"
check_file "pyproject.toml"
check_file "MANIFEST.in"
echo ""

echo "Checking Python Package..."
check_file "envpool2/__init__.py"
echo ""

echo "Checking Tests..."
check_file "tests/__init__.py"
check_file "tests/test_basic.py"
check_file "tests/test_comparison.py"
echo ""

echo "Checking Documentation..."
check_file "MIGRATION_PHASE2.md"
check_file "BUILD.md"
check_file "PHASE2_SUMMARY.md"
echo ""

echo "======================================"
echo "Summary:"
echo "  ✓ Success: $success files"
echo "  ✗ Missing: $failure files"
echo "======================================"

if [ $failure -eq 0 ]; then
    echo ""
    echo "🎉 Phase 2 Migration: ALL FILES VERIFIED!"
    echo ""
    echo "Next steps:"
    echo "  1. Build: pip install -e ."
    echo "  2. Test:  pytest tests/ -v"
    echo "  3. Read:  MIGRATION_PHASE2.md"
    exit 0
else
    echo ""
    echo "⚠️  Some files are missing. Please check the output above."
    exit 1
fi
