#!/bin/bash
# Replace "import std;" with traditional headers in module preamble

set -e

echo "Fixing C++20 modules to use traditional includes..."

# Common headers needed
HEADERS='module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <print>
#include <random>
#include <semaphore>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

export'

# Find all .cppm files
find envpool/modules -name "*.cppm" | while read file; do
    echo "Processing: $file"

    # Check if file contains "import std;"
    if grep -q "import std;" "$file"; then
        # Replace "module;" + "export module" + "import std;" with the new pattern
        sed -i '0,/^module;/{
            s|^module;|'"${HEADERS}"'|
        }' "$file"

        # Remove the "import std;" line
        sed -i '/^import std;$/d' "$file"

        echo "  ✓ Fixed"
    else
        echo "  - Skipped (no 'import std;')"
    fi
done

echo ""
echo "✓ All modules fixed!"
echo "Modules now use traditional includes in module preamble"
