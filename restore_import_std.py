#!/usr/bin/env python3
"""Restore 'import std;' in C++20 modules"""

import re
from pathlib import Path

def restore_module(file_path):
    """Restore a single module file to use import std"""
    print(f"Processing: {file_path}")

    with open(file_path, 'r') as f:
        content = f.read()

    # Check if file has the #include block we added
    if '#include <algorithm>' not in content:
        print("  - Skipped (no includes to remove)")
        return False

    # Pattern to match module; followed by includes and export module
    # We want to replace it with: module;\nexport module X;\nimport std;

    # Find the includes block between module; and export module
    pattern = r'module;\s*\n(?:#include[^\n]+\n)+\s*export module ([^;]+);'

    def replacement(match):
        module_name = match.group(1)
        return f'module;\n\nexport module {module_name};\n\nimport std;'

    new_content = re.sub(pattern, replacement, content)

    if new_content == content:
        print("  - No changes made")
        return False

    # Write back
    with open(file_path, 'w') as f:
        f.write(new_content)

    print("  ✓ Restored import std")
    return True

def main():
    print("Restoring 'import std;' in C++20 modules...\n")

    modules_dir = Path('envpool/modules')
    fixed_count = 0

    for cppm_file in sorted(modules_dir.rglob('*.cppm')):
        if restore_module(cppm_file):
            fixed_count += 1

    print(f"\n✓ Restored {fixed_count} modules!")
    print("Modules now use 'import std;' again")

if __name__ == '__main__':
    main()
