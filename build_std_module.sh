#!/bin/bash
# Build the std module for libc++

mkdir -p build_clang20/std_module
cd build_clang20/std_module

# Compile std module
clang++-20 -std=c++23 -stdlib=libc++ -fmodules \
  -c /usr/lib/llvm-20/share/libc++/v1/std.cppm \
  -Xclang -emit-module-interface \
  -o std.pcm

echo "std module built: $(pwd)/std.pcm"
