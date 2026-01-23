#!/usr/bin/env python3
"""Restore 'import std;' in C++20 modules - v2"""

from pathlib import Path

def restore_module(file_path):
    """Restore a single module file to use import std"""
    print(f"Processing: {file_path}")

    with open(file_path, 'r') as f:
        lines = f.readlines()

    # Find module; line
    module_idx = None
    export_idx = None
    includes_start = None
    includes_end = None

    for i, line in enumerate(lines):
        if line.strip() == 'module;':
            module_idx = i
        elif line.startswith('#include'):
            if includes_start is None:
                includes_start = i
            includes_end = i
        elif line.startswith('export module '):
            export_idx = i
            break

    if module_idx is None or export_idx is None:
        print("  - Skipped (no module structure)")
        return False

    if includes_start is None:
        print("  - Skipped (no includes to remove)")
        return False

    # Remove all include lines and the empty line/comment before export
    # Find the line with "// Import C++ standard library" comment if it exists
    comment_idx = None
    for i in range(includes_end + 1, export_idx):
        if '// Import C++ standard library' in lines[i]:
            comment_idx = i
            break

    # Build new content
    new_lines = []
    new_lines.extend(lines[:module_idx + 1])  # Up to and including "module;"
    new_lines.append('\n')
    new_lines.append('export module ' + lines[export_idx].split('export module ')[1])
    new_lines.append('\n')
    new_lines.append('import std;\n')

    # Add everything after export module line
    new_lines.extend(lines[export_idx + 1:])

    # Write back
    with open(file_path, 'w') as f:
        f.writelines(new_lines)

    print("  ✓ Restored import std")
    return True

def main():
    print("Restoring 'import std;' in C++20 modules (v2)...\n")

    modules_dir = Path('envpool/modules')
    fixed_count = 0

    for cppm_file in sorted(modules_dir.rglob('*.cppm')):
        if restore_module(cppm_file):
            fixed_count += 1

    print(f"\n✓ Restored {fixed_count} modules!")
    print("Modules now use 'import std;' again")

if __name__ == '__main__':
    main()
