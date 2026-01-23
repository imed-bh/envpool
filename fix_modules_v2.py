#!/usr/bin/env python3
"""Replace 'import std;' with traditional includes in C++20 modules - v2"""

import re
from pathlib import Path

# Headers to include in module preamble
HEADERS = """
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
"""

def fix_module(file_path):
    """Fix a single module file"""
    print(f"Processing: {file_path}")

    with open(file_path, 'r') as f:
        lines = f.readlines()

    # Find module; and export module lines
    module_idx = None
    export_idx = None
    import_std_idx = None

    for i, line in enumerate(lines):
        if line.strip() == 'module;':
            module_idx = i
        elif line.startswith('export module '):
            export_idx = i
        elif line.strip() == 'import std;':
            import_std_idx = i

    if module_idx is None or export_idx is None:
        print("  - Skipped (no module structure)")
        return False

    # Insert headers after module; and before export module
    if import_std_idx is not None:
        # Remove import std; line
        del lines[import_std_idx]
        if import_std_idx < export_idx:
            export_idx -= 1

    # Insert headers after module; line
    lines.insert(module_idx + 1, HEADERS)

    # Write back
    with open(file_path, 'w') as f:
        f.writelines(lines)

    print("  ✓ Fixed")
    return True

def main():
    print("Fixing C++20 modules to use traditional includes (v2)...\n")

    modules_dir = Path('envpool/modules')
    fixed_count = 0

    for cppm_file in sorted(modules_dir.rglob('*.cppm')):
        if fix_module(cppm_file):
            fixed_count += 1

    print(f"\n✓ Fixed {fixed_count} modules!")
    print("Modules now use traditional includes in module preamble")

if __name__ == '__main__':
    main()
