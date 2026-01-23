#!/usr/bin/env python3
"""Replace 'import std;' with traditional includes in C++20 modules"""

import re
from pathlib import Path

# Headers to include in module preamble
HEADERS = """module;

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

export"""

def fix_module(file_path):
    """Fix a single module file"""
    print(f"Processing: {file_path}")

    with open(file_path, 'r') as f:
        content = f.read()

    # Check if file contains "import std;"
    if 'import std;' not in content:
        print("  - Skipped (no 'import std;')")
        return False

    # Replace "module;\nexport module X;\nimport std;" pattern
    # with "module;\n#includes...\nexport module X;"
    pattern = r'module;\s*\nexport module ([^;]+);\s*\nimport std;'
    replacement = HEADERS + r' module \1;'

    new_content = re.sub(pattern, replacement, content)

    # Also remove any standalone "import std;" lines
    new_content = new_content.replace('\nimport std;\n', '\n')

    # Write back
    with open(file_path, 'w') as f:
        f.write(new_content)

    print("  ✓ Fixed")
    return True

def main():
    print("Fixing C++20 modules to use traditional includes...\n")

    modules_dir = Path('envpool/modules')
    fixed_count = 0

    for cppm_file in modules_dir.rglob('*.cppm'):
        if fix_module(cppm_file):
            fixed_count += 1

    print(f"\n✓ Fixed {fixed_count} modules!")
    print("Modules now use traditional includes in module preamble")

if __name__ == '__main__':
    main()
